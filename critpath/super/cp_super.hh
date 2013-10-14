#ifndef CP_SUPER
#define CP_SUPER

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

extern int TraceOutputs;

class SuperInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      Execute = 1,
      Complete = 2,
      NumStages
  };
private:
  T events[NumStages];

public:
 //std::shared_ptr<T>   endBB;
 //std::shared_ptr<T> startBB;

  virtual ~SuperInst() { 
  }

  //Things from CP_Node image
  uint16_t _opclass=0;
  bool _isload=0;
  bool _isstore=0;
  bool _isctrl=0;
  bool _ctrl_miss=0;
  uint16_t _icache_lat=0;
  uint16_t _prod[7]={0,0,0,0,0,0,0};
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint16_t _ex_lat=0;
  bool _serialBefore=0;
  bool _serialAfter=0;
  bool _nonSpec=0;
  uint16_t _st_lat=0;
  uint64_t _pc=0;
  uint16_t _upc=0;
  uint64_t _eff_addr;
  bool _floating=false;
  bool _iscall=false;


  SuperInst(const CP_NodeDiskImage &img, uint64_t index):
              dg_inst_base<T,E>(index){
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    _icache_lat=img._icache_lat;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _ex_lat=img._cc-img._ec;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._xc-img._wc;
    _pc=img._pc;
    _upc=img._upc;
    _floating=img._floating;
    _iscall=img._iscall;
  }

  SuperInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }
  virtual unsigned numStages() {
    return NumStages;
  }
  virtual uint64_t cycleOfStage(const unsigned i) {
    return events[i].cycle(); 
  }
  virtual unsigned eventComplete() {
    return Complete; 
  }
  virtual unsigned memComplete() {
    return Complete; 
  }

};


class cp_super : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  
  typedef dg_inst<T, E> Inst_t;

public:
  cp_super() : CP_DG_Builder<T,E>() {
  }

  virtual ~cp_super() {
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;
  uint64_t min_cycle=0;
  uint64_t max_cycle=0;

  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if(TraceOutputs) {
      dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);  
  
      out << index + Prof::get().skipInsts << ": ";
      out << inst.cycleOfStage(0) << " ";
      out << inst.cycleOfStage(1) << " ";

      CriticalPath::traceOut(index,img,op);
      out << "\n";
    }
  }

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) {
    SuperInst* inst = new SuperInst(img,index);
    std::shared_ptr<SuperInst> sh_inst(inst);
    getCPDG()->addInst(sh_inst,index);
  /*  
    if(op->isBBHead() || op->isMem()) {
      //only one memory instruction per basic block
      prev_bb_end=cur_bb_end;
      T* event_ptr = new T();
      cur_bb_end.reset(event_ptr);
    } 
*/
    addDeps(sh_inst,img);
    fixMin(*inst);

    uint64_t last_cycle=inst->cycleOfStage(SuperInst::Complete);
    if(max_cycle < last_cycle) {
      max_cycle = last_cycle;
    }
  }

private:
  typedef std::vector<std::shared_ptr<SuperInst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;

  virtual void addDeps(std::shared_ptr<SuperInst>& inst,const CP_NodeDiskImage &img) { 
    //setBBReadyCycle_cc(inst,img);
    setExecuteCycle_s(inst,img);
    setCompleteCycle_s(inst,img);
  }

#if 0
  //This node when current ccores BB is active
  virtual void setBBReadyCycle_cc(SuperInst& inst, const CP_NodeDiskImage &img) {
    CCoresBB::iterator I,E;
    /*for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        SuperInst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, SuperInst::Complete,
                               inst, SuperInst::BBReady, 0);
    }*/
    if(prev_bb_end) {
      inst.startBB=prev_bb_end;
      getCPDG()->insert_edge(*prev_bb_end,
                               inst, SuperInst::BBReady, 0);
    }
  }
#endif 

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_s(std::shared_ptr<SuperInst> &n, const CP_NodeDiskImage &img) {
    //getCPDG()->insert_edge(*n, SuperInst::BBReady,
    //                       *n, SuperInst::Execute, 0, true);
    //regular dependence
    for (int i = 0; i < 7; ++i) {
      unsigned prod = n->_prod[i];
      if (prod <= 0 || prod >= n->index()) {
        continue;
      }
      getCPDG()->insert_edge(n->index()-prod, SuperInst::Complete,
                             *n, SuperInst::Execute, 0, true);
    }
    //memory dependence
    if (n->_mem_prod > 0) {
      SuperInst& prev_node = static_cast<SuperInst&>( 
                          getCPDG()->queryNodes(n->index()-n->_mem_prod));

      if (prev_node._isstore && n->_isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node.index(), SuperInst::Complete,
                                  *n, SuperInst::Execute, 0, true);
      } else if (prev_node._isstore && n->_isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node.index(), SuperInst::Complete,
                                  *n, SuperInst::Execute, 0, true);
      } else if (prev_node._isload && n->_isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node.index(), SuperInst::Complete,
                                  *n, SuperInst::Execute, 0, true);
      }
    }


    //set_min_cycle -- cheat to set the min cycle
    if(n->index()>1 && n->cycleOfStage(SuperInst::Execute) < min_cycle) {
      SuperInst& prev_node = static_cast<SuperInst&>( 
                            getCPDG()->queryNodes(n->index()-1));

      getCPDG()->insert_edge(prev_node, SuperInst::Execute,
                             *n, SuperInst::Execute,  
                             min_cycle - prev_node.cycleOfStage(SuperInst::Execute),  
                             E_CHT);
    }

/*
    if(n->index>1 && ) {
      SuperInst& prev_node = static_cast<SuperInst&>( 
                            getCPDG()->queryNodes(n->index()-1));
      
    }*/

    

    //resource dependnece
    SuperInst* min_node = static_cast<SuperInst*>(
         addResource(n->_opclass, n->cycleOfStage(SuperInst::Execute), 
                                   getFUIssueLatency(n->_opclass), n.get()));

    if(min_node) {
      getCPDG()->insert_edge(min_node->index(), SuperInst::Execute,
          *n, SuperInst::Execute, getFUIssueLatency(min_node->_opclass),E_FU);
    }

    //memory dependence
    if(n->_isload || n->_isstore) {
      checkNumMSHRs(n);
    }
  }

  virtual void checkNumMSHRs(std::shared_ptr<SuperInst>& n) {
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
    BaseInst_t* min_node =
         addMSHRResource(reqDelayT + n->cycleOfStage(SuperInst::Execute), 
                         mshrT, n, n->_eff_addr, 1, rechecks, extraLat);

    if(min_node) {
        getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                       *n, SuperInst::Execute, mshrT+respDelayT, E_MSHR);
    }
  }

  virtual unsigned getNumFUAvailable(int opclass) { 
    return 10;
  }

  virtual void setCompleteCycle_s(std::shared_ptr<SuperInst>& inst, const CP_NodeDiskImage &img) {
    if(inst->_isstore) {
      getCPDG()->insert_edge(*inst, SuperInst::Execute,
                             *inst, SuperInst::Complete, inst->_st_lat);
    } else {
      getCPDG()->insert_edge(*inst, SuperInst::Execute,
                             *inst, SuperInst::Complete, inst->_ex_lat);
    }

/*  if(cur_bb_end) {
      inst.endBB = cur_bb_end; // have instruction keep
      getCPDG()->insert_edge(inst, SuperInst::Complete,
                               *cur_bb_end, 0);
    }
*/
  }

  virtual void fixMin(SuperInst& si) {
/*    if(si._iscall) {
      
    }*/

    if(si._index % 10000 == 0) {
      min_cycle = si.cycleOfStage(SuperInst::Execute);
    }

    //delete irrelevent 
    auto upperMSHRResp = MSHRResp.upper_bound(min_cycle);
    MSHRResp.erase(MSHRResp.begin(),upperMSHRResp); 

    //for funcUnitUsage
    for(FuUsage::iterator I=fuUsage.begin(),EE=fuUsage.end();I!=EE;++I) {
      FuUsageMap& fuUseMap = *I;
      for(FuUsageMap::iterator i=++fuUseMap.begin(),e=fuUseMap.end();i!=e;) {
        uint64_t cycle = i->first;
        assert(cycle!=0);
        if (cycle + 50  < min_cycle) {
          i = fuUseMap.erase(i);
        } else {
          //++i;
          break;
        }
      }
    }

    for(auto I=nodeResp.begin(),EE=nodeResp.end();I!=EE;++I) {
      NodeRespMap& respMap = *I;
      for(typename NodeRespMap::iterator i=respMap.begin(),e=respMap.end();i!=e;) {
        uint64_t cycle = i->first;
        if (cycle  < min_cycle) {
          i = respMap.erase(i);
        } else {
          //++i;
          break;
        }
      }
    }
  }

  uint64_t numCycles() {
    return max_cycle;
    //getCPDG()->finish(maxIndex);
    //return getCPDG()->getMaxCycles();
  }



};

#endif //CP_SUPER
