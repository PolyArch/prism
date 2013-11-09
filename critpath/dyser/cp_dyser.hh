
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


  private:
    // number of times dyser configured..
    unsigned _num_config = 0;
    unsigned _num_config_loop_switching = 0;
    unsigned _num_config_config_switching = 0;

  protected:
    void incrConfigSwitch(unsigned lp, unsigned extra) {
      _num_config += lp + extra;
      _num_config_loop_switching += lp;
      _num_config_config_switching += extra;
    }

  protected:
    // dyser size : default: 5x5
    unsigned _dyser_size = 25;
    unsigned _dyser_fu_fu_lat = 1;

    unsigned _num_cycles_switch_config = 3;
    unsigned _num_cycles_to_fetch_config = 64;
    bool useReductionConfig = false;
    bool insertCtrlMissConfigPenalty = false;
    bool coalesceMemOps = true;
    bool tryBundleDySEROps = false;

    InstPtr createDyComputeInst(Op *op, uint64_t index) {
      InstPtr dy_compute = InstPtr(new dyser_compute_inst(op->img,
                                                          index));
      if (op) {
        keepTrackOfInstOpMap(dy_compute, op);
      }
      return dy_compute;
    }

    InstPtr createDySinCosInst(Op *op) {
      InstPtr dy_compute = InstPtr(new dyser_sincos_inst());
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

    InstPtr createDyConfigInst(unsigned cyclesToFetch) {
      InstPtr dy_config = InstPtr(new dyser_config(cyclesToFetch));
      return dy_config;
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
                               *dyInst, dyser_compute_inst::DyReady,
                               _dyser_fu_fu_lat,
                               E_DyDep);
      }
      if (prevInst.get() != 0) {
        // Ready is inorder
        getCPDG()->insert_edge(*prevInst, dyser_compute_inst::DyReady,
                               *dyInst, dyser_compute_inst::DyReady,
                               0,
                               E_DyRR);
      }
    }

    virtual void setReadyToExecute(InstPtr dyInst, Op *op,
                                   InstPtr prevInst,
                                   unsigned overrideIssueLat = (unsigned)-1) {
      // Ready to execute
      getCPDG()->insert_edge(*dyInst, dyser_compute_inst::DyReady,
                             *dyInst, dyser_compute_inst::DyExecute, 0,
                             E_DyRE);

      if (prevInst.get() != 0) {

        unsigned issueLat = (SI->shouldIncludeInCSCount(op)
                             ? getFUIssueLatency(op->img._opclass)
                             : 0);
        if (issueLat && (op->img._opclass ==  9 || op->img._opclass == 8))
          //override issueLatency for NBody: (fsqrt,fdiv -> rsqrt)
          issueLat = issueLat / 2;

        if (overrideIssueLat != (unsigned)-1)
          issueLat = overrideIssueLat;

        // Execute to Execute
        getCPDG()->insert_edge(*prevInst, dyser_compute_inst::DyExecute,
                               *dyInst, dyser_compute_inst::DyExecute,
                               issueLat,
                               E_DyFU);
      }
    }

    virtual void setExecuteToComplete(InstPtr dyInst, Op *op,
                                      InstPtr prevInst,
                                      unsigned overrideLat = (unsigned)-1) {

      unsigned op_lat = (SI->shouldIncludeInCSCount(op)
                         ? getFUOpLatency(op->img._opclass)
                         : 0);
      if (overrideLat != (unsigned)-1)
        op_lat = overrideLat;

      getCPDG()->insert_edge(*dyInst, dyser_compute_inst::DyExecute,
                             *dyInst, dyser_compute_inst::DyComplete,
                             op_lat,
                             E_DyEP);

      if (prevInst.get() != 0) {
        // complete is inorder to functional unit.
        getCPDG()->insert_edge(*prevInst, prevInst->eventComplete(),
                               *dyInst, dyInst->eventComplete(),
                               0,
                               E_DyPP);
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

  protected:
    LoopInfo *StackLoop = 0;
    unsigned StackLoopIter = 0;
    Op *PrevOp = 0;

    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op)
    {
      bool shouldCompleteLoop = false;
      //ModelState prev_model_state = dyser_model_state;
      LoopInfo *li = getLoop(op,
                             PrevOp && PrevOp->isReturn(),
                             StackLoop);

      if (CurLoop != li) {
        // we switched to a different loop

        // check whether we are in call to sin/cos function
        if (CurLoop
            && canDySERize(CurLoop)
            && op->func()->nice_name() == "sincosf") {
          StackLoop = CurLoop;
          StackLoopIter = CurLoopIter;
        }

        // otherwise, complete DySER Loop
        if (!StackLoop && (CurLoop != 0) & canDySERize(CurLoop)) {
          completeDySERLoop(CurLoop, CurLoopIter);

          if (useReductionConfig) {
            // Insert dyser config instruction with double the penalty
            // Coarse model
            incrConfigSwitch(1, 0);
            ConfigInst = insertDyConfig(_num_cycles_to_fetch_config);
          }
        }
        CurLoop = li;
        CurLoopIter = 0;
        global_loop_iter = 0;

        if (StackLoop
            && PrevOp && PrevOp->isReturn()
            && PrevOp->func()->nice_name() == "sincosf") {
          StackLoop = 0;
          CurLoopIter = StackLoopIter;
          StackLoopIter = 0;
        }

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

      if (!StackLoop && !canDySERize(li)) {
        // We are in the CPU mode
        // use the default insert_inst
        CP_OPDG_Builder<T, E>::insert_inst(img, index, op);
        //_default_cp.insert_inst(img, index, op);
        insert_inst_to_default_pipe(img, index, op);
      } else {
        // Create the instruction, but add it to the loop trace.
        // but do not map op <-> inst.
        InstPtr inst = createInst(img, index, 0);
        getCPDG()->addInst(inst, index);

        trackLoopInsts(CurLoop, op, inst, img);
      }
      if (shouldCompleteLoop) {
        completeDySERLoop(CurLoop, CurLoopIter);
        CurLoopIter = 0; // reset loop counter.
      }
      PrevOp = op;
    }

    virtual void accelSpecificStats(std::ostream& out, std::string &name) {
      out << " numConfig:" << _num_config
          << " loops: " << _num_config_loop_switching
          << " config:" << _num_config_config_switching
          << "\n";
    }

    uint64_t numCycles() {
      //if (CurLoop) {
      //  completeDySERLoop(CurLoop, CurLoopIter);
      //}
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

      if (strcmp(name, "disallow-internal-control-in-dyser") == 0)
        SliceInfo::mapInternalControlToDySER = false;

      if (strcmp(name, "dyser-switch-latency") == 0)
        _num_cycles_switch_config = atoi(optarg);
      if (strcmp(name, "dyser-use-reduction-tree") == 0)
        useReductionConfig = true;

      if (strcmp(name, "dyser-insert-ctrl-miss-penalty") == 0)
        insertCtrlMissConfigPenalty = true;

      if (strcmp(name, "dyser-disallow-coalesce-mem-ops") == 0)
        coalesceMemOps = false;

      if (strcmp(name, "dyser-try-bundle-ops") == 0)
        tryBundleDySEROps = true;
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
      if (!si->shouldDySERize(canVectorize(li, false)))
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
    InstPtr ConfigInst = 0;
    bool justSwitchedConfig = false;
    std::list<LoopInfo *> _ConfigCache;

    virtual void completeDySERLoop(LoopInfo *DyLoop,
                                   unsigned curLoopIter)
    {
      if (PrevLoop != DyLoop) {
        incrConfigSwitch(1, 0);
        PrevLoop = DyLoop;
        if (_ConfigCache.size() == 0)
          // first time  -- no penalty - to simulate our trace.
          _ConfigCache.push_front(DyLoop);
        else {
          auto I = std::find(_ConfigCache.begin(), _ConfigCache.end(),
                             DyLoop);
          if (I != _ConfigCache.end()) {
            // we have the config in cache
            ConfigInst = insertDyConfig(_num_cycles_switch_config);
            _ConfigCache.remove(DyLoop);
          } else {
            // Pay the price...
            ConfigInst = insertDyConfig(_num_cycles_to_fetch_config);
          }
          // Manage the cache.
          _ConfigCache.push_front(DyLoop);

          // forget about least recently used
          if (_ConfigCache.size() > 8)
            _ConfigCache.pop_back();

          justSwitchedConfig = true;
        }
      }
      insert_inst_trace_to_default_pipe();


      if (0) {
        this->completeDySERLoopWithIT(DyLoop, curLoopIter);
      } else {
        this->completeDySERLoopWithLI(DyLoop, curLoopIter);
      }

      justSwitchedConfig = false;

      if (_lastInst->_ctrl_miss) {
        if (insertCtrlMissConfigPenalty) {
          // We model this with inserting two dyconfig num_cycles..
          incrConfigSwitch(1, 0);
          ConfigInst = insertDyConfig(_num_cycles_to_fetch_config);
        }
      }

      if (getenv("DUMP_MAFIA_PIPE") != 0) {
        std::cout << " ============ completeDySERLoop==========\n";
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
      unsigned cssize = SI->cs_size();
      unsigned extraConfigRequired = (cssize / _dyser_size);
      incrConfigSwitch(0, extraConfigRequired);

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
      if (getenv("MAFIA_DUMP_SLICE_INFO")) {
        std::cout << "=========Sliceinfo (cp_dyser) =========\n";
        SI->dump();
        std::cout << "=======================================\n";
      }
      unsigned extraConfigRequired = (SI->cs_size()/_dyser_size);
      _num_config += extraConfigRequired;
      _num_config_config_switching += extraConfigRequired;
      unsigned numInDySER = 0;
      std::map<Op*, InstPtr> memOp2Inst;
      std::map<Op*, InstPtr> bundledOp2Inst;
      DySERizingLoop = true;
      for (int idx = 0; idx < curLoopIter; ++idx) {
        for (auto I = LI->rpo_rbegin(), E = LI->rpo_rend(); I != E; ++I) {
          BB *bb = *I;
          for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
            Op *op = *OI;
            InstPtr inst = createInst(op->img, 0, op);
            // update cache execution delay if necessary
            updateInstWithTraceInfo(op, inst, false);
            if (!SI->isInLoadSlice(op)) {
              ++ numInDySER;
            }
            if (numInDySER > _dyser_size) {
              if (_num_cycles_switch_config != 0) {
                ConfigInst = insertDyConfig(_num_cycles_switch_config);
                justSwitchedConfig = true;
              }
              // All the sends follow this instruction will be
              // dependent on this, but not receives.
              numInDySER = 0;
            }
            if (!SI->isInLoadSlice(op)) {
              Op *firstOp = SI->getBundledNode(op);

              if (firstOp && tryBundleDySEROps) {
                if (bundledOp2Inst.count(firstOp) == 0) {
                  insert_sliced_inst(SI, op, inst);
                  bundledOp2Inst[firstOp] = inst;
                } else {
                  this->keepTrackOfInstOpMap(bundledOp2Inst[firstOp], op);
                }
              } else
                insert_sliced_inst(SI, op, inst);
            } else {
              // load slice -- coalesce if possible
              if (coalesceMemOps && (op->isLoad() || op->isStore())) {
                Op *firstOp = SI->getFirstMemNode(op);
                if (memOp2Inst.count(firstOp) == 0) {
                  insert_sliced_inst(SI, op, inst);
                  memOp2Inst[firstOp] = inst;
                } else {
                  // we already created the inst
                  this->keepTrackOfInstOpMap(memOp2Inst[firstOp], op);
                }
              } else {
                insert_sliced_inst(SI, op, inst);
              }
            }
          }
        }
      }
      DySERizingLoop = false;
      cleanupLoopInstTracking();
    }

    InstPtr _last_dyser_inst = 0;
    virtual InstPtr insertDySend(Op *op) {

      InstPtr dy_send = createDySendInst(op);

      if (justSwitchedConfig) {
        assert(ConfigInst.get() != 0);
        getCPDG()->insert_edge(*ConfigInst, ConfigInst->eventComplete(),
                               *dy_send, Inst_t::Ready, 0,
                               E_DyCR);
      }

      addPipeDeps(dy_send, op);
      pushPipe(dy_send);
      inserted(dy_send);
      _last_dyser_inst = dy_send;
      return dy_send;
    }

    virtual InstPtr insertDyRecv(Op *op, InstPtr dy_inst, unsigned latency = 0) {
      InstPtr dy_recv = createDyRecvInst(op);

      getCPDG()->insert_edge(*dy_inst, dy_inst->eventComplete(),
                             *dy_recv, Inst_t::Ready,
                             latency,
                             E_RDep);

      addDeps(dy_recv, op);
      pushPipe(dy_recv);
      inserted(dy_recv);
      _last_dyser_inst = dy_recv;
      return dy_recv;
    }

    virtual InstPtr insertDyConfig(unsigned numCyclesToFetch) {
      InstPtr dy_config = createDyConfigInst(numCyclesToFetch);
      if (_last_dyser_inst.get() != 0)
        getCPDG()->insert_edge(*_last_dyser_inst,
                               _last_dyser_inst->eventCommit(),
                               *dy_config, Inst_t::Ready, 0,
                               E_DyCR);
      addDeps(dy_config);
      pushPipe(dy_config);
      inserted(dy_config);
      _last_dyser_inst = dy_config;
      return dy_config;
    }


    virtual InstPtr insert_sliced_inst(SliceInfo *SI, Op *op, InstPtr inst,
                                       bool useOpMap = true,
                                       InstPtr prevInst = 0,
                                       bool emitDyRecv = true,
                                       unsigned dyRecvLat = 0) {
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

      if (justSwitchedConfig) {
        assert(ConfigInst.get() != 0);
        getCPDG()->insert_edge(*ConfigInst, ConfigInst->eventComplete(),
                               *dy_inst, dyser_compute_inst::DyReady,
                               0,
                               E_DyCR);
      }

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
      if (emitDyRecv && SI->isADySEROutput(op) && !allUsesAreStore(op)) {
        insertDyRecv(op, dy_inst, dyRecvLat);
      }
      return dy_inst;
    }

    bool allUsesAreStore(Op* op) {
      for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
        if (!(*UI)->isStore())
          return false;
      }
      return true;
    }


    // overrides
    std::map<Op*, uint16_t> _iCacheLat;

    void trackLoopInsts(LoopInfo *li, Op *op, InstPtr inst,
                        const CP_NodeDiskImage &img) {
      // Optimistic -- ????
      CP_OPDG_Builder<T, E>::trackLoopInsts(li, op, inst, img);
      _iCacheLat[op] = std::min(_iCacheLat[op], inst->_icache_lat);
    }

    void updateInstWithTraceInfo(Op *op, InstPtr inst,
                                bool useInst) {
      CP_OPDG_Builder<T, E>::updateInstWithTraceInfo(op, inst, useInst);
      inst->_icache_lat = _iCacheLat[op];
    }

    void cleanupLoopInstTracking() {
      CP_OPDG_Builder<T, E>::cleanupLoopInstTracking();
      _iCacheLat.clear();
    }

  };
}


#endif
