
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

    std::map<Op*, Op*> coalescedMemNodes;
    std::set<Op*> coalescedMemFirstNodes;
    LoopInfo *LI = 0;
  public:

    static bool mapInternalControlToDySER;

    bool isFirstMemNode(Op* op) const {
      return (coalescedMemFirstNodes.count(op) > 0);
    }

    bool isCoalesced(Op *op) const {
      if (coalescedMemNodes.find(op) == coalescedMemNodes.end())
        return false;
      return true;
    }

    Op *getFirstMemNode(Op *op) {
      Op *curOp = op;
      unsigned numIter = 0;
      while (numIter < coalescedMemNodes.size()) {
        std::map<Op*, Op*>::iterator I = coalescedMemNodes.find(curOp);
        if (I == coalescedMemNodes.end())
          return curOp;
        curOp = I->second;
        ++numIter;
      }
      if (numIter >= coalescedMemNodes.size())
        return op;
      return curOp;
    }

  public:
    SliceInfo(LoopInfo *li, unsigned dyser_size) {
      LI = li;
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
          } else if (op->isStore()) {
            auto &opSet = pc2Ops[op->cpc().first];
            for (auto OI = opSet.begin(), OE = opSet.end(); OI != OE; ++OI) {
              workList.push_back(*OI);
              IsInLoadSlice[*OI] = true;
              storeOps.insert(*OI);
            }
          } else if (op->isCtrl()) {
            auto &opSet = pc2Ops[op->cpc().first];
            for (auto OI = opSet.begin(), OE = opSet.end(); OI != OE; ++OI) {
              // Internal control can be mapped to dyser itself
              if (!mapInternalControlToDySER || li->isLatch(op->bb())) {
                workList.push_back(*OI);
                IsInLoadSlice[*OI] = true;
              } else {
                IsInLoadSlice[*OI] = false;
              }
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

      optimizeCS();

      unsigned numTry = 0;
      do {
        if (++numTry > 2)
          break;
        _cs_size = 0;
#if 0
        for (auto I = pc2Ops.begin(), E = pc2Ops.end(); I != E; ++I) {
          Op *op = *I->second.begin();
          if (!isInLoadSlice(op) && shouldIncludeInCSCount(op))
            ++_cs_size;
        }
#endif
        for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {
          if (!isInLoadSlice(*I) && shouldIncludeInCSCount(*I))
            ++ _cs_size;
        }

        if (dyser_size >= _cs_size)
          break;

        break;
#if 0
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
#endif
      } while (true);



      bool dumpIOInfo = (getenv("MAFIA_DEBUG_SLICEINFO_IO") != 0);
      if (dumpIOInfo) {
        std::cout << "============ SLICEINFO IO ===============\n";
      }
      for (auto I = rpoIndex.begin(), E = rpoIndex.end(); I != E; ++I) {
        Op *op = I->first;
        if (dumpIOInfo)
          printDasm(op);

        for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
          Op *UseOp = *UI;
          if (dumpIOInfo) {
            std::cout << "   use: "; printDasm(UseOp);
          }
          if (isInLoadSlice(op)) {
            if (IsInLoadSlice.count(UseOp) && !isInLoadSlice(UseOp))
              if (op->isLoad() || op->isStore() || !shouldIncludeLdUops(op)) {
                IsInput[op] = true;
                if (dumpIOInfo) {
                  std::cout << "          --> input\n";
                }
              }
          } else {
            if (!IsInLoadSlice.count(UseOp)) {
              // It means the value is used outside
              // or it is just some spurious use.
              // For example
              //  <reg> = mul a, b
              //  output = add <reg> <other>
              // ...
              //  <reg> = xor <reg>, <reg>
              //  or used as part of wide operation
              // We are assuming it is a spurious use if the use
              // distance is greater than 50 or the use is not in the
              // first microop.
              if ( abs(UseOp->cpc().first - op->cpc().first) <= 50
                   || UseOp->cpc().second == 0) {
                IsOutput[op] = true;
                if (dumpIOInfo) {
                  std::cout << "          --> output\n";
                }
              }
              continue;
            }
            if (isInLoadSlice(UseOp)) {
              IsOutput[op] = true;
              if (dumpIOInfo) {
                std::cout << "          --> output\n";
              }
            }
          }
        }
      }
      if (dumpIOInfo) {
        std::cout << "============ SLICEINFO IO ===============\n";
      }

      for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {

        if ((*I)->isCall() && !isInLoadSlice(*I)) {
          if ((*I)->getCalledFuncName() == "sincosf")
            _hasSinCos = true;
        }

        if (!((*I)->isLoad() || (*I)->isStore()))
          continue;
        if ((*I)->getCoalescedOp() != 0) {
          std::cout << "mem:" << *I  << "--" << "mem:" << (*I)->getCoalescedOp() << "\n";
          if ( (*I) == (*I)->getCoalescedOp() ) {
            std::cout << "ACC_SIZE = 0 : ";
            printDasm(*I);
          }
          coalescedMemNodes[(*I)->getCoalescedOp()] = *I;
        } else {
          coalescedMemFirstNodes.insert(*I);
        }
      }
      std::cout << "Number of FirstNodes: " << coalescedMemFirstNodes.size()
                << "\n";

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


    void optimizeCS() {
      for (auto I = rpoIndex.begin(), E = rpoIndex.end(); I != E; ++I) {
        Op *op = I->first;
        if (isInLoadSlice(op))
          continue;

        bool hasDepInCS = false;
        bool aOutput = false;
        bool hasUseInCS = false;

        for (auto DI = op->d_begin(), DE = op->d_end(); DI != DE; ++DI) {
          Op *DepOp = *DI;
          if (DepOp == op)
            continue;
          if (IsInLoadSlice.count(DepOp) && !isInLoadSlice(DepOp)) {
            hasDepInCS = true;
            break;
          }
        }

        for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
          Op *UseOp = *UI;
          if (UseOp == op)
            continue;
          if (!IsInLoadSlice.count(UseOp) || isInLoadSlice(UseOp))
            aOutput = true;
          if (IsInLoadSlice.count(UseOp) && !isInLoadSlice(UseOp))
            hasUseInCS = true;
        }
        if (aOutput && !hasDepInCS && !hasUseInCS) {
          // This node should very well to in LS
          IsInLoadSlice[op] = true;
        }
      }

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

      // all loads belong to LS
      if (disasm.find("MOVSS_XMM_M") != std::string::npos)
        return true;
      return false;
    }

    bool shouldIncludeInCSCount(Op *op) {
      uint64_t pc = op->cpc().first;
      int upc    = op->cpc().second;
      std::string disasm =  ExecProfile::getDisasm(pc, upc);

      // Reg to Reg Mov
      if (disasm.find("MOVAPS_XMM_XMM") != std::string::npos)
        return false;
      return true;
    }


    void printDasm(Op *op) {
      uint64_t pc = op->cpc().first;
      int upc    = op->cpc().second;
      std::cout << pc << "," << upc << " : " << "<" << op << ">"
                << ExecProfile::getDisasm(pc, upc) << "\n";
    }


    void dump() {
      for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {
        Op *op = *I;
        uint64_t pc = op->cpc().first;
        int upc    = op->cpc().second;
        if (op->bb_pos() == 0) {
          std::cout << "\n";
          if (LI->isLatch(op->bb())) {
            std::cout << "<Latch>";
          }
          if (LI->loop_head() == op->bb()) {
            std::cout << "<Head>";
          }
        }

        std::cout << ((isInLoadSlice(op))?"L": " ")
                  << ((!isInLoadSlice(op))?"C": " ")
                  << ((isAInputToDySER(op))?"I": " " )
                  << ((isADySEROutput(op))?"O": " ")
                  << " "
                  << pc << "," << upc << " : " << "<" << op << ">"
                  << ExecProfile::getDisasm(pc, upc) << "\n";
      }
    }

    typedef std::vector<Op*>::iterator iterator;
    iterator begin() { return OpList.begin(); }
    iterator end()   { return OpList.end(); }


    bool _hasSinCos = false;
    bool hasSinCos() const {
      return _hasSinCos;
    }

    unsigned getNumInputs() const {
      unsigned ret = 0;
      for (auto I = IsInput.begin(), E = IsInput.end(); I != E; ++I) {
        if (I->second)
          ++ret;
      }
      return ret;
    }
    unsigned getNumOutputs() const {
      unsigned ret = 0;
      for (auto I = IsOutput.begin(), E = IsOutput.end(); I != E; ++I) {
        if (I->second)
          ++ret;
      }
      return ret;
    }

    unsigned getNumLoadSlice() const {
      unsigned ret = 0;
      for (auto I = IsInLoadSlice.begin(), E = IsInLoadSlice.end(); I != E; ++I) {
        if (I->second)
          ++ret;
      }
      return ret;
    }


    bool shouldDySERize(bool vectorizable) {
      if (vectorizable) {
        // try dyserization -- vectorizable provides more benefits
        return true;
      }
      int Total = OpList.size();
      int InSize =  getNumInputs();
      int OutSize = getNumOutputs();
      int lssize =  getNumLoadSlice();

      if (InSize == 0 || OutSize == 0) {
        // no input or output -- no dyser
        return false;
      }

      if (InSize + OutSize > (Total - lssize))
        return false;

      return true;
    }

  };
} // end namespace dyser

#endif
