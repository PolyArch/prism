#ifndef CP_DG_BUILDER_HH
#define CP_DG_BUILDER_HH

#include "cp_dep_graph.hh"
#include "critpath.hh"
#include "exec_profile.hh"

#include <memory>
#include "op.hh"

#include "edge_table.hh"
#include "prof.hh"
#include "pugixml/pugixml.hpp"
#include <unordered_set>
#include "cp_utils.hh"

#define MAX_RES_DURATION 64

template<typename T, typename E>
class CP_DG_Builder : public CriticalPath {

public:
  typedef dg_inst_base<T, E> BaseInst_t;
  typedef dg_inst<T, E> Inst_t;

  typedef std::shared_ptr<BaseInst_t> BaseInstPtr;
  typedef std::shared_ptr<Inst_t> InstPtr;

  CP_DG_Builder() : CriticalPath() {
     MSHRUseMap[0];
     MSHRUseMap[(uint64_t)-10000];
     MSHRResp[0];
     MSHRResp[(uint64_t)-10000];
     LQ.resize(LQ_SIZE);
     SQ.resize(SQ_SIZE);
     activityMap.insert(1);
     rob_head_at_dispatch=0;
     lq_head_at_dispatch=0;
     sq_head_at_dispatch=0;

     rob_growth_rate=mylog2(0);
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

    if(_isInOrder) {
      FETCH_TO_DISPATCH_STAGES=3;

      N_ALUS=ISSUE_WIDTH; //only need 1-issue, 2-issue
      N_FPU=1;
      RW_PORTS=1;
      N_MUL=1;

      IBUF_SIZE=16;
      PIPE_DEPTH=16;
      PHYS_REGS=90;

      //overide incoming defaults
      IQ_WIDTH=8;
      ROB_SIZE=80;
      PEAK_ISSUE_WIDTH=ISSUE_WIDTH+2;

    } else {
      N_ALUS=std::min(std::max(1,ISSUE_WIDTH*3/4),6);
      if(ISSUE_WIDTH==2) {
        N_ALUS=2;
      }
      N_FPU=std::min(std::max(1,ISSUE_WIDTH/2),4);
      RW_PORTS=std::min(std::max(1,ISSUE_WIDTH/2),2);
      N_MUL=std::min(std::max(1,ISSUE_WIDTH/2),2);

      IBUF_SIZE=32;
      PIPE_DEPTH=20;
      PHYS_REGS=256;
      PEAK_ISSUE_WIDTH=ISSUE_WIDTH+2;

    }

    //IBUF_SIZE=FETCH_TO_DISPATCH_STAGES * ISSUE_WIDTH;
  }

  bool _supress_errors=false;
  //Functions to surpress and enable errors
  virtual void supress_errors(bool val) {
    _supress_errors=val;
  }


  //Public function which inserts functions from the trace
  virtual void insert_inst(const CP_NodeDiskImage &img,
                   uint64_t index, Op* op) {

    InstPtr sh_inst = createInst(img,index,op);
    getCPDG()->addInst(sh_inst,index);
    addDeps(sh_inst,op);
    pushPipe(sh_inst);
    inserted(sh_inst);
  }


  virtual void pushPipe(std::shared_ptr<Inst_t>& sh_inst) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(depInst) {
      uint64_t commitCycle = depInst->cycleOfStage(Inst_t::Commit);
      uint64_t fetchCycle = sh_inst->cycleOfStage(Inst_t::Fetch);
      int64_t diff = fetchCycle - commitCycle;
      if(diff > 0) {
        nonPipelineCycles+=diff;
      }
    }
    getCPDG()->pushPipe(sh_inst);
    rob_head_at_dispatch++;
  }

  virtual dep_graph_t<Inst_t,T,E> * getCPDG() = 0;

  virtual uint64_t numCycles() {
    return getCPDG()->getMaxCycles();
  }
 

  virtual uint64_t finish() {
    getCPDG()->finish(maxIndex);
    uint64_t final_cycle = getCPDG()->getMaxCycles();
    activityMap.insert(final_cycle);
    activityMap.insert(final_cycle+1);
    std::cout << _name << " has finished!\n";
    std::cout << _name << " cleanup until cycle: " << final_cycle+3000 << "\n";
    cleanUp(final_cycle+3000);
    return final_cycle;
  }

  virtual void printEdgeDep(Inst_t& inst, int ind,
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

    outs() << " lq:" << lqSize();
    outs() << " sq:" << sqSize();

    outs() << "\n";
  }


protected:

  int lqSize() {
     return (lq_head_at_dispatch<=LQind) ? (LQind-lq_head_at_dispatch):
      (LQind-lq_head_at_dispatch+32);
  }

  int sqSize() {
    return (sq_head_at_dispatch<=SQind) ? (SQind-sq_head_at_dispatch):
      (SQind-sq_head_at_dispatch+32);
  }


  //Code to map instructions to ops, and back
  std::unordered_map<Op *, BaseInstPtr> _op2InstPtr;
  //std::map<Inst_t *, Op*> _inst2Op;

  virtual InstPtr createInst(const CP_NodeDiskImage &img, 
                             uint64_t index, Op *op)
  {
    InstPtr ret = InstPtr(new Inst_t(img, index, op));
    if (op) {
      keepTrackOfInstOpMap(ret, op);
    }
    return ret;
  }

  /*
   * This is never called!  (so Tony commented it)
  void remove_instr(dg_inst_base<T, E> *p) {
    Inst_t *ptr = dynamic_cast<Inst_t*>(p);
    assert(ptr);
    _inst2Op.erase(ptr);
  }*/

  void createDummy(const CP_NodeDiskImage &img,uint64_t index, Op *op, 
      int dtype = dg_inst_dummy<T,E>::DUMMY_MOVE) {
    std::shared_ptr<dg_inst_dummy<T,E>> dummy_inst
                = std::make_shared<dg_inst_dummy<T,E>>(img,index,op,dtype); 
    keepTrackOfInstOpMap(dummy_inst,op);
    getCPDG()->addInst(dummy_inst,index);
  }



  //puts the correct instruction here
  //returns true if there is an error
  dg_inst_base<T,E>* fixDummyInstruction(dg_inst_base<T,E>* depInst, 
                                         bool& out_of_bounds, bool& error) {
    if(depInst->isDummy()) {
      auto dummy_inst = dynamic_cast<dg_inst_dummy<T,E>*>(depInst);
      unsigned dummy_prod=0;

      if(dummy_inst->_dtype == dg_inst_dummy<T,E>::DUMMY_STACK_SLOT) { 
        //dummy through memory
        if(dummy_inst->_isload) {
          error |= dummy_inst->getMemProd(dummy_prod);
        } else {
          error |= dummy_inst->getDataProdOfStore(dummy_prod);
        }
      } else if (dummy_inst->_dtype == dg_inst_dummy<T,E>::DUMMY_MOVE) { 
        //dummy through data
        error |= dummy_inst->getProd(dummy_prod);
      }

      if (dummy_prod ==0 ||
          dummy_prod >= dummy_inst->index() || 
          !getCPDG()->hasIdx(dummy_inst->index()-dummy_prod)) {
        out_of_bounds|=true;
        return NULL;
      }

      dg_inst_base<T,E>* retInst = &(getCPDG()->queryNodes(dummy_inst->index()-dummy_prod));
      return fixDummyInstruction(retInst,out_of_bounds,error);
    }
    return depInst;
  }


  virtual void keepTrackOfInstOpMap(BaseInstPtr ret, Op *op) {
    _op2InstPtr[op] = ret;
    //_inst2Op[ret.get()] = op;
  }

  virtual Op* getOpForInst(Inst_t &n, bool allowNull = false) {
    /* //old version
     * auto I2Op = _inst2Op.find(&n);
    if (I2Op != _inst2Op.end())
      return I2Op->second;
    if (!allowNull) {
      if (n.hasDisasm())
        return 0; // Fake instructions ....
      assert(0 && "inst2Op map does not have inst??");
    }
    */
    if (n._op!=NULL) {
      return n._op; 
    }

    if (!allowNull) {
      if (n.hasDisasm()) {
        return 0; // Fake instructions ....
      }
      assert(0 &&  "inst2Op map does not have inst??");
    }
    return 0;
  }

  virtual BaseInstPtr getInstForOp(Op *op) {
    auto Op2I = _op2InstPtr.find(op);
    if (Op2I != _op2InstPtr.end())
      return Op2I->second;
    return 0;
  }

  //function to add dependences onto uarch event nodes -- for the pipeline
  virtual void addDeps(std::shared_ptr<Inst_t>& inst, Op* op = NULL) {
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

  typedef std::vector<std::shared_ptr<BaseInst_t>> ResQueue;  
  ResQueue SQ, LQ; 
  unsigned SQind;
  unsigned LQind;
 
  typedef std::map<uint64_t,std::vector<Inst_t*>> SingleCycleRes;
  SingleCycleRes wbBusRes;
  SingleCycleRes issueRes;

  typedef std::map<uint64_t,int> FuUsageMap;
  typedef std::map<uint64_t,FuUsageMap> FuUsage;
 
  std::map<uint64_t,std::set<uint64_t>> MSHRUseMap;
  std::map<uint64_t,std::shared_ptr<BaseInst_t>> MSHRResp;

  typedef typename std::map<uint64_t,std::shared_ptr<BaseInst_t>> NodeRespMap;
  typedef typename std::map<uint64_t,NodeRespMap> NodeResp;

  std::map<uint64_t,uint64_t> minAvail; //res->min avail time
  FuUsage fuUsage;
  NodeResp nodeResp; 

  typedef std::set<uint64_t> ActivityMap;
  ActivityMap activityMap;
  
  typedef std::unordered_map<Op*,std::set<Op*>> MemDepSet;
  MemDepSet memDepSet;

  int rob_head_at_dispatch;
  int prev_rob_head_at_dispatch;
  int prev_squash_penalty;
  float avg_rob_head;
  float rob_growth_rate;

  int lq_head_at_dispatch;
  int sq_head_at_dispatch;

  int pipe_index;
  uint16_t lq_size_at_dispatch[PSIZE];
  uint16_t sq_size_at_dispatch[PSIZE];



  void cleanup() {
    getCPDG()->cleanup();
  }

  uint64_t maxIndex;
  uint64_t _curCycle;
  uint64_t _lastCleaned=0;
  uint64_t _latestCleaned=0;

  virtual void cleanUp(uint64_t curCycle) {
    _lastCleaned=curCycle;
    _latestCleaned=std::max(curCycle,_latestCleaned);

    //delete irrelevent 
    typename SingleCycleRes::iterator upperbound_sc;
    upperbound_sc = wbBusRes.upper_bound(curCycle);
    wbBusRes.erase(wbBusRes.begin(),upperbound_sc); 
    upperbound_sc = issueRes.upper_bound(curCycle);
    issueRes.erase(issueRes.begin(),upperbound_sc); 

    //summing up idle cycles
    uint64_t prevCycle=0;
    for(auto I=activityMap.begin(),EE=activityMap.end();I!=EE;) {
      uint64_t cycle=*I;
      if(prevCycle!=0 && cycle-prevCycle>10) {
        idleCycles+=(cycle-prevCycle-10);
      }
      if (cycle + 100  < curCycle && activityMap.size() > 2) {
        I = activityMap.erase(I);
        prevCycle=cycle;
      } else {
        break;
      }
    }


    //for funcUnitUsage
    for(auto &pair : fuUsage) {
      auto& fuUseMap = pair.second;
      /*
      for(FuUsageMap::iterator i=++fuUseMap.begin(),e=fuUseMap.end();i!=e;) {
        uint64_t cycle = i->first;
        assert(cycle!=0);
        if (cycle + MAX_RES_DURATION  < curCycle) {
          i = fuUseMap.erase(i);
        } else {
          break;
        }
      }*/

      //delete irrelevent
      if(fuUseMap.begin()->first < curCycle) {
        //debug MSHRUse Deletion
        //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
        auto upperUse = --fuUseMap.upper_bound(curCycle);
        auto firstUse = fuUseMap.begin();
        if(upperUse->first > firstUse->first) {
          fuUseMap.erase(firstUse,upperUse); 
        }
      }
    }




    //DEBUG MSHR Usage
/*    for(auto i=MSHRUseMap.begin(),e=MSHRUseMap.end();i!=e;) {
      uint64_t cycle = i->first;

      if(cycle > 10000000) {
        break;
      }
      std::cerr << i->first << " " << i->second.size() << ", ";
      ++i;
    }
    std::cerr << "(" << MSHRUseMap.size() << ")\n";*/


    //delete irrelevent
    if(MSHRUseMap.begin()->first < curCycle) {
      //debug MSHRUse Deletion
      //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
      auto upperMSHRUse = --MSHRUseMap.upper_bound(curCycle);
      auto firstMSHRUse = MSHRUseMap.begin();
      if(upperMSHRUse->first > firstMSHRUse->first) {
        MSHRUseMap.erase(firstMSHRUse,upperMSHRUse); 
      }
    }

    //delete irrelevent 
    auto upperMSHRResp = MSHRResp.upper_bound(curCycle-200);
    auto firstMSHRResp = MSHRResp.begin();
    if(upperMSHRResp->first > firstMSHRResp->first) {
      MSHRResp.erase(firstMSHRResp,upperMSHRResp); 
    }


    for(typename NodeResp::iterator i=nodeResp.begin(),e=nodeResp.end();i!=e;++i) {
      NodeRespMap& respMap = i->second;

      /*
      for(typename NodeRespMap::iterator i=respMap.begin(),e=respMap.end();i!=e;) {
        uint64_t cycle = i->first;

        if (cycle + MAX_RES_DURATION  < curCycle) {
          i = respMap.erase(i);
        } else {
          break;
        }
      }*/

      //delete irrelevent
      if(respMap.begin()->first < curCycle) {
        //debug MSHRUse Deletion
        //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
        auto upperUse = --respMap.upper_bound(curCycle);
        auto firstUse = respMap.begin();
        if(upperUse->first > firstUse->first) {
          respMap.erase(firstUse,upperUse); 
        }
      }

    }

  }

  virtual void insertLSQ(std::shared_ptr<BaseInst_t> inst) {
    //Update Queues: IQ, LQ, SQ
    if (inst->_isload) {
      LQ[LQind]=inst;
      LQind=(LQind+1)%LQ_SIZE;
    }
    if (inst->_isstore) {
      SQ[SQind]=inst;
      SQind=(SQind+1)%SQ_SIZE;
    }
  }


  virtual void inserted(std::shared_ptr<Inst_t>& inst) {
    maxIndex = inst->index();
    _curCycle = inst->cycleOfStage(Inst_t::Fetch);

    insertLSQ(inst);

    //add to activity
    for(int i = 0; i < Inst_t::NumStages -1 + inst->_isstore; ++i) {
      uint64_t cyc = inst->cycleOfStage(i);
      activityMap.insert(cyc);
      
      if(i == Inst_t::Fetch) {
        activityMap.insert(cyc+3);
      }
      if(i == Inst_t::Complete) {
        activityMap.insert(cyc+2);
      }


    }
    cleanUp(_curCycle);
  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addMSHRResource(uint64_t min_cycle, uint32_t duration, 
                     std::shared_ptr<BaseInst_t> cpnode,
                     uint64_t eff_addr,
                     int re_check_frequency,
                     int& rechecks,
                     uint64_t& extraLat,
                     bool not_coalescable=false) { 

    int maxUnits = Prof::get().dcache_mshrs;
    rechecks=0; 

    uint64_t cur_cycle=min_cycle;
    std::map<uint64_t,std::set<uint64_t>>::iterator
                       cur_cycle_iter,next_cycle_iter,last_cycle_iter;
  
    assert(MSHRUseMap.begin()->first < min_cycle);
 
    cur_cycle_iter = --MSHRUseMap.upper_bound(min_cycle);

    uint64_t filter_addr=(((uint64_t)-1)^(Prof::get().cache_line_size-1));
    uint64_t addr = eff_addr & filter_addr;

    //std::cout << addr << ", " << filter_addr << " (" << Prof::get().cache_line_size << "," << cpnode->_eff_addr << ")\n";


    //keep going until we find a spot .. this is gauranteed
    while(true) {
      assert(cur_cycle_iter->second.size() <= maxUnits);
      assert(cur_cycle_iter->first < ((uint64_t)-20000));

      if(cur_cycle_iter->second.size() < maxUnits && //max means cache-blocked
         cur_cycle_iter->second.count(addr)) {
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
          assert(cur_cycle_iter->first < ((uint64_t)-20000));
          cur_cycle=cur_cycle_iter->first;
        } else {
          cur_cycle+=re_check_frequency;
          cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
          assert(cur_cycle_iter->first < ((uint64_t)-20000));
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
            assert(cur_cycle_iter->first < ((uint64_t)-20000));
          } else {
            cur_cycle+=re_check_frequency;
            cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
            rechecks++;
            assert(cur_cycle_iter->first < ((uint64_t)-20000));
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

  void checkResourceEmpty(FuUsageMap& fuUse) {
    if(fuUse.size() == 0) {
      for(auto &fuUse : fuUsage) {
        fuUse.second[0]=0; //begining usage is 0
        fuUse.second[(uint64_t)-10000]=0; //end usage is 0 too (1000 to prevent overflow)
      }
    }
  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addResource(uint64_t resource_id, uint64_t min_cycle_in, 
                     uint32_t duration, int maxUnits, 
                     std::shared_ptr<BaseInst_t> cpnode) {
    FuUsageMap& fuUseMap = fuUsage[resource_id];
    uint64_t minAvailRes = minAvail[resource_id]; //todo: not implemented yet
    NodeRespMap& nodeRespMap = nodeResp[resource_id];

    checkResourceEmpty(fuUseMap);
   
    uint64_t min_cycle = std::max(min_cycle_in,minAvailRes); //TODO: is this every useful?
    assert(fuUseMap.begin()->first <= min_cycle);


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

        
        FuUsageMap::iterator delete_cycle_iter;
        bool delete_me=false;

        //now do something cool!
        if(cur_cycle_iter->second == maxUnits) {
          //first, we can delete nodeResp, cause noone will use this for sure!
          nodeRespMap.erase(cur_cycle);

          if(cur_cycle_iter != fuUseMap.begin()) {
            auto prev_cycle_iter = std::prev(cur_cycle_iter);
            if(prev_cycle_iter->second == maxUnits) {
               //prev_cycle_iter!=fuUseMap.begin()) {
               //auto two_prev_cycle_iter = std::prev(prev_cycle_iter);
               delete_cycle_iter=cur_cycle_iter;
               delete_me=true;
            }
          }
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
        
        if(delete_me) {
          fuUseMap.erase(delete_cycle_iter);
        }

        auto respIter = nodeRespMap.find(cur_cycle);
        nodeRespMap[cur_cycle+duration]=cpnode;

        if(respIter == nodeRespMap.end() || min_cycle == cur_cycle) {
          return NULL;
        } else {
          return respIter->second.get();
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
    checkSerializing(inst);

    //-----FETCH ENERGY-------
    //Need to get ICACHE accesses somehow
    if(inst._icache_lat>0) {
      icache_read_accesses++;
    }
    if(inst._icache_lat>20) {
      icache_read_misses++;
    }



    if(_isInOrder) {
//      checkBranchPredict(inst,op); //just control dependence
    } else {
      //checkIQStalls(inst);
      checkLSQStalls(inst);
    }
  }


  //This eliminates LSQ entries which can be freed before "cycle"
  virtual void cleanLSQEntries(uint64_t cycle) {
    //HANDLE LSQ HEADS
    //Loads leave at complete
    do {
      auto depInst = LQ[lq_head_at_dispatch];
      if (depInst.get() == 0) {
        break;
      }
      //TODO should this be complete or commit?
      if (depInst->cycleOfStage(depInst->memComplete()) < cycle ) {
        LQ[lq_head_at_dispatch] = 0;
        lq_head_at_dispatch += 1;
        lq_head_at_dispatch %= LQ_SIZE;
      } else {
        break;
      }
    } while (lq_head_at_dispatch != LQind);

    do {
      auto depInst = SQ[sq_head_at_dispatch];
      if(depInst.get() == 0) {
        break;
      }
      if (depInst->cycleOfStage(depInst->memComplete()) < cycle ) {
        SQ[sq_head_at_dispatch] = 0;
        sq_head_at_dispatch += 1;
        sq_head_at_dispatch %= SQ_SIZE;
      } else {
        break;
      }
    } while (sq_head_at_dispatch != SQind);
    lq_size_at_dispatch[getCPDG()->getPipeLoc()%PSIZE] = lqSize();
    sq_size_at_dispatch[getCPDG()->getPipeLoc()%PSIZE] = sqSize();
  }


  virtual void setDispatchCycle(Inst_t &inst) {
    checkFD(inst);
    checkDD(inst);
    checkDBW(inst);

    if(_isInOrder) {
      //nothing
      // TODO: This is here because with out it, fetch and dispatch get
      // decoupled from the rest of the nodes, causing memory explosion
      // b/c clean dosen't get called
      // ideally, we should 
      checkIOInFlight(inst); //Finite Rob Size
    } else {
      checkROBSize(inst); //Finite Rob Size
      checkIQStalls(inst);
      checkLSQSize(inst[Inst_t::Dispatch],inst._isload,inst._isstore);

      // ------ ENERGY EVENTS -------
      if(inst._floating) {
        iw_fwrites++;
      } else {
        iw_writes++;
      }
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

    cleanLSQEntries(dispatch_cycle);
  }

  virtual void setReadyCycle(Inst_t &inst) {
    checkDR(inst);
    checkNonSpeculative(inst);
    checkDataDep(inst);
    if(_isInOrder) {
      //checkExecutePipeStalls(inst);
    } else {
      //no other barriers
    }

    /*
    // num src regs is wrong... eventually fix this in gem5
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
      checkPipelength(*inst);
    } else {
      //TODO: below two things should be merged
      checkIssueWidth(*inst);
    }

    checkFuncUnits(inst);
    if(inst->_isload) {
      //loads acquire MSHR at execute
      checkNumMSHRs(inst,false);
    }

    if(!_isInOrder) {
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
    } /*else {
      checkInorderComplete
    }*/

    //Energy Events
    if(op && op->numUses()>0) {
      if(inst._floating) {
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
    } else {
      checkEC(inst);
    }
    checkCC(inst);
    checkCBW(inst);
 
    getCPDG()->commitNode(inst.index());

    if(!_isInOrder) {
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
          int newReads=curCycle-prevCycle;

          assert(prevCycle!=0);

/*          if(newReads > 10) {
            std::cout << newReads << "reads: " << prevCycle << " " << curCycle << "\n";
          }*/
          rob_reads+=newReads;
        }
      }
    }
    setEnergyStatsPerInst(inst);
  }

  virtual void setEnergyStatsPerInst(Inst_t& inst)
  {
    ++committed_insts;
    committed_int_insts += !inst._floating;
    committed_fp_insts += inst._floating;
    committed_branch_insts += inst._isctrl;
    committed_load_insts += inst._isload;
    committed_store_insts += inst._isstore;
    func_calls += inst._iscall;
  }

  virtual void setWritebackCycle(std::shared_ptr<Inst_t>& inst) {
    //if(!_isInOrder) {
      if(inst->_isstore) {
        int st_lat=stLat(inst->_st_lat,inst->_cache_prod,
                         inst->_true_cache_prod,inst->isAccelerated);
        getCPDG()->insert_edge(*inst, Inst_t::Commit,
                               *inst, Inst_t::Writeback, 2+st_lat, E_WB);
        checkNumMSHRs(inst,true);
        checkPP(*inst);
      }
    //}
  }

  virtual bool predictTaken(Inst_t& dep_n, Inst_t &n) {
    bool predict_taken = false;
    if(dep_n._pc == n._pc) { //this probably doesn't happen much!
      predict_taken = dep_n._upc + 1 != n._upc;
    } else {
      predict_taken |= n._pc < dep_n._pc;
      predict_taken |= n._pc > dep_n._pc+3;//x86 jumps are 2-3 bytes, I think
    }

    return predict_taken;
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
      bool predict_taken=predictTaken(*depInst,n);
      if(predict_taken && lat==0/* TODO: check this*/) {
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
                             n, Inst_t::Fetch, 1,E_SER);
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
    //TODO: Do we need this?
    //Inst_t* prevInst = getCPDG()->peekPipe(-1);
    //if(prevInst && !prevInst->_isctrl) {
      //length+=n._icache_lat; //TODO: Which is correct, this one seems to conservative sometimes
      //length=std::max(length,n._icache_lat);  //this steems too optimistic sometimes
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
            -(FETCH_TO_DISPATCH_STAGES)*FETCH_WIDTH);
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
         bool predict_taken=!predictTaken(*depInst,n);
     
        int adjusted_squash_cycles=2;
        int insts_to_squash=SQUASH_WIDTH;
        if(!_isInOrder){
          adjusted_squash_cycles=prev_squash_penalty-2*predict_taken;
          insts_to_squash=(prev_squash_penalty-2)*SQUASH_WIDTH;
        }
        squashed_insts+=insts_to_squash;


        getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                               n, Inst_t::Fetch,
                               //n._icache_lat + prev_squash_penalty 
                               std::max((int)n._icache_lat, adjusted_squash_cycles)
                               + 1,E_CM); //two to commit, 1 to 

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
    if (InstQueue.size() < (unsigned)IQ_WIDTH) {
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



  int numInstsNotInRob=0;
  //This should be called after FBW
  virtual Inst_t &checkLSQStalls(Inst_t &n) {
     Inst_t* prevInst = getCPDG()->peekPipe(-1); 
     if(!prevInst) {
       return n;
     }

     uint64_t cycleOfFetch = n.cycleOfStage(Inst_t::Fetch);
     if(prevInst->cycleOfStage(Inst_t::Fetch) == cycleOfFetch) {
       return n; //only check this on first fetch of new cycle
     }
   

     int pipeLoc = getCPDG()->getPipeLoc();

     //Caculate number of insts not in rob at time of fetch
     int i = 1;
     for(; i < PSIZE; ++i) {
       Inst_t* depInst = getCPDG()->peekPipe(-i); 
       if(!depInst) {
         return n;
       }
       if(depInst->cycleOfStage(Inst_t::Dispatch) <= cycleOfFetch) {
         break;
       } 
     }
 
    numInstsNotInRob=i;
    int blocking_dispatch=i;

    if(numInstsNotInRob + lq_size_at_dispatch[((pipeLoc-blocking_dispatch))%PSIZE]
         > LQ_SIZE) {
      Inst_t* depInst = getCPDG()->peekPipe(-blocking_dispatch); 
      getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                              n, Inst_t::Fetch, 2, E_LQTF); //3 cycles emperical
    }


    
    
 
/*
  //Getting rid of this because i don't think lsq should stall fetch --
 //it should work like a rob for just loads, where it stalls dispatch

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
    */
    return n;
    
  }


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
  virtual Inst_t &checkIOInFlight(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-36);  //TODO: parameterize
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Dispatch, 1,E_ROB);
    return n;
  }


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
  //This function delays the event until there is an open spot in the LSQ
  virtual void checkLSQSize(T& event,bool isload, bool isstore) {
    if (isload) {
      if(LQ[LQind]) {
        getCPDG()->insert_edge(*LQ[LQind], LQ[LQind]->memComplete(),
                               event, 1+2,E_LSQ); //TODO: complete or commit
      }
    }
    if (isstore) {
      if(SQ[SQind]) {
        getCPDG()->insert_edge(*SQ[SQind], SQ[SQind]->memComplete(),
                               event, 1,E_LSQ);
      }
    }
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
        getCPDG()->insert_edge(*SQ[ind], SQ[ind]->memComplete(),
                               n, Inst_t::Ready, 1,E_NSpc);
      }
      Inst_t* depInst = getCPDG()->peekPipe(-1); 
      if(depInst) {
        getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                                 n, Inst_t::Ready, 2,E_NSpc);
      }

    }
    return n;
  }
 

  //Data dependence
  virtual Inst_t &checkDataDep(Inst_t &n) {
    //register dependence
    checkRegisterDependence(n);

    //memory dependence
    if(n._isload || n._isstore) {
      checkMemoryDependence(n);
      insert_mem_predicted_edges(n); 
    }
    return n;
  }

  // Register Data Dependence
  std::unordered_set<Op*> ops_with_missing_deps;

  virtual Inst_t &checkRegisterDependence(Inst_t &n) {
    const int NumProducer = MAX_SRC_REGS; // FIXME: X86 specific
    for (int i = 0; i < NumProducer; ++i) {
      unsigned prod = n._prod[i];
      if (prod <= 0 || prod >= n.index()) {
        continue;
      }

      if(!getCPDG()->hasIdx(n.index()-prod)) {
        if(!_supress_errors) {
          if(ops_with_missing_deps.count(n._op)==0) {
            ops_with_missing_deps.insert(n._op);
            std::cerr << "WARNING: OP:" << n._op->id() 
                      << ", func:" << n._op->func()->nice_name()
                      << " is missing an op (-" << prod << ")\n";
          }
        }
        continue;
      }
      BaseInst_t& depInst =
        getCPDG()->queryNodes(n.index()-prod);

      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             n, Inst_t::Ready, 0, E_RDep);
    }
    return n;
  }


  // Memory Dependence
  virtual void checkMemoryDependence(Inst_t &n) {
    if ( (n._mem_prod > 0 && n._mem_prod < n.index() )  ) {
      BaseInst_t& dep_inst=getCPDG()->queryNodes(n.index()-n._mem_prod);
      addTrueMemDep(dep_inst,n);
      //if(dep_inst.isPipelineInst()) {
      //  Inst_t& prev_node = static_cast<Inst_t&>(dep_inst);
      //  insert_mem_dep_edge(prev_node, n);
      //}
    }
  }


  //Instructions dep_n and n are truely dependent
  virtual void addTrueMemDep(BaseInst_t& dep_n, BaseInst_t& n) {
    //This should only get seen as a mem dep, if the dependent instruction is 
    //actually not complete "yet"
    if(dep_n.cycleOfStage(dep_n.eventComplete()) > 
           n.cycleOfStage(n.eventReady())) {

      if(n._op!=NULL && dep_n._op!=NULL) {
        //add this to the memory predictor list
        memDepSet[n._op].insert(dep_n._op);
      } else {
        //We better insert the mem-dep now, because it didn't get added to memDepSet
        //This is sort of an error, should probably fix any cases like this.
        insert_mem_dep_edge(dep_n,n);
      }
    }
  }

  int numMemChecks=0;
  virtual void checkClearMemDep() {
    if(++numMemChecks==250000) {
      memDepSet.clear();
    }
  }

  //Inserts edges predicted by memory dependence predictor
  virtual void insert_mem_predicted_edges(BaseInst_t& n) {
      checkClearMemDep();
      //insert edges 
      for(auto i=memDepSet[n._op].begin(), e=memDepSet[n._op].end(); i!=e;++i) {
        Op* dep_op = *i;
        BaseInstPtr sh_inst = getInstForOp(dep_op);
        if(sh_inst) {
          insert_mem_dep_edge(*sh_inst,n);
        }
      } 
  }


  virtual void insert_mem_dep_edge(BaseInst_t &prev_node, BaseInst_t &n)
  {
    if (prev_node._isstore && n._isload) {
      // RAW true dependence
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventReady(), 0, E_MDep);
    } else if (prev_node._isstore && n._isstore) {
      // WAW dependence (output-dep)
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventComplete(), 0, E_MDep);
    } else if (prev_node._isload && n._isstore) {
      // WAR dependence (load-store)
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventReady(), 0, E_MDep);
    }
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

  virtual unsigned getNumFUAvailable(Inst_t &n) {
    return getNumFUAvailable(n._opclass);
  }

  virtual unsigned getNumFUAvailable(uint64_t opclass) {
    if(opclass > 50) {
      return 1;
    }

    switch(opclass) {
    case 0: //No_OpClass
      return ROB_SIZE+IQ_WIDTH;
    case 1: //IntALU
      return N_ALUS;

    case 2: //IntMult
    case 3: //IntDiv
      return N_MUL;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return N_FPU;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return N_MUL_FPU;
    case 30: //MemRead
    case 31: //MemWrite
      return RW_PORTS;

    default:
      return 4; //hopefully this never happens
    }
    return 4; //and this!
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

  static unsigned getFUOpLatency(int opclass) {
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

  void countOpclassEnergy(int opclass) {
    switch(opclass) {

    case 0: //No_OpClass
      return;

    case 1: //IntALU
      int_ops++;
      return;
    case 2: //IntMult
      int_ops++;
      mult_ops++;
      return;
    case 3: //IntDiv
      int_ops++;
      return;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      fp_ops++;
      return;

    default:
      return;
    }
    assert(0);
    return;
  }



  //KNOWN_HOLE: SSE Issue Latency Missing
  virtual unsigned getFUIssueLatency(Inst_t &n) {
    return getFUIssueLatency(n._opclass);
  }

  //Check Functional Units to see if they are full
  virtual void checkFuncUnits(std::shared_ptr<Inst_t>& inst) {
    countOpclassEnergy(inst->_opclass);

    // no func units if load, store, or if it has no opclass
    //if (n._isload || n._isstore || n._opclass==0)
    if (inst->_opclass==0) {
      return;
    }
    
    int fuIndex = fuPoolIdx(inst->_opclass);
    int maxUnits = getNumFUAvailable(inst->_opclass); //opclass
    Inst_t* min_node = static_cast<Inst_t*>(
         addResource(fuIndex, inst->cycleOfStage(Inst_t::Execute), 
                                   getFUIssueLatency(*inst), maxUnits, inst));

    if (min_node) {
      getCPDG()->insert_edge(*min_node, Inst_t::Execute,
                        *inst, Inst_t::Execute, getFUIssueLatency(*min_node),E_FU);
    }
  }

  virtual bool l1dTiming(bool isload, bool isstore, int ex_lat, int st_lat,
                 int& mlat, int& reqDelayT, int& respDelayT, int& mshrT){
    assert(isload || isstore);
    if(isload) {
      mlat = ex_lat;
    } else {
      mlat = st_lat;
    }
    if(mlat <= Prof::get().dcache_hit_latency + 
               Prof::get().dcache_response_latency+3) {
      //We don't need an MSHR for non-missing loads/stores
      return false;
    }

    reqDelayT =Prof::get().dcache_hit_latency; // # cyc before MSHR
    respDelayT=Prof::get().dcache_response_latency; // # cyc MSHR release
    mshrT = mlat - reqDelayT - respDelayT;  // actual MSHR time
    assert(mshrT>0);
    return true;
  }


  //check MSHRs to see if they are full
  virtual void checkNumMSHRs(std::shared_ptr<Inst_t>& n, bool store) {
    int ep_lat=epLat(n->_ex_lat,n->_opclass,n->_isload,n->_isstore,
                  n->_cache_prod,n->_true_cache_prod,n->isAccelerated);
    int st_lat=stLat(n->_st_lat,n->_cache_prod,n->_true_cache_prod,
                     n->isAccelerated);


    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 

    int rechecks=0;
    uint64_t extraLat=0;
    if(!store) { //if load

      //have to wait this long before each event
      
#if 0
      int insts_to_squash=instsToSquash();
      int squash_cycles = squashCycles(insts_to_squash)/2;
      int recheck_cycles = squash_cycles;


      if(recheck_cycles < 7) {
        recheck_cycles=7;
      }

      if(recheck_cycles > 9) {
        recheck_cycles=9;
      }

#else
      //int insts_to_squash=FETCH_TO_DISPATCH_STAGES*FETCH_WIDTH;
      //int recheck_cycles=FETCH_TO_DISPATCH_STAGES;
      int insts_to_squash=13;
      int recheck_cycles=9;

#endif

      assert(recheck_cycles>=1); 
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
        
        n->saveInst(sh_dummy_inst);

        //pushPipe(sh_dummy_inst);
        //assert(min_node); 
        if(!min_node) {
        }

        n->reset_inst();
        setFetchCycle(*n);
        /*getCPDG()->insert_edge(*min_node, min_node->memComplete(),
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);*/

        getCPDG()->insert_edge(*dummy_inst, Inst_t::Execute,
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);

        setDispatchCycle(*n);
        setReadyCycle(*n);
        checkRE(*n);

        //std::cout << insts_to_squash << "," << rechecks << "\n";
        //squashed_insts+=rechecks*(insts_to_squash*0.8);
        squashed_insts+=insts_to_squash+(rechecks-1)*insts_to_squash/1.5;

        //we need to make sure that the processor stays active during squash
        uint64_t i=sh_dummy_inst->cycleOfStage(Inst_t::Execute);
        for(;i<n->cycleOfStage(Inst_t::Execute);++i) {
          activityMap.insert(i);
        }

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
    int len=0;
    if(n._nonSpec) {
      len+=1;
    }
    getCPDG()->insert_edge(n, Inst_t::Ready,
                           n, Inst_t::Execute, len,E_RE);
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
    getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                          n, Inst_t::Execute, 1,E_IBW);
    return n;
  }

  //for inorder only, 
  virtual Inst_t &checkPipelength(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Execute, -INORDER_EX_DEPTH,E_EPip);
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

#if 0
   //this was incredibly stupid ... i am ashamed
   
  //make sure current instructions are done executing before getting the next
  //This is for inorder procs...
  virtual Inst_t &checkExecutePipeStalls(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
  
    int lat=epLat(depInst->_ex_lat,depInst->_opclass,depInst->_isload,
                  depInst->_isstore,depInst->_cache_prod,
                  depInst->_true_cache_prod,
                  true,INORDER_EX_DEPTH);

    getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                           n, Inst_t::Ready, lat-INORDER_EX_DEPTH,E_EPip);

/*
    if(depInst->isMem() && lat > 8) { //
      getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                              n, Inst_t::Ready, 0,E_EPip);
    }*/

    return n;
  }
#endif

  int stLat(int st_lat, bool cache_prod, 
            bool true_cache_prod, bool isAccelerator) {
    int lat = st_lat;
 /*
    if(cache_prod && true_cache_prod) {
      lat = Prof::get().dcache_hit_latency + 
            Prof::get().dcache_response_latency;
    }
 */
    if( (applyMaxLatToAccel && isAccelerator ) ||
        !applyMaxLatToAccel) {
      if(lat > _max_mem_lat) {
        lat = _max_mem_lat;
      }
    }
    return lat;
  }

  //logic to determine ep latency based on information in the trace
  int epLat(int ex_lat, int opclass, bool isload, bool isstore, 
            bool cache_prod, bool true_cache_prod, bool isAccelerator) {
    //memory instructions bear their memory latency here.  If we have a cache
    //producer, that means we should be in the cache, so drop the latency
    int lat;
    if(isload) {
      if(cache_prod && true_cache_prod) {
        lat = Prof::get().dcache_hit_latency + 
              Prof::get().dcache_response_latency;
      } else {
        lat = ex_lat;
      }
    } else {
      //don't want to use this dynamic latency
      //lat = n._ex_lat;
      lat = getFUOpLatency(opclass);
    }

    if( (applyMaxLatToAccel && isAccelerator ) ||
        !applyMaxLatToAccel) {
      //check if latencies are bigger than max
      if(isload || isstore) { //if it's a mem
        if(lat > _max_mem_lat) {
          lat = _max_mem_lat;
        }
      } else { //if it's an ex
        if(lat > _max_ex_lat) {
          lat = _max_ex_lat;
        }
      }
    }
    return lat;
  }

  //==========COMPLTE ==============
  //Complete After Execute
  virtual Inst_t &checkEP(Inst_t &n) {
    int lat = epLat(n._ex_lat,n._opclass,n._isload,
                    n._isstore,n._cache_prod,n._true_cache_prod,
                    n.isAccelerated);

    lat = n.adjustExecuteLatency(lat);
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

  std::unordered_map<Op*,int> dummies_dep;
  std::unordered_map<Op*,int> errors_dep;

  //Cache Line Producer
  virtual void checkPP(Inst_t &n) {
    //lets only do this for loads

    if(n._isload) {
      //This edge doesn't work...
      uint64_t cache_prod = n._cache_prod;

      if (cache_prod > 0 && cache_prod < n.index()) {
        if(!getCPDG()->hasIdx(n.index()-cache_prod)) {
          if(!errors_dep.count(n._op)) {
            std::cerr << "ERROR: Cache Prod MISSING!: ";
            if(n._op) {
              std::cerr << n._op->dotty_name();
            } else {
              std::cerr <<"no op->id()";
            }
            std::cerr << "\n";
          }
          errors_dep[n._op]++;
          return;
        }

        BaseInst_t& depInst = getCPDG()->queryNodes(n.index()-cache_prod);

        if(depInst.isDummy()) {
          if(!dummies_dep.count(depInst._op)) {
            std::cerr << "Dummy Dep: "  << n._op->dotty_name()
                      << " -> "   << depInst._op->dotty_name() << "\n";
          }
          dummies_dep[depInst._op]++;
          return;
        }

        if(depInst._isload) {
          getCPDG()->insert_edge(depInst, depInst.memComplete(),
                                 n, n.memComplete(), 1, E_PP);
        }
      }
    }
    return;
  }

  //For inorder, commit comes after execute, by 
  //
  virtual Inst_t &checkEC(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Execute,
                           n, Inst_t::Commit, INORDER_EX_DEPTH,E_EPip);
    return n;
  }


  //Commit follows complete
  virtual Inst_t &checkPC(Inst_t &n) {
    if(!_isInOrder) {
      getCPDG()->insert_edge(n, Inst_t::Complete,
                             n, Inst_t::Commit, 2,E_PC);  
    } else {
      getCPDG()->insert_edge(n, Inst_t::Complete,
                             n, Inst_t::Commit, 0,E_PC);  
    }
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
    if(!_isInOrder) {
      return 1 +  insts_to_squash/SQUASH_WIDTH 
               + (insts_to_squash%SQUASH_WIDTH!=0);  
    } else {
      return 0;
    }
  }

  virtual Inst_t &checkSquashPenalty(Inst_t &n) {
    //BR_MISS_PENALTY no longer used
    //Instead we calculate based on the rob size, how long it's going to
    //take to squash.  This is a guess, to some degree, b/c we don't
    //know what happened after the control instruction was executed,
    //but it might be close enough.
    if (n._ctrl_miss) {

        int insts_to_squash=instsToSquash();
        int squash_cycles = squashCycles(insts_to_squash); 
        

        //int squash_cycles = (rob_head_at_dispatch + 1- 12*rob_growth_rate)/SQUASH_WIDTH;

        if(squash_cycles<5) {
          squash_cycles=5;
        } /*else if (squash_cycles > (ROB_SIZE-10)/SQUASH_WIDTH) {
          squash_cycles = (ROB_SIZE-10)/SQUASH_WIDTH;
        }*/

        uint64_t rob_insert_cycle = n.cycleOfStage(Inst_t::Dispatch)-1;
        uint64_t complete_cycle = n.cycleOfStage(Inst_t::Complete);
        uint64_t max_cycles = ((complete_cycle-rob_insert_cycle)*ISSUE_WIDTH*.9)/SQUASH_WIDTH + 2;

        if(squash_cycles > max_cycles) {
          squash_cycles=max_cycles;
        }


        //add two extra cycles, because commit probably has to play catch up
        //because it can't commit while squashing
        getCPDG()->insert_edge(n, Inst_t::Complete,
                               n, Inst_t::Commit,squash_cycles+1,E_SQUA);

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
  virtual void setEnergyEvents(pugi::xml_document& doc, int nm) {
    //set the normal events based on the m5out/stats file
    CriticalPath::setEnergyEvents(doc,nm);

    uint64_t totalCycles=numCycles();
    uint64_t busyCycles=totalCycles-idleCycles;

    pugi::xml_node system_node = doc.child("component").find_child_by_attribute("name","system");
    pugi::xml_node core_node =
              system_node.find_child_by_attribute("name","core0");

    sa(system_node,"total_cycles",totalCycles);
    sa(system_node,"idle_cycles", idleCycles);
    sa(system_node,"busy_cycles",busyCycles);

    //std::cout << "squash: " << squashed_insts
    //          << "commit: " << committed_insts << "\n";

    //Modify relevent events to be what we predicted
    double squashRatio=0;
    if(committed_insts!=0) {
      squashRatio =(double)squashed_insts/(double)committed_insts;
    }
    if(!_isInOrder) {
      std::cout << "Squash Ratio for \"" << _name << "\" is " << squashRatio << "\n";
    }
    double highSpecFactor = 1.00+1.5*squashRatio;
    double specFactor = 1.00+squashRatio;
    double halfSpecFactor = 1.00+0.5*squashRatio;
    double fourthSpecFactor = 1.00+0.25*squashRatio;
    //double eigthSpecFactor = 1.00+0.125*squashRatio;
    double sixteenthSpecFactor = 1.00+0.0625*squashRatio;

    //uint64_t intOps=committed_int_insts-committed_load_insts-committed_store_insts;
    //uint64_t intOps=committed_int_insts;

    sa(core_node,"total_instructions",(uint64_t)(committed_insts*specFactor));
    sa(core_node,"int_instructions",(uint64_t)(committed_int_insts*specFactor));
    sa(core_node,"fp_instructions",(uint64_t)(committed_fp_insts*specFactor));
    sa(core_node,"branch_instructions",(uint64_t)(committed_branch_insts*highSpecFactor));
    sa(core_node,"branch_mispredictions",(uint64_t)(mispeculatedInstructions*sixteenthSpecFactor));
    sa(core_node,"load_instructions",(uint64_t)(committed_load_insts*fourthSpecFactor));
    sa(core_node,"store_instructions",(uint64_t)(committed_store_insts*fourthSpecFactor));

    sa(core_node,"committed_instructions", committed_insts);
    sa(core_node,"committed_int_instructions", committed_int_insts);
    sa(core_node,"committed_fp_instructions", committed_fp_insts);

    sa(core_node,"total_cycles",totalCycles);
    sa(core_node,"idle_cycles", idleCycles); //TODO: how to get this?
    sa(core_node,"busy_cycles",busyCycles);

    uint64_t calc_rob_reads=rob_reads;
    Inst_t* prevInst = getCPDG()->peekPipe(-1); 
    if(!prevInst) {
      calc_rob_reads+=totalCycles;
    } else {
      calc_rob_reads+=totalCycles-prevInst->cycleOfStage(Inst_t::Commit);
    }

    if(!_isInOrder) {
      if(idleCycles < calc_rob_reads) {
        sa(core_node,"ROB_reads",(uint64_t)(calc_rob_reads-idleCycles)*halfSpecFactor);
      } else {
        sa(core_node,"ROB_reads",0);
      }
    } else {
      sa(core_node,"ROB_reads",(uint64_t)0);
    } 

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

    sa(core_node,"ialu_accesses",(uint64_t)(int_ops*specFactor));
    sa(core_node,"fpu_accesses",(uint64_t)(fp_ops*specFactor));
    sa(core_node,"mul_accesses",(uint64_t)(mult_ops*specFactor));

    sa(core_node,"cdb_alu_accesses",(uint64_t)(int_ops*specFactor));
    sa(core_node,"cdb_fpu_accesses",(uint64_t)(fp_ops*specFactor));
    sa(core_node,"cdb_mul_accesses",(uint64_t)(mult_ops*specFactor));

    // ---------- icache --------------
    pugi::xml_node icache_node =
              core_node.find_child_by_attribute("name","icache");
    sa(icache_node,"read_accesses",icache_read_accesses);
    sa(icache_node,"read_misses",icache_read_misses);
    //sa(icache_node,"conflicts",Prof::get().icacheReplacements);
  }

  virtual void countAccelSGRegEnergy(Op* op, Subgraph* sg, std::set<Op*>& opset,
      uint64_t& fp_ops, uint64_t& mult_ops, uint64_t& int_ops,
      uint64_t& regfile_reads, uint64_t& regfile_freads,
      uint64_t& regfile_writes, uint64_t& regfile_fwrites) {

    if(op->isFloating()) {
      fp_ops++;
    } else if (op->opclass()==2) {
      int_ops++;
      mult_ops++;
    } else {
      int_ops++;
    }

    for(auto di = op->adj_d_begin(),de = op->adj_d_end();di!=de;++di) {
      Op* dop = *di;
      if(opset.count(dop) /*&& li->forwardDep(dop,op)*/) {
        //std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
        if(!sg->hasOp(dop)) {
          if(dop->isFloating()) {
            regfile_freads+=1;
           } else {
            regfile_reads+=1;
          }
        }
      }
    }
  
    for(auto ui = op->u_begin(),ue = op->u_end();ui!=ue;++ui) {
      Op* uop = *ui;
      if(opset.count(uop) /*&& li->forwardDep(op,uop)*/) {
        //std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
        if(!sg->hasOp(uop)) {
          if(uop->isFloating()) {
            regfile_fwrites+=1;
           } else {
            regfile_writes+=1;
          }
          break;
        }
      }
    }
  }


};


#endif

