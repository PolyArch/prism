
#ifndef VECTORIZE_LEGALITY_HH
#define VECTORIZE_LEGALITY_HH

#include <map>
#include <vector>

#include "loopinfo.hh"

class VectorizationLegality {

  LoopInfo *_cached_curloop = 0;

  std::map<LoopInfo *, bool> hasVectorizableMemAccessMap;
  std::map<LoopInfo *, bool> hasNonStridedMemAccessMap;

protected:
public: //screw this multi-inheritance nonsense  : )

  LoopInfo *getLoop(Op *op,
                    bool prevInstAReturn = false, LoopInfo *stack_loop = 0) {
    // not a first instruction.
    if (op->bb_pos() != 0)
      return _cached_curloop;

    // is this bb starts another loop?
    LoopInfo *next_loop = op->func()->getLoop(op->bb());

    if (next_loop) {
      _cached_curloop = next_loop;
      return _cached_curloop;
    }

    // is op in the current loop itself?
    if (_cached_curloop && _cached_curloop->inLoop(op->bb())) {
      return _cached_curloop;
    }

    if (prevInstAReturn && stack_loop && stack_loop->inLoop(op->bb())) {
      _cached_curloop = stack_loop;
      return _cached_curloop;
    }

    // op is not in a loop.
    _cached_curloop = 0;
    return _cached_curloop;
  }



  // utility function to check whether the loop is legally
  // vectorizable with respect to memory access.
  virtual bool hasVectorizableMemAccess(LoopInfo *li,
                                        bool nonStrideAccessLegal) {
    auto I = hasVectorizableMemAccessMap.find(li);
    if (I != hasVectorizableMemAccessMap.end())
      return I->second;

    bool vectorizableMemAccess = true;
    bool hasNonStridedMemAccess = false;
    for (auto BBI = li->body_begin(), BBE = li->body_end();
         BBI != BBE && vectorizableMemAccess; ++BBI) {

      for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end();
           I != E && vectorizableMemAccess; ++I) {

        if (!(*I)->isLoad() && !(*I)->isStore())
          continue;

        int stride = 0;
        if (!(*I)->getStride(&stride)) {
          hasNonStridedMemAccess = true;
        }
#if 0
        else if (stride > 16) { // more than 16 bytes
          hasNonStridedMemAccess = true;
        }
#endif
        else if (stride > 32) { // Tony: more than 64 bytes -- very liberal
          hasNonStridedMemAccess = true;
        }


        if (!nonStrideAccessLegal) {
          if (hasNonStridedMemAccess)
            vectorizableMemAccess = false;
        }
        // FIXME:: we do not handle non strided stores yet.
        // Tony :: if so, then lets not vectorize past "long long" stride (64 bytes)
        if ((*I)->isStore() && (hasNonStridedMemAccess || stride > 64)) { 
          vectorizableMemAccess = false;
        }
      }
    }

    #if 0
    if(!old_loop_dep) {
      vectorizableMemAccess = false;
    }
    #endif

    if(!li->isLoopFullyParallelizable()) {
      vectorizableMemAccess=false;
    }

    hasVectorizableMemAccessMap.insert(std::make_pair(li,
                                                      vectorizableMemAccess));
    hasNonStridedMemAccessMap.insert(std::make_pair(li,
                                                    hasNonStridedMemAccess));
    return vectorizableMemAccess;
  }

  // Can we vectorize the loop?
  // clients override this
  virtual bool canVectorize(LoopInfo *li,
                            bool nonStrideAccessLegal,
                            double acceptableIncrFactor) {
    // no loop.
    if (!li)
      return false;

    // no inner loop.
    if (!li->isInnerLoop()) {
      return false;
    }

    // if control flow is going to increase number of instructions too much,
    // skip the loop from vectorization
    if (!li->isSuperBlockProfitable(acceptableIncrFactor))
      return false;

    bool vec_mem = hasVectorizableMemAccess(li, nonStrideAccessLegal);
    /*if(vec_mem) {
      std::cout << "Loop" << li->id() << " is vectorizable!";
    }*/
    return vec_mem;
  }

  //Venkat's method for loop dependence was to calculate wether any
  //nodes were may alias.  I am now using a more direct approach
  virtual bool old_loop_dep(LoopInfo* li) {
    for (auto BBI = li->body_begin(), BBE = li->body_end();
         BBI != BBE; ++BBI) {

      for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end();
           I != E; ++I) {

        if (!(*I)->isLoad() && !(*I)->isStore())
          continue;

        for (auto DI = (*I)->m_begin(), DE = (*I)->m_end(); DI != DE; ++DI) {
          Op *DepOp = *DI;
          assert(DepOp->isLoad() || DepOp->isStore());
          // Same -- broadcast load should handle this
          if (DepOp == (*I))
            continue;
          if (DepOp->cpc().first == (*I)->cpc().first)
            continue;

          // load - load
          if (DepOp->isLoad() && (*I)->isLoad())
            continue;

          // Dependend op is not in the same loop
          if (!li->inLoop(DepOp->bb()))
            continue;

          // Same memory touched by two ops in the same loop...

          // SIMD can handle must alias
          // FIXME:: Same Address accessed is not guaranteed to be
          // must-alias. For example, there may be a control instruction
          // between them or it may be happenstance.
          if (DepOp->isSameEffAddrAccessed(*I))
            continue;

          return false;
          break;
        }
      }
    }
    return true;
  }

  virtual bool hasNonStridedMemAccess(LoopInfo *li,
                                      bool nonStrideAccessLegal) {
    if (!hasNonStridedMemAccessMap.count(li))
      hasVectorizableMemAccess(li, nonStrideAccessLegal);
    assert(hasNonStridedMemAccessMap.count(li));
    return hasNonStridedMemAccessMap[li];
  }

  // utility functions
  bool isStrideAccess(Op *op) {
    int stride = 0;
    return op->getStride(&stride);
  }

  bool isStrideAccess(Op *op, int chkStride) {
    int stride = 0;
    if (!op->getStride(&stride)) {
      return false;
    }
    return stride == chkStride;
  }

};

#endif
