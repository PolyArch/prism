
#ifndef CP_SIMD_HH
#define CP_SIMD_HH

#include "cp_opdg_builder.hh"

#include "simd_inst.hh"
#include "exec_profile.hh"
#include "vectorization_legality.hh"

#include "cp_args.hh"

namespace simd {

  class cp_simd : public ArgumentHandler,
                  public VectorizationLegality,
                  public CP_OPDG_Builder<dg_event,
                                       dg_edge_impl_t<dg_event> >

  {

    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;
    typedef std::shared_ptr<simd_inst> SimdInstPtr;

    dep_graph_impl_t<Inst_t, T, E> cpdg;

    enum ModelState {
      IN_SCALAR, // instructions execute in scalar mode
      IN_SIMD    // instructions execute in simd fashion
    };

    unsigned _simd_len = 4;
    bool _useInstTrace = false;

  public:
    cp_simd() : CP_OPDG_Builder<T, E> () {
    }

    virtual ~cp_simd() {}

    virtual void traceOut(uint64_t index,
                          const CP_NodeDiskImage &img, Op* op) {
    }


  protected:
    bool VectorizingLoop = false;

    virtual bool useOpDependence() const {
      return VectorizingLoop;
    }

  public:
    uint64_t numCycles() {
      if (CurLoop) {
        completeSIMDLoop(CurLoop, CurLoopIter);
      }
      return _lastInst->finalCycle();
    }

    void handle_argument(const char *name, const char *optarg) {
      if (strcmp(name, "simd-len") == 0) {
        _simd_len = atoi(optarg);
        if (_simd_len == 0)
          _simd_len = 4;
        return;
      }
      if (strcmp(name, "simd-use-inst-trace") == 0) {
        _useInstTrace = true;
      }
    }

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

    bool simd_state  = IN_SCALAR;


    std::map<Op*, unsigned> _op2Count;

    void printDisasm(uint64_t pc, int upc) {
      std::cout << pc << "," << upc << " : "
                << ExecProfile::getDisasm(pc, upc) << "\n";
    }
    void printDisasm(Op *op) {
      std::cout << "<" << op << ">: ";
      printDisasm(op->cpc().first, op->cpc().second);
    }

    void printLoop(LoopInfo *li) {

      if (!ExecProfile::hasProfile())
        return;

      std::cout << "======================================\n";
      std::cout << "================" << li << "==========\n";
      for (auto BBI = li->body_begin(), BBE = li->body_end(); BBI != BBE; ++BBI) {
        std::cout << "BB<" << *BBI << "> "
                  << ((li->loop_head() == *BBI)?" Head ":"")
                  << ((li->isLatch(*BBI))?" Latch ":"") << "\n";
        std::cout << "Pred::";
        for (auto PI = (*BBI)->pred_begin(), PE = (*BBI)->pred_end(); PI != PE; ++PI) {
          std::cout << " BB<" << *PI << ">";
        }
        std::cout << "\n";
        std::cout << "Succ::";
        for (auto PI = (*BBI)->succ_begin(), PE = (*BBI)->succ_end(); PI != PE; ++PI) {
          std::cout << " BB<" << *PI << ">";
        }
        std::cout << "\n";
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end(); I != E; ++I) {
          Op *op = *I;
          uint64_t pc = op->cpc().first;
          int upc = op->cpc().second;
          std::cout << pc << "," <<upc << " : "
                    << "<" << op << "> "
                    << ((op == (*BBI)->lastOp())?" L ": "  ")
                    << ExecProfile::getDisasm(pc, upc) << "\n";
        }
        std::cout << "\n";
      }
      std::cout << "================" << li << "==========\n";
      std::cout << "======================================\n";
    }


    InstPtr createShuffleInst(InstPtr inst, Op *op = 0)
    {
      InstPtr ret = InstPtr(new shuffle_inst());
      if (op)
        keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    InstPtr createSIMDInst(Op *op)
    {
      InstPtr ret = InstPtr(new Inst_t(op->img, 0));
      keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    bool isStrideAccess(Op *op, int chkStride) {
      int stride = 0;
      if (!op->getStride(&stride)) {
        return false;
      }
      return stride == chkStride;
    }

    std::vector<std::pair<Op*, InstPtr> > vecloop_InstTrace;
    std::map<Op*, uint16_t> _cacheLat;

    void trackSIMDLoop(LoopInfo *li, Op* op, InstPtr inst) {
      vecloop_InstTrace.push_back(std::make_pair(op, inst));
      ++ _op2Count[op];
      //std::cout << "Executed: "  << _op2Count[op] << " times : ";
      //printDisasm(op->cpc().first, op->cpc().second);
      //std::cout << "\n";

      if (op->isLoad()) {
        _cacheLat[op] = std::max(inst->_ex_lat, _cacheLat[op]);
      } else if (op->isStore()) {
        _cacheLat[op] = std::max(inst->_st_lat, _cacheLat[op]);
      }
    }

    void updateForSIMD(Op *op, InstPtr inst, bool useInst) {
      if (op->isLoad()) {
        uint16_t inst_lat = useInst ? inst->_ex_lat : 0;
        inst->_ex_lat = std::max(inst_lat, _cacheLat[op]);
      }
      if (op->isStore()) {
        uint16_t inst_lat = useInst ? inst->_st_lat : 0;
        inst->_st_lat = std::max(inst_lat, _cacheLat[op]);
      }
    }


    void updateForSIMD(Op *op, InstPtr inst) {
      if (op->isLoad()) {
        inst->_ex_lat = std::max(inst->_ex_lat, _cacheLat[op]);
      }
      if (op->isStore()) {
        inst->_st_lat = std::max(inst->_st_lat, _cacheLat[op]);
      }
    }


    void addSIMDDeps(InstPtr &n, Op *op) {
      VectorizingLoop = true;
      addDeps(n, op);
      VectorizingLoop = false;
    }

    void completeSIMDLoop(LoopInfo *li, int CurLoopIter) {
      if (_useInstTrace)
        completeSIMDLoopWithInstTrace(li, CurLoopIter);
      else
        completeSIMDLoopWithLI(li, CurLoopIter);
    }

    void completeSIMDLoopWithInstTrace(LoopInfo *li, int CurLoopIter) {
      //if (vecloop_InstTrace.size() != 0) {
      // std::cout << "Completing SIMD Loop:" << li
      //          << " with iterCnt: " << CurLoopIter << "\n";
      //}
      //if (li) {
      //  printLoop(li);
      //}
      std::map<Op*, bool> emitted;
      //std::cout << "\n====== completeSIMDLoopWithIT ======>>>\n";
      // Add trace to the pipe
      for (auto I = vecloop_InstTrace.begin(), E = vecloop_InstTrace.end();
           I != E; ++I) {
        Op *op = I->first;
        //printDisasm(op);
        // assert((int)_op2Count[op] == CurLoopIter
        //      && "Different control path inside simd loop??");

        if ((CurLoopIter == (int)_simd_len) && emitted.count(op))
          continue;

        emitted.insert(std::make_pair(op, true));

        InstPtr inst = I->second;
        // update for SIMD
        updateForSIMD(op, inst, true);

        addDeps(inst, op);
        pushPipe(inst);
        inserted(inst);

        // handle broadcast_loads
        //  load followed by shuffles ...
        if (op->isLoad() && isStrideAccess(op, 0)) {
          inst = createShuffleInst(inst);
          addDeps(inst);
          pushPipe(inst);
          inserted(inst); // bookkeeping
        }
      }
      //std::cout << "<<<====== completeSIMDLoopWithIT ======\n";
      // clear out instTrace, counts and cache latency
      vecloop_InstTrace.clear();
      _op2Count.clear();
      _cacheLat.clear();
    }

    void completeSIMDLoopWithLI(LoopInfo *li, int CurLoopIter) {

      // if loop did not finish executing fully
      if (CurLoopIter != (int)_simd_len) {
        completeSIMDLoopWithInstTrace(li, CurLoopIter);
        return;
      }

      // printLoop(li);

      //std::cout << "\n====== completeSIMDLoopWithLI ======>>>\n";
      for (auto I = li->rpo_begin(), E = li->rpo_end(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          //printDisasm(op);
          InstPtr inst = createSIMDInst(op);
          updateForSIMD(op, inst, false);

          addSIMDDeps(inst, op);
          pushPipe(inst);
          inserted(inst);

          if (op->isLoad() && isStrideAccess(op, 0)) {
            inst = createShuffleInst(inst, op);
            addDeps(inst);
            pushPipe(inst);
            inserted(inst);
          }
        }
      }
      //std::cout << "<<<====== completeSIMDLoopWithLI ======\n";

      vecloop_InstTrace.clear();
      _op2Count.clear();
      _cacheLat.clear();
    }



    LoopInfo *CurLoop = 0;
    unsigned CurLoopIter = 0;
    uint64_t global_loop_iter = 0;

    //
    // Override insert_inst to transform to SIMD graph
    //
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {

      bool insertSIMDInst = false;
      LoopInfo *li = getLoop(op);

      if (CurLoop != li) {
        //std::cout << "\n";
        #ifdef TRACE_INST
        // We switched to a different loop, complete simd_loop
        std::cout << "DiffLoop<" << li << ">:: ";
        printDisasm(op);
        if (op == op->bb()->lastOp()) {
          std::cout << "\n";
        }
        #endif
        if (CurLoop && canVectorize(CurLoop)) {
          completeSIMDLoop(CurLoop, CurLoopIter);
        }
        CurLoop = li;
        CurLoopIter = 0;
        global_loop_iter = 0;
      } else if (!CurLoop) {

        #ifdef TRACE_INST
        std::cout << "SameLoop<" << CurLoop << ">:: ";
        printDisasm(op);
        if (op == op->bb()->lastOp()) {
          std::cout << "\n";
        }
        #endif
      } else if (CurLoop) {
        #ifdef TRACE_INST
        std::cout << "SameLoop<" << CurLoop << ">:: ";
        printDisasm(op);
        if (op == op->bb()->lastOp()) {
          std::cout << "\n";
        }
        #endif
        // Same Loop, incr iteration
        if (op == op->bb()->lastOp() // last instruction in the bb
            && CurLoop->isLatch(op->bb())) // Latch in the current loop)
          {
            // Increment loop counter that signals the trace
            // completed "CurLoopIter" times
            //
            ++CurLoopIter;
            ++global_loop_iter;
            #if 0
            std::cout << "Loop: " << CurLoop
                      << " executed " << global_loop_iter
                      << "[ " << CurLoopIter << " ] times\n";
            #endif
          }
        // set insertSIMDInstr if curloopiter is a multiple of _simd_len.
        insertSIMDInst =  (canVectorize(CurLoop)
                           && CurLoopIter && (CurLoopIter % _simd_len == 0));
      }


      if (!canVectorize(li)) {
        // Create the instruction
        InstPtr inst = createInst(img, index, op);

        // Add to the graph to do memory management
        // and get static dependence from original dependence graph...
        //std::cout << "Adding instruction with index:: " << index << "\n";
        getCPDG()->addInst(inst, index);

        addDeps(inst, op); // Add Dependence Edges
        pushPipe(inst);    // push in to the pipeline
        inserted(inst);    // Book keeping
      } else {
        // Create the instruction -- but donot track op <-> inst
        InstPtr inst = createInst(img, index, 0);
        getCPDG()->addInst(inst, index);
        trackSIMDLoop(li, op, inst);
      }


      if (insertSIMDInst) {
        completeSIMDLoop(CurLoop, CurLoopIter);
        CurLoopIter = 0; // reset loop counter..
      }
    }

  };

} // SIMD namespace

#endif
