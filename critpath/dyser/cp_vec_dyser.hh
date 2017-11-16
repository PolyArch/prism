
#ifndef CP_VEC_DYSER_HH
#define CP_VEC_DYSER_HH

#include "cp_dyser.hh"

namespace DySER {
  class cp_vec_dyser : public cp_dyser {

  protected:
    unsigned _dyser_vec_len = 16;
    bool nonStrideAccessLegal = false;
    bool countDepthNodesForConfig = false;
    bool forceVectorize = false;
  public:
    cp_vec_dyser() : cp_dyser() { }
    virtual ~cp_vec_dyser() { }

    void handle_argument(const char *name, const char *optarg) {
      cp_dyser::handle_argument(name, optarg);

      if (strcmp(name, "dyser-vec-len") == 0) {
        _dyser_vec_len = atoi(optarg);
        if (_dyser_vec_len == 0)
          _dyser_vec_len = 16;
      }
      if (strcmp(name, "disallow-non-stride-vec") == 0)
        nonStrideAccessLegal = false;
      if (strcmp(name, "allow-non-stride-vec") == 0)
        nonStrideAccessLegal = true;
      if (strcmp(name, "dyser-count-depth-nodes-for-config") == 0)
        countDepthNodesForConfig = true;
      if (strcmp(name, "dyser-force-vectorize") == 0)
        forceVectorize = true;
    }


    int num_sends_recvs(LoopInfo* LI) {

      SI = SliceInfo::get(LI, _dyser_size);

      int num_sends=0, num_recvs=0;

      for(auto I=LI->body_begin(),E=LI->body_end();I!=E;++I) {
        BB* bb = *I;
        for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
          Op* op = *II;
          if(!op->isLoad() &&  SI->isAInputToDySER(op)) {
            num_sends++;
          }
          if(SI->isADySEROutput(op) && !allUsesAreStore(op)) {
            num_recvs++;
          }
        }
      }
      int num_configs = SI->cs_size() / _dyser_size;
      return num_sends+num_recvs+num_configs*_num_cycles_switch_config;
    }


    virtual float estimated_benefit(LoopInfo* li) {
      if(!li->isInnerLoop()) {
        return 0.0f;
      }

      if(!canDySERize(li)) {
        return 0.0f;
      }

      uint64_t totalIterCount = li->getTotalIters();
      uint64_t totalDynamicInst = li->numInsts();
      uint64_t totalStaticInstCount = li->getStaticInstCount();
      uint64_t totalInstInSB = totalIterCount * totalStaticInstCount;
      
      if(totalInstInSB ==0) {
        return 0.0f;
      }

      bool no_vec = !canVectorize(li, nonStrideAccessLegal,_dyser_inst_incr_factor);

      if(no_vec) {
        float speedup = 1.1f;
        return speedup;
      } 

      float avg_insts_per_iter = totalDynamicInst / (float) totalIterCount;
      float speedup = avg_insts_per_iter / 1.05f /
                       ( (totalStaticInstCount + num_sends_recvs(li)) / (float) _dyser_vec_len ); 

      return speedup;
    }


    bool shouldCompleteThisLoop(LoopInfo *CurLoop,
                                unsigned CurLoopIter) override
    {
      if (!CurLoop)
        return false;
      if (!canDySERize(CurLoop))
        return false;
      if (CurLoopIter == 0)
        return false;
      // complete this as soon as it completes a iteration
      // we cannot vectorize this.
      bool noVectorize = !canVectorize(CurLoop,
                                       nonStrideAccessLegal,
                                       _dyser_inst_incr_factor);

      if (noVectorize && !forceVectorize) {
        return true;
      }

      // wait until we _dyser_vec_len worth of iteration
      if ( (CurLoopIter % _dyser_vec_len) == 0)
        return true;
      return false;
    }

    void dumpCloneOp2InstMap() {
      for (auto I = cloneOp2InstMap.begin(), E = cloneOp2InstMap.end();
           I != E; ++I) {
        Op *op = I->first;
        std::cout << "Op: " << op << "\n";
        for (auto J = I->second.begin(), JE = I->second.end();
             J != JE; ++J) {
          InstPtr inst = J->second;
          std::cout << "\t\t" << J->first;
          dumpInst(inst);
        }
      }
    }

    virtual void completeDySERLoopWithIT(LoopInfo *DyLoop,
                                         unsigned curLoopIter,
                                         bool loopDone) override
    {
      assert(0);
      if (!(curLoopIter && ((curLoopIter % _dyser_vec_len) == 0))) {
        cp_dyser::completeDySERLoopWithIT(DyLoop,
                                          CurLoopIter, loopDone);
        return;
      }

      assert(DyLoop);
      SI = SliceInfo::get(DyLoop, _dyser_size);
      assert(SI);

      unsigned cssize = SI->cs_size();

      // 19/16 = 1 extraConfigRequired
      unsigned extraConfigRequired = (cssize / _dyser_size);

      unsigned numClones = _dyser_size / cssize;
      numClones = floor_to_pow2(numClones);
      if (numClones == 0)
        numClones = 1;

      unsigned depth = (_dyser_vec_len / numClones);
      if (depth == 0)
        depth = 1;

      incrConfigSwitch(0, extraConfigRequired);

      std::map<Op*, unsigned> emitted;
      for (auto I = _loop_InstTrace.begin(), E = _loop_InstTrace.end();
           I != E; ++I) {
        auto op_n_Inst  = *I;
        Op *op = op_n_Inst.first;

        // emit one instruction per op for load slice
        if (curLoopIter == _dyser_vec_len) {
          if (SI->isInLoadSlice(op)) {
            if (emitted.count(op))
              continue;
          }
        }
        // emit _dyser_vec_len times for op in computation slice.
        InstPtr inst = op_n_Inst.second;
        insert_sliced_inst(SI, op, inst, false, false);
      }
    }


    bool useCloneOpMap = false;
    unsigned pipeId = 0;
    std::map<Op*, std::map<unsigned, InstPtr > > cloneOp2InstMap;
    //std::map<unsigned, InstPtr > > sincosInstMap;

    BaseInstPtr getInstForOp(Op* op) {
      bool dumpClone = true;
      InstPtr ret = 0;
      if (!useCloneOpMap || SI->isInLoadSlice(op, true)) {
        ret = std::static_pointer_cast<Inst_t>(cp_dyser::getInstForOp(op));
        dumpClone = false;
      } else if (cloneOp2InstMap.count(op) != 0 &&
                 cloneOp2InstMap[op].count(pipeId) != 0) {
        ret = cloneOp2InstMap[op][pipeId];
        dumpClone = false;
      }
      if (getenv("MAFIA_DUMP_DEP")) {
        std::cout << "   inst< " << ret.get() << "> op:";
        printDisasmPC(op->cpc().first, op->cpc().second);
      }
      if (dumpClone) {
        // we are here because of phi??
        // dumpCloneOp2InstMap();
      }
      return ret;
#if 0
      assert(0);

      InstPtr retInst = cp_dyser::getInstForOp(op);
      //cache it here
      cloneOp2InstMap[op][0] = retInst;
      return retInst;
#endif
    }

    virtual void completeDySERLoopWithLI(LoopInfo *LI, int curLoopIter,
                                         bool loopdone) override {

      if(TRACE_DYSER_MODEL) {
        std::cout << "completeDySERLoop -- VECTOR, iter:" << curLoopIter;
        if(loopdone) {
          std::cout << ", loop DONE";
        } else {
          std::cout << " loop not done";
        }
        std::cout<< "\n";
      }


      if (getenv("MAFIA_DYSER_LOOP_ARG") != 0) {
        if (canVectorize(LI, nonStrideAccessLegal, _dyser_inst_incr_factor)) {
          std::cout << "Vectorizable: curLoopIter:" << curLoopIter << "\n";
        } else {
          std::cout << "NonVectorizable:" << curLoopIter << "\n";
        }
      }
      // we can vectorize 2, 4, 8, 16 .. to max _dyser_vec_len

      if (curLoopIter <= 3 ) {
        cp_dyser::completeDySERLoopWithLI(LI, curLoopIter, loopdone);
        return;
      }
      if (curLoopIter & (curLoopIter - 1)) {
        // call recursively
        int loopIter = floor_to_pow2(curLoopIter);
        assert(loopIter && loopIter >= 4 && loopIter < curLoopIter);
        completeDySERLoopWithLI(LI, loopIter, false);
        completeDySERLoopWithLI(LI, (curLoopIter - loopIter), loopdone);
        return;
      }


      assert(LI);
      SI = SliceInfo::get(LI, _dyser_size);
      assert(SI);


      unsigned vec_len = curLoopIter;
      unsigned cssize = SI->cs_size();

      // 19/16 = 1 extraConfigRequired
      unsigned extraConfigRequired = (cssize / _dyser_size);

      unsigned numClones = _dyser_size / cssize;
      numClones = floor_to_pow2(numClones);
      if (numClones == 0)
        numClones = 1;

      if (numClones > vec_len)
        numClones = vec_len;

      unsigned depth = (vec_len / numClones);
      if (depth == 0)
        depth = 1;

      assert(numClones*depth == vec_len);

      incrConfigSwitch(0,  extraConfigRequired);

      if (getenv("MAFIA_DUMP_SLICE_INFO")) {
        std::cout << "=========Sliceinfo =========\n";
        std::cout << "=== depth:" << depth << ", num_clones: " << numClones
                  << " cssize: " << cssize   << "\n";
        SI->dump();
        std::cout << "============================\n";
      }


      unsigned numInDySER = 0;
      DySERizingLoop = true;

      for (auto I = LI->rpo_rbegin(), E = LI->rpo_rend(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          if (isOpMerged(op)) {
            continue;
          }
          InstPtr inst = createInst(op->img, 0, op,false);
          updateInstWithTraceInfo(op, inst, false);
          //dumpInst(inst);

          if (!SI->isInLoadSlice(op)) {
            if (cloneOp2InstMap.count(op) == 0) {
              for (unsigned j = 0; j < depth*numClones; ++j) {
                cloneOp2InstMap[op][j] = 0;
              }
            }

            // Emit vec_len nodes for compute slice
            for (unsigned clone = 0; clone < numClones; ++clone) {
              for (unsigned j = 0; j < depth; ++j) {
                if ((j == 0 || countDepthNodesForConfig)
                    && SI->shouldIncludeInCSCount(op)) {
                  ++numInDySER;
                  if (getenv("DUMP_MAFIA_DYSER_EXEC"))
                    std::cout << " NumInDySER: " << numInDySER << "\n";
                }
                if (numInDySER >= _dyser_size) {
                  if (_num_cycles_switch_config != 0) {
                    ConfigInst = insertDyConfig(_num_cycles_switch_config);
                    justSwitchedConfig = true;
                  }
                  numInDySER = 0;
                }

                useCloneOpMap = true;

                //printDisasmPC(op->cpc().first, op->cpc().second);

                pipeId = clone*depth + j;
                unsigned prevPipeId = ((j == 0)
                                       ? clone*depth + depth - 1
                                       : pipeId - 1);

                InstPtr dy_inst =
                  insert_sliced_inst(SI, op, inst, loopdone,
                                     false,
                                     cloneOp2InstMap[op][prevPipeId],
                                     (clone == 0 && j == 0),
                                     (clone+1 == numClones && j+1 == depth),
                                     SI->hasSinCos()? (24*depth): 0);
                useCloneOpMap = false;
                cloneOp2InstMap[op][pipeId] = dy_inst;
                pipeId = 0;

                //if (j != 0) {
                //  setExecuteToExecute(cloneOp2InstMap[op][j-1],
                //                      dy_inst);
                //}
              }
            }
          } else {
            if (op->isLoad() && isStrideAccess(op, 0)) {
              // if stride access is zero, compiler hoist the load up and
              // uses DSendPQ. Example. NBody
              // Is it going to mess up mem_prod stuff???
              inst->_isload = false;
            }
            // emit one node for load slice
            // vectorized code cannot coalesce in the compiler yet,
            // and we cannot do it either.

            if (op->isLoad() || op->isStore()) {
              // create the inst
              // handle the non stride access...
              if (isStrideAccess(op)) {
                insert_sliced_inst(SI, op, inst, loopdone);
              } else {
                for (unsigned i = 0; i < _dyser_vec_len-1; ++i) {
                  InstPtr tmpInst = createInst(op->img, 0, op, false);
                  insert_sliced_inst(SI, op, tmpInst, loopdone);
                  updateInstWithTraceInfo(op, inst, false);
                  //this->cpdgAddInst(inst, inst->_index); //Tony aded this
                }
                insert_sliced_inst(SI, op, inst, loopdone);
                this->keepTrackOfInstOpMap(inst, op);
              }
            } else {
              if(op->numUses()==0 && op->numDeps()==0 && op->shouldIgnoreInAccel()) {
                //do nothing
              } else { 
                insert_sliced_inst(SI, op, inst, loopdone);
              }
            }
          }
        }
      }
      DySERizingLoop = false;

      if (getenv("MAFIA_DYSER_DUMP_CLONE_OP_MAP")){
        dumpCloneOp2InstMap();
      }

    }

    bool isThisOpCoalesced(Op *op) {
      // only load and store can be coalesced.
      if (!(op->isLoad() || op->isStore()))
        return false;

    }

   
  };
}



#endif
