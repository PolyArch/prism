
#ifndef SLICEINFO_HH
#define SLICEINFO_HH

#include <map>
#include "loopinfo.hh"

namespace DySER {
  class SliceInfo {

    static std::map<LoopInfo*, SliceInfo*> _info_cache;

    std::map<Op*, bool> IsInLoadSlice;
    std::map<Op*, unsigned> rpoIndex;
    std::map<Op*, bool> IsInput;
    std::map<Op*, bool> IsOutput;
    unsigned _cs_size = 0;

  public:
    SliceInfo(LoopInfo *li) {

      if (!li)
        return;

      std::vector<Op*> workList;
      for (auto BBI = li->rpo_begin(), BBE = li->rpo_end();
           BBI != BBE; ++BBI) {
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end();
             I != E; ++I) {
          Op *op = *I;

          if (op->isLoad() || op->isStore() || op->isCtrl()) {
            workList.push_back(op);
            IsInLoadSlice[op] = true;
          } else {
            IsInLoadSlice[op] = false;
          }
          rpoIndex[op] = rpoIndex.size();
          //std::cout << op << " : " << rpoIndex[op] << "\n";
        }
      }
      // do a backward slice
      while (!workList.empty()) {
        Op *op = workList.front();
        workList.erase(workList.begin());

        if (op->isStore() || op->isCtrl()) {
          // FIXME:: handle stores/ control instruction dependencies.
          continue;
        }
        for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
          Op *DepOp = *I;
          // not in the loop
          if (rpoIndex.find(DepOp) == rpoIndex.end())
            continue;
          // loop dependent..
          if (rpoIndex[DepOp] >= rpoIndex[op])
            continue;
          // already in LS
          if (IsInLoadSlice[DepOp])
            continue;

          IsInLoadSlice[DepOp] = true;
          workList.push_back(DepOp);
        }
      }
      for (auto I = rpoIndex.begin(), E = rpoIndex.end(); I != E; ++I){
        Op *op = I->first;
        //std::cout << op << " : " << rpoIndex[op] << "\n";

        if (!isInLoadSlice(op))
          ++_cs_size;

        for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
          Op *UseOp = *UI;
          if (isInLoadSlice(op)) {
            if (IsInLoadSlice.count(UseOp) && isInLoadSlice(UseOp))
              IsInput[op] = true;
          } else {
            if (!IsInLoadSlice.count(UseOp) || isInLoadSlice(UseOp))
              IsOutput[op] = true;
          }
        }
      }
    }

    bool isInLoadSlice(Op *op) const {
      assert(IsInLoadSlice.count(op));
      return IsInLoadSlice.find(op)->second;
    }
    unsigned getRPO(Op *op) const {
      assert(rpoIndex.count(op));
      return rpoIndex.find(op)->second;
    }

    unsigned size() const {
      return rpoIndex.size();
    }

    unsigned cs_size() const {
      return _cs_size;
    }

    bool isAInputToDySER(Op *op) const {
      return (IsInput.count(op) > 0);
    }

    bool isADySEROutput(Op *op) const {
      return (IsOutput.count(op) > 0);
    }

    static SliceInfo* get(LoopInfo *LI) {
      auto I = _info_cache.find(LI);
      if (I != _info_cache.end())
        return I->second;
      SliceInfo *Info = new SliceInfo(LI);
      _info_cache[LI] = Info;
      return Info;
    }

  };
} // end namespace dyser

#endif
