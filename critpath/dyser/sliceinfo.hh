
#ifndef SLICEINFO_HH
#define SLICEINFO_HH

#include <map>
#include <set>
#include "loopinfo.hh"
#include "exec_profile.hh"

namespace DySER {

  class SliceInfo {

    static std::map<LoopInfo*, SliceInfo*> _info_cache;

    std::vector<Op*> OpList;

    std::map<Op*, bool> IsInLoadSlice;
    std::map<Op*, unsigned> rpoIndex;
    std::map<Op*, bool> IsInput;
    std::map<Op*, bool> InvariantInput;
    std::map<Op*, bool> IsOutput;

    std::map<Op*, std::set<Op*>> edges;

    unsigned _cs_size = 0;

    std::map<uint64_t, std::set<Op*> > pc2Ops;
    std::set<Op*> controlOps;
    std::set<Op*> storeOps;

    std::set<Op*> internalCtrlOps;

    std::map<Op*, Op*> coalescedMemNodes;
    std::set<Op*> coalescedMemFirstNodes;
    std::map<Op*,std::set<Op*>> coalescedMemList;

    LoopInfo *LI = 0;

    public:

    //helper function to print node
    void print_dot_node(std::ostream& out, Op* op, bool load_slice, bool comp_slice) {
      out << "\"" << op->id() << "\" [";
      std::string extra("");
   
#if 0
      if(critEdges && (*critEdges)[op][op].size()!=0) {
        stringstream ss;
        ss << "\\n";
        for(const auto& i : (*critEdges)[op][op]) {
          unsigned type = i.first;
          double weight = i.second;
          if(/*type == E_RDep || */weight < weight_min) {
            continue;
          }
          ss << edge_name[type] << ":" << weight << "\\n";
        }
    
        extra=ss.str();
      }
#endif
      out << op->dotty_name_and_tooltip(extra);
    
      if(load_slice && comp_slice) {
        out << ",style=filled, color=mediumorchid1";
      } else if (load_slice) {
        out << ",style=filled, color=lightblue";
      } else if (comp_slice) {
        out << ",style=filled, color=lightcoral";
      } else {
        out << ",style=filled, color=white";
      }
      
      out << "]\n";
    }

    void toDotFile(std::ostream& out) {
      out << "digraph GA{\n";
  
      out << " graph [fontname = \"helvetica\"];\n";
      out << " node  [fontname = \"helvetica\"];\n";
      out << " edge  [fontname = \"helvetica\"];\n";  
      out << "labelloc=\"t\";\n";
      out <<  "label=\"" << LI->nice_name_full();
 
      out << "\";\n"; //end label

      std::set<Op*> seenOps;

      for(auto I=LI->body_begin(),E=LI->body_end();I!=E;++I) {
        BB* bb = *I;
        out << "subgraph"
            << "\"cluster_bb" << bb->rpoNum() << "\"{"       
            << "label=\"BB " << bb->rpoNum() << "\"\n";
    
        out << "style=\"filled,rounded\"\n";
        out << "color=lightgrey\n";

        for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
          Op* op = *II;

          if(op->isMem()) {
            Op* f_op = getFirstMemNode(op);
  
            if(coalescedMemList.count(f_op)) {
              out << "subgraph"
                << "\"cluster_mem" << op->id() << "\"{"       
                << "label=\"mem" << op->id() << "\"\n";
              out << "style=\"filled,rounded\"\n";
              out << "color=cornsilk\n";

              for(auto i = coalescedMemList[op].begin(), e=coalescedMemList[op].end();
                  i!=e;++i) {
                Op* c_op = *i;
                seenOps.insert(c_op);
                print_dot_node(out,c_op,IsInLoadSlice[c_op],!IsInLoadSlice[c_op]);
              }
              out << "}\n";
            } 
          }

          if(seenOps.count(op)==0) {
            print_dot_node(out,op,IsInLoadSlice[op],!IsInLoadSlice[op]);
          }
          
          for(auto ui=op->u_begin(),ue=op->u_end();ui!=ue;++ui) {
            Op* uop = *ui;
            edges[op].insert(uop);
          }
        }

        for(auto si=bb->succ_begin(),  se=bb->succ_end(); si!=se; ++si) {
          BB* succ_bb = *si;
          if(!LI->inLoop(succ_bb)) {
            continue;
          }

          if(bb->len() > 0 && succ_bb->len() > 0) {
            out << "\"" << bb->lastOp()->id() << "\"->" 
                << "\"" << succ_bb->firstOp()->id() << "\" [";
   
            out << "ltail=\"cluster_bb" << bb->rpoNum() << "\" ";
            out << "lhead=\"cluster_bb" << succ_bb->rpoNum() << "\" ";
            out << "arrowhead=open arrowsize=1.5 weight=1 penwidth=4 color=blue ];\n";
          }
        }
        out << "}\n";

     }

     //print edges:
     for(const auto& item : edges) {
       Op* op = item.first;
       for(Op* uop : item.second) {
         out << op->id() << " -> " << uop->id() << " ["; 
         if(!LI->reachable_in_iter(op,uop)) {
           out << "color=red,constraint=false";
         }
         out << "]\n";
       }
     }


     out << "}\n";

   }

    static bool mapInternalControlToDySER;
    static bool useRPOIndexForOutput;

    bool isFirstMemNode(Op* op) const {
      return (coalescedMemFirstNodes.count(op) > 0);
    }

    bool isInternalCtrl(Op* op) const {
      return internalCtrlOps.count(op);
    }

    bool isCoalesced(Op *op) const {
      if (coalescedMemNodes.find(op) == coalescedMemNodes.end())
        return false;
      return true;
    }

    //What is this supposed to do?
    //Why 
    Op *getFirstMemNode(Op *op) {
      Op *curOp = op;
      unsigned numIter = 0;
      while (numIter < coalescedMemNodes.size()) {
        std::map<Op*, Op*>::iterator I = coalescedMemNodes.find(curOp);
        if (I == coalescedMemNodes.end()) {
          return curOp;
        }
        curOp = I->second;
        ++numIter;
      }
      if (numIter >= coalescedMemNodes.size()) {
        //std::cout << op->id() << " not " << curOp->id() << "\n";
        return op;
      }
      return curOp;
    }

    std::map<Op*, Op*> bundledNodesMap;
    Op *getBundledNode(Op *op) {
      auto I = bundledNodesMap.find(op);
      if (I != bundledNodesMap.end())
        return I->second;
      return 0;
    }


/*    static bool Op::checkDisasmHas(Op *op, const char *chkStr) {
      uint64_t pc = op->cpc().first;
      int upc = op->cpc().second;
      std::string disasm =  ExecProfile::getDisasm(pc, upc);
      if (disasm.find(chkStr) != std::string::npos)
        return true;
      return false;
    }*/

  protected:

    bool checkForBundledNodes(Op *op, Op *nxop, Op *nx2op) {
      if (!op || !nxop || !nx2op)
        return false;

      // FIXME::
      // For needle
      // Bundle max :: check the ops, and uses
      if (!Op::checkDisasmHas(op, "CMP_R_R"))
        return false;

      if (Op::checkDisasmHas(nxop, "CMOVLE_R_R")
          && Op::checkDisasmHas(nx2op, "CMOVLE_R_R"))
        return true;
      if (Op::checkDisasmHas(nxop, "CMOVNLE_R_R")
          && Op::checkDisasmHas(nx2op, "CMOVNLE_R_R"))
        return true;

      return false;
    }

    void computeBundledNodes() {
      for (unsigned i = 0, e = OpList.size(); i < e; ++i) {
        Op *op = OpList[i];
        Op *nxop = (i+1 < e) ? OpList[i+1] : 0;
        Op *nx2op = (i+2 < e)? OpList[i+2] : 0;
        if (checkForBundledNodes(op, nxop, nx2op)) {
          bundledNodesMap[nxop] = op;
          bundledNodesMap[nx2op] = op;
          i += 2; // skip next two ops
        }
      }
    }

  public:
    SliceInfo(LoopInfo *li, unsigned dyser_size, bool can_vec) {
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
      for (auto BBI = li->rpo_rbegin(), BBE = li->rpo_rend(); BBI != BBE; ++BBI) {
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end(); I != E; ++I) {
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
              if (!mapInternalControlToDySER || li->isLatch(op->bb()) || 
                    li->hasNonLoopSuccessor(op->bb())) {
                workList.push_back(*OI);
                IsInLoadSlice[*OI] = true;
              } else {
                internalCtrlOps.insert(*OI);
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

        // FIXME:: handle control instruction dependencies.

        //don't slice backwards from store ops, except the 
        if (storeOps.count(op)) {
          //TODO: should I remove this conditional? (so that store addrs not sliced)
          //(tony)
          if(!op->isStore()) {
            continue;
          }
        }

        for (auto I = op->d_begin(), E = op->d_end(); I != E; ++I) {
          Op *DepOp = *I;

          if (op->isStore()) { //Tony: slice store addresses
            if(!op->isMemAddrOp(DepOp)) {
              continue;
            } else { 
              //it is a memory op!
              storeOps.erase(DepOp); //remove it from storeOp list so it's not ignored
                                      //when it's processed next
            }
          }

          // not in the loop
          if (rpoIndex.find(DepOp) == rpoIndex.end())
            continue;
          // loop dependent..  (Tony: does this sufficiently captpure loop dependency?)
          if (rpoIndex[DepOp] >= rpoIndex[op])
            continue;
          // already in LS
          if (IsInLoadSlice[DepOp])
            continue;

          if (DepOp->is_clear_xor()) { //tony: just ignore it for now
            continue; 
          }

          IsInLoadSlice[DepOp] = true;
          workList.push_back(DepOp);
        }
      }

      optimizeCS(can_vec);

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

        if (getenv("MAFIA_DO_NOT_USE_OP_FOR_IO") == 0) {
          for (auto UI = op->d_begin(), UE = op->d_end(); UI != UE; ++UI) {
            Op *DepOp = *UI;

            if (dumpIOInfo) {
              std::cout << "   op.: "; printDasm(DepOp);
            }


            if (isInLoadSlice(op)) {
              if (!IsInLoadSlice.count(DepOp))
                continue;
              if (!isInLoadSlice(DepOp)) {
                IsInput[DepOp] = true;
                if (dumpIOInfo) {
                  std::cout << "           ---> input from CS??\n";
                }
              }
            } else {
              if (op == DepOp) {
                // spurious use or accumulate
                // FIXME: make this robust...
                if (Op::checkDisasmHas(op, "ADDSS_XMM_XMM")) {
                  if (dumpIOInfo) {
                    std::cout << "           ---> input,output\n";
                  }

                  IsInput[op] = true;
                  IsOutput[op] = true;
                  continue;
                }
              }

              if (!IsInLoadSlice.count(DepOp)) {
                InvariantInput[DepOp] = true;
                if (dumpIOInfo) {
                  std::cout << "           ---> input (invariant)\n";
                }
                continue;
              }
              if (isInLoadSlice(DepOp)) {
                IsInput[DepOp] = true;
                if (dumpIOInfo) {
                  std::cout << "           ---> input\n";
                }
              }
            }
          }
        }

        for (auto UI = op->u_begin(), UE = op->u_end(); UI != UE; ++UI) {
          Op *UseOp = *UI;
          if (dumpIOInfo) {
            std::cout << "   use: "; printDasm(UseOp);
          }

          if (Op::checkDisasmHas(op, "RSQRTSS_XMM_XMM") && op == UseOp) {
            continue; // It is not a use -- spurious use from gem5..
          }
          if (Op::checkDisasmHas(op, "CMP_R_R") &&
              Op::checkDisasmHas(op, "UCOMISS_XMM_XMM")) {
            continue; // It is not a uses
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
              // We are assuming it is a spurious use if is part of XOR
              // or MOV

              if (Op::checkDisasmHas(UseOp, "XOR"))
                continue;
              if (Op::checkDisasmHas(UseOp, "MOVAPS_XMM_XMM"))
                continue;
              IsOutput[op] = true;
              if (dumpIOInfo) {
                std::cout << "          --> output used outside the loop\n";
              }

              continue;
            }
            if (isInLoadSlice(UseOp)) {
              IsOutput[op] = true;
              if (dumpIOInfo) {
                std::cout << "          --> output in the loop\n";
              }
              continue;
            }
            // used in the CS itself then
            // -- only a output if rpoIndex(op) < rpoIndex(useop)
            if (!rpoIndex.count(op) || !rpoIndex.count(UseOp))
              continue;
            if (!useRPOIndexForOutput)
              continue;
            // This is not working -- commenting it for cutcp
            if (rpoIndex[op] >= rpoIndex[UseOp]) {
              IsOutput[op] = true;
              if (dumpIOInfo) {
                std::cout << "          --> output, in loop cs "
                          << rpoIndex[op] << " --> " << rpoIndex[UseOp] << "\n";
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
            if ((*I)->getCalledFuncName() == "sincosf"
              || (*I)->getCalledFuncName() == "__sincos"
              || (*I)->getCalledFuncName() == "__acos"
              || (*I)->getCalledFuncName() == "__asin"
              || (*I)->getCalledFuncName() == "__libm_sse2_sincosf") {
              _hasSinCos = true;
            }
        }

        if (!((*I)->isLoad() || (*I)->isStore()))
          continue;
        if ((*I)->getCoalescedOp() != 0) {

          if (dumpIOInfo) { //tony put this here
            std::cout << "mem:" << *I <<"--"<< "mem:" << (*I)->getCoalescedOp() << "\n";
          }

          if ( (*I) == (*I)->getCoalescedOp() ) {

            std::cout << "ACC_SIZE = 0 : ";
            printDasm(*I);

          }
          coalescedMemNodes[(*I)->getCoalescedOp()] = *I;
        } else {
          coalescedMemFirstNodes.insert(*I);
          coalescedMemList[*I].insert(*I);
        }
      }

      for (auto I = OpList.begin(), E = OpList.end(); I != E; ++I) {
        Op* op = *I;
        Op* first_op = getFirstMemNode(op);

        coalescedMemList[first_op].insert(op);
      }


      if (dumpIOInfo) {
        //Tony put this here to hide this
        std::cout << "Number of FirstNodes: " << coalescedMemFirstNodes.size()
                  << "\n";
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


    void optimizeCS(bool can_vec) {
      if(!can_vec) { 
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
      computeBundledNodes();

    }


    bool isAInputToDySER(Op *op) const {
      return (IsInput.count(op) > 0);
    }

    bool isADySEROutput(Op *op) const {
      return (IsOutput.count(op) > 0);
    }

    static SliceInfo* setup(LoopInfo* LI, unsigned dyser_size,  bool can_vec) {
      auto I = _info_cache.find(LI);
      if (I != _info_cache.end())
       return I->second;

       SliceInfo *Info = new SliceInfo(LI, dyser_size,can_vec);
      _info_cache[LI] = Info;
      return Info;
    }

    static bool has(LoopInfo* LI, unsigned dyser_size) {
      auto I = _info_cache.find(LI);
      return (I != _info_cache.end());
    }

    static SliceInfo* get(LoopInfo *LI, unsigned dyser_size) {
      auto I = _info_cache.find(LI);
      if (I != _info_cache.end())
        return I->second;

      assert(0);
      SliceInfo *Info = new SliceInfo(LI, dyser_size,true);
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
      if (disasm.find("MOVSS_XMM_P") != std::string::npos)
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


    bool shouldDySERize(bool vectorizable);

  };
} // end namespace dyser

#endif
