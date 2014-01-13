
#ifndef CP_SIMD_HH
#define CP_SIMD_HH

#include "cp_opdg_builder.hh"

#include "simd_inst.hh"
#include "exec_profile.hh"
#include "vectorization_legality.hh"

#include "cp_args.hh"

static unsigned floor_to_pow2(unsigned num)
{
  if (num & (num-1)) {
    num = num - 1;
    num |= (num >> 1);
    num |= (num >> 2);
    num |= (num >> 4);
    num |= (num >> 8);
    num |= (num >> 16);
    num += 1;
    num >>= 1;
  }
  return num;
}

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


    dep_graph_impl_t<Inst_t, T, E> cpdg;

    enum ModelState {
      IN_SCALAR, // instructions execute in scalar mode
      IN_SIMD    // instructions execute in simd fashion
    };

    unsigned _simd_vec_len = 4;
    bool _useInstTrace = false;
    bool _simd_full_dataflow = false;
    bool nonStrideAccessLegal = false;
    bool unalignedVecAccess = false;
    bool useReductionTree = false;
    bool useSplittedOps   = true;
    bool useMergeOps = true;
    unsigned simd_exec_width = 1;
    double  simd_inst_incr_factor = 0.5;
  public:
    cp_simd() : CP_OPDG_Builder<T, E> () {
    }

    virtual ~cp_simd() {}

    virtual void traceOut(uint64_t index,
                          const CP_NodeDiskImage &img, Op* op) {
      //printDisasm(op);
      //CP_OPDG_Builder<T, E>::traceOut(index, img, op);
    }

    std::map<Op*, bool> splittedOpMap;
    bool isOpSplitted(Op *op) {
      if (!useSplittedOps)
        return false;

      auto I = splittedOpMap.find(op);
      if (I != splittedOpMap.end())
        return I->second;

      bool splitted = false;
      uint64_t pc = op->cpc().first;
      int upc = op->cpc().second;
      std::string disasm = ExecProfile::getDisasm(pc, upc);

      if ((disasm.find("ADDSS_XMM_XMM") != std::string::npos)
          || (disasm.find("SUBSS_XMM_XMM") != std::string::npos)
          || (disasm.find("MULSS_XMM_XMM") != std::string::npos)
          || (disasm.find("MULSS_XMM_M") != std::string::npos && upc == 1)
          || (disasm.find("SUBSS_XMM_M") != std::string::npos && upc == 1)
          || (disasm.find("SUBSS_XMM_P") != std::string::npos && upc == 2)
          || (disasm.find("RSQRTSS_XMM_XMM") != std::string::npos)) {
        splitted = true;
      }

      splittedOpMap[op] = splitted;
      return splitted;
    }

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
      if ((disasm.find("MOVSS_XMM_M") != std::string::npos && upc == 0))
        merged = true;

      mergedOpMap[op] = merged;
      return merged;
    }


    Op* getMaxResultOp(std::set<Op*> &resultsOp) {
      Op *maxResultOp = 0;
      unsigned max = 0;
      for (auto I = resultsOp.begin(), E = resultsOp.end(); I != E; ++I) {
        Op *op = *I;
        BaseInstPtr inst = getInstForOp(op);
        if (!inst.get())
          continue;
        unsigned completeCycle = inst->cycleOfStage(inst->eventComplete());
        if (completeCycle > max) {
          max = completeCycle;
          maxResultOp = op;
        }
      }
      return maxResultOp;
    }

    unsigned _startPipe = 0;
    virtual void markStartPipe() {
      _startPipe = getCPDG()->getPipeLoc();
    }

    void dumpPipe() {
      return ;
      int offset = getCPDG()->getPipeLoc() - _startPipe;
      assert(offset > 0);
      for (int i = offset; i >= 0; --i) {
        assert(i >= 0);
        Inst_t *ptr = getCPDG()->peekPipe(-i);
        if (!ptr)
          continue;
        InstPtr inst = InstPtr(ptr);


        for (unsigned j = 0; j < inst->numStages(); ++j) {
          std::cout << std::setw(5) << inst->cycleOfStage(j) << " ";
        }
        std::cout << "\n";
        continue;

        if (inst->hasDisasm()) {
          std::cout << inst->getDisasm() << "\n";
          continue;
        }

        //Inst_t *n = inst.get();
        Op *op = getOpForInst(*inst, true);
        if (op)
          printDisasm(op);
        else
          std::cout << "\n";
      }
      _startPipe = 0;
    }
  protected:
    bool VectorizingLoop = false;

    virtual bool useOpDependence() const {
      return VectorizingLoop;
    }

  public:

    bool shouldVectorize(LoopInfo *CurLoop) {
      return canVectorize(CurLoop, nonStrideAccessLegal,
                          simd_inst_incr_factor);
    }

    virtual uint64_t finish() {
      if (CurLoop && shouldVectorize(CurLoop)) {
        // complete the loop ...
        completeSIMDLoop(CurLoop, CurLoopIter, false, true);
      }
     
      return CP_DG_Builder::finish();
    }

/*
    uint64_t numCycles() {
      if (CurLoop && shouldVectorize(CurLoop)) {
        // complete the loop ...
        completeSIMDLoop(CurLoop, CurLoopIter, false, true);
      }
      return _lastInst->finalCycle();
    }
*/
    void handle_argument(const char *name, const char *optarg) {
      if (strcmp(name, "simd-len") == 0) {
        _simd_vec_len = atoi(optarg);
        if (_simd_vec_len == 0)
          _simd_vec_len = 4;
        return;
      }
      if (strcmp(name, "simd-use-inst-trace") == 0)
        _useInstTrace = true;

      if (strcmp(name, "simd-full-dataflow") == 0) {
        _useInstTrace = true;
        _simd_full_dataflow = true;
      }

      if (strcmp(name, "allow-non-stride-vec") == 0)
        nonStrideAccessLegal = true;

      if (strcmp(name, "unaligned-vec-access") == 0)
        unalignedVecAccess = true;

      if (strcmp(name, "use-reduction-tree") == 0)
        useReductionTree = true;
      if (strcmp(name, "disallow-splitted-op") == 0)
        useSplittedOps = false;
      if (strcmp(name, "disallow-merge-op") == 0)
        useMergeOps = false;

      if (strcmp(name, "simd-exec-width") == 0) {
        useSplittedOps = false; //off splitted -- instead use the width
        useMergeOps = false;
        simd_exec_width = atoi(optarg);
      }
      if (strcmp(name, "simd-inst-incr-factor") == 0) {
        simd_inst_incr_factor = atof(optarg);
      }

    }

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

    bool simd_state  = IN_SCALAR;


    std::map<Op*, unsigned> _op2Count;


    void printLoop(LoopInfo *li) {

      //if (!ExecProfile::hasProfile())
      //  return;

      static std::set<LoopInfo*> li_printed;
      if (li_printed.count(li))
        return;

      li_printed.insert(li);


      std::cout << "======================================\n";
      std::cout << "================" << li << "==========\n";
      if (li->isInnerLoop())
        std::cout << "Inner Loop\n";
      else
        std::cout << "Not an inner Loop\n";


      if (shouldVectorize(li))
        std::cout << "Vectorizable\n";
      else
        std::cout << "Nonvectorizable\n";

      for (auto BBI = li->rpo_rbegin(), BBE = li->rpo_rend(); BBI != BBE; ++BBI) {
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
                    << (isStrideAccess(op)?" S ": "  ")
                    << ExecProfile::getDisasm(pc, upc) << "\n";
        }
        std::cout << "\n";
      }
      std::cout << "================" << li << "==========\n";
      std::cout << "======================================\n";
      std::cout.flush();
    }


    InstPtr createShuffleInst(InstPtr inst, Op *op = 0)
    {
      InstPtr ret = InstPtr(new shuffle_inst(op));
      ret->isAccelerated=true;
      if (op)
        keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    InstPtr createPackInst(int prod1, int prod2, Op *op = 0)
    {
      InstPtr ret = InstPtr(new pack_inst(prod1, prod2,op));
      ret->isAccelerated=true;
      if (op)
        keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    InstPtr createReduceInst(InstPtr di, Op *op = 0)
    {
      InstPtr ret = InstPtr(new reduce_inst(di, op));
      ret->isAccelerated=true;
      if (op)
        keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    InstPtr createUnpackInst(InstPtr di, Op *op = 0)
    {
      InstPtr ret = InstPtr(new unpack_inst(di, op));
      ret->isAccelerated=true;
      if (op)
        keepTrackOfInstOpMap(ret, op);
      return ret;
    }

    InstPtr createSIMDInst(Op *op)
    {
      InstPtr ret = InstPtr(new Inst_t(op->img, 0, op));
      ret->isAccelerated=true;
      keepTrackOfInstOpMap(ret, op);
      return ret;
    }

#if 0
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
#endif

    void addSIMDDeps(InstPtr &n, Op *op) {
      VectorizingLoop = true;
      addDeps(n, op);
      VectorizingLoop = false;
    }


    void completeSIMDLoop(LoopInfo *li, int CurLoopIter,
                          bool useIT, bool loopDone) {
      static std::set<LoopInfo*> dumped;

      insert_inst_trace_to_default_pipe();

      markStartPipe();
      if (_useInstTrace || useIT)
        completeSIMDLoopWithInstTrace(li, CurLoopIter, useIT, _simd_vec_len);
      else
        completeSIMDLoopWithLI(li, CurLoopIter, loopDone);
      if (!dumped.count(li)) {
        dumped.insert(li);
        dumpPipe();
      }

    cleanupLoopInstTracking();

    static unsigned numCompleted = 0;
    if (getenv("MAFIA_DEBUG_SIMD_LOOP")) {
      unsigned configParam = atoi(getenv("MAFIA_DEBUG_SIMD_LOOP"));
      ++numCompleted;
      if (configParam < numCompleted)
        exit(0);
    }
  }

    void completeSIMDLoopWithInstTrace(LoopInfo *li,
                                       int CurLoopIter,
                                       bool forceIT,
                                       unsigned vec_len) {
      //if (vecloop_InstTrace.size() != 0) {
      // std::cout << "Completing SIMD Loop:" << li
      //          << " with iterCnt: " << CurLoopIter << "\n";
      //}
      //if (li) {
      //  printLoop(li);
      //}
      std::vector<InstPtr> unpackInsts;
      std::map<Op*, bool> emitted;
      int curIter = -1;
      int nextIterationToPushPipe = 0;
      //std::cout << "\n====== completeSIMDLoopWithIT ======>>>\n";
      // Add trace to the pipe
      for (auto I = _loop_InstTrace.begin(), E = _loop_InstTrace.end();
           I != E; ++I) {
        Op *op = I->first;

        if ((CurLoopIter == (int)vec_len) && emitted.count(op))
          continue;

        bool isAccelerated = false;
        if (_simd_full_dataflow && (CurLoopIter >= 4)) {
          isAccelerated = true;
          if (op->bb_pos() == 0 && li->loop_head() == op->bb()) {
            ++curIter;
            if (curIter != nextIterationToPushPipe) {
              int tmpVal = floor_to_pow2(CurLoopIter - nextIterationToPushPipe);
              if (tmpVal > 3)
                nextIterationToPushPipe += tmpVal;
              else {
                nextIterationToPushPipe = curIter;
                isAccelerated = false;
              }
            }
          }
          if (curIter != nextIterationToPushPipe)
            continue;
        }

        emitted.insert(std::make_pair(op, true));

        unpackInsts.clear();
        unpackInsts.resize(vec_len);
        if (!forceIT && op->isLoad()) {
          if (!isStrideAccess(op)) {
            // we need to create unpack instruction for the loads
            uint64_t maxDepCycle = 0;
            InstPtr maxDepInst  = 0;

            for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
              Op *DepOp = *I;
              InstPtr depInst = 
                std::static_pointer_cast<Inst_t>(getInstForOp(DepOp));
              if (!depInst.get())
                continue;
              uint64_t completeCycle =
                depInst->cycleOfStage(depInst->eventComplete());
              if (completeCycle > maxDepCycle) {
                maxDepCycle = completeCycle;
                maxDepInst = depInst;
              }
            }
            if (maxDepInst.get() != 0) {
              for (unsigned i = 0; i < vec_len; ++i) {
                // create a instruction for depop
                InstPtr unpack = createUnpackInst(maxDepInst);
                addInstToProducerList(maxDepInst);
                addInstListDeps(unpack, 0);
                pushPipe(unpack);
                inserted(unpack);
                unpackInsts[i] = unpack;
              }
            }
          }
        }

        InstPtr inst = I->second;
        if (isAccelerated)
          inst->isAccelerated = true;

        keepTrackOfInstOpMap(inst, op);

        // update for SIMD
        // updateInstWithTraceInfo(op, inst, true);

        if (unpackInsts[0].get()) {
          // create the depedence edge
          getCPDG()->insert_edge(*unpackInsts[0].get(),
                                 unpackInsts[0]->eventComplete(),
                                 *inst.get(), Inst_t::Ready, 0, E_RDep);
        }

        addDeps(inst, op);
        pushPipe(inst);
        inserted(inst);

        // handle broadcast_loads or non strided loads
        //  load followed by shuffles ...
        if (op->isLoad()) {
            if (unalignedVecAccess) {
              // Create another instruction to simulate the unaligned access
              InstPtr inst = createSIMDInst(op);
              updateInstWithTraceInfo(op, inst, false);
              addSIMDDeps(inst, op);
              pushPipe(inst);
              inserted(inst);
            }

          if (isStrideAccess(op, 0)) {
            InstPtr sh_inst = createShuffleInst(inst);
            addPipeDeps(sh_inst, 0);
            pushPipe(sh_inst);
            inserted(sh_inst); // bookkeeping
          }
          // Non strided -- create more loads
          if (!forceIT && !isStrideAccess(op)) {
            std::vector<unsigned> loadInsts(vec_len);
            loadInsts[0] =  0;
            for (unsigned i = 1; i < vec_len; ++i) {
              InstPtr tmpInst = createInst(op->img,
                                           inst->index(), op);
              if (unpackInsts[i].get()) {
                // create the depedence edge
                getCPDG()->insert_edge(*unpackInsts[i].get(),
                                       unpackInsts[i]->eventComplete(),
                                       *inst.get(), Inst_t::Ready,
                                       0, E_RDep);
              }
              addDeps(tmpInst);
              pushPipe(tmpInst);
              inserted(tmpInst);
              loadInsts[i] = i;
            }
            unsigned InstIdx = vec_len;
            unsigned numInst = vec_len;
            while (numInst > 1) {
              for (unsigned i = 0, j = 0; i < numInst; i += 2, ++j) {
                unsigned op0Idx = loadInsts[i];
                unsigned op1Idx = loadInsts[i+1];

                InstPtr tmpInst = createPackInst(InstIdx - op0Idx,
                                                 InstIdx - op1Idx,
                                                 op);
                addPipeDeps(tmpInst, op);
                pushPipe(tmpInst);
                inserted(tmpInst);
                loadInsts[j] = InstIdx++;
              }
              numInst = numInst/2;
            }
          }
        }
        }
      //std::cout << "<<<====== completeSIMDLoopWithIT ======\n";
      // clear out instTrace, counts and cache latency
      //cleanupLoopInstTracking();
      }

  void completeSIMDLoopWithLI(LoopInfo *li, int CurLoopIter, bool loopDone) {
      std::set<Op*> internalCtrlOps;
      std::set<Op*> resultOps;
      std::vector<InstPtr> unpackInsts;

      uint64_t pc = 0;
      for (auto I = li->rpo_rbegin(), E = li->rpo_rend(); I != E; ++I) {
        BB *bb = *I;
        Op *ref_op = 0;
        for (auto OI = bb->op_rbegin(), OE = bb->op_rend(); OI != OE; ++OI) {
          Op *op = *OI;
          if (pc != op->cpc().first) {
            ref_op = op;
            pc = op->cpc().first;
          }
          if (!li->isLatch(bb) &&
              ref_op->isCtrl() && !(ref_op->isCall() || ref_op->isReturn())) {
            internalCtrlOps.insert(op);
          }
          for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
            Op *UseOp = *UI;
            if (li->inLoop(UseOp->bb()))
              continue;
            // A use outside the loop -- can we consider it a output
            // A result op
            resultOps.insert(op);
          }
        }
      }

      if (CurLoopIter <= 3) {
        // if loop did not finish executing fully
        if (CurLoopIter != (int)_simd_vec_len) {
          if (useReductionTree && loopDone) {
            for (auto I = resultOps.begin(), E = resultOps.end(); I != E; ++I) {
              Op *op = *I; //getMaxResultOp(resultOps)) {
              //Tony: I put the cast here, because i guess you need it.  hope it works!
              BaseInstPtr resultInst_base =  this->getInstForOp(op);
              InstPtr resultInst = std::static_pointer_cast<Inst_t>(resultInst_base);
              // FIXME: very conservative code -- for kmeans ????
              for (unsigned i = 0; i < _simd_vec_len-1; ++i) {
                InstPtr tmpInst = createReduceInst(resultInst, op);
                addInstToProducerList(resultInst);
                addInstListDeps(tmpInst, 0);
                pushPipe(tmpInst);
                inserted(tmpInst);
                resultInst = tmpInst;
              }
            }
          }
        }
        completeSIMDLoopWithInstTrace(li, CurLoopIter, true, _simd_vec_len);
        return;
      }

      if (CurLoopIter & (CurLoopIter - 1)) {
        // not a power of 2
        int loopIter = floor_to_pow2(CurLoopIter);
        assert(loopIter && loopIter >= 4 && loopIter < CurLoopIter);
        redoTrackLoopInsts(li, loopIter);
        completeSIMDLoopWithLI(li, loopIter, false);

        completeSIMDLoopWithLI(li, (CurLoopIter - loopIter), loopDone);
        return;
      }

      unsigned vec_len = CurLoopIter;
      assert(vec_len <= _simd_vec_len);


      for (auto I = li->rpo_rbegin(), E = li->rpo_rend(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;

          // If it is internal control, and not a call or return
          // -- we are not creating instruction
          if (internalCtrlOps.count(op)) {
            continue;
          }
          // if this op will be merged to others in SIMD event graph, skip
          if (isOpMerged(op))
            continue;

          unpackInsts.clear();
          unpackInsts.resize(vec_len);

          if (op->isLoad()) {
            if (!isStrideAccess(op)) {
              // we need to create unpack instruction for the loads
              uint64_t maxDepCycle = 0;
              InstPtr maxDepInst  = 0;

              for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
                Op *DepOp = *I;
                InstPtr depInst = std::static_pointer_cast<Inst_t>(
                                               getInstForOp(DepOp));
                if (!depInst.get())
                  continue;
                uint64_t completeCycle =
                  depInst->cycleOfStage(depInst->eventComplete());
                if (completeCycle > maxDepCycle) {
                  maxDepCycle = completeCycle;
                  maxDepInst = depInst;
                }
              }
              if (maxDepInst.get() != 0) {
                unsigned acc_bits = op->img._acc_size * 8;
                // FIXME: Decouple _simd_len from number of iterations to be vectorized.
                unsigned num_registers_needed = acc_bits/32;
                if (num_registers_needed == 0)
                  num_registers_needed = 1;
                num_registers_needed = 2; // Override for TreeSearch ... TOTALLY WRONG THING TO DO
                for (unsigned i = 0; i < vec_len; ++i) {
                  for (unsigned j = 0; j < num_registers_needed; ++j) {
                    // create a instruction for depop
                    InstPtr unpack = createUnpackInst(maxDepInst);
                    addInstToProducerList(maxDepInst);
                    addInstListDeps(unpack, 0);
                    pushPipe(unpack);
                    inserted(unpack);

                    // create another instruction to move to integer register
                    InstPtr unpackMov = createUnpackInst(unpack);
                    addInstToProducerList(unpack);
                    addInstListDeps(unpackMov, 0);
                    pushPipe(unpackMov);
                    inserted(unpackMov);
                    unpackInsts[i] = unpackMov;
                  }
                }
              }
            }
          }

          //printDisasm(op);
          InstPtr inst = createSIMDInst(op);
          updateInstWithTraceInfo(op, inst, false);

          if (unpackInsts[0].get()) {
            // create the depedence edge
            getCPDG()->insert_edge(*unpackInsts[0].get(),
                                   unpackInsts[0]->eventComplete(),
                                   *inst.get(), Inst_t::Ready, 0, E_RDep);
          }

          addSIMDDeps(inst, op);
          pushPipe(inst);
          inserted(inst);

          if (isOpSplitted(op)) {
            //insert the instruction again.
            InstPtr inst = createSIMDInst(op);
            updateInstWithTraceInfo(op, inst, false);
            addSIMDDeps(inst, op);
            pushPipe(inst);
            inserted(inst);
          }

          // simd_exec_width only for non-mem, non-ctrl instructions.
          // FIXME:: Should apply for the instruction -- not just microops
          if (simd_exec_width > 1
              && !(op->isLoad() || op->isStore() || op->isCtrl())) {
            for (unsigned i = 1; i < simd_exec_width; ++i) {
              InstPtr inst = createSIMDInst(op);
              updateInstWithTraceInfo(op, inst, false);
              addSIMDDeps(inst, op);
              pushPipe(inst);
              inserted(inst);
            }
          }

          if (op->isLoad()) {
            if (unalignedVecAccess) {
              // Create another instruction to simulate the unaligned access
              InstPtr inst = createSIMDInst(op);
              updateInstWithTraceInfo(op, inst, false);
              addSIMDDeps(inst, op);
              pushPipe(inst);
              inserted(inst);
            }

            if (isStrideAccess(op, 0)) {
              InstPtr sh_inst = createShuffleInst(inst, op);
              addPipeDeps(sh_inst, 0);
              pushPipe(sh_inst);
              inserted(sh_inst);
            }

            // Non strided -- create more loads
            if (!isStrideAccess(op)) {
              std::vector<unsigned> loadInsts(vec_len);
              loadInsts[0] =  0;
              for (unsigned i = 1; i < vec_len; ++i) {
                InstPtr tmpInst = createInst(op->img, inst->index(), op);
                // // They should atleas tmpInst
                // for (unsigned j = 0; j < tmpInst->numStages(); ++j) {
                //  tmpInst[j]->setCycle(inst->cycleOfStage(j));
                //}
                if (unpackInsts[i].get()) {
                  // create the depedence edge
                  getCPDG()->insert_edge(*unpackInsts[i].get(),
                                         unpackInsts[i]->eventComplete(),
                                         *tmpInst.get(), Inst_t::Ready,
                                         0, E_RDep);
                }
                addSIMDDeps(tmpInst, op);
                pushPipe(tmpInst);
                inserted(tmpInst);
                loadInsts[i] = i;
              }
              unsigned InstIdx = vec_len;
              unsigned numInst = vec_len;
              while (numInst > 1) {
                for (unsigned i = 0, j = 0; i < numInst; i += 2, ++j) {
                  unsigned op0Idx = loadInsts[i];
                  unsigned op1Idx = loadInsts[i+1];

                  InstPtr tmpInst = createPackInst(InstIdx - op0Idx,
                                                   InstIdx - op1Idx,
                                                   op);
                  addPipeDeps(tmpInst, 0);
                  pushPipe(tmpInst);
                  inserted(tmpInst);
                  loadInsts[j] = InstIdx ++;
                }
                numInst = numInst/2;
              }
            }
          }
        }
      }

      //std::cout << "<<<====== completeSIMDLoopWithLI ======\n";
      if (CurLoopIter != (int)_simd_vec_len)
        cleanupLoopInstTracking(li, CurLoopIter);
  }



    LoopInfo *CurLoop = 0;
    unsigned CurLoopIter = 0;
    uint64_t global_loop_iter = 0;

    bool isLastInstACall = false;
    bool isLastInstAReturn = false;
    bool forceCompleteWithIT = false;
    LoopInfo *StackLoop = 0;
    unsigned StackLoopIter = 0;

    //
    // Override insert_inst to transform to SIMD graph
    //
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {

      bool insertSIMDInst = false;
      LoopInfo *li = getLoop(op, isLastInstAReturn, StackLoop);


      if (getenv("MAFIA_DEBUG_SIMD_LOOP")) {
        if (li)
          printLoop(li);
      }

      if (CurLoop != li) {

        if (isLastInstACall
            && CurLoop
            && shouldVectorize(CurLoop)) {
          if (op->func()->nice_name() == "__libm_sse2_sincosf") {
            // We can vectorize this function as well.
            StackLoop = CurLoop;
            StackLoopIter = CurLoopIter;
            forceCompleteWithIT = true;
          }
        }

        //std::cout << "\n";
        if (getenv("MAFIA_DUMP_SIMD_LOOP")) {
          // We switched to a different loop, complete simd_loop
          std::cout << "Diff Loop<" << li << ">" << CurLoopIter << ":: StackLoop <" << StackLoop << ">" << StackLoopIter << "::"
                    << ((isLastInstACall)?"L..Call":"") << ((isLastInstAReturn)?"L..Return":"");
          printDisasm(op);
          if (op == op->bb()->lastOp()) {
            std::cout << "\n";
          }
        }

        if (!StackLoop
            && CurLoop
            && shouldVectorize(CurLoop)) {
          completeSIMDLoop(CurLoop, CurLoopIter, forceCompleteWithIT, true);
          forceCompleteWithIT = false;
        }
        CurLoop = li;
        CurLoopIter = 0;
        global_loop_iter = 0;

        if (isLastInstAReturn) {
          StackLoop = 0;
          CurLoopIter = StackLoopIter;
          StackLoopIter = 0;
          if (getenv("MAFIA_DUMP_SIMD_LOOP")) {
            // We switched to a different loop, complete simd_loop
            std::cout << "  Reset Loop<" << li << ">" << CurLoopIter << ":: StackLoop <" << StackLoop << ">" << StackLoopIter << "::";
          }

        }

      } else if (!CurLoop) {

        if (getenv("MAFIA_DUMP_SIMD_LOOP")) {
          std::cout << "SameLoop<" << CurLoop << ">" << CurLoopIter << ":: " << ">:: StackLoop<" << StackLoop << ">" << StackLoopIter << "::";
          printDisasm(op);
          if (op == op->bb()->lastOp()) {
            std::cout << "\n";
          }
        }
      } else if (CurLoop) {
        if (getenv("MAFIA_DUMP_SIMD_LOOP")) {
          std::cout << "SameLoop<" << CurLoop << ">" << CurLoopIter<< ":: " << ">:: StackLoop<" << StackLoop << ">" << StackLoopIter << "::";
          printDisasm(op);
          if (op == op->bb()->lastOp()) {
            std::cout << "\n";
          }
        }
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
        insertSIMDInst =  (shouldVectorize(CurLoop)
                           && CurLoopIter && (CurLoopIter % _simd_vec_len == 0)
                           && !StackLoop);
      }

      if (op) {
        isLastInstACall = op->isCall();
        isLastInstAReturn = op->isReturn();
      }

      if (!StackLoop && !shouldVectorize(li)) {

        insert_inst_to_default_pipe(img, index, op);

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
        //std::cout << "vector Adding instruction with index:: " << index << "\n";

        InstPtr inst = createInst(img, index, 0);
        getCPDG()->addInst(inst, index);
        trackLoopInsts(li, op, inst, img);
      }


      if (insertSIMDInst) {
        completeSIMDLoop(CurLoop, CurLoopIter, forceCompleteWithIT, false);
        CurLoopIter = 0; // reset loop counter..
        forceCompleteWithIT = false;
      }
    }

  };

} // SIMD namespace

#endif
