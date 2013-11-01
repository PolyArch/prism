
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
        } else if (stride > 16) { // more than 16 bytes
          hasNonStridedMemAccess = true;
        }

        if (!nonStrideAccessLegal) {
          if (hasNonStridedMemAccess)
            vectorizableMemAccess = false;
        }

        // FIXME:: we do not handle non strided stores yet.
        if ((*I)->isStore() && hasNonStridedMemAccess)
          vectorizableMemAccess = false;

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

          vectorizableMemAccess = false;
          break;
        }
      }
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
                            bool nonStrideAccessLegal) {
    // no loop.
    if (!li)
      return false;

    // no inner loop.
    if (!li->isInnerLoop()) {
      return false;
    }

    return hasVectorizableMemAccess(li,
                                    nonStrideAccessLegal);
  }


  virtual bool hasNonStridedMemAccess(LoopInfo *li,
                                      bool nonStrideAccessLegal) {
    if (!hasNonStridedMemAccessMap.count(li))
      hasVectorizableMemAccess(li, nonStrideAccessLegal);
    assert(hasNonStridedMemAccessMap.count(li));
    return hasNonStridedMemAccessMap[li];
  }

};

#endif
