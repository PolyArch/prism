
#ifndef VECTORIZE_LEGALITY_HH
#define VECTORIZE_LEGALITY_HH

#include <map>

#include "loopinfo.hh"

class VectorizationLegality {

  std::map<LoopInfo *, bool> hasVectorizableMemAccessMap;

protected:

  // utility function to check whether the loop is legally
  // vectorizable with respect to memory access.
  virtual bool hasVectorizableMemAccess(LoopInfo *li) {
    auto I = hasVectorizableMemAccessMap.find(li);
    if (I != hasVectorizableMemAccessMap.end())
      return I->second;

    bool vectorizableMemAccess = true;
    for (auto BBI = li->body_begin(), BBE = li->body_end();
         BBI != BBE && vectorizableMemAccess; ++BBI) {

      for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end();
           I != E && vectorizableMemAccess; ++I) {

        if (!(*I)->isLoad() && !(*I)->isStore())
          continue;

        int stride = 0;
        if (!(*I)->getStride(&stride)) {
          vectorizableMemAccess = false;
          break;
        }

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
    return vectorizableMemAccess;
  }

  // Can we vectorize the loop?
  // clients override this
  virtual bool canVectorize(LoopInfo *li) {
    // no loop.
    if (!li)
      return false;

    // no inner loop.
    if (!li->isInnerLoop())
      return false;

    return hasVectorizableMemAccess(li);
  }

};

#endif
