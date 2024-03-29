
#ifndef CP_DYSER_HH
#define CP_DYSER_HH

#include "cp_args.hh"
#include "cp_opdg_builder.hh"
#include "dyser_inst.hh"
#include "vectorization_legality.hh"
#include "sliceinfo.hh"
#include "cp_utils.hh"


#define TRACE_DYSER_MODEL 0

/*
 *
 */
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

    uint64_t _num_loads = 0;
    uint64_t _num_stores = 0;

    enum MODEL_STATE {
      IN_CPU = 0,
      IN_DYSER = 1
    };

    std::map<Op*, bool> mergedOpMap;
    bool isOpMerged(Op *op) {
      if (!useMergeOps)
        return false;
      auto I = mergedOpMap.find(op);
      if (I != mergedOpMap.end())
        return I->second;
      bool merged = false;
      uint64_t pc = op->cpc().first;
      uint64_t upc = op->cpc().second;
      std::string disasm = ExecProfile::getDisasm(pc, upc);
      if ((disasm.find("MOVSS_XMM_M") != std::string::npos && upc == 2))
        merged = true;

      mergedOpMap[op] = merged;
      return merged;
    }

  //Tony: save out the slices
  virtual uint64_t finish() {
    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        std::cout << "loop " << li->id() << " found\n";

        if(li->isInnerLoop()) {
          //std::cout << "Printing slice" << li->id() << "\n";
          if(!SliceInfo::has(li,_dyser_size)) {
            //std::cout << "---------------------------------not\n";
            continue;
          }
          SliceInfo* si = SliceInfo::get(li, _dyser_size);
          //save out the slice info
          std::stringstream si_name;
          si_name << "analysis/" << li->nice_name_full_filename()<<"."<<_run_name << ".slice.dot";
          std::ofstream outf;
          outf.open(si_name.str(),std::ofstream::out | std::ofstream::trunc);
          si->toDotFile(outf);
        }
      }
    }

    return numCycles(); //default does nothing
  }


  protected:
    // number of times dyser configured..
    unsigned _num_config = 0;
    unsigned _num_config_loop_switching = 0;
    unsigned _num_config_config_switching = 0;
    double _dyser_inst_incr_factor = 0.5;
    bool _dyser_full_dataflow = false;
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
    unsigned _num_cycles_ctrl_miss_penalty = 8;

    unsigned _send_recv_latency = 1;

    bool useReductionConfig = false;
    bool insertCtrlMissConfigPenalty = false;
    bool coalesceMemOps = true;
    bool tryBundleDySEROps = false;
    bool useMergeOps = false;

    uint64_t _dyser_int_ops;
    uint64_t _dyser_fp_ops;
    uint64_t _dyser_mult_ops;

    uint64_t _dyser_int_ops_acc=0;
    uint64_t _dyser_fp_ops_acc=0;
    uint64_t _dyser_mult_ops_acc=0;

    bool was_acc_on=false;

    // do we need this????
    uint64_t _dyser_regfile_freads;
    uint64_t _dyser_regfile_reads;

    uint64_t _dyser_regfile_freads_acc=0;
    uint64_t _dyser_regfile_reads_acc=0;


    InstPtr createDyComputeInst(Op *op, uint64_t index, SliceInfo *SI) {
      InstPtr dy_compute = InstPtr(new dyser_compute_inst(op, op->img,
                                                          index));
      dy_compute->isAccelerated = true;
      if (op) {
//        keepTrackOfInstOpMap(dy_compute, op);

        if (SI->shouldIncludeInCSCount(op)) {
          switch(dy_compute->_opclass) {
          default: break;

          case 3:
          case 1: ++_dyser_int_ops; break;
          case 2: ++_dyser_mult_ops; break;

          case 4:  case 5:
          case 6:  case 7:
          case 8:  case 9: ++ _dyser_fp_ops; break;
          }
        }

        for (auto OI = op->d_begin(), OE = op->d_end(); OI != OE; ++OI) {
          Op *DepOp = *OI;
          if (!DepOp)
            continue;
          if (!SI->isInLoadSlice(DepOp, true))
            continue;
          if (DepOp->img._floating)
            ++ _dyser_regfile_freads;
          else
            ++ _dyser_regfile_reads;
        }
      }
      return dy_compute;
    }

    InstPtr createDySinCosInst(Op *op) {
      InstPtr dy_compute = InstPtr(new dyser_sincos_inst(op));
      dy_compute->isAccelerated = true;
      if (op) {
        keepTrackOfInstOpMap(dy_compute, op);
      }
      return dy_compute;
    }

    InstPtr createDySendInst(Op *op) {
      InstPtr dy_send = InstPtr(new dyser_send(op));
      dy_send->isAccelerated = true;
      return dy_send;
    }

    InstPtr createDyRecvInst(Op *op) {
      InstPtr dy_recv = InstPtr(new dyser_recv(op));
      dy_recv->isAccelerated = true;
      return dy_recv;
    }

    InstPtr createDyConfigInst(unsigned cyclesToFetch) {
      InstPtr dy_config = InstPtr(new dyser_config(cyclesToFetch));
      dy_config->isAccelerated = true;
      return dy_config;
    }


    virtual void setDataDep(InstPtr dyInst, Op *op, InstPtr prevInst) {
      // Take case of ready
      for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
        Op *DepOp = *I;

        if (op == DepOp
            && Op::checkDisasmHas(op, "RSQRTSS_XMM_XMM")) {
          continue;
        }

        InstPtr depInst = std::static_pointer_cast<Inst_t>(
                                                getInstForOp(DepOp));
        if (!depInst.get())
          continue;
        // we should never have dependence on dyser_recv -- a spurious use...
        if (depInst->hasDisasm() && depInst->getDisasm().find("dyser_recv") != std::string::npos)
          continue;
        if (getenv("DUMP_MAFIA_PIPE_DEP")) {
          std::cout << "\t:";  dumpInst(depInst);
        }
        // FIXME:: Assumption - 1 cycle to get to next functional unit.
        getCPDG()->insert_edge(*depInst, depInst->eventComplete(),
                               *dyInst, dyser_compute_inst::DyReady,
                               _dyser_fu_fu_lat,
                               E_DyDep);
      }
      if (prevInst.get() != 0) {
        // Ready is inorder
        if (!_dyser_full_dataflow) {
          getCPDG()->insert_edge(*prevInst, dyser_compute_inst::DyReady,
                                 *dyInst, dyser_compute_inst::DyReady,
                                 0,
                                 E_DyRR);
        }
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
                             ? getFUIssueLatency(op->img._opclass,op)
                             : 0);
        if (issueLat && (op->img._opclass ==  9 || op->img._opclass == 8))
          //override issueLatency for NBody: (fsqrt,fdiv -> rsqrt)
          issueLat = issueLat / 2;

        if (overrideIssueLat != (unsigned)-1)
          issueLat = overrideIssueLat;

        if (!_dyser_full_dataflow) {
          // Execute to Execute
          getCPDG()->insert_edge(*prevInst, dyser_compute_inst::DyExecute,
                                 *dyInst, dyser_compute_inst::DyExecute,
                                 issueLat,
                                 E_DyFU);
        }
      }
    }

    virtual void setExecuteToComplete(InstPtr dyInst, Op *op,
                                      InstPtr prevInst,
                                      unsigned overrideLat = (unsigned)-1) {

      unsigned op_lat = (SI->shouldIncludeInCSCount(op)
                         ? getFUOpLatency(op->img._opclass,op)
                         : 0);
      if (overrideLat != (unsigned)-1)
        op_lat = overrideLat;

      if (applyMaxLatToAccel) {
        op_lat = std::min((unsigned)_max_ex_lat, op_lat);
      }

      getCPDG()->insert_edge(*dyInst, dyser_compute_inst::DyExecute,
                             *dyInst, dyser_compute_inst::DyComplete,
                             op_lat,
                             E_DyEP);

      if (prevInst.get() != 0) {
        if (!_dyser_full_dataflow) {
          // complete is inorder to functional unit.
          getCPDG()->insert_edge(*prevInst, prevInst->eventComplete(),
                                 *dyInst, dyInst->eventComplete(),
                                 0,
                                 E_DyPP);
        }
      }
    }

  public:
    cp_dyser(): CP_OPDG_Builder<T, E> () {
      //getCPDG()->no_horizon();
    }
    virtual ~cp_dyser() {}

    virtual void traceOut(uint64_t index,
                          const CP_NodeDiskImage &img, Op* op) {
    }

    virtual void setGraph(dep_graph_impl_t<Inst_t,T,E>* cpdg) override {
      _cpdg=cpdg;
    }   

    virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
      if(_cpdg==NULL) {
        _cpdg=new dep_graph_impl_t<Inst_t,T,E>();
      }
      return _cpdg;
    };
    dep_graph_impl_t<Inst_t,T,E>* _cpdg;

  //TODO: Check    
  virtual int is_accel_on() {
    if(!PrevOp) {
      return 0;
    }
    LoopInfo *li = getLoop(PrevOp,
                           PrevOp && PrevOp->isReturn(),
                           StackLoop);

    if(li && (StackLoop || canDySERize(li))) {
      was_acc_on=true;
      return li->id();
    } else {
      return 0;
    }
  }

  protected:
    LoopInfo *StackLoop = 0;
    unsigned StackLoopIter = 0;
    Op *PrevOp = 0;
    bool _shouldCompleteLoop = false;
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) override
    {

      //ModelState prev_model_state = dyser_model_state;
      LoopInfo *li = getLoop(op,
                             PrevOp && PrevOp->isReturn(),
                             StackLoop);

      if (_shouldCompleteLoop) {
        completeDySERLoop(CurLoop, CurLoopIter, (CurLoop != li));
        CurLoopIter = 0; // reset loop counter.
        _shouldCompleteLoop = false;
      }


      if (CurLoop != li) {
        // we switched to a different loop

        if(TRACE_DYSER_MODEL) {
          std::cout << "NOT THE SAME LOOP\n";
        }


        // check whether we are in call to sin/cos function
        if (CurLoop
            && canDySERize(CurLoop)
            && PrevOp->func()->isSinCos()) {

          if(TRACE_DYSER_MODEL) {
            std::cout << "FOUND SIN/COS\n";
          }

          StackLoop = CurLoop;
          StackLoopIter = CurLoopIter;
        }

        // otherwise, complete DySER Loop
        if (!StackLoop && (CurLoop != 0) & canDySERize(CurLoop)) {
          completeDySERLoop(CurLoop, CurLoopIter, true);

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
            && PrevOp->func()->isSinCos() ) {
          StackLoop = 0;
          CurLoopIter = StackLoopIter;
          StackLoopIter = 0;
        }

        if(TRACE_DYSER_MODEL) {
          if(CurLoop) {
            std::cout << "NEW LOOP: " << CurLoop->id() << "\n";
          } else {
            std::cout << "DySER LOOP Done\n";
          }
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
        _shouldCompleteLoop = shouldCompleteThisLoop(CurLoop,
                                                     CurLoopIter);
          if(TRACE_DYSER_MODEL) {
            if(_shouldCompleteLoop) {
              std::cout << "TIME TO COMPLETE\n";
            }
          }

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
        InstPtr inst = createInst(img, index, op,false); 
        //this->cpdgAddInst(inst, index); -- this gets added later

        trackLoopInsts(CurLoop, op, inst, img);
      }

      PrevOp = op;
    }

    void accelSpecificStats(std::ostream& out, std::string &name) override {
      out << " numConfig:" << _num_config
          << " loops: " << _num_config_loop_switching
          << " config:" << _num_config_config_switching;
    }

/*
    uint64_t numCycles() override {
      //if (CurLoop) {
      //  completeDySERLoop(CurLoop, CurLoopIter);
      //}
      return _lastInst->finalCycle();
    }
*/

    void handle_argument(const char *name, const char *optarg) override {
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
      if (strcmp(name, "dyser-use-rpo-index-for-output") == 0)
        SliceInfo::useRPOIndexForOutput = true;

      if (strcmp(name, "dyser-config-fetch-latency") == 0)
        _num_cycles_to_fetch_config = atoi(optarg);

      if (strcmp(name, "dyser-config-switch-latency") == 0)
        _num_cycles_switch_config = atoi(optarg);

      if (strcmp(name, "dyser-ctrl-miss-config-penalty") == 0)
        _num_cycles_ctrl_miss_penalty = atoi(optarg);

      if (strcmp(name, "dyser-use-reduction-tree") == 0)
        useReductionConfig = true;

      if (strcmp(name, "dyser-insert-ctrl-miss-penalty") == 0)
        insertCtrlMissConfigPenalty = true;

      if (strcmp(name, "dyser-disallow-coalesce-mem-ops") == 0)
        coalesceMemOps = false;

      if (strcmp(name, "dyser-try-bundle-ops") == 0)
        tryBundleDySEROps = true;

      if (strcmp(name, "dyser-allow-merge-ops") == 0)
        useMergeOps = true;

      if (strcmp(name, "dyser-send-recv-latency") == 0)
        dyser_inst::Send_Recv_Latency = atoi(optarg);

      if (strcmp(name, "dyser-inst-incr-factor") == 0)
        _dyser_inst_incr_factor = atof(optarg);

      if (strcmp(name, "dyser-full-dataflow") == 0)
        _dyser_full_dataflow = true;
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

      if (!li->isSuperBlockProfitable(_dyser_inst_incr_factor))
        return false;

      bool can_vectorize = canVectorize(li, false, _dyser_inst_incr_factor);
      SliceInfo::setup(li, _dyser_size, can_vectorize);

      SliceInfo *si = SliceInfo::get(li, _dyser_size);
      if (si->cs_size() == 0)
        return false;
      if (!si->shouldDySERize(can_vectorize))
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
                                   unsigned curLoopIter,
                                   bool loopDone)
    {

      if(TRACE_DYSER_MODEL) {
        std::cout << "completeDySERLoop, iter:" << curLoopIter;
        if(loopDone) {
          std::cout << ", loop DONE";
        } else {
          std::cout << " loop not done";
        }
        std::cout<< "\n";
      }


      Op* first_op = _loop_InstTrace.front().first;
      assert(first_op);

      if (PrevLoop != DyLoop) {
        incrConfigSwitch(1, 0);
        PrevLoop = DyLoop;
        if (_ConfigCache.size() == 0)
          // first time  -- no penalty - to simulate our trace.
          _ConfigCache.push_front(DyLoop);
        else {
          auto I = std::find(_ConfigCache.begin(), _ConfigCache.end(), DyLoop);
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
          if (_ConfigCache.size() > 16)
            _ConfigCache.pop_back();

          justSwitchedConfig = true;
        }
      }
      insert_inst_trace_to_default_pipe();


      if (0) {
        this->completeDySERLoopWithIT(DyLoop, curLoopIter, loopDone);
      } else {
        this->completeDySERLoopWithLI(DyLoop, curLoopIter, loopDone);
      }

      //Tony added -- only clean until cur loop iter
      this->cleanupLoopInstTracking(DyLoop, CurLoopIter);     

      //TODO: fix following so that it uses correct dyser instructions
      //put remaining instructions into trace as CPU insts

      for (auto I = _loop_InstTrace.begin(), E = _loop_InstTrace.end();I!=E;++I) {
        Op *op = I->first;
        InstPtr inst = I->second;

        inst->_type=77;
        this->cpdgAddInst(inst, inst->_index);
        addDeps(inst, op);
        pushPipe(inst);
        inserted(inst);
      }

      this->cleanupLoopInstTracking();

      justSwitchedConfig = false;

      if (lastInst()->_ctrl_miss) {
        if (insertCtrlMissConfigPenalty) {
          // We model this with inserting two dyconfig num_cycles..
          incrConfigSwitch(1, 0);
          // We will switch to the config and switch back ....
          ConfigInst = insertDyConfig(_num_cycles_switch_config*2
                                      + _num_cycles_ctrl_miss_penalty);
          assert(lastInst()->_op);
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
                                         unsigned curLoopIter,
                                         bool loopDone) {
      assert(0);

      assert(DyLoop);

      SI = SliceInfo::get(DyLoop, _dyser_size);
      assert(SI);
      unsigned cssize = SI->cs_size();
      unsigned extraConfigRequired = (cssize / _dyser_size);
      incrConfigSwitch(0, extraConfigRequired);

      DySERizingLoop = true;

      for (auto I = _loop_InstTrace.begin(), E = _loop_InstTrace.end();
           I != E; ++I) {
        auto op_n_Inst  = *I;
        Op *op = op_n_Inst.first;
        InstPtr inst = op_n_Inst.second;
        insert_sliced_inst(SI, op, inst, loopDone);
      }
      DySERizingLoop = false;
    }


    virtual void completeDySERLoopWithLI(LoopInfo *LI, int curLoopIter,
                                         bool loopDone) {

      if(TRACE_DYSER_MODEL) {
        std::cout << "completeDySERLoop -- SCALAR, iter:" << curLoopIter;
        if(loopDone) {
          std::cout << ", loop DONE";
        } else {
          std::cout << " loop not done";
        }
      }

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

      if(TRACE_DYSER_MODEL) {
        std::cout << "try\n";
      }

      for (int idx = 0; idx < curLoopIter; ++idx) {
        if(TRACE_DYSER_MODEL) {
          std::cout << "iter: " << idx << "\n";
        }

        memOp2Inst.clear();
        bundledOp2Inst.clear();
        for (auto I = LI->rpo_rbegin(), E = LI->rpo_rend(); I != E; ++I) {
          BB *bb = *I;

          if(TRACE_DYSER_MODEL) {
            std::cout << "bb:" << bb->rpoNum() << "\n";
          }

          for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
            Op *op = *OI;

            if(TRACE_DYSER_MODEL) {
              std::cout << "op: " << op->id() << "\n";
            }
            if (isOpMerged(op))
              continue;
            if (SI->isInternalCtrl(op))
              continue;
            //we don't know the index, so lets not worry
            InstPtr inst = createInst(op->img, 0, op, false);
            //Tony: don't track map!

            if(TRACE_DYSER_MODEL) {
              std::cout << "created instruction: " << op->id() << "\n";
            }

            // update cache execution delay
            updateInstWithTraceInfo(op, inst, false);
            //this->cpdgAddInst(inst, inst->_index); //Tony: added this


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
                  insert_sliced_inst(SI, op, inst, loopDone);
                  bundledOp2Inst[firstOp] = inst;
                } else {
                  this->keepTrackOfInstOpMap(bundledOp2Inst[firstOp], op);
                }
              } else {
                insert_sliced_inst(SI, op, inst, loopDone);
              }
            } else {
              //ignore things like limm that have no uses/deps
              if(op->numUses()==0 && op->numDeps()==0 && op->shouldIgnoreInAccel()) {
                //do nothing
              } else { 

                // load slice -- coalesce if possible
                if (coalesceMemOps && (op->isLoad() || op->isStore())) {
                  Op *firstOp = SI->getFirstMemNode(op);
                  if (memOp2Inst.count(firstOp) == 0) {
                    insert_sliced_inst(SI, op, inst, loopDone);
                    memOp2Inst[firstOp] = inst;
                  } else {
                    // we already created the inst
                    this->keepTrackOfInstOpMap(memOp2Inst[firstOp], op);
                  }
                } else {
                  insert_sliced_inst(SI, op, inst, loopDone);
                }
              }
            }
          }
        }
      }
      DySERizingLoop = false;
    }

    InstPtr _last_dyser_inst = 0;
  virtual InstPtr insertDySend(Op *op) {

    InstPtr depInst = std::static_pointer_cast<Inst_t>(getInstForOp(op));
    InstPtr dy_send = createDySendInst(op);

    if (justSwitchedConfig) {
      if (ConfigInst.get() != 0) {
        getCPDG()->insert_edge(*ConfigInst, ConfigInst->eventComplete(),
                               *dy_send, Inst_t::Ready, 
                               0, E_DyCR);
      }
    }

    if(depInst) {
      //depInst->addOperandInst(depInst);  // these seem bad! ERROR MEM LEAK
      dy_send->addOperandInst(depInst);  // better?
    }

    addDeps(dy_send, op);
    pushPipe(dy_send);
    inserted(dy_send);
    add_inst_checked(dy_send);
    _last_dyser_inst = dy_send;

    if (op) {
      keepTrackOfInstOpMap(dy_send, op);
    }

    return dy_send;
  }

  virtual InstPtr insertDyRecv(Op *op, InstPtr dy_inst, unsigned latency = 0) {
    InstPtr dy_recv = createDyRecvInst(op);

    dy_recv->adjustExecuteLatency(latency);

    dy_recv->addOperandInst(dy_inst); //These appear okay

     /*getCPDG()->insert_edge(*dy_inst, dy_inst->eventComplete(),
                           *dy_recv, Inst_t::Ready,
                           dy_recv->adjustExecuteLatency(latency),E_RDep);*/

    addDeps(dy_recv, op);
    pushPipe(dy_recv);
    inserted(dy_recv);
    add_inst_checked(dy_recv);
    _last_dyser_inst = dy_recv;

    if (op) {
      keepTrackOfInstOpMap(dy_recv, op);
    }

    return dy_recv;
  }

  virtual InstPtr insertDyConfig(unsigned numCyclesToFetch) {
    if (numCyclesToFetch == 0)
      return 0;
    InstPtr dy_config = createDyConfigInst(numCyclesToFetch);
    if (_last_dyser_inst.get() != 0)
      getCPDG()->insert_edge(*_last_dyser_inst,
                             _last_dyser_inst->eventCommit(),
                             *dy_config, Inst_t::Ready, 0,
                             E_DyCR);
    addDeps(dy_config);
    pushPipe(dy_config);
    inserted(dy_config);
    add_inst_checked(dy_config);
    _last_dyser_inst = dy_config;
    return dy_config;
  }


  virtual InstPtr insert_sliced_inst(SliceInfo *SI, Op *op, InstPtr inst,
                                     bool loopDone,
                                     bool useOpMap = true,
                                     InstPtr prevInst = 0,
                                     bool emitDySendForCS = true,
                                     bool emitDyRecv = true,
                                     unsigned dyRecvLat = 0) {
    if (SI->isInLoadSlice(op)) {
      if (op->isLoad() || op->isStore() || op->shouldIgnoreInAccel()) {
        inst->isAccelerated = true;
      }

      if(TRACE_DYSER_MODEL) {
        std::cout << "insert load sliced inst: " << op->id() << "\n";
      }

      // FIXME:: Should all loadslice is not floating point ???
      //if (op->isLoad())
      if (getenv("MAFIA_DYSER_ENERGY_HACK") != 0)
      {
        // change it to int
        inst->_floating = false;
      }
      addDeps(inst, op);
      pushPipe(inst);
      inserted(inst);
      add_inst_checked(inst);

      if (!op->isLoad() // Skip DySER Load because we morph load to dyload
          &&  SI->isAInputToDySER(op)
          && dyser_inst::Send_Recv_Latency > 0) {

        // Insert a dyser send instruction to pipeline
        return insertDySend(op);
      }
      return inst;
    }

    if(TRACE_DYSER_MODEL) {
      std::cout << "insert comp sliced inst: " << op->id() << "\n";
    }

    if(op->is_clear_xor()) {
      return inst; //get out if its just a clear xor
    }

    //add_inst_checked(inst); //add it even if compute

    inst->isAccelerated = true;
    // Computation Slice!
    if (emitDySendForCS && SI->isAInputToDySER(op)) { //we want this to refer to the old iteration
      // We are here, if op is an accumulate type instruction.
      // We should insert this op in the loop head itself...
      //insertDySend(op);
    }

    InstPtr prevPipelinedInst = ((useOpMap)
                                 ? std::static_pointer_cast<Inst_t>(
                                   getInstForOp(op))
                                 : prevInst);

    InstPtr dy_inst = createDyComputeInst(op,
                                          inst->index(),
                                          SI);

    if (justSwitchedConfig) {
      if (ConfigInst.get() != 0) {
        getCPDG()->insert_edge(*ConfigInst, ConfigInst->eventComplete(),
                               *dy_inst, dyser_compute_inst::DyReady,
                               0,
                               E_DyCR);
      }
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


    if (getenv("DUMP_MAFIA_DYSER_EXEC") != 0) {
      std::cout << "      "; dumpInst(dy_inst);
    }

    keepTrackOfInstOpMap(dy_inst, op);
    add_inst_checked(dy_inst);

    if (emitDyRecv && SI->isADySEROutput(op) && !allUsesAreStore(op)
        && (loopDone || dyser_inst::Send_Recv_Latency > 0)) {
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
    //std::map<Op*, uint16_t> _iCacheLat;

    //void trackLoopInsts(LoopInfo *li, Op *op, InstPtr inst,
    //                    const CP_NodeDiskImage &img) {
    //  // Optimistic -- ????
    //  CP_OPDG_Builder<T, E>::trackLoopInsts(li, op, inst, img);
    //  _iCacheLat[op] = std::min(_iCacheLat[op], inst->_icache_lat);
    //}

    //void updateInstWithTraceInfo(Op *op, InstPtr inst,
    //                            bool useInst) {
    //  CP_OPDG_Builder<T, E>::updateInstWithTraceInfo(op, inst, useInst);
    //  inst->_icache_lat = _iCacheLat[op];
    //}

    //virtual void cleanupLoopInstTracking() {
    //  CP_OPDG_Builder<T, E>::cleanupLoopInstTracking();
    //  _iCacheLat.clear();
    //}

    void pushPipe(InstPtr &inst) {
      CP_OPDG_Builder<T, E>::pushPipe(inst);
      if (inst->_isload) {
        ++_num_loads;
      } else if (inst->_isstore) {
        ++_num_stores;
      }
    }

    void setEnergyEvents(pugi::xml_document &doc, int nm) {
      CP_OPDG_Builder<T, E>::setEnergyEvents(doc, nm);

      pugi::xml_node system_node =
        doc.child("component").find_child_by_attribute("name","system");
      pugi::xml_node core_node =
        system_node.find_child_by_attribute("name","core0");

          // ---------- icache --------------
      pugi::xml_node dcache_node =
        core_node.find_child_by_attribute("name", "dcache");
      sa(dcache_node, "read_accesses",   _num_loads);
      sa(dcache_node, "write_accesses", _num_stores);

      double read_miss_rate = ((Prof::get().dcacheReads != 0)?
                               ((double)Prof::get().dcacheReadMisses
                                / (double)Prof::get().dcacheReads): 0.0);
      double write_miss_rate = ((Prof::get().dcacheReads != 0)?
                                ((double)Prof::get().dcacheReadMisses
                                 / (double)Prof::get().dcacheReads): 0.0);

      uint64_t read_misses = _num_loads * read_miss_rate;
      uint64_t write_misses = _num_stores * write_miss_rate;

      sa(dcache_node, "read_misses", read_misses);
      sa(dcache_node, "write_misses", write_misses);
    }


    virtual void calcAccelEnergy(std::string fname_base, int nm) {
      std::string fname=fname_base + std::string(".accel");

      std::string outf = fname + std::string(".out");
      std::cout <<  " dyser accel(" << nm << "nm)... ";
      std::cout.flush();

      execMcPAT(fname, outf);
      float ialu  = std::stof(grepF(outf,"Integer ALUs",7,4));
      float fpalu = std::stof(grepF(outf,"Floating Point Units",7,4));
      float calu  = std::stof(grepF(outf,"Complex ALUs",7,4));
      float reg   = std::stof(grepF(outf,"Register Files",7,4));
      float total = ialu + fpalu + calu + reg;
      std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ")\n";
    }



    //TODO: WHAT SHOULD I DO?
    virtual double accel_leakage() override {

      if(was_acc_on) {
        was_acc_on=false;
        bool lc = mc_accel->XML->sys.longer_channel_device;

        float ialu = RegionStats::get_leakage(mc_accel->cores[0]->exu->exeu,lc)* _dyser_size/4.0;
        float mul  = RegionStats::get_leakage(mc_accel->cores[0]->exu->mul,lc) * _dyser_size/4.0;

        float net  = RegionStats::get_leakage(mc_accel->nlas[0]->bypass,lc) * _dyser_size;

        //cout << "alu: " << nALUs_on << " " << ialu << "\n";
        //cout << "mul: " << nMULs_on << " " << mul << "\n";
        //cout << "imu: " << imus_on  << " " << imu << "\n";
        //cout << "net: " << 1        << " " << net << "\n";

        return ialu + mul + net;
        //  float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
        //  float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 
        //  float network = mc_accel->nlas[0]->bypass->rt_power.readOp.dynamic; 
      } else {
        return 0;
      }
    }


    

    NLAProcessor* mc_accel = NULL;
    virtual void setupMcPAT(const char* filename, int nm) 
    {
      CriticalPath::setupMcPAT(filename,nm); //do base class

      printAccelMcPATxml(filename,nm);
      ParseXML* mcpat_xml= new ParseXML(); 
      mcpat_xml->parse((char*)filename);
      mc_accel = new NLAProcessor(mcpat_xml); 

      /*system_core* core = &mc_accel->XML->sys.core[0];

      core->ALU_per_core=Prof::get().int_alu_count;
      core->MUL_per_core=Prof::get().mul_div_count;
      core->FPU_per_core=Prof::get().fp_alu_count;

      core->archi_Regs_IRF_size=8;
      core->archi_Regs_FRF_size=8;
      core->phy_Regs_IRF_size=64;
      core->phy_Regs_FRF_size=64;

      core->num_nla_units=8;
      core->imu_width=4;
      core->imu_entries=32;

      core->instruction_window_size=32;*/

      pumpCoreEnergyEvents(mc_accel,0); //reset all energy stats to 0
    } 

    virtual void pumpAccelMcPAT(uint64_t totalcycles) {
      pumpAccelEnergyEvents(totalcycles);
      mc_accel->computeEnergy();
    }

    virtual void pumpAccelEnergyEvents(uint64_t totalCycles) {
      mc_accel->XML->sys.total_cycles=totalCycles;

      //Func Units
      system_core* core = &mc_accel->XML->sys.core[0];
      core->ialu_accesses    = (uint64_t)(_dyser_int_ops );
      core->fpu_accesses     = (uint64_t)(_dyser_fp_ops  );
      core->mul_accesses     = (uint64_t)(_dyser_mult_ops);

      core->cdb_alu_accesses = (uint64_t)(_dyser_int_ops );
      core->cdb_fpu_accesses = (uint64_t)(_dyser_fp_ops  );
      core->cdb_mul_accesses = (uint64_t)(_dyser_mult_ops);

      //Reg File
      core->int_regfile_reads    = (uint64_t)(_dyser_regfile_reads  ); 
      core->int_regfile_writes   = (uint64_t)(_dyser_int_ops
                                              + _dyser_mult_ops); 
      core->float_regfile_reads  = (uint64_t)(_dyser_regfile_freads ); 
      core->float_regfile_writes = (uint64_t)(_dyser_fp_ops); 

      //  core->nla_network_accesses=_nla_network_writes;

      accAccelEnergyEvents();
    }

    virtual double accel_region_en() override {
   
      float ialu  = mc_accel->cores[0]->exu->exeu->rt_power.readOp.dynamic; 
      float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
      float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 
      float reg  = mc_accel->cores[0]->exu->rfu->rt_power.readOp.dynamic; 
  
//      float network = mc_accel->nlas[0]->bypass->rt_power.readOp.dynamic; 
  
  
      return ialu + fpalu + calu + reg;
    }

    virtual void accAccelEnergyEvents() {
      _dyser_int_ops_acc         +=  _dyser_int_ops        ;
      _dyser_fp_ops_acc          +=  _dyser_fp_ops         ;
      _dyser_mult_ops_acc        +=  _dyser_mult_ops       ;
      _dyser_regfile_reads_acc   +=  _dyser_regfile_reads  ;
      _dyser_regfile_freads_acc  +=  _dyser_regfile_freads ;
  
//      _dyser_network_reads_acc   +=  _dyser_network_reads  ;
//      _dyser_network_writes_acc  +=  _dyser_network_writes ;
  
      _dyser_int_ops         = 0;
      _dyser_fp_ops          = 0;
      _dyser_mult_ops        = 0;
      _dyser_regfile_reads   = 0; 
      _dyser_regfile_freads  = 0; 
  
//      _dyser_network_reads   = 0; 
//      _dyser_network_writes  = 0; 
  
    }



    // Handle enrgy events for McPAT XML DOC
    virtual void printAccelMcPATxml(std::string fname_base, int nm) {

      #include "mcpat-defaults.hh"
      pugi::xml_document accel_doc;
      std::istringstream ss(xml_str);
      pugi::xml_parse_result result = accel_doc.load(ss);

      if (!result) {
        std::cerr << "XML Malformed\n";
        return;
      }

      pugi::xml_node system_node =
        accel_doc.child("component").find_child_by_attribute("name", "system");

      //set the total_cycles so that we get power correctly
      sa(system_node, "total_cycles", numCycles());
      sa(system_node, "busy_cycles", 0);
      sa(system_node, "idle_cycles", numCycles());

      sa(system_node, "core_tech_node", nm);
      sa(system_node, "device_type", 0);


      pugi::xml_node core_node =
        system_node.find_child_by_attribute("name", "core0");
      sa(core_node, "total_cycles", numCycles());
      sa(core_node, "busy_cycles", 0);
      sa(core_node, "idle_cycles", numCycles());

      sa(core_node, "ALU_per_core", Prof::get().int_alu_count);
      sa(core_node, "MUL_per_core", Prof::get().mul_div_count);
      sa(core_node, "FPU_per_core", Prof::get().fp_alu_count);

      sa(core_node, "ialu_accesses", _dyser_int_ops_acc);
      sa(core_node, "fpu_accesses", _dyser_fp_ops_acc);
      sa(core_node, "mul_accesses", _dyser_mult_ops_acc);

      sa(core_node, "archi_Regs_IRF_size", 8);
      sa(core_node, "archi_Regs_FRF_size", 8);
      sa(core_node, "phy_Regs_IRF_size", 64);
      sa(core_node, "phy_Regs_FRF_size", 64);

      sa(core_node, "int_regfile_reads", _dyser_regfile_reads_acc);
      sa(core_node, "int_regfile_writes", _dyser_int_ops_acc + _dyser_mult_ops_acc);
      sa(core_node, "float_regfile_reads", _dyser_regfile_freads_acc);
      sa(core_node, "float_regfile_writes", _dyser_fp_ops_acc);

      std::string fname=fname_base + std::string(".accel");
      accel_doc.save_file(fname.c_str());
    }

#if 0
    virtual void setEnergyStatsPerInst(std::shared_ptr<Inst_t>& inst)
    {
      ++committed_insts;

      committed_int_insts += !inst._floating;
      committed_fp_insts += inst._floating;

      committed_branch_insts += inst._isctrl;


      // TODO::if we just load one value always, replace that with dsend...
      committed_load_insts += inst._isload;

      committed_store_insts += inst._isstore;
      func_calls += inst._iscall;
    }
#endif
  };
}


#endif
