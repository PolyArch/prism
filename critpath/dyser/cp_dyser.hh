
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
    unsigned _num_config_loop_switching = 0;
    unsigned _num_config_config_switching = 0;

    // dyser size : default: 5x5
    unsigned _dyser_size = 25;
    unsigned _dyser_fu_fu_lat = 1;

    InstPtr createDyComputeInst(Op *op, uint64_t index) {
      InstPtr dy_compute = InstPtr(new dyser_compute_inst(op->img,
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

    virtual void setDataDep(InstPtr dyInst, Op *op, InstPtr prevInst) {
      // Take case of ready
      for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
        Op *DepOp = *I;
        InstPtr depInst = getInstForOp(DepOp);
        if (!depInst.get())
          continue;
        if (getenv("DUMP_MAFIA_PIPE_DEP")) {
          std::cout << "\t:";  dumpInst(depInst);
        }
        // FIXME:: Assumption - 2 cycle to get to next functional unit.
        getCPDG()->insert_edge(*depInst, depInst->eventComplete(),
                               *dyInst, dyser_inst::DyReady,
                               _dyser_fu_fu_lat,
                               E_DyDep);
      }
      if (prevInst.get() != 0) {
        // Ready is inorder
        getCPDG()->insert_edge(*prevInst, dyser_inst::DyReady,
                               *dyInst, dyser_inst::DyReady,
                               0,
                               E_DyRR);
      }
    }

    virtual void setReadyToExecute(InstPtr dyInst, Op *op, InstPtr prevInst) {
      // Ready to execute
      getCPDG()->insert_edge(*dyInst, dyser_inst::DyReady,
                             *dyInst, dyser_inst::DyExecute, 0,
                             E_DyRE);

      if (prevInst.get() != 0) {

        unsigned issueLat = (SI->shouldIncludeInCSCount(op)
                             ? getFUIssueLatency(op->img._opclass)
                             : 0);
        if (issueLat && (op->img._opclass ==  9 || op->img._opclass == 8))
          //override issueLatency for NBody: (fsqrt,fdiv -> rsqrt)
          issueLat = issueLat / 2;


        // Execute to Execute
        getCPDG()->insert_edge(*prevInst, dyser_inst::DyExecute,
                               *dyInst, dyser_inst::DyExecute,
                               issueLat,
                               E_DyFU);
      }
    }

    virtual void setExecuteToComplete(InstPtr dyInst, Op *op,
                                      InstPtr prevInst) {

      unsigned op_lat = (SI->shouldIncludeInCSCount(op)
                         ? getFUOpLatency(op->img._opclass)
                         : 0);
      getCPDG()->insert_edge(*dyInst, dyser_inst::DyExecute,
                             *dyInst, dyser_inst::DyComplete,
                             op_lat,
                             E_DyEC);

      if (prevInst.get() != 0) {
        // complete is inorder to functional unit.
        getCPDG()->insert_edge(*prevInst, dyser_inst::DyComplete,
                               *dyInst, dyser_inst::DyComplete,
                               0,
                               E_DyCC);
      }
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
        CP_OPDG_Builder<T, E>::insert_inst(img, index, op);
        _default_cp.insert_inst(img, index, op);
      } else {
        // Create the instruction, but add it to the loop trace.
        // but do not map op <-> inst.
        InstPtr inst = createInst(img, index, 0);
        getCPDG()->addInst(inst, index);

        trackLoopInsts(CurLoop, op, inst);
      }
      if (shouldCompleteLoop) {
        completeDySERLoop(CurLoop, CurLoopIter);
        CurLoopIter = 0; // reset loop counter.
      }
    }

    virtual void accelSpecificStats(std::ostream& out, std::string &name) {
      out << " numConfig:" << _num_config
          << " loops: " << _num_config_loop_switching
          << " config:" << _num_config_config_switching
          << "\n";
    }

    uint64_t numCycles() {
      if (CurLoop) {
        completeDySERLoop(CurLoop, CurLoopIter);
      }
      return _lastInst->finalCycle();
    }

    void handle_argument(const char *name, const char *optarg) {
      if (strcmp(name, "dyser-size") == 0) {
        _dyser_size = atoi(optarg);
        if (_dyser_size < 16)
          _dyser_size = 16; //minimum size
      }
      if (strcmp(name, "dyser-fu-fu-latency") == 0) {
        _dyser_fu_fu_lat = atoi(optarg);
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
      if (!li)
        return false;
      if (!li->isInnerLoop())
        return false;
      SliceInfo *si = SliceInfo::get(li, _dyser_size);
      if (si->cs_size() == 0)
        return false;
      return true;
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


#if 0
    std::vector<std::pair<Op*, InstPtr> > loop_InstTrace;

    std::map<Op*, uint16_t> _cacheLat;
    std::map<Op*, bool> _ctrlMiss;
    virtual void trackDySERLoop(LoopInfo *li, Op *op, InstPtr inst) {
      loop_InstTrace.push_back(std::make_pair(op, inst));
      if (op->isLoad()) {
        _cacheLat[op] = std::max(inst->_ex_lat, _cacheLat[op]);
      } else if (op->isStore()) {
        _cacheLat[op] = std::max(inst->_st_lat, _cacheLat[op]);
      } else if (op->isCtrl()) {
        // optimistic : If one succeeds, all succeeds.
        _ctrlMiss[op] &= inst->_ctrl_miss;
      }
    }

    void updateForDySER(Op *op, InstPtr inst, bool useInst = true) {
      if (op->isLoad()) {
        uint16_t inst_lat = useInst ? inst->_ex_lat : 0;
        inst->_ex_lat = std::max(inst_lat, _cacheLat[op]);
      }
      if (op->isStore()) {
        uint16_t inst_lat = useInst ? inst->_st_lat : 0;
        inst->_st_lat = std::max(inst_lat, _cacheLat[op]);
      }
      if (op->isCtrl()) {
        if (useInst)
          inst->_ctrl_miss &= _ctrlMiss[op];
        else
          inst->_ctrl_miss = _ctrlMiss[op];
      }
    }
#endif


    LoopInfo *PrevLoop = 0;
    virtual void completeDySERLoop(LoopInfo *DyLoop,
                                   unsigned curLoopIter)
    {
      if (PrevLoop != DyLoop) {
        ++_num_config;
        ++_num_config_loop_switching;
        PrevLoop = DyLoop;
        std::cout << "Loop Switching\n";
      }
      insert_inst_to_default_pipe();


      if (0) {
        this->completeDySERLoopWithIT(DyLoop, curLoopIter);
      } else {
        this->completeDySERLoopWithLI(DyLoop, curLoopIter);
      }

      static uint64_t numCompleted = 0;
      if (getenv("MAFIA_DEBUG_DYSER_LOOP")) {
        unsigned break_at_config = 1;

        if (getenv("MAFIA_DEBUG_NUM_CONFIG")) {
          break_at_config = atoi(getenv("MAFIA_DEBUG_NUM_CONFIG"));
        }

        SliceInfo *s = SliceInfo::get(DyLoop, _dyser_size);
        std::cout << "cs_size : " << s->cs_size() << "  dyser_size: "  << _dyser_size << "\n";
        unsigned configParam = atoi(getenv("MAFIA_DEBUG_DYSER_LOOP"));
        if (break_at_config == _num_config_loop_switching) {
          ++numCompleted;
          if (configParam < numCompleted)
            exit(0);
        }
      }
    }

    virtual void completeDySERLoopWithIT(LoopInfo *DyLoop,
                                         unsigned curLoopIter) {
      assert(DyLoop);

      SI = SliceInfo::get(DyLoop, _dyser_size);
      assert(SI);


      _num_config += (SI->cs_size()/_dyser_size);
      _num_config_config_switching += (SI->cs_size()/_dyser_size);
      DySERizingLoop = true;

      for (unsigned i = 0, e = _loop_InstTrace.size(); i != e; ++i) {
        auto op_n_Inst  = _loop_InstTrace[i];
        Op *op = op_n_Inst.first;
        InstPtr inst = op_n_Inst.second;
        insert_sliced_inst(SI, op, inst);
      }
      DySERizingLoop = false;
      cleanupLoopInstTracking();
    }


    virtual void completeDySERLoopWithLI(LoopInfo *LI, int curLoopIter) {
      SI = SliceInfo::get(LI, _dyser_size);
      assert(SI);
      if (getenv("DUMP_MAFIA_PIPE")) {
        std::cout << "=========Sliceinfo =========\n";
        SI->dump();
        std::cout << "============================\n";
      }
      unsigned extraConfigRequired = (SI->cs_size()/_dyser_size);
      _num_config += extraConfigRequired;
      _num_config_config_switching += extraConfigRequired;

      DySERizingLoop = true;
      for (auto I = LI->rpo_rbegin(), E = LI->rpo_rend(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          InstPtr inst = createInst(op->img, 0, op);
          // update cache execution delay if necessary
          updateInstWithTraceInfo(op, inst, false);
          insert_sliced_inst(SI, op, inst);
        }
      }
      DySERizingLoop = false;
      cleanupLoopInstTracking();
    }

    virtual InstPtr insertDySend(Op *op) {

      InstPtr dy_send = createDySendInst(op);
      addPipeDeps(dy_send, op);
      pushPipe(dy_send);
      inserted(dy_send);
      return dy_send;
    }

    virtual InstPtr insertDyRecv(Op *op, InstPtr dy_inst) {
      InstPtr dy_recv = createDyRecvInst(op);

      getCPDG()->insert_edge(*dy_inst, dy_inst->eventComplete(),
                             *dy_recv, Inst_t::Ready, 0,
                             E_RDep);

      addDeps(dy_recv, op);
      pushPipe(dy_recv);
      inserted(dy_recv);

      return dy_recv;
    }

    virtual InstPtr insert_sliced_inst(SliceInfo *SI, Op *op, InstPtr inst,
                                       bool useOpMap = true,
                                       InstPtr prevInst = 0,
                                       bool emitDyRecv = true) {
      if (SI->isInLoadSlice(op)) {
        addDeps(inst, op);
        pushPipe(inst);
        inserted(inst);

        if (!op->isLoad() // Skip DySER Load because we morph load to dyload
            &&  SI->isAInputToDySER(op)) {
          // Insert a dyser send instruction to pipeline
          return insertDySend(op);
        }
        return inst;
      }

      InstPtr prevPipelinedInst = ((useOpMap)
                                   ? getInstForOp(op)
                                   : prevInst);

      InstPtr dy_inst = createDyComputeInst(op,
                                            inst->index());
      //Assumptions::
      // All Instruction in CS can be scheduled to DySER
      // Latency between complete->next instruction is zero
      // Honor data dependence
      setDataDep(dy_inst, op, prevPipelinedInst);
      // Honor ready to Execute
      setReadyToExecute(dy_inst, op, prevPipelinedInst);
      // Honor Execute->Complete (latency)
      setExecuteToComplete(dy_inst, op, prevPipelinedInst);


      if (getenv("DUMP_MAFIA_PIPE") != 0) {
        std::cout << "      "; dumpInst(dy_inst);
      }

      keepTrackOfInstOpMap(dy_inst, op);
      if (emitDyRecv && !op->isStore() && SI->isADySEROutput(op)) {
        insertDyRecv(op, dy_inst);
      }
      return dy_inst;
    }


  };
}


#endif
