
#ifndef CP_DYSER_HH
#define CP_DYSER_HH

#include "cp_args.hh"
#include "cp_opdg_builder.hh"
#include "dyser_inst.hh"
#include "vectorization_legality.hh"
#include "sliceinfo.hh"

namespace DySER {

  class cp_dyser : public ArgumentHandler,
                   public VectorizationLegality,
                   public CP_OPDG_Builder<dg_event,
                                        dg_edge_impl_t<dg_event> > {

  protected:
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;

    enum MODEL_STATE {
      IN_CPU = 0,
      IN_DYSER = 1
    };


    // number of times dyser configured..
    unsigned _num_config = 0;
    // dyser size
    unsigned _dyser_size = 16;

    InstPtr createDyComputeInst(Op *op, uint64_t index) {
      InstPtr dy_compute = InstPtr(new dyser_inst(op->img,
                                                  index));
      if (op) {
        keepTrackOfInstOpMap(dy_compute, op);
      }
      return dy_compute;
    }

    InstPtr createDySendInst(Op *op) {
      InstPtr dy_send = InstPtr(new dyser_send());
      if (op) {
        keepTrackOfInstOpMap(dy_send, op);
      }
      return dy_send;
    }

    InstPtr createDyRecvInst(Op *op) {
      InstPtr dy_recv = InstPtr(new dyser_recv());
      if (op) {
        keepTrackOfInstOpMap(dy_recv, op);
      }
      return dy_recv;
    }

    void setDataDep(InstPtr &n) {
      for (int i = 0; i < 7; ++i) {
        int prod = n->_prod[i];
        if ((uint64_t)prod >= n->index()) {
          continue;
        }
        dg_inst_base<T,E>& depInst =
          getCPDG()->queryNodes(n->index()-prod);

        Inst_t *ptr = dynamic_cast<Inst_t*>(&depInst);
        assert(ptr);
        Op *op = getOpForInst(*ptr);
        InstPtr recentDepInst = getInstForOp(op);

        getCPDG()->insert_edge(*recentDepInst,
                               recentDepInst->eventComplete(),
                               *n, Inst_t::Ready,
                               0,
                               E_RDep);
      }
    }
    void setReadyToExecute(InstPtr &n) {
      getCPDG()->insert_edge(*n, Inst_t::Ready,
                             *n, Inst_t::Execute, 0,
                             E_RE);
    }
    void setExecuteToComplete(InstPtr &n) {
      getCPDG()->insert_edge(*n, Inst_t::Execute,
                             *n, Inst_t::Complete,
                             n->_ex_lat,
                             E_EP);

    }
    void setExecuteToExecute(InstPtr &P,
                             InstPtr &N) {
      getCPDG()->insert_edge(*P, Inst_t::Execute,
                             *N, Inst_t::Execute,
                             N->_ex_lat,
                             E_EE);
    }

  public:
    cp_dyser(): CP_OPDG_Builder<T, E> () {
    }
    virtual ~cp_dyser() {}

    virtual void traceOut(uint64_t index,
                          const CP_NodeDiskImage &img, Op* op) {
    }

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }
    dep_graph_impl_t<Inst_t, T, E> cpdg;

    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op)
    {
      bool shouldCompleteLoop = false;
      //ModelState prev_model_state = dyser_model_state;
      LoopInfo *li = getLoop(op);

      if (CurLoop != li) {
        // we switched to a different loop, complete DySER Loop
        if (canDySERize(CurLoop)) {
          completeDySERLoop(CurLoop, CurLoopIter);
        }
        CurLoop = li;
        CurLoopIter = 0;
        global_loop_iter = 0;
      } else if (!CurLoop) {
        // no loop
      } else if (CurLoop) {
        // Same loop
        if (op == op->bb()->lastOp() // last instruction in the bb
            && CurLoop->isLatch(op->bb())) // latch for current loop
          {
            ++CurLoopIter;
            ++global_loop_iter;
          }
        shouldCompleteLoop = shouldCompleteThisLoop(CurLoop,
                                                    CurLoopIter);
      }

      if (!canDySERize(li)) {
        // We are in the CPU mode
        // use the default insert_inst
        CP_DG_Builder<T, E>::insert_inst(img, index, op);
      } else {
        // Create the instruction, but add it to the loop trace.
        // but do not map op <-> inst.
        InstPtr inst = createInst(img, index, 0);
        getCPDG()->addInst(inst, index);

        trackDySERLoop(CurLoop, op, inst);
      }
      if (shouldCompleteLoop) {
        completeDySERLoop(CurLoop, CurLoopIter);
        CurLoopIter = 0; // reset loop counter.
      }
    }

    uint64_t numCycles() {
      if (CurLoop) {
        completeDySERLoop(CurLoop, CurLoopIter);
      }
      return _lastInst->finalCycle() + _num_config * 64;
    }

    void handle_argument(const char *name, const char *optarg) {
      if (strcmp(name, "dyser-size") == 0) {
        _dyser_size = atoi(optarg);
        if (_dyser_size < 16)
          _dyser_size = 16;
      }
    }



  protected:
    LoopInfo *CurLoop = 0;
    unsigned CurLoopIter = 0;
    uint64_t global_loop_iter = 0;

    SliceInfo *SI = 0;
    bool DySERizingLoop = false;
    bool useOpDependence() const {
      return DySERizingLoop;
    }
    virtual bool canDySERize(LoopInfo *li) {
      // all inner loops are candidate for dyserization.
      return (li && li->isInnerLoop());
    }

    virtual bool shouldCompleteThisLoop(LoopInfo *CurLoop,
                                        unsigned CurLoopIter)
    {
      if (!CurLoop)
        return false;
      if (!canDySERize(CurLoop))
        return false;
      if (CurLoopIter == 0)
        return false;
      return true;
    }


    std::vector<std::pair<Op*, InstPtr> > loop_InstTrace;
    virtual void trackDySERLoop(LoopInfo *li, Op *op, InstPtr inst) {
      loop_InstTrace.push_back(std::make_pair(op, inst));
    }


    LoopInfo *PrevLoop = 0;
    virtual void completeDySERLoop(LoopInfo *DyLoop,
                                   unsigned curLoopIter)
    {
      if (PrevLoop != DyLoop) {
        ++_num_config;
        PrevLoop = DyLoop;
      }

      if (0) {
        completeDySERLoopWithIT(DyLoop, curLoopIter);
      } else {
        completeDySERLoopWithLI(DyLoop, curLoopIter);
      }
    }

    virtual void completeDySERLoopWithIT(LoopInfo *DyLoop,
                                         unsigned curLoopIter) {
      assert(DyLoop);

      SI = SliceInfo::get(DyLoop);
      assert(SI);


      _num_config += (SI->size()/_dyser_size);
      DySERizingLoop = true;

      for (unsigned i = 0, e = loop_InstTrace.size(); i != e; ++i) {
        auto op_n_Inst  = loop_InstTrace[i];
        Op *op = op_n_Inst.first;
        InstPtr inst = op_n_Inst.second;
        insert_sliced_inst(SI, op, inst);
      }
      DySERizingLoop = false;
      loop_InstTrace.clear();
    }


    virtual void completeDySERLoopWithLI(LoopInfo *LI, int curLoopIter) {
      SI = SliceInfo::get(LI);
      assert(SI);
      _num_config += (SI->size()/_dyser_size);
      DySERizingLoop = true;
      for (auto I = LI->rpo_begin(), E = LI->rpo_end(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          InstPtr inst = createInst(op->img, 0, op);
          insert_sliced_inst(SI, op, inst);
        }
      }
      DySERizingLoop = false;
      loop_InstTrace.clear();
    }

    virtual InstPtr insert_sliced_inst(SliceInfo *SI, Op *op, InstPtr inst,
                                    bool pipeLined = true) {
      if (SI->isInLoadSlice(op)) {
        addDeps(inst, op);
        pushPipe(inst);
        inserted(inst);

        if (!op->isLoad() // Skip DySER Load because we morph load to dyload
            &&  SI->isAInputToDySER(op)) {
          // Insert a dyser send instruction to pipeline
          InstPtr dy_send = createDySendInst(op);
          addDeps(dy_send, op);
          pushPipe(dy_send);
          inserted(dy_send);
          return dy_send;
        }
        return inst;
      }

      InstPtr prev_inst = getInstForOp(op);

      InstPtr dy_inst = createDyComputeInst(op,
                                            inst->index());
      //Assumptions::
      // All Instruction in CS can be scheduled to DySER
      // Latency between complete->next instruction is zero
      // Honor data dependence
      setDataDep(dy_inst);
      // Honor ready to Execute
      setReadyToExecute(dy_inst);
      // Honor Execute->Complete (latency)
      setExecuteToComplete(dy_inst);
      if (pipeLined) {
        // Honor DySER InOrder execution
        if (prev_inst.get() != 0)
          setExecuteToExecute(prev_inst, dy_inst);
      }

      keepTrackOfInstOpMap(dy_inst, op);
      if (!op->isStore() && SI->isADySEROutput(op)) {
        InstPtr dy_recv = createDyRecvInst(op);
        addDeps(dy_recv, op);
        pushPipe(dy_recv);
        inserted(dy_recv);
        getCPDG()->insert_edge(*dy_inst, dy_inst->eventComplete(),
                               *dy_recv, Inst_t::Ready, 0,
                               E_RDep);
      }
      return dy_inst;
    }


  };
}


#endif
