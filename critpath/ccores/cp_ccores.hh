#ifndef CP_CCORES
#define CP_CCORES

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

//#include "cp_ccores_all.hh"
#include "pathprof.hh"
//#include "functioninfo.hh"
//#include "loopinfo.hh"
#include "mutable_queue.hh"
#include "ccores_inst.hh"


class cp_ccores : public ArgumentHandler,
                public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef dg_inst<T, E> Inst_t;

public:
  cp_ccores() : CP_DG_Builder<T,E>() {
  }

  virtual ~cp_ccores() {
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;

  uint64_t _curCCoresStartCycle=0, _curCCoresStartInst=0;
  uint64_t _totalCCoresCycles=0, _totalCCoresInsts=0;

  virtual void accelSpecificStats(std::ostream& out) {
    out << " (ccores-only " << _totalCCoresCycles
        << " ccores-insts " << _totalCCoresInsts
       << ")";
  }

  unsigned _ccores_num_mem=1, _ccores_bb_runahead=1, _ccores_max_ops=1000,_ccores_iops=2;
  void handle_argument(const char *name, const char *optarg) {
    if (strcmp(name, "ccores-num-mem") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_num_mem = temp;
      } else {
        std::cerr << "ERROR: ccores-num-mem arg\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-bb-runahead") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_bb_runahead = temp;
      } else {
        std::cerr << "ERROR: ccores-bb-runahead arg\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-max-ops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_max_ops = temp;
      } else {
        std::cerr << "ERROR: ccores-max-ops arg\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-iops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_iops = temp;
      } else {
        std::cerr << "ERROR: ccores-max-ops arg\"" << optarg << "\" is invalid\n";
      }
    }


  }
#if 0
  class CTarget {
  public:
    double computeValue();
    std::map<LoopInfo*,int> lichildren;
    std::map<FunctionInfo*,int> fichildren;
  };

  class CTargetFunc {
  public:
    FunctionInfo* fi;
    double computeValue() {
      return 0; 
    }
  };

  class CTargetLoop {
  public:
    LoopInfo* li;
    double computeValue() {
         return 0;
    }
  };
#endif

  int totalOpsUsed=0;
  std::set<LoopInfo*> _li_ccores;
  std::set<FunctionInfo*> _fi_ccores;

  virtual void setDefaultsFromProf() {
    CP_DG_Builder::setDefaultsFromProf();

    mutable_priority_queue<float, LoopInfo*> PQL;
    mutable_priority_queue<float, FunctionInfo*> PQF;
    //std::priority_queue<

    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        if(!li->cantFullyInline()) {
          float inlineValue=inline_value(li); 
          std::cout << "->" << li->nice_name() << " --- " << inlineValue << "\n";
          PQL.push(li,inlineValue);
        } else {
          //std::cout << "can't inline " << li->nice_name() << "\n";
        }

      }
      if(!fi->cantFullyInline()) {
        float inlineValue = inline_value(fi);
        std::cout << "->" << fi->nice_name() << " === " << inlineValue << "\n";
        PQF.push(fi,inlineValue);
      } else {
        //std::cout << "can't inline " << fi->nice_name() << "\n";
      }
    } 

  ///  std::set<FunctionInfo*> funcsTouched;
//    std::set<LoopInfo*> loopsTouched;

    //get some blocks!
    int total;
    while( (total=totalInstsUsed()) < (int)_ccores_max_ops) {
      float func_queue_v=0, loop_queue_v=0;
      
      if(PQF.size()) {
        func_queue_v = PQF.begin()->first;
      }
      if(PQL.size()) {
        loop_queue_v = PQL.begin()->first;
      }

      //if both queues are empty, stop trying to add stuff
      if(func_queue_v == 0 && loop_queue_v == 0) {
        break;
      }

      if(loop_queue_v > func_queue_v) { //add a loop
        
        LoopInfo* li=PQL.begin()->second;
          float inlineValue=PQL.begin()->first; 
          std::cout << "pick: " << li->func()->nice_name() << "_" << li->id()
                    << " === " << inlineValue << "\n";

        PQL.erase(li);
        if(total + li->inlinedStaticInsts() < (int)_ccores_max_ops) {
          _li_ccores.insert(li);
        }
        //get list of all loops and all functions inlined, and remove them
        //li->getInlinedFuncs(funcsTouched,loopsTouched);

      } else { //add a func
        FunctionInfo* fi=PQF.begin()->second;
           float inlineValue = PQF.begin()->first;
           std::cout << "pick: " << fi->nice_name() << " === " << inlineValue << "\n";

        PQF.erase(fi);
        if(total + fi->inlinedStaticInsts() < (int)_ccores_max_ops) {
          _fi_ccores.insert(fi);
        }
        //get list of all loops and all functions inlined, and remove them

      }
    }
  }


  int totalInstsUsed() {
    //first check 
    int totalUsed=0;
    uint64_t totalDynamicInsts=0;

    std::cout << "Funcs: ";
    for(auto i=_fi_ccores.begin(),e=_fi_ccores.end();i!=e;) {
      FunctionInfo* fi = *i;
      bool redundant = fi->calledOnlyFrom(_fi_ccores,_li_ccores);
      if(redundant) {
        _fi_ccores.erase(i++);

      } else {
        totalUsed+=fi->inlinedStaticInsts();
        totalDynamicInsts+=fi->totalDynamicInlinedInsts();
        std::cout << fi->nice_name() 
                  << "(" << fi->inlinedStaticInsts()
                  << "," << fi->totalDynamicInlinedInsts() << ") ";
        ++i;
      }
    }
    std::cout << "\nLoops: ";
    for(auto i=_li_ccores.begin(),e=_li_ccores.end();i!=e;) {
      LoopInfo* li = *i;
      bool redundant = li->calledOnlyFrom(_fi_ccores,_li_ccores);
      if(redundant) {
        _li_ccores.erase(i++);
      } else {
        totalUsed+=li->inlinedStaticInsts();
        totalDynamicInsts+=li->totalDynamicInlinedInsts();
        std::cout << li->nice_name()
                  << "(" << li->inlinedStaticInsts()
                  << "," << li->totalDynamicInlinedInsts() << ") ";
        ++i;
      }
    }
    std::cout<< "\n";
    std::cout << "--- total used = " << totalUsed 
               << ", total insts" << totalDynamicInsts << "---\n";
    return totalUsed;
  }

  float inline_value(LoopInfo* li) {
    return (double)li->totalDynamicInlinedInsts()/
           ((double)li->inlinedStaticInsts());
  }

  float inline_value(FunctionInfo* fi) {
    return (double)fi->totalDynamicInlinedInsts()/
           ((double)fi->inlinedStaticInsts());
  }

  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (!getTraceOutputs())
      return;

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);
    if (inst.isPipelineInst()) {
      CP_DG_Builder::traceOut(index, img, op);
    } else {
      outs() << index + Prof::get().skipInsts << ": ";
      outs() << inst.cycleOfStage(0) << " ";
      outs() << inst.cycleOfStage(1) << " ";
      outs() << inst.cycleOfStage(2) << " ";
      CriticalPath::traceOut(index, img, op);
      outs() << "\n";
    }
  }


  bool first_op = true;

  LoopInfo* _li_ccore=NULL;
  FunctionInfo* _fi_ccore=NULL;

  bool inCCore() {
    return _li_ccore || _fi_ccore;
  }

  FunctionInfo* _prevFunc=NULL;
  //FunctionInfo* _prevBB=NULL;

  uint64_t _startCCoresCycle=0;
  void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) {
    
    if(first_op) {
      first_op=false;
      auto p = Prof::get().findEntry(op,_fi_ccores,_li_ccores);
      _li_ccore=p.first;
      _fi_ccore=p.second;
      //if(p.first==NULL && p.second==NULL) {
      //}
      if(_fi_ccore) {
        _prevFunc=op->func();
      }
    }

    assert(!(_li_ccore && _fi_ccore));

    bool transitioned=false;
    if(!inCCore()) {
      //check if this op entered a function ccore
      FunctionInfo* fi = op->func();
      if(_prevFunc!=fi && _fi_ccores.count(fi)) {
        _fi_ccore=fi;
        transitioned=true;
      } else if(op->bb_pos()==0) {
        //check if this op entered a loop ccore
        LoopInfo* li = fi->innermostLoopFor(op->bb());
        if(li && _li_ccores.count(li)) {
          _li_ccore=li;
          transitioned=true;
        }
      }
    } else {
      //check if we should exit the ccore

       //loop should have first dibs on catching
      if(_li_ccore) {  
        if(op->bb_pos()==0) {
          FunctionInfo* fi = op->func();
          LoopInfo* li = fi->innermostLoopFor(op->bb());

          //difficult to do this test,        
          if(li && li->isParentOf(_li_ccore)) {
            transitioned=true; //broke because returned into next loop
          } else if (fi->callsFunc(_li_ccore->func())) {
            transitioned=true; //broke because of return from loop
          } else if (fi==_li_ccore->func() && !_li_ccore->inLoop(op->bb())) {
            transitioned=true; //we exited the loop into the current func
            //or we predicted the wrong loop, w/e
          }
        }
      } else {
        assert(_fi_ccore);
        FunctionInfo* fi = op->func();
        if(fi==_fi_ccore && op->isReturn()) {
          transitioned=true;
        }
          /*
          if(li->callsFunc(_fi_ccore)) {     
            transition=true;
          } else if (fi->calls(_fi_ccore)) {  
            transition=true;
          }*/
      }
      if(transitioned) {
        _li_ccore=NULL;
        _fi_ccore=NULL;
      } 
    }

    if(inCCore()) {
      CCoresInst* cc_inst = new CCoresInst(img,index);
      std::shared_ptr<CCoresInst> sh_inst(cc_inst);
      getCPDG()->addInst(sh_inst,index);

      _totalCCoresInsts++;

      if(transitioned) {
        Inst_t* prevInst = getCPDG()->peekPipe(-1);
        assert(prevInst);
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
        getCPDG()->insert_edge(*prevInst, Inst_t::Commit,
                               *cur_bb_end, 8/_ccores_iops, E_CXFR);
         _startCCoresCycle=cc_inst->cycleOfStage(CCoresInst::BBReady);
      }
  
      if(endOfBB(op,img)) {
        //only one memory instruction per basic block
        prev_bb_end=cur_bb_end;
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
      }
      addCCoreDeps(sh_inst,img);

/*      prevRet = op->isReturn();
      if(prevRet) {
        inCCore=false;
      }*/
    } else {
      Inst_t* inst = new Inst_t(img,index);
      std::shared_ptr<Inst_t> sh_inst(inst);
      getCPDG()->addInst(sh_inst,index);
      if(transitioned) {
        if(cur_bb_end) {
          getCPDG()->insert_edge(*cur_bb_end,
              *inst, Inst_t::Fetch, 4/_ccores_iops, E_CXFR);
          uint64_t endCCoresCycle=cur_bb_end->cycle();
          _totalCCoresCycles+=endCCoresCycle-_startCCoresCycle;
        }
      }
      addDeps(sh_inst);
      pushPipe(sh_inst);
      inserted(sh_inst);
    }

  }

private:
  typedef std::vector<std::shared_ptr<CCoresInst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;


  unsigned numMem=0;
  bool endOfBB(Op* op, const CP_NodeDiskImage& img) {
    if(op->isMem()) {
      numMem+=1;
    } 
    if(img._serialBefore || img._serialAfter || 
       op->isBBHead() || numMem==_ccores_num_mem) {
      numMem=0;
      return true;
    }
    return false;
  }


  virtual void addCCoreDeps(std::shared_ptr<CCoresInst>& inst,
                            const CP_NodeDiskImage &img) { 
    setBBReadyCycle_cc(*inst,img);
    setExecuteCycle_cc(inst,img);
    setCompleteCycle_cc(*inst,img);
  }

  //This node when current ccores BB is active
  virtual void setBBReadyCycle_cc(CCoresInst& inst, const CP_NodeDiskImage &img) {
    CCoresBB::iterator I,E;
    /*for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        CCoresInst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, CCoresInst::Complete,
                               inst, CCoresInst::BBReady, 0);
    }*/
    if(prev_bb_end) {
      inst.startBB=prev_bb_end;
      getCPDG()->insert_edge(*prev_bb_end,
                               inst, CCoresInst::BBReady, 0);
    }
  }

  //"Execute" represents when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_cc(std::shared_ptr<CCoresInst>& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(*inst, CCoresInst::BBReady,
                           *inst, CCoresInst::Execute, 0, true);

    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst->_prod[i];
      if (prod <= 0 || prod >= inst->index()) {
        continue;
      }
      dg_inst_base<T,E>& dep_inst = getCPDG()->queryNodes(inst->index()-prod);
      getCPDG()->insert_edge(dep_inst, dep_inst.eventComplete(),
                             *inst, CCoresInst::Execute, 0, true);
    }

    //Memory dependence enforced by BB ordering, in the restricted case
    //when when bb-runahead is on, or num-mem is >1, then we should enforce this
    //of course, this means that CCORES would need dependence resolution hardware

      //p[op]=this;                                                 |      BB* bb = i->second;                                                     |    op->setIsCtrl(img._isctrl);
      // 
      //memoy dependence
     //memory dependence
    if (inst->_mem_prod > 0 && inst->_mem_prod < inst->index()) {
      BaseInst_t& prev_node = getCPDG()->queryNodes(inst->index()-inst->_mem_prod);

      if (prev_node._isstore && inst->_isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  *inst, CCoresInst::Execute, 0, true);
      } else if (prev_node._isstore && inst->_isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  *inst, CCoresInst::Execute, 0, true);
      } else if (prev_node._isload && inst->_isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  *inst, CCoresInst::Execute, 0, true);
      }
    }

    //check to make sure that L1 cache bandwidth is satisfied
    if(inst->_isload || inst->_isstore) {
      checkNumMSHRs(inst);
    }
  }



  

  virtual void setCompleteCycle_cc(CCoresInst& inst, const CP_NodeDiskImage &img) {
    int lat=epLat(inst._ex_lat,inst._opclass,inst._isload,
                  inst._isstore,inst._cache_prod,inst._true_cache_prod);

    getCPDG()->insert_edge(inst, CCoresInst::Execute,
                           inst, CCoresInst::Complete, lat);

    if(cur_bb_end) {
      inst.endBB = cur_bb_end; // have instruction keep
      getCPDG()->insert_edge(inst, CCoresInst::Complete,
                               *cur_bb_end, 0);
    }

  }

  uint64_t numCycles() {
    getCPDG()->finish(maxIndex);
    return getCPDG()->getMaxCycles();
  }

  virtual void checkNumMSHRs(std::shared_ptr<CCoresInst>& n,uint64_t minT=0) {
    int ep_lat=epLat(n->_ex_lat,n->_opclass,n->_isload,n->_isstore,
                  n->_cache_prod,n->_true_cache_prod);

    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,n->_st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 

    int rechecks=0;
    uint64_t extraLat=0;

    uint64_t access_time=reqDelayT + n->cycleOfStage(CCoresInst::Execute);

    if (minT > access_time) {
      minT=access_time;
    } 

    if(n->_isload) {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, CCoresInst::Execute, mshrT+respDelayT, E_MSHR);
      }
    } else {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, CCoresInst::Execute, mshrT+respDelayT, E_MSHR);
        }
    }
  }

};

















#endif //CP_CCORES
