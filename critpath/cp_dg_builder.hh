#ifndef CP_DG_BUILDER_HH
#define CP_DG_BUILDER_HH

#if 0
extern int FETCH_TO_DISPATCH_STAGES;
extern int FETCH_WIDTH;
extern int D_WIDTH;
extern int ISSUE_WIDTH;
extern int EXECUTE_WIDTH;
extern int WRITEBACK_WIDTH;
extern int COMMIT_WIDTH;
extern int SQUASH_WIDTH;
extern int IQ_WIDTH;

extern int ROB_SIZE;

extern int BR_MISS_PENALTY;
extern int IN_ORDER_BR_MISS_PENALTY;
extern int LQ_SIZE;
extern int SQ_SIZE;
#endif

#include "cp_dep_graph.hh"
#include "critpath.hh"
#include <memory>
#include "op.hh"

#include "edge_table.hh"
#include "prof.hh"
#define MAX_FU_POOLS 8
#include "pugixml/pugixml.hpp"

template<typename T, typename E>
class CP_DG_Builder : public CriticalPath {

public:
  typedef dg_inst_base<T, E> BaseInst_t;
  typedef dg_inst<T, E> Inst_t;

  CP_DG_Builder() : CriticalPath() {
     fuUsage.resize(MAX_FU_POOLS);
     nodeResp.resize(MAX_FU_POOLS);
     for(int i = 0; i < MAX_FU_POOLS; ++i) {
       fuUsage[i][0]=0; //begining usage is 0
       fuUsage[i][(uint64_t)-1000]=0; //end usage is 0 too (1000 to prevent overflow)
     }
     MSHRUseMap[0];
     MSHRUseMap[(uint64_t)-1000];
     LQ.resize(LQ_SIZE);
     SQ.resize(SQ_SIZE);
     rob_head_at_dispatch=0;
     lq_head_at_dispatch=0;
     sq_head_at_dispatch=0;

     rob_growth_rate=0;
     avg_rob_head=20;
  }

  virtual ~CP_DG_Builder() {
  }

  virtual void setWidth(int i) {
    FETCH_WIDTH = i;
    D_WIDTH = i;
    ISSUE_WIDTH = i;
    WRITEBACK_WIDTH = i;
    COMMIT_WIDTH = i;
    SQUASH_WIDTH = i;
  }

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    Inst_t* inst = new Inst_t(img,index);
    std::shared_ptr<Inst_t> sh_inst(inst);
    getCPDG()->addInst(sh_inst,index);
    addDeps(sh_inst,op);
    pushPipe(sh_inst);
    inserted(sh_inst);
  }

  virtual void pushPipe(std::shared_ptr<Inst_t>& sh_inst) {
    getCPDG()->pushPipe(sh_inst);
    rob_head_at_dispatch++;
  }

  virtual dep_graph_t<Inst_t,T,E> * getCPDG() = 0;

  uint64_t numCycles() {
    getCPDG()->finish(maxIndex);
    return getCPDG()->getMaxCycles();
  }

  void printEdgeDep(Inst_t& inst, int ind,
                    unsigned default_type1, unsigned default_type2 = E_NONE)
  {
    if (!getTraceOutputs())
      return;
    E* laEdge = inst[ind].lastArrivingEdge();
    unsigned lae;
    if (laEdge) {
      lae = laEdge->type();
      if ((lae != default_type1 && lae!=default_type2) || laEdge->len()>1) {
        outs() << edge_name[lae];
        outs() << laEdge->len();
      }
    }
    outs() << ",";
  }

  virtual void traceOut(uint64_t index,
                        const CP_NodeDiskImage &img, Op* op) {
    if (!getTraceOutputs())
      return;

    Inst_t& inst =
      static_cast<Inst_t&>(getCPDG()->queryNodes(index));

    outs() << index + Prof::get().skipInsts << ": ";
    outs() << inst.cycleOfStage(0) << " ";
    outs() << inst.cycleOfStage(1) << " ";
    outs() << inst.cycleOfStage(2) << " ";
    outs() << inst.cycleOfStage(3) << " ";
    outs() << inst.cycleOfStage(4) << " ";
    outs() << inst.cycleOfStage(5) << " ";

    if (img._isstore) {
      outs() << inst.cycleOfStage(6) << " ";
    }

    //outs() << (_isInOrder ? "io":"ooo");

    printEdgeDep(inst,0,E_FF);
    printEdgeDep(inst,1,E_FD);
    printEdgeDep(inst,2,E_DR);
    printEdgeDep(inst,3,E_RE);
    printEdgeDep(inst,4,E_EP);
    printEdgeDep(inst,5,E_PC,E_CC);

    if (img._isstore) {
      printEdgeDep(inst,6,E_WB);
    }

    outs() << " rs:" << rob_head_at_dispatch
           << "-" << (int)(rob_growth_rate*100);
    outs() << " iq:" << InstQueue.size();

    int lqSize = (lq_head_at_dispatch<=LQind) ? (LQind-lq_head_at_dispatch):
      (LQind-lq_head_at_dispatch+32);
    outs() << " lq:" << lqSize;

    int sqSize = (sq_head_at_dispatch<=SQind) ? (SQind-sq_head_at_dispatch):
      (SQind-sq_head_at_dispatch+32);
    outs() << " sq:" << sqSize;

    outs() << "\n";
  }


protected:

  //function to add dependences onto uarch event nodes -- for the pipeline
  virtual void addDeps(std::shared_ptr<Inst_t>& inst, Op* op=NULL) { 
    setFetchCycle(*inst);
    setDispatchCycle(*inst);
    setReadyCycle(*inst);
    setExecuteCycle(inst); //uses shared_ptr
    setCompleteCycle(*inst,op);
    setCommittedCycle(*inst);
    setWritebackCycle(inst); //uses shared_ptr
  }

  //Variables to hold dynamic state not directly expressible in graph theory
  typedef std::multimap<uint64_t,Inst_t*> ResVec;
  ResVec InstQueue;

  typedef std::vector<std::shared_ptr<Inst_t>> ResQueue;  
  ResQueue SQ, LQ; 
  unsigned SQind;
  unsigned LQind;
 
  typedef std::map<uint64_t,std::vector<Inst_t*>> SingleCycleRes;
  SingleCycleRes wbBusRes;
  SingleCycleRes issueRes;

  typedef std::map<uint64_t,int> FuUsageMap;
  typedef std::vector<FuUsageMap> FuUsage;
 
  std::map<uint64_t,std::set<uint64_t>> MSHRUseMap;
  std::map<uint64_t,std::shared_ptr<BaseInst_t>> MSHRResp;

  typedef typename std::map<uint64_t,BaseInst_t*> NodeRespMap;
  typedef typename std::vector<NodeRespMap> NodeResp;

  FuUsage fuUsage;
  NodeResp nodeResp; 

  typedef std::set<uint64_t> ActivityMap;
  ActivityMap activityMap;
  

  int rob_head_at_dispatch;
  int prev_rob_head_at_dispatch;
  int prev_squash_penalty;
  float avg_rob_head;
  float rob_growth_rate;

  int lq_head_at_dispatch;
  int sq_head_at_dispatch;

  void cleanup() {
    getCPDG()->cleanup();
  }

  uint64_t maxIndex;
  uint64_t _curCycle;

  virtual void inserted(std::shared_ptr<Inst_t>& inst) {
    maxIndex = inst->index();
    _curCycle = inst->cycleOfStage(Inst_t::Fetch);

    //Update Queues: IQ, LQ, SQ
    if (inst->_isload) {
      LQ[LQind]=inst;
      LQind=(LQind+1)%LQ_SIZE;
    }
    if (inst->_isstore) {
      SQ[SQind]=inst;
      SQind=(SQind+1)%SQ_SIZE;
    }

    //delete irrelevent 
    typename SingleCycleRes::iterator upperbound_sc;
    upperbound_sc = wbBusRes.upper_bound(_curCycle);
    wbBusRes.erase(wbBusRes.begin(),upperbound_sc); 
    upperbound_sc = issueRes.upper_bound(_curCycle);
    issueRes.erase(issueRes.begin(),upperbound_sc); 

     
    //add to activity
    for(int i = 0; i < Inst_t::NumStages -1 + inst->_isstore; ++i) {
      activityMap.insert(inst->cycleOfStage(i));
    }

    //summing up idle cycles
    uint64_t prevCycle=0;
    for(auto I=activityMap.begin(),EE=activityMap.end();I!=EE;) {
      uint64_t cycle=*I;
      if(prevCycle!=0 && cycle-prevCycle>14) {
        idleCycles+=(cycle-prevCycle-14);
      }
      if (cycle + 50  < _curCycle && activityMap.size() > 2) {
        I = activityMap.erase(I);
        prevCycle=cycle;
      } else {
        //++I;
        break;
      }
    }

    //for funcUnitUsage
    for(FuUsage::iterator I=fuUsage.begin(),EE=fuUsage.end();I!=EE;++I) {
      FuUsageMap& fuUseMap = *I;
      for(FuUsageMap::iterator i=++fuUseMap.begin(),e=fuUseMap.end();i!=e;) {
        uint64_t cycle = i->first;
        assert(cycle!=0);
        if (cycle + 50  < _curCycle) {
          i = fuUseMap.erase(i);
        } else {
          //++i;
          break;
        }
      }
    }


/*    //DEBUG MSHR Usage
    for(auto i=MSHRUseMap.begin(),e=MSHRUseMap.end();i!=e;) {
      uint64_t cycle = i->first;

      if(cycle > 10000000) {
        break;
      }
      std::cerr << i->first << " " << i->second.size() << ", ";
      ++i;
    }
    std::cerr << "(" << MSHRUseMap.size() << ")\n";*/


    //delete irrelevent
    auto upperMSHRUse = --MSHRUseMap.upper_bound(_curCycle);
    auto firstMSHRUse = MSHRUseMap.begin();

    if(upperMSHRUse->first > firstMSHRUse->first) {
      MSHRUseMap.erase(firstMSHRUse,upperMSHRUse); 
    }


    //delete irrelevent 
    auto upperMSHRResp = MSHRResp.upper_bound(_curCycle);
    MSHRResp.erase(MSHRResp.begin(),upperMSHRResp); 


    for(auto I=nodeResp.begin(),EE=nodeResp.end();I!=EE;++I) {
      NodeRespMap& respMap = *I;
      for(typename NodeRespMap::iterator i=respMap.begin(),e=respMap.end();i!=e;) {
        uint64_t cycle = i->first;
/*
        std::cout << "remove? (" << i->first << "," << _curCycle << ") " << i->second->_index << " " << i->second->_isload << " " << i->second->_isstore << "nri: " << nri << " " << i->second << i->second->cycleOfStage(0) << " " << i->second->cycleOfStage(5);
        if(i->second->_isstore) {
          std::cout << " " << i->second->cycleOfStage(6);
        }
        std::cout << "\n";
*/

    /*    if(!(i->second->_isload || i->second->_isstore)) {
          std::cout << "wtf " << i->second->_index << " " << i->second->_isload << " " << i->second->_isstore << "nri:" << nri << " " << i->second << "\n";
          assert(0);
        }*/

        if (cycle  < _curCycle) {
          //std::cout << respMap.size() << " (yes)\n";
          i = respMap.erase(i);
          //std::cout << respMap.size() << "\n";
        } else {
          
          //++i;
          break;
        }
      }
    }

  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addMSHRResource(uint64_t min_cycle, uint32_t duration, 
                     std::shared_ptr<BaseInst_t> cpnode,
                     uint64_t eff_addr,
                     int re_check_frequency,
                     int& rechecks,
                     uint64_t& extraLat) { 

    int maxUnits = Prof::get().dcache_mshrs;
    rechecks=0; 

    uint64_t cur_cycle=min_cycle;
    std::map<uint64_t,std::set<uint64_t>>::iterator
                       cur_cycle_iter,next_cycle_iter,last_cycle_iter;
   
    cur_cycle_iter = --MSHRUseMap.upper_bound(min_cycle);

    uint64_t filter_addr=(((uint64_t)-1)^(Prof::get().cache_line_size-1));
    uint64_t addr = eff_addr & filter_addr;

    //std::cout << addr << ", " << filter_addr << " (" << Prof::get().cache_line_size << "," << cpnode->_eff_addr << ")\n";


    //keep going until we find a spot .. this is gauranteed
    while(true) {
      assert(cur_cycle_iter->second.size() <= maxUnits);
      if(cur_cycle_iter->second.count(addr)) {
        //return NULL; //just add myself to some other MSHR, and return
        //we found a match... iterate until addr is gone
        while(true) {
          cur_cycle_iter--;
          if(cur_cycle_iter->second.count(addr)==0) {
            cur_cycle_iter++;
            auto  respIter = MSHRResp.find(cur_cycle_iter->first);
            if(respIter!=MSHRResp.end()) {
              return respIter->second.get();
            } else {
              return NULL; 
            }
          }
        }
      }

      //If no spots, keep looking later (pessimistic)
      if(cur_cycle_iter->second.size() == maxUnits) {
        if(re_check_frequency<=1) {
          ++cur_cycle_iter;
          cur_cycle=cur_cycle_iter->first;
        } else {
          cur_cycle+=re_check_frequency;
          cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
          rechecks++;
        }
        continue;
      }

      //we have found a spot with less than maxUnits, now we need to check
      //if there are enough cycles inbetween
      assert(cur_cycle_iter->second.size() < maxUnits);

      next_cycle_iter=cur_cycle_iter;
      bool foundSpot=false;
      
      while(true) {
        ++next_cycle_iter;
        if(next_cycle_iter->first >= cur_cycle+duration) {
          foundSpot=true;
          last_cycle_iter=next_cycle_iter;
          break;
        } else if(next_cycle_iter->second.size() == maxUnits) {
          //we couldn't find a spot, so increment cur_iter and restart
          if(re_check_frequency<=1) {
            ++next_cycle_iter;
            cur_cycle_iter=next_cycle_iter;
            cur_cycle=cur_cycle_iter->first;
          } else {
            cur_cycle+=re_check_frequency;
            cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
            rechecks++;
          }
          break;
        }
      }

      if(foundSpot) {
        if(cur_cycle_iter->first!=cur_cycle) {
          MSHRUseMap[cur_cycle]=cur_cycle_iter->second; 
          cur_cycle_iter = MSHRUseMap.find(cur_cycle); 
        }
        cur_cycle_iter->second.insert(addr);

 
        //iterate through the others
        ++cur_cycle_iter; //i already added here
        while(cur_cycle_iter->first < cur_cycle + duration) {
          assert(cur_cycle_iter->second.size() < maxUnits);
          cur_cycle_iter->second.insert(addr);
          ++cur_cycle_iter;
        }

        //cur_cycle_iter now points to the iter aftter w
        //final cycle iter
        if(cur_cycle_iter->first==cur_cycle+duration) {
          //no need to add a new marker
        } else {
          //need to return to original usage here
          --cur_cycle_iter;
          MSHRUseMap[cur_cycle+duration] = cur_cycle_iter->second;
          MSHRUseMap[cur_cycle+duration].erase(addr);
        }
       
        auto respIter = MSHRResp.find(cur_cycle);
        MSHRResp[cur_cycle+duration]=cpnode;

        if(rechecks>0) {
          respIter = --MSHRResp.upper_bound(cur_cycle);
          extraLat = cur_cycle-respIter->first;
        }

        if(respIter == MSHRResp.end() || min_cycle == cur_cycle) {
          return NULL;
        } else {
          return respIter->second.get();
        }
        
      }
    }
    assert(0);
    return NULL;
  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addResource(int opclass, uint64_t min_cycle, uint32_t duration, 
                       BaseInst_t* cpnode) { 
    return NULL;

    int fuIndex = fuPoolIdx(opclass);
    FuUsageMap& fuUseMap = fuUsage[fuIndex];
    NodeRespMap& nodeRespMap = nodeResp[fuIndex];

    int maxUnits = getNumFUAvailable(opclass);
  
    uint64_t cur_cycle=min_cycle;
    FuUsageMap::iterator cur_cycle_iter,next_cycle_iter,last_cycle_iter;
   
    cur_cycle_iter = --fuUseMap.upper_bound(min_cycle);

    //keep going until we find a spot .. this is gauranteed
    while(true) {
      assert(cur_cycle_iter->second <= maxUnits);
      if(cur_cycle_iter->second == maxUnits) {
        ++cur_cycle_iter;
        cur_cycle=cur_cycle_iter->first;
        continue;
      }
      //we have found a spot with less than maxUnits, now we need to check
      //if there are enough cycles inbetween
      assert(cur_cycle_iter->second < maxUnits);

      next_cycle_iter=cur_cycle_iter;
      bool foundSpot=false;
      
      while(true) {
        ++next_cycle_iter;
        if(next_cycle_iter->first >= cur_cycle+duration) {
          foundSpot=true;
          last_cycle_iter=next_cycle_iter;
          break;
        } else if(next_cycle_iter->second == maxUnits) {
          //we couldn't find a spot, so increment cur_iter and restart
          ++next_cycle_iter;
          cur_cycle_iter=next_cycle_iter;
          cur_cycle=cur_cycle_iter->first;
          break;
        }
      }

      if(foundSpot) {
        if(cur_cycle_iter->first==cur_cycle) {
          //if we started on time point we already have
          cur_cycle_iter->second++;
        } else {
          //create new entry for this cycle
          fuUseMap[cur_cycle]=cur_cycle_iter->second+1; 
          cur_cycle_iter = fuUseMap.find(cur_cycle); 
        }
 
        //iterate through the others
        ++cur_cycle_iter;
        while(cur_cycle_iter->first < cur_cycle + duration) {
          assert(cur_cycle_iter->second < maxUnits);
          cur_cycle_iter->second++;
          ++cur_cycle_iter;
        }

        //final cycle iter
        if(cur_cycle_iter->first==cur_cycle+duration) {
          //no need to add a new marker
        } else {
          //need to return to original usage here
          --cur_cycle_iter;
          fuUseMap[cur_cycle+duration] = cur_cycle_iter->second-1;
          assert(fuUseMap[cur_cycle+duration]>=0);
        }
        
        typename NodeRespMap::iterator respIter = nodeRespMap.find(cur_cycle);
        nodeRespMap[cur_cycle+duration]=cpnode;

        if(respIter == nodeRespMap.end() || min_cycle == cur_cycle) {
          return NULL;
        } else {
          return respIter->second;
        }
        
      }
    }
    assert(0);
    return NULL;
  }

  virtual void setFetchCycle(Inst_t& inst) {
    checkFBW(inst);
    //checkICache(inst);
    checkPipeStalls(inst);
    checkControlMispeculate(inst);
    checkFF(inst);

    //-----FETCH ENERGY-------
    //Need to get ICACHE accesses somehow
    if(inst._icache_lat>0) {
      icache_read_accesses++;
    }
    if(inst._icache_lat>1) {
      icache_read_misses++;
    }



    if(_isInOrder) {
//      checkBranchPredict(inst,op); //just control dependence
    } else {
      //checkIQStalls(inst);
      //checkLSQStalls(inst);
    }
  }

  virtual void setDispatchCycle(Inst_t &inst) {
    checkFD(inst);
    checkDD(inst);
    checkDBW(inst);
    checkSerializing(inst);

    if(_isInOrder) {
      //nothing
    } else {
      checkROBSize(inst); //Finite Rob Size
      checkIQStalls(inst);
      checkLSQSize(inst);

      // ------ ENERGY EVENTS -------
      if(inst._floating) {
        iw_fwrites++;
      } else {
        iw_writes++;
      }

      //rename_fwrites+=inst._numFPDestRegs;
      //rename_writes+=inst._numIntDestRegs;

      if(inst._floating) {
        rename_freads+=inst._numSrcRegs;
        rename_fwrites+=inst._numFPDestRegs+inst._numIntDestRegs;
      } else {
        rename_reads+=inst._numSrcRegs;
        rename_writes+=inst._numFPDestRegs+inst._numIntDestRegs;
      }

    }

    //HANDLE ROB STUFF
    uint64_t dispatch_cycle = inst.cycleOfStage(Inst_t::Dispatch);
    do {
      Inst_t* depInst = getCPDG()->peekPipe(-rob_head_at_dispatch);
      if(!depInst) {
        break;
      }
      if(depInst->cycleOfStage(Inst_t::Commit) < dispatch_cycle ) {
        rob_head_at_dispatch--;
      } else {
        break;
      }
    } while(rob_head_at_dispatch>=1);

    avg_rob_head = 0.01 * rob_head_at_dispatch + 
                   0.99 * avg_rob_head;

    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(depInst==0 || !depInst->_ctrl_miss) { 
      rob_growth_rate = 0.92f * rob_growth_rate
                      + 0.08f * (rob_head_at_dispatch - prev_rob_head_at_dispatch); 
    } else {
      rob_growth_rate=0;
    }

    prev_rob_head_at_dispatch=rob_head_at_dispatch;

    //HANDLE LSQ HEADS
    //Loads leave at complete
    do {
      Inst_t* depInst = LQ[lq_head_at_dispatch].get();
      if(!depInst) {
        break;
      }
      if(depInst->cycleOfStage(Inst_t::Complete) < dispatch_cycle ) {
        lq_head_at_dispatch+=1;
        lq_head_at_dispatch%=LQ_SIZE; 
      } else {
        break;
      }
    } while(lq_head_at_dispatch!=LQind);

    do {
      Inst_t* depInst = SQ[sq_head_at_dispatch].get();
      if(!depInst) {
        break;
      }
      if(depInst->cycleOfStage(Inst_t::Writeback) < dispatch_cycle ) {
        sq_head_at_dispatch+=1;
        sq_head_at_dispatch%=SQ_SIZE; 
      } else {
        break;
      }
    } while(sq_head_at_dispatch!=SQind);
  
  }
  

  virtual void setReadyCycle(Inst_t &inst) {
    checkDR(inst);
    checkNonSpeculative(inst);
    checkDataDep(inst);
    if(_isInOrder) {
      checkExecutePipeStalls(inst);
    } else {
      //no other barriers
    }

    /*
    // num src regs is wrong... eventually fix this in gem5, for now just cheat
    if(inst._floating) {
      regfile_freads+=inst._numSrcRegs;
    } else {
      regfile_reads+=inst._numSrcRegs;
    }*/

    regfile_fwrites+=inst._numFPDestRegs;
    regfile_writes+=inst._numIntDestRegs;

    regfile_freads+=inst._numFPSrcRegs;
    regfile_reads+=inst._numIntSrcRegs;

  }

  virtual void setExecuteCycle(std::shared_ptr<Inst_t>& inst) {
    checkRE(*inst);

    if(_isInOrder) {
      checkEE(*inst); //issue in order
      checkInorderIssueWidth(*inst); //in order issue width
    } else {
      checkIssueWidth(*inst);
      checkFuncUnits(*inst);

      if(inst->_isload) {
        //loads acquire MSHR at execute
        checkNumMSHRs(inst,false);
      }

      //Non memory instructions can leave the IQ now!
      //Push onto the IQ by cycle that we leave IQ (execute cycle)
      if(!inst->isMem()) {
        InstQueue.insert(
               std::make_pair(inst->cycleOfStage(Inst_t::Execute),inst.get()));
      }
    }
    if(inst->_floating) {
      iw_freads+=2;
    } else {
      iw_reads+=2;
    }
  }

  virtual void setCompleteCycle(Inst_t &inst, Op* op) {
    checkEP(inst);
    if (!_isInOrder) {
      if(inst._isload) {
        checkPP(inst);
      }
      checkWriteBackWidth(inst);

      //Memory instructions can leave the IQ now!
      //Push onto the IQ by cycle that we leave IQ (complete cycle)
      if(inst.isMem()) {
        InstQueue.insert(
               std::make_pair(inst.cycleOfStage(Inst_t::Complete),&inst));
      }
    } else {
      //checkExecutePipeline
    }

    //Energy Events
    if(op && op->numUses()>0) {
      if( inst._floating) {
        //regfile_fwrites++;
      } else {
        //regfile_writes++;
      }
    }


  }
  virtual void setCommittedCycle(Inst_t &inst) {
    checkPC(inst);
    if(!_isInOrder) {
      checkSquashPenalty(inst);
    }
    checkCC(inst);
    checkCBW(inst);
 
    getCPDG()->commitNode(inst.index());

    //Energy
    rob_writes+=2;
    rob_reads++;

    //Calculate extra rob_reads due to the pipeline checking extra instructions
    //before it fills up the commit width
    Inst_t* prevInst = getCPDG()->peekPipe(-1); 
    Inst_t* widthInst = getCPDG()->peekPipe(-COMMIT_WIDTH); 
    if(prevInst && widthInst) {
      uint64_t widthCycle = widthInst->cycleOfStage(Inst_t::Commit);
      uint64_t prevCycle = prevInst->cycleOfStage(Inst_t::Commit);
      uint64_t curCycle = inst.cycleOfStage(Inst_t::Commit);
      if(prevCycle!=curCycle && widthCycle + 1 != curCycle) {
        rob_reads+=curCycle-prevCycle;
      }
    }

    committed_insts++;
    committed_int_insts+=!inst._floating;
    committed_fp_insts+=inst._floating;
    committed_branch_insts+=inst._isctrl;
    committed_load_insts+=inst._isload;
    committed_store_insts+=inst._isstore;
    func_calls+=inst._iscall;
  }

  virtual void setWritebackCycle(std::shared_ptr<Inst_t>& inst) {
    if(!_isInOrder) {
      if(inst->_isstore) {
        getCPDG()->insert_edge(*inst, Inst_t::Commit,
                               *inst, Inst_t::Writeback, 2+inst->_st_lat, E_WB);
        checkNumMSHRs(inst,true);
        checkPP(*inst);
      }
    }
  }

  //==========FETCH ==============
  //Inorder fetch
  virtual Inst_t &checkFF(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    int lat=n._icache_lat;
    
    
    if(depInst->_isctrl) {
      //check if previous inst was a predict taken branch
      bool predict_taken = false;
      if(depInst->_pc == n._pc) {
        predict_taken = depInst->_upc +1 != n._pc;
      } else {
        predict_taken |= n._pc < depInst->_pc;
        predict_taken |= n._pc > depInst->_pc+8;//proxy for inst width
      }
      if(predict_taken && lat==0) {
        lat+=1;
      }
      
/*
      if(lat==0) { //fetch ends at a control inst
        Inst_t* depInst2 = getCPDG()->peekPipe(-2);
        if(depInst2 && depInst2->cycleOfStage(0)==depInst->cycleOfStage(0)) {
          lat=1;
        }
      }
*/
    }
        

    getCPDG()->insert_edge(*depInst, Inst_t::Fetch,
                           n, Inst_t::Fetch,lat,E_FF);
    return n;
  }

  virtual Inst_t &checkSerializing(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    } 

    if(n._serialBefore || depInst->_serialAfter) {
      getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                             n, Inst_t::Dispatch, 4,E_SER);
    }
    return n;
  }


/*
  //Icache miss
  //KNOWN_ISSUE:  what happens to accelerated icache miss?
  virtual Inst_t &checkICache(Inst_t&n) {
    if(n._icache_lat>0) {
      getCPDG()->insert_edge(n.index()-1,Inst_t::Fetch,
                             n,Inst_t::Fetch,n._icache_lat,E_IC);
    }
    return n;
  }
*/
  //Finite fetch bandwidth
  virtual Inst_t &checkFBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-FETCH_WIDTH);
    if(!depInst) {
      return n;
    }

    int length=1;
    //Inst_t* prevInst = getCPDG()->peekPipe(-1);
    //if(prevInst && !prevInst->_isctrl) {
    //  length+=n._icache_lat;
    //}

    getCPDG()->insert_edge(*depInst, Inst_t::Fetch,
                           n, Inst_t::Fetch, length, E_FBW); 
    return n;
  }

  virtual Inst_t &checkPipeStalls(Inst_t &n) {
    // 0   -1     -2        -3          -4 
    // F -> De -> Rename -> Dispatch -> Rob
    // F -> De -> Rename -> Dispatch -> Rob
    // F -> De -> Renane -> Dispatch -> Rob

    // ??? Should I add n._icache_lat?
    Inst_t* depInst = getCPDG()->peekPipe(
            -FETCH_TO_DISPATCH_STAGES*FETCH_WIDTH);
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                     n, Inst_t::Fetch, 0,E_FPip);

    return n;
  }
/*
  //check branch predict edges -- (in order)
  virtual Inst_t &checkBranchPredict(Inst_t &n, Op* op) {
    //No branch prediction implemented
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) { 
      return n;
    }
    if(depInst->_isctrl) {
      getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                             n, Inst_t::Fetch,
                             n._icache_lat + 1,E_BP);
    }
    
    return n;
  }
*/

  

  //Control Dep
  virtual Inst_t &checkControlMispeculate(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    if (depInst->_ctrl_miss) {
        getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                               n, Inst_t::Fetch,
                               n._icache_lat + //don't add in the icache latency?
          prev_squash_penalty + 1); //two to commit, 1 to 

        //we need to make sure that the processor stays active during squash
        uint64_t i=depInst->cycleOfStage(Inst_t::Complete);
        for(;i<n.cycleOfStage(Inst_t::Fetch);++i) {
          activityMap.insert(i);
        }

        mispeculatedInstructions++;
    }
    return n;
  }

  //inst queue full stalls
  virtual Inst_t &checkIQStalls(Inst_t &n) {

    //First, erase all irrelevant nodes
    typename ResVec::iterator upperbound;
    upperbound = InstQueue.upper_bound(n.cycleOfStage(Inst_t::Dispatch));
    InstQueue.erase(InstQueue.begin(),upperbound);

    //Next, check if the IQ is full
    if (InstQueue.size() < (unsigned)IQ_WIDTH-1) {
      return n;
    }

    Inst_t* min_node=0;
    for (auto I=InstQueue.begin(), EE=InstQueue.end(); I!=EE; ++I) {
      Inst_t* i =I->second;  //I->second.get();
      if(i->iqOpen==false) {
        i->iqOpen=true;
        min_node=i;
        break;
      } else {
        //assert(0);
      }
    }
    
    //min_node=InstQueue.upper_bound(0)->second.get();
    assert(min_node);
    getCPDG()->insert_edge(*min_node, Inst_t::Execute,
                           n, Inst_t::Dispatch, n._icache_lat + 1, E_IQ);
    return n;
  }



 /* 
 //Getting rid of this because i don't think lsq should stall fetch --
 //it should work like a rob for just loads, where it stalls dispatch
  virtual Inst_t &checkLSQStalls(Inst_t &n) {
    //number of load/store instructions not executed exceeds the LSQ_WIDTH,
    //stall the fetch
    //TODO XXX This isn't really consistent with simulator code... overly
    //conservative
    if (LQ.size() < (unsigned)LQ_SIZE && SQ.size() < (unsigned)SQ_SIZE)
      return n;

    uint64_t min_issued_cycle = (uint64_t)-1;

    Inst_t* min_node=0;
    for (auto I = LQ.begin(), EE = LQ.end(); I!=EE; ++I) {
      Inst_t* i = I->second.get();
      min_node=i;           
    }
    for (auto I = SQ.begin(), EE = SQ.end(); I!=EE; ++I) {
      Inst_t* i = I->second.get();
      min_node=i;
    }
    getCPDG()->insert_edge(min_node, Inst_t::Commit,
                           n, Inst_t::Fetch, n._icache_lat+1,E_LSQ);
    return n;
  }
*/

  //========== DISPATCH ==============

  //Dispatch follows fetch
  virtual Inst_t &checkFD(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Fetch,
                     n, Inst_t::Dispatch, FETCH_TO_DISPATCH_STAGES, E_FD);
    return n;
  }

  //Dispatch bandwdith
  //This edge is possibly never required?
  virtual Inst_t &checkDBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-D_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                          n, Inst_t::Dispatch, 1,E_DBW);
    return n;
  }

  //Dispatch Inorder
  virtual Inst_t &checkDD(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                          n, Inst_t::Dispatch, 0,E_DD);
    return n;
  }


  //Check Serializing
  //didn't work as well at decode
/*
  virtual Inst_t &checkSerializing(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    } 

    if(n._serialBefore || depInst->_serialAfter) {
      getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                             n, Inst_t::Dispatch, 3,E_SER);
    }
    return n;
  }
*/


  //Finite ROB (commit to dispatch)
  virtual Inst_t &checkROBSize(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-ROB_SIZE); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Dispatch, 1,E_ROB);
    return n;
  }

  //Finite LQ & SQ
  virtual Inst_t &checkLSQSize(Inst_t &n) {
    if (n._isload) {
      if(LQ[LQind]) {
        getCPDG()->insert_edge(*LQ[LQind], Inst_t::Commit,
                               n, Inst_t::Dispatch, 1,E_LSQ);
      }
      //LQ[LQind]=inst;
    }
    if (n._isstore) {
      if(SQ[SQind]) {
        getCPDG()->insert_edge(*SQ[SQind], Inst_t::Writeback,
                               n, Inst_t::Dispatch, 1,E_LSQ);
      }
    }
    return n;
  }


  //========== READY ==============
  //Ready Follows Dispatch
  virtual Inst_t &checkDR(Inst_t &n)
  {
    getCPDG()->insert_edge(n, Inst_t::Dispatch,
                     n, Inst_t::Ready, 0,E_DR);
    return n;
  }

   //NonSpeculative Insts
  virtual Inst_t &checkNonSpeculative(Inst_t &n) {
    if (n._nonSpec) {
      int ind=(SQind-1+SQ_SIZE)%SQ_SIZE;
      if(SQ[ind]) {
        getCPDG()->insert_edge(*SQ[ind], Inst_t::Writeback,
                               n, Inst_t::Ready, 1,E_NSpc);
      }
    }
    return n;
  }
 

  //Data dependence
  virtual Inst_t &checkDataDep(Inst_t &n) {
    //register dependence
    for (int i = 0; i < 7; ++i) {
      unsigned prod = n._prod[i];
      if (prod <= 0 || prod >= n.index()) {
        continue;
      }
      BaseInst_t& depInst =
            getCPDG()->queryNodes(n.index()-prod);

      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             n, Inst_t::Ready, 0,E_RDep);
    }

    //memory dependence
    if (n._mem_prod > 0 && n._mem_prod < n.index()) {
      
      BaseInst_t& dep_inst=getCPDG()->queryNodes(n.index()-n._mem_prod);

      if(dep_inst.isPipelineInst()) {
        Inst_t& prev_node = static_cast<Inst_t&>(dep_inst);
        if (prev_node._isstore && n._isload) {
          //data dependence
          getCPDG()->insert_edge(prev_node.index(), Inst_t::Complete,
                                    n, Inst_t::Ready, 0, E_MDep);
        } else if (prev_node._isstore && n._isstore) {
          //anti dependence (output-dep)
          getCPDG()->insert_edge(prev_node.index(), Inst_t::Complete,
                                    n, Inst_t::Complete, 0, E_MDep);
        } else if (prev_node._isload && n._isstore) {
          //anti dependence (load-store)
          getCPDG()->insert_edge(prev_node.index(), Inst_t::Complete,
                                    n, Inst_t::Complete, 0, E_MDep);
        }
      }
    }
    return n;
  }

  //==========EXECUTION ==============
  //Execution follows Ready

  //get index for fuPool
  virtual int fuPoolIdx(int opclass1) {
    if (opclass1 > 9 && opclass1 < 30) {
      return 0;
    }

    switch(opclass1) {
    default: 
      return 1;
    case 1: //IntALU
      return 2;
    case 2: //IntMult
    case 3: //IntDiv
      return 3;
    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return 4;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return 5;
    case 30: //MemRead
    case 31: //MemWrite
      return 6;
    }
    assert(0);
  }

  virtual bool isSameOpClass(int opclass1, int opclass2) {
    if (opclass1 == opclass2)
      return true;
    if (opclass1 > 9 && opclass2 > 9 && opclass1 < 30 && opclass2 < 30) {
      //all simd units are same
      return true;
    }
    switch(opclass1) {
    default: return false;
    case 1:
      return (opclass2 == 1);
    case 2:
    case 3:
      return (opclass2 == 2 || opclass2 == 3);
    case 4:
    case 5:
    case 6:
      return (opclass2 == 4 || opclass2 == 5 || opclass2 == 6);
    case 7:
    case 8:
    case 9:
      return (opclass2 == 7 || opclass2 == 8 || opclass2 == 9);
    case 30: //MemRead
    case 31: //MemWrite
      return (opclass2 == 30 || opclass2 == 31);
    }
    return false;
  }

  virtual unsigned getNumFUAvailable(Inst_t &n) {
    return getNumFUAvailable(n._opclass);
  }

  virtual unsigned getNumFUAvailable(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return ROB_SIZE+IQ_WIDTH;
    case 1: //IntALU
      return 6;

    case 2: //IntMult
    case 3: //IntDiv
      return 2;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return 4;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return 2;
    case 30: //MemRead
    case 31: //MemWrite
      return 2;
    default:
      return 4;
    }
    return 4;
  }

  virtual unsigned getFUIssueLatency(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return Prof::get().int_alu_issueLat;

    case 2: //IntMult
      return Prof::get().mul_issueLat;
    case 3: //IntDiv
      return Prof::get().div_issueLat;

    case 4: //FloatAdd
      return Prof::get().fadd_issueLat;
    case 5: //FloatCmp
      return Prof::get().fcmp_issueLat;
    case 6: //FloatCvt
      return Prof::get().fcvt_issueLat;
    case 7: //FloatMult
      return Prof::get().fmul_issueLat;
    case 8: //FloatDiv
      return Prof::get().fdiv_issueLat;
    case 9: //FloatSqrt
      return Prof::get().fsqrt_issueLat;
    default:
      return 1;
    }
    return 1;
  }

  virtual unsigned getFUOpLatency(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return Prof::get().int_alu_opLat;

    case 2: //IntMult
      return Prof::get().mul_opLat;
    case 3: //IntDiv
      return Prof::get().div_opLat;

    case 4: //FloatAdd
      return Prof::get().fadd_opLat;
    case 5: //FloatCmp
      return Prof::get().fcmp_opLat;
    case 6: //FloatCvt
      return Prof::get().fcvt_opLat;
    case 7: //FloatMult
      return Prof::get().fmul_opLat;
    case 8: //FloatDiv
      return Prof::get().fdiv_opLat;
    case 9: //FloatSqrt
      return Prof::get().fsqrt_opLat;
    default:
      return 1;
    }
    return 1;
  }


  //KNOWN_HOLE: SSE Issue Latency Missing
  virtual unsigned getFUIssueLatency(Inst_t &n) {
    return getFUIssueLatency(n._opclass);
  }

  //Check Functional Units to see if they are full
  virtual Inst_t &checkFuncUnits(Inst_t &n) {
    // no func units if load, store, or if it has no opclass
    //if (n._isload || n._isstore || n._opclass==0)
    if (n._opclass==0) {
      return n;
    }

    if (n._opclass==2) {
      mult_ops++;
    }

    Inst_t* min_node = static_cast<Inst_t*>(
         addResource(n._opclass, n.cycleOfStage(Inst_t::Execute), 
                                   getFUIssueLatency(n), &n));

    if(min_node) {
      //TODO: check min->node start
      getCPDG()->insert_edge(min_node->index(), Inst_t::Execute,
                        n, Inst_t::Execute, getFUIssueLatency(*min_node),E_FU);
    }
    return n;
  }

  //check MSHRs to see if they are full
  virtual void checkNumMSHRs(std::shared_ptr<Inst_t>& n, bool store) {
    int mlat;
    assert(n->_isload || n->_isstore);
    if(n->_isload) {
      mlat = n->_ex_lat;
    } else {
      mlat = n->_st_lat;
    }
    if(mlat <= Prof::get().dcache_hit_latency + 
               Prof::get().dcache_response_latency+3) {
      //We don't need an MSHR for non-missing loads/stores
      return;
    }

    int reqDelayT  = Prof::get().dcache_hit_latency; // # cycles delayed before acquiring the MSHR
    int respDelayT = Prof::get().dcache_response_latency; // # cycles delayed after releasing the MSHR
    int mshrT = mlat - reqDelayT - respDelayT;  // the actual time of using the MSHR
 
    assert(mshrT>0);
    int rechecks=0;
    uint64_t extraLat=0;
    if(!store) { //if load

      //have to wait this long before each event
      int insts_to_squash=instsToSquash();
      int squash_cycles = squashCycles(insts_to_squash);
      int recheck_cycles = squash_cycles + 4;
      
      BaseInst_t* min_node =
         addMSHRResource(reqDelayT + n->cycleOfStage(Inst_t::Execute), 
              mshrT, n, n->_eff_addr, recheck_cycles, rechecks, extraLat);
      if(rechecks==0) {
        /*if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
               *n, Inst_t::Execute, respDelayT+extraLat, E_MSHR);
        }*/
        //don't do anything!

      } else { //rechecks > 0 -- time to refetch
        //create a copy of the instruction to serve as a dummy
        Inst_t* dummy_inst = new Inst_t();
        dummy_inst->copyEvents(n.get());
        //Inst_t* dummy_inst = new Inst_t(*n);
        std::shared_ptr<Inst_t> sh_dummy_inst(dummy_inst);
        

        pushPipe(sh_dummy_inst);
        assert(min_node); 

        n->reset_inst();
        setFetchCycle(*n);
        /*getCPDG()->insert_edge(*min_node, min_node->memComplete(),
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);*/

        getCPDG()->insert_edge(*dummy_inst, Inst_t::Execute,
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);

        setDispatchCycle(*n);
        setReadyCycle(*n);
        checkRE(*n);        

        squashed_insts+=rechecks*insts_to_squash;
      }
    } else { //if store
       BaseInst_t* min_node =
           addMSHRResource(reqDelayT + n->cycleOfStage(Inst_t::Commit), 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
        getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                 *n, Inst_t::Writeback, mshrT+respDelayT, E_MSHR);
      }
    }

    return;
  }


  //Ready to Execute Stage -- no delay
  virtual Inst_t &checkRE(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Ready,
                           n, Inst_t::Execute, 0,E_RE);
    return n;
  }

  //Inorder Execute
  virtual Inst_t &checkEE(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    assert(depInst);
    getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                           n, Inst_t::Execute, 0,E_EE);
    return n;
  }

  //Execute Bandwidth (issue width)
  virtual Inst_t &checkInorderIssueWidth(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-ISSUE_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                          n, Inst_t::Commit, 1,E_IBW);
    return n;
  }

  //Insert Dynamic Edge for constraining issue width, 
  //each inst reserves slots in the issueRes map
  virtual Inst_t &checkIssueWidth(Inst_t &n) {    
    //find an issue slot
    int index = n.cycleOfStage(Inst_t::Execute); //ready to execute cycle
    int orig_index = index;
    while(issueRes[index].size()==ISSUE_WIDTH) {
      index++;
    }

    //slot found in index
    issueRes[index].push_back(&n);

    //don't add edge if not necessary
    if(index == orig_index) {
      return n;
    }

    //otherwise, add a dynamic edge to execute from last inst of previous cycle
    Inst_t* dep_n = issueRes[index-1].back();
    getCPDG()->insert_edge(*dep_n, Inst_t::Execute,
                           n, Inst_t::Execute, 1,E_IBW);
    return n;
  }

  //make sure current instructions are done executing before getting the next
  //This is for inorder procs...
  virtual Inst_t &checkExecutePipeStalls(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    if(depInst->isMem() && depInst->_ex_lat> 8) {
      getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                              n, Inst_t::Ready, -8,E_EPip);
    }

    return n;
  }

  //==========COMPLTE ==============
  //Complete After Execute
  virtual Inst_t &checkEP(Inst_t &n) {

    //Ok, so memory instructions bear their memory latency here.  If we have a cache
    //producer, that means we should be in the cache, so drop the latency
    int lat=0;

    if(n._isload) {
      if(n._cache_prod) {
        lat = Prof::get().dcache_hit_latency + Prof::get().dcache_response_latency;
      } else {
        lat = n._ex_lat;
      }
    } else {
      //don't want to use this dynamic latency
      //lat = n._ex_lat;
      lat = getFUOpLatency(n._opclass);
    }

    getCPDG()->insert_edge(n, Inst_t::Execute,
                           n, Inst_t::Complete, lat, E_EP);
    return n;
  }

  //issueRes map
  virtual Inst_t &checkWriteBackWidth(Inst_t &n) {    
    //find an issue slot
    uint64_t index = n.cycleOfStage(Inst_t::Complete); //ready to execute cycle
    while(wbBusRes[index].size()==WRITEBACK_WIDTH) {
      index++;
    }

    //slot found in index
    wbBusRes[index].push_back(&n);

    //don't add edge if not necessary
    if(index == n.cycleOfStage(Inst_t::Complete)) {
      return n;
    }

    //otherwise, add a dynamic edge to execute from last inst of previous cycle
    Inst_t* dep_n = wbBusRes[index-1].back();
    getCPDG()->insert_edge(*dep_n, Inst_t::Complete,
                           n, Inst_t::Complete, 1, E_WBBW);
    return n;
  }

  //Cache Line Producer
  virtual Inst_t &checkPP(Inst_t &n) {
    return n;
   
    //This edge doesn't work...
    uint64_t cache_prod = n._cache_prod;

    if (cache_prod > 0 && cache_prod < n.index()) {
      BaseInst_t& depInst = getCPDG()->queryNodes(n.index()-cache_prod);

      getCPDG()->insert_edge(depInst, depInst.memComplete(),
                             n, n.memComplete(), 0, E_PP);
    }
    return n;
  }

  //Commit follows complete
  virtual Inst_t &checkPC(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Complete,
                           n, Inst_t::Commit, 2,E_PC);  
    //simulator says: minimum two cycles between commit and complete
    return n;
  }

  // heuristic for calculating insts to squash
  int instsToSquash() {
    int insts_to_squash;
    
    insts_to_squash=rob_head_at_dispatch;
    if(rob_growth_rate-0.05>0) {
      insts_to_squash+=12*(rob_growth_rate-0.05);
    }

    return insts_to_squash*0.6f + avg_rob_head*0.4f;
  }
  
  int squashCycles(int insts_to_squash) {
    return 1 +  insts_to_squash/SQUASH_WIDTH 
             + (insts_to_squash%SQUASH_WIDTH!=0);
  }

  virtual Inst_t &checkSquashPenalty(Inst_t &n) {
    //BR_MISS_PENALTY no longer used
    //Instead we calculate based on the rob size, how long it's going to
    //take to squash.  This is a guess, to some degree, b/c we don't
    //know what happened after the control instruction was executed,
    //but it might be close enough.
    if (n._ctrl_miss) {

        int insts_to_squash=instsToSquash();
        squashed_insts+=insts_to_squash;

        int squash_cycles = squashCycles(insts_to_squash); 


        //int squash_cycles = (rob_head_at_dispatch + 1- 12*rob_growth_rate)/SQUASH_WIDTH;

        if(squash_cycles<5) {
          squash_cycles=5;
        } else if (squash_cycles > (ROB_SIZE-10)/SQUASH_WIDTH) {
          squash_cycles = (ROB_SIZE-10)/SQUASH_WIDTH;
        }

        //add two extra cycles, because commit probably has to play catch up
        //because it can't commit while squashing
        getCPDG()->insert_edge(n, Inst_t::Complete,
                               n, Inst_t::Commit,squash_cycles+2,E_SQUA);

        prev_squash_penalty=squash_cycles;
    }
    return n;
  }

  //in order commit
  virtual Inst_t &checkCC(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Commit, 0,E_CC);
    return n;
  }

  //Finite commit width
  virtual Inst_t &checkCBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-COMMIT_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                          n, Inst_t::Commit, 1,E_CBW);
    return n;
  }


  // Handle enrgy events for McPAT XML DOC
  virtual void setEnergyEvents(pugi::xml_document& doc) {
    //set the normal events based on the m5out/stats file
    CriticalPath::setEnergyEvents(doc);
 
    uint64_t busyCycles=numCycles()-idleCycles;

    pugi::xml_node system_node = doc.child("component").find_child_by_attribute("name","system");
    pugi::xml_node core_node = 
              system_node.find_child_by_attribute("name","core0");
   
    sa(system_node,"total_cycles",numCycles());
    sa(system_node,"idle_cycles", idleCycles);
    sa(system_node,"busy_cycles",busyCycles);

    //Modify relevent events to be what we predicted
    double squashRatio = (double)squashed_insts/(double)(committed_insts);
    double highSpecFactor = 1.00+1.5*squashRatio;
    double specFactor = 1.00+squashRatio;
    double halfSpecFactor = 1.00+0.5*squashRatio;
    double fourthSpecFactor = 1.00+0.25*squashRatio;
    //double eigthSpecFactor = 1.00+0.125*squashRatio;
    double sixteenthSpecFactor = 1.00+0.0625*squashRatio;


    uint64_t intOps=committed_int_insts-committed_load_insts-committed_store_insts;

    sa(core_node,"total_instructions",(uint64_t)(committed_insts*specFactor));
    sa(core_node,"int_instructions",(uint64_t)(intOps*specFactor));
    sa(core_node,"fp_instructions",(uint64_t)(committed_fp_insts*specFactor));
    sa(core_node,"branch_instructions",(uint64_t)(committed_branch_insts*highSpecFactor));
    sa(core_node,"branch_mispredictions",(uint64_t)(mispeculatedInstructions*sixteenthSpecFactor));
    sa(core_node,"load_instructions",(uint64_t)(committed_load_insts*fourthSpecFactor));
    sa(core_node,"store_instructions",(uint64_t)(committed_store_insts*fourthSpecFactor));

    sa(core_node,"committed_instructions",committed_insts);
    sa(core_node,"committed_int_instructions",committed_int_insts);
    sa(core_node,"committed_fp_instructions",committed_fp_insts);

    sa(core_node,"total_cycles",numCycles());
    sa(core_node,"idle_cycles", idleCycles); //TODO: how to get this?
    sa(core_node,"busy_cycles",busyCycles);

    sa(core_node,"ROB_reads",(uint64_t)(rob_reads-idleCycles)*halfSpecFactor);
    sa(core_node,"ROB_writes",(uint64_t)(rob_writes*specFactor)+squashed_insts);

    sa(core_node,"rename_reads",(uint64_t)(rename_reads*specFactor));
    sa(core_node,"rename_writes",(uint64_t)(rename_writes*specFactor)+squashed_insts);
    sa(core_node,"fp_rename_reads",(uint64_t)(rename_freads*specFactor));
    sa(core_node,"fp_rename_writes",(uint64_t)(rename_fwrites*specFactor));
                                                                    
    sa(core_node,"inst_window_reads",(uint64_t)(iw_reads*specFactor)+busyCycles);
    sa(core_node,"inst_window_writes",(uint64_t)(iw_writes*specFactor)+squashed_insts);
    sa(core_node,"inst_window_wakeup_accesses",(uint64_t)(iw_writes*specFactor));

    sa(core_node,"fp_inst_window_reads",(uint64_t)(iw_freads*specFactor));
    sa(core_node,"fp_inst_window_writes",(uint64_t)(iw_fwrites*specFactor));
    sa(core_node,"fp_inst_window_wakeup_accesses",(uint64_t)(iw_fwrites*specFactor));

    sa(core_node,"int_regfile_reads",(uint64_t)(regfile_reads*halfSpecFactor));
    sa(core_node,"int_regfile_writes",(uint64_t)(regfile_writes*specFactor));
    sa(core_node,"float_regfile_reads",(uint64_t)(regfile_freads*halfSpecFactor));
    sa(core_node,"float_regfile_writes",(uint64_t)(regfile_fwrites*specFactor));

    sa(core_node,"function_calls",(uint64_t)(func_calls*specFactor));

    sa(core_node,"ialu_accesses",(uint64_t)(committed_int_insts*specFactor));
    sa(core_node,"fpu_accesses",(uint64_t)(committed_fp_insts*specFactor));
    sa(core_node,"mul_accesses",(uint64_t)(mult_ops*specFactor));

    sa(core_node,"cdb_alu_accesses",(uint64_t)(committed_int_insts*specFactor));
    sa(core_node,"cdb_fpu_accesses",(uint64_t)(committed_fp_insts*specFactor));
    sa(core_node,"cdb_mul_accesses",(uint64_t)(mult_ops*specFactor));
     
  }
};


#endif










#if 0
  virtual unsigned getFUIssueLatency(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return 1;

    case 2: //IntMult
      return 1;
    case 3: //IntDiv
      return 19;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return 1;
    case 7: //FloatMult
      return 1;
    case 8: //FloatDiv
      return 12;
    case 9: //FloatSqrt
      return 24;
    default:
      return 1;
    }
    return 1;
  }
#endif
