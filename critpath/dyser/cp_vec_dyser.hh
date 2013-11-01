
#ifndef CP_VEC_DYSER_HH
#define CP_VEC_DYSER_HH

#include "cp_dyser.hh"


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


namespace DySER {
  class cp_vec_dyser : public cp_dyser {

  protected:
    unsigned _dyser_vec_len = 16;

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
      // complete this as soon as it completes a iteration
      // we cannot vectorize this.
      if (!canVectorize(CurLoop))
        return true;
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
                                         unsigned curLoopIter)
    {
      if (!(curLoopIter && ((curLoopIter % _dyser_vec_len) == 0))) {
        cp_dyser::completeDySERLoopWithIT(DyLoop,
                                          CurLoopIter);
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

      _num_config += extraConfigRequired;

      std::map<Op*, unsigned> emitted;
      for (unsigned i = 0, e = loop_InstTrace.size(); i != e; ++i) {
        auto op_n_Inst  = loop_InstTrace[i];
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
        insert_sliced_inst(SI, op, inst, false);
      }
      loop_InstTrace.clear();
    }


    bool useCloneOpMap = false;
    unsigned pipeId = 0;
    std::map<Op*, std::map<unsigned, InstPtr > > cloneOp2InstMap;
    InstPtr getInstForOp(Op* op) {
      bool dumpClone = true;
      InstPtr ret = 0;
      if (!useCloneOpMap || SI->isInLoadSlice(op, true)) {
        ret =  cp_dyser::getInstForOp(op);
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

    virtual void completeDySERLoopWithLI(LoopInfo *LI,
                                         int curLoopIter)
    {
      if (!(curLoopIter && ((curLoopIter % _dyser_vec_len) == 0))) {
        cp_dyser::completeDySERLoopWithLI(LI, curLoopIter);
        return;
      }
      assert(LI);
      SI = SliceInfo::get(LI, _dyser_size);
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

      assert(numClones*depth == _dyser_vec_len);

      _num_config += extraConfigRequired;

      DySERizingLoop = true;

      for (auto I = LI->rpo_rbegin(), E = LI->rpo_rend(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          InstPtr inst = createInst(op->img, 0, op);

          //dumpInst(inst);

          if (!SI->isInLoadSlice(op)) {
            if (cloneOp2InstMap.count(op) == 0) {
              for (unsigned j = 0; j < depth; ++j) {
                cloneOp2InstMap[op][j] = 0;
              }
            }

            // Emit dyser_vec_len nodes for compute slice
            for (unsigned clone = 0; clone < numClones; ++clone) {
              for (unsigned j = 0; j < depth; ++j) {
                useCloneOpMap = true;
                pipeId = j;
                InstPtr dy_inst =
                  insert_sliced_inst(SI, op, inst,
                                     false,
                                     ((j == 0)
                                      ? cloneOp2InstMap[op][depth-1]
                                      : cloneOp2InstMap[op][j-1]),
                                     (clone+1 == numClones && j+1 == depth));
                pipeId = 0;
                useCloneOpMap = false;
                cloneOp2InstMap[op][j] = dy_inst;
                //if (j != 0) {
                //  setExecuteToExecute(cloneOp2InstMap[op][j-1],
                //                      dy_inst);
                //}
              }
            }
          } else {
            // emit one node for load slice
            insert_sliced_inst(SI, op, inst);
          }
        }
      }
      DySERizingLoop = false;

      if (getenv("MAFIA_DYSER_DUMP_CLONE_OP_MAP")){
        dumpCloneOp2InstMap();
      }

      loop_InstTrace.clear();
    }

  };
}



#endif
