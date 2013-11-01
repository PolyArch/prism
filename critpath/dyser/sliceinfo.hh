
#ifndef SLICEINFO_HH
#define SLICEINFO_HH

#include <map>
#include "loopinfo.hh"
#include "exec_profile.hh"

namespace DySER {
  class SliceInfo {

    static std::map<LoopInfo*, SliceInfo*> _info_cache;

    std::vector<Op*> OpList;

    std::map<Op*, bool> IsInLoadSlice;
    std::map<Op*, unsigned> rpoIndex;
    std::map<Op*, bool> IsInput;
    std::map<Op*, bool> IsOutput;
    unsigned _cs_size = 0;

    std::map<uint64_t, std::set<Op*> > pc2Ops;
    std::set<Op*> controlOps;
    std::set<Op*> storeOps;

  public:
    SliceInfo(LoopInfo *li, unsigned dyser_size) {

      if (!li)
        return;

      // construct pc2Ops
      for (auto I = li->rpo_rbegin(), E = li->rpo_rend();
           I != E; ++I) {
        BB *bb = *I;
        for (auto OI = bb->op_begin(), OE = bb->op_end();
             OI != OE; ++OI) {
          Op *op = *OI;
          pc2Ops[op->cpc().first].insert(op);
          OpList.push_back(op);
        }
      }

      std::vector<Op*> workList;
      for (auto BBI = li->rpo_rbegin(), BBE = li->rpo_rend();
           BBI != BBE; ++BBI) {
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end();
             I != E; ++I) {
          Op *op = *I;
          if (op->isLoad()) {
            workList.push_back(op);
            IsInLoadSlice[op] = true;
            if (shouldIncludeLdUops(op)) {
              auto &opSet = pc2Ops[op->cpc().first];
              for (auto OI = opSet.begin(), OE = opSet.end(); OI != OE; ++OI) {
                workList.push_back(*OI);
                IsInLoadSlice[*OI] = true;
              }
            }
          } else if (op->isStore() || op->isCtrl()) {
            auto &opSet = pc2Ops[op->cpc().first];
            for (auto OI = opSet.begin(), OE = opSet.end(); OI != OE; ++OI) {
              workList.push_back(*OI);
              IsInLoadSlice[*OI] = true;
              if (op->isStore())
                storeOps.insert(*OI);
              if (op->isCtrl())
                controlOps.insert(*OI);
            }
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

        if (storeOps.count(op)) {
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

      unsigned numTry = 0;
      do {
        if (++numTry > 2)
          break;
        _cs_size = 0;
        for (auto I = pc2Ops.begin(), E = pc2Ops.end(); I != E; ++I) {
          Op *op = *I->second.begin();
          if (!isInLoadSlice(op) && shouldIncludeInCS(op))
            ++_cs_size;
        }
        if (dyser_size >= _cs_size)
          break;

        // move instruction with no operands to load-slice
        for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {
          Op *op = *I;
          if (op->numUses() == 0
              && op->numDeps() == 0
              && op->numMemDeps() == 0
              && !IsInLoadSlice[op]) {
            IsInLoadSlice[op] = true;
            -- _cs_size;
            if (dyser_size >= _cs_size)
              break;
          }
        }
      } while (true);

      for (auto I = rpoIndex.begin(), E = rpoIndex.end(); I != E; ++I) {
        Op *op = I->first;
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

    bool isInLoadSlice(Op *op, bool allowNull  = false) const {
      if (allowNull) {
        if (!IsInLoadSlice.count(op))
          return true; // No info means -- the op executes in main processor.
      }
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

    static SliceInfo* get(LoopInfo *LI, unsigned dyser_size) {
      auto I = _info_cache.find(LI);
      if (I != _info_cache.end())
        return I->second;
      SliceInfo *Info = new SliceInfo(LI, dyser_size);
      _info_cache[LI] = Info;
      return Info;
    }
    bool shouldIncludeLdUops(Op *op) {
      uint64_t pc = op->cpc().first;
      int upc    = op->cpc().second;
      std::string disasm =  ExecProfile::getDisasm(pc, upc);
      if (disasm.find("MOVSS_XMM_M") != std::string::npos)
        return true;
      return false;
    }

    bool shouldIncludeInCS(Op *op) {
      uint64_t pc = op->cpc().first;
      int upc    = op->cpc().second;
      std::string disasm =  ExecProfile::getDisasm(pc, upc);
      if (disasm.find("MOVAPS_XMM_XMM") != std::string::npos)
        return false;
      return true;
    }

    void dump() {
      for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {
        Op *op = *I;
        uint64_t pc = op->cpc().first;
        int upc    = op->cpc().second;
        if (op->bb_pos() == 0)
          std::cout << "\n";
        std::cout << ((isInLoadSlice(op))?"L ": " ")
                      << pc << "," << upc << " : "
                      << ExecProfile::getDisasm(pc, upc) << "\n";
      }
    }

    typedef std::vector<Op*>::iterator iterator;
    iterator begin() { return OpList.begin(); }
    iterator end()   { return OpList.end(); }

  };
} // end namespace dyser

#endif
