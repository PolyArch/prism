
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

      if (strcmp(name, "dyser-vec-len")) {
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


    void completeDySERLoopWithIT(LoopInfo *DyLoop,
                           unsigned curLoopIter)
    {
      if (!(curLoopIter && ((curLoopIter % _dyser_vec_len) == 0))) {
        cp_dyser::completeDySERLoopWithIT(DyLoop,
                                          CurLoopIter);
        return;
      }

      assert(DyLoop);
      SliceInfo *SI = SliceInfo::get(DyLoop);
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

    void completeDySERLoopWithLI(LoopInfo *LI,
                                 unsigned curLoopIter)
    {
      if (!(curLoopIter && ((curLoopIter % _dyser_vec_len) == 0))) {
        cp_dyser::completeDySERLoopWithLI(LI, curLoopIter);
        return;
      }
      assert(LI);
      SliceInfo *SI = SliceInfo::get(LI);
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

      DySERizingLoop = true;
      for (auto I = LI->rpo_begin(), E = LI->rpo_end(); I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end(); OI != OE; ++OI) {
          Op *op = *OI;
          InstPtr inst = InstPtr(new Inst_t(op->img, 0));
          if (!SI->isInLoadSlice(op)) {
            // Emit dyser_vec_len nodes for compute slice
            for (unsigned k = 0; k < _dyser_vec_len; k += depth) {
              for (unsigned j = 0; j < depth; ++j) {
                insert_sliced_inst(SI, op, inst, (k!=0));
              }
            }
          } else {
            // emit one node for load slice
            insert_sliced_inst(SI, op, inst);
          }
        }
      }
      DySERizingLoop = false;
      loop_InstTrace.clear();
    }

  };
}



#endif
