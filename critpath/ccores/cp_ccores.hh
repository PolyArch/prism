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

  uint64_t _startCCoresCycle=0;
  uint64_t _curCCoresStartInst=0;
  uint64_t _totalCCoresCycles=0, _totalCCoresInsts=0;
  //uint64_t

  virtual void accelSpecificStats(std::ostream& out, std::string &name) {
    out << " (ccores-only " << _totalCCoresCycles
        << " ccores-insts " << _totalCCoresInsts
       << ")";
  }

  //energy things
  uint64_t _ccores_int_ops=0, _ccores_fp_ops=0, _ccores_mult_ops=0;

  //Acc stats
  uint64_t _ccores_int_ops_acc=0, _ccores_fp_ops_acc=0, _ccores_mult_ops_acc=0;

  //arguements
  unsigned _ccores_num_mem=1, _ccores_max_ops=1000,_ccores_iops=2;
  unsigned _ccores_bb_runahead=0, _ccores_full_dataflow=0;
  unsigned _st_buf_size=16;

  void handle_argument(const char *name, const char *optarg) {
    if (strcmp(name, "ccores-num-mem") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_num_mem = temp;
      } else {
         std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-max-ops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_max_ops = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-iops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_iops = temp;
      } else {
         std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-bb-runahead") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_bb_runahead = temp;
      } else {
         std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "ccores-full-dataflow") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _ccores_full_dataflow = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
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
  //        std::cout << "->" << li->nice_name() << " (value " << inlineValue << ")\n";
          PQL.push(li,inlineValue);
        } else {
          //std::cout << "can't inline " << li->nice_name() << "\n";
        }

      }
      if(!fi->cantFullyInline()) {
        float inlineValue = inline_value(fi);
//        std::cout << "->" << fi->nice_name() << " (value " << inlineValue << ")\n";
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
          //float inlineValue=PQL.begin()->first; 
          //std::cout << "-------------------------- pick loop: " 
            //        << li->func()->nice_name() << "_" << li->id()
              //      << "  (value " << inlineValue << ")\n";

        PQL.erase(li);
        if(total + li->inlinedStaticInsts() < (int)_ccores_max_ops) {
          _li_ccores.insert(li);
        }
        //get list of all loops and all functions inlined, and remove them
        //li->getInlinedFuncs(funcsTouched,loopsTouched);

      } else { //add a func
        FunctionInfo* fi=PQF.begin()->second;
           //float inlineValue = PQF.begin()->first;
          // std::cout << "------------------------- pick func: " 
            //         << fi->nice_name() << " (value " << inlineValue << ")\n";

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

//    std::cout << "Funcs: ";
    for(auto i=_fi_ccores.begin(),e=_fi_ccores.end();i!=e;) {
      FunctionInfo* fi = *i;
      bool redundant = fi->calledOnlyFrom(_fi_ccores,_li_ccores);
      if(redundant) {
        _fi_ccores.erase(i++);

      } else {
        totalUsed+=fi->inlinedStaticInsts();
        totalDynamicInsts+=fi->totalDynamicInlinedInsts();
  //      std::cout << fi->nice_name() 
    //              << "(" << fi->inlinedStaticInsts()
      //            << "," << fi->totalDynamicInlinedInsts() << ") ";
        ++i;
      }
    }
//    std::cout << "\nLoops: ";
    for(auto i=_li_ccores.begin(),e=_li_ccores.end();i!=e;) {
      LoopInfo* li = *i;
      bool redundant = li->calledOnlyFrom(_fi_ccores,_li_ccores);
      if(redundant) {
        _li_ccores.erase(i++);
      } else {
        totalUsed+=li->inlinedStaticInsts();
        totalDynamicInsts+=li->totalDynamicInlinedInsts();
//        std::cout << li->nice_name()
  //                << "(" << li->inlinedStaticInsts()
    //              << "," << li->totalDynamicInlinedInsts() << ") ";
        ++i;
      }
    }
//    std::cout<< "\n";
  //  std::cout << "--- total used = " << totalUsed 
    //           << ", total insts" << totalDynamicInsts << "---\n";
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


    if(!getCPDG()->hasIdx(index)) {
      outs() << "elided\n";
      return;
    }

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);
    if(inst.isDummy()) {
      outs() << "dummy\n";
    } else if (inst.isPipelineInst()) {
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
      if(_fi_ccore || _li_ccore) {
        _startCCoresCycle=1;
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
      if(transitioned) {
        Inst_t* prevInst = getCPDG()->peekPipe(-1);
        assert(prevInst);

        T* event_ptr = new T();
        prev_bb_end.reset(event_ptr);;
        event_ptr = new T();
        cur_bb_end.reset(event_ptr);
        getCPDG()->insert_edge(*prevInst, Inst_t::Commit,
                               *prev_bb_end, 8/_ccores_iops, E_CXFR);
 
        getCPDG()->insert_edge(*prev_bb_end,
                                 *cur_bb_end, 0, E_CSBB);
       
        // _startCCoresCycle=cc_inst->cycleOfStage(CCoresInst::BBReady);
         _startCCoresCycle=prev_bb_end->cycle();
      }

      if(op->shouldIgnoreInAccel()) {
        createDummy(img,index,op);
        return;
      }
      if(op->plainMove()) {
        createDummy(img,index,op);
        return;
      }
      if(op->isConstLoad()) {
        createDummy(img,index,op,dg_inst_dummy<T,E>::DUMMY_CONST_LOAD);
        return;
      }
      if(op->isStack()) {
        createDummy(img,index,op,dg_inst_dummy<T,E>::DUMMY_STACK_SLOT);
        return;
      }

      CCoresInst* cc_inst = new CCoresInst(img,index,op);
      std::shared_ptr<CCoresInst> sh_inst(cc_inst);

      getCPDG()->addInst(sh_inst,index);

      _totalCCoresInsts++;
  
      if(endOfBB(op,img)) {
        //only one memory instruction per basic block
        prev_prev_bb_end=prev_bb_end;
        prev_bb_end=cur_bb_end;
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
        
        if(prev_bb_end) {
          getCPDG()->insert_edge(*prev_bb_end,
                                 *cur_bb_end, 0, E_CSBB);
          //uint64_t clean_cycle = prev_bb_end->cycle();
        }

        if(prev_prev_bb_end) {
          cleanUp(prev_prev_bb_end->cycle()-
                  std::min((uint64_t)1000,prev_prev_bb_end->cycle()));
        }
      }
      addCCoreDeps(sh_inst,img);
      //make sure this instruction comes after cleaning
      assert(sh_inst->cycleOfStage(0) >= _cs->latestCleaned);
      assert(sh_inst->cycleOfStage(1) >= _cs->latestCleaned);
      assert(sh_inst->cycleOfStage(2) >= _cs->latestCleaned);


      //have to insert it into the lsq as well
      //after all the deps are determined
      insertLSQ(sh_inst); 
      

      if(sh_inst->_floating) {
        _ccores_fp_ops++;
      } else if (sh_inst->_opclass==2) {
        _ccores_mult_ops++;
      } else {
        _ccores_int_ops++; //TODO: FIX!
      }

      //final stage add here
      keepTrackOfInstOpMap(sh_inst,op);

/*      prevRet = op->isReturn();
      if(prevRet) {
        inCCore=false;
      }*/
    } else {
      InstPtr sh_inst = createInst(img,index,op,false);
      getCPDG()->addInst(sh_inst,index);
      if(transitioned) { 
        if(cur_bb_end) {
          getCPDG()->insert_edge(*cur_bb_end,
              *sh_inst, Inst_t::Fetch, 4/_ccores_iops, E_CXFR);
          uint64_t endCCoresCycle=cur_bb_end->cycle();
          _totalCCoresCycles+=endCCoresCycle-_startCCoresCycle;
          _startCCoresCycle=0;
        }
      }
      addDeps(sh_inst,op);
      pushPipe(sh_inst);
      inserted(sh_inst);
    }

  }

private:
  typedef std::vector<std::shared_ptr<CCoresInst>> CCoresBB;
  std::shared_ptr<T> prev_prev_bb_end, prev_bb_end, cur_bb_end;


  unsigned numMem=0;
  bool endOfBB(Op* op, const CP_NodeDiskImage& img) {
    if(!_elide_mem) { //normal path
      if(op->isMem()) {
        numMem+=1;
      } 
    } else {
      if(op->isStore()) {
        numMem+=1;
      } 
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
    assert((prev_bb_end==NULL && cur_bb_end==NULL) ||
           (prev_bb_end!=cur_bb_end)                  );
    setBBReadyCycle_cc(*inst,img);
    setExecuteCycle_cc(inst,img);
    setCompleteCycle_cc(*inst,img);
    setWritebackCycle_cc(inst,img);
    assert((prev_bb_end==NULL && cur_bb_end==NULL) ||
           (prev_bb_end!=cur_bb_end)                  );

  }

  //This node when current ccores BB is active
  virtual void setBBReadyCycle_cc(CCoresInst& inst, const CP_NodeDiskImage &img) {
    CCoresBB::iterator I,E;
    /*for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        CCoresInst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, CCoresInst::Complete,
                               inst, CCoresInst::BBReady, 0);
    }*/

    //make sure we haven't run out of LSQ entries
    checkLSQSize(inst[CCoresInst::BBReady],inst._isload,inst._isstore);

    if(prev_bb_end) {

      T* horizon_event = getCPDG()->getHorizon();
      uint64_t horizon_cycle = 0;
      if(horizon_event) {
        horizon_cycle=horizon_event->cycle();
      }
  
      if(horizon_event && prev_bb_end->cycle() < horizon_cycle) { 
        getCPDG()->insert_edge(*horizon_event,
                               *prev_bb_end, 0, E_HORZ);   
      }



      inst.startBB=prev_bb_end;
      getCPDG()->insert_edge(*prev_bb_end,
                               inst, CCoresInst::BBReady, 0, E_BBN);
    }

    //at this point, we can clean up the old entries
    cleanLSQEntries(inst.cycleOfStage(CCoresInst::BBReady));
  }

  bool error_with_dummy_inst=false;

  //"Execute" represents when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_cc(std::shared_ptr<CCoresInst>& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(*inst, CCoresInst::BBReady,
                           *inst, CCoresInst::Execute, 0, E_BBA);

    for (int i = 0; i < MAX_SRC_REGS; ++i) {
      unsigned prod = inst->_prod[i];
      if (prod <= 0 || prod >= inst->index()) {
        continue;
      }
      if(!getCPDG()->hasIdx(inst->index()-prod)) {
        continue;
      }
      dg_inst_base<T,E>* dep_inst = &getCPDG()->queryNodes(inst->index()-prod);

      bool out_of_bounds=false, error=false;
      dep_inst = fixDummyInstruction(dep_inst,out_of_bounds,error); //FTFY! : )

      if(error && error_with_dummy_inst==false) {
        error_with_dummy_inst=true;
        std::cerr << "ERROR: Some problem with dummy inst" << inst->_op->id() << "\n";
      }
      if(out_of_bounds) {
        continue;
      }

      getCPDG()->insert_edge(*dep_inst, dep_inst->eventComplete(),
                             *inst, CCoresInst::Execute, 0, E_RDep);
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
       
      if(&prev_node != &(*inst)) {

        if (prev_node._isstore && inst->_isload) {
          //data dependence
          getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                    *inst, CCoresInst::Execute, 0, E_MDep);
        } else if (prev_node._isstore && inst->_isstore) {
          //anti dependence (output-dep)
          getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                    *inst, CCoresInst::Execute, 0, E_MDep);
        } else if (prev_node._isload && inst->_isstore) {
          //anti dependence (load-store)
          getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                    *inst, CCoresInst::Execute, 0, E_MDep);
        }
      } else {
        //TODO: why would this ever happen
        static bool fail=false;
        if(!fail) {
          fail=true;
          std::cout << "ERROR: Why MDep b/t same node?\n";
          std::cout << "ind:" << inst->index();
          std::cout << " mem_prod:" << inst->_mem_prod << "\n";
        }
      }
    }

    //check to make sure that L1 cache bandwidth is satisfied
    if(inst->_isload) {
      if(!_elide_mem) {
        checkNumMSHRs(inst);
      }
    }
  }



  

  virtual void setCompleteCycle_cc(CCoresInst& inst, const CP_NodeDiskImage &img) {
    int lat=epLat(inst._ex_lat,&inst,inst._isload,
                  inst._isstore,inst._cache_prod,inst._true_cache_prod,true);

    getCPDG()->insert_edge(inst, CCoresInst::Execute,
                           inst, CCoresInst::Complete, lat, E_EP);
    
    if(cur_bb_end) {
      inst.endBB = cur_bb_end; // have instruction keep

      if(_ccores_full_dataflow) {
         if(inst._isctrl && inst._ctrl_miss) {
          getCPDG()->insert_edge(inst, CCoresInst::Complete,
                                 *cur_bb_end, 1,E_BBC);
        }
      } else if(_ccores_bb_runahead) {
        if(inst._isctrl) {
          getCPDG()->insert_edge(inst, CCoresInst::Complete,
                                 *cur_bb_end, 0,E_BBC);
        }
      } else {
        getCPDG()->insert_edge(inst, CCoresInst::Complete,
                               *cur_bb_end, 0,E_BBC);
      }
    }
    checkPP(inst);
  }

  virtual void setWritebackCycle_cc(std::shared_ptr<CCoresInst>& inst, const CP_NodeDiskImage &img) {
    if(inst->_isstore) {
      int st_lat=stLat(inst->_st_lat,inst->_cache_prod,
                       inst->_true_cache_prod,true/*is accelerated*/);
      getCPDG()->insert_edge(*inst, CCoresInst::Complete,
                             *inst, CCoresInst::Writeback, 2+st_lat, E_WB);
      checkNumMSHRs(inst,true);
      //checkPP(*inst); //this is interesting
    }

  }

  virtual uint64_t numCycles() {
    uint64_t curCycle = CP_DG_Builder::numCycles();

     if(_startCCoresCycle!=0) {
       _totalCCoresCycles+= curCycle-_startCCoresCycle;
       _startCCoresCycle=curCycle;
     }

    return curCycle;
  }

  virtual void checkNumMSHRs(std::shared_ptr<CCoresInst>& n,uint64_t minT=0) {
    int ep_lat=epLat(n->_ex_lat,n.get(),n->_isload,n->_isstore,
                  n->_cache_prod,n->_true_cache_prod,true);
    int st_lat=stLat(n->_st_lat,n->_cache_prod,n->_true_cache_prod,true);

    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,st_lat,
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
    } else { //Store
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, CCoresInst::Writeback, mshrT+respDelayT, E_MSHR);
        }
    }
  }


  NLAProcessor* mc_accel = NULL;
  virtual void setupMcPAT(const char* filename, int nm) 
  {
    CriticalPath::setupMcPAT(filename,nm); //do base class

    printAccelMcPATxml(filename,nm);
    ParseXML* mcpat_xml= new ParseXML(); 
    mcpat_xml->parse((char*)filename);
    mc_accel = new NLAProcessor(mcpat_xml); 
    pumpCoreEnergyEvents(mc_accel,0); //reset all energy stats to 0
  }

  virtual void pumpAccelMcPAT(uint64_t totalCycles) {
    pumpAccelEnergyEvents(totalCycles);
    mc_accel->computeEnergy();
  }

  virtual void pumpAccelEnergyEvents(uint64_t totalCycles) {
    mc_accel->XML->sys.total_cycles=totalCycles;

    //Func Units
    system_core* core = &mc_accel->XML->sys.core[0];
    core->ialu_accesses    = (uint64_t)(_ccores_int_ops );
    core->fpu_accesses     = (uint64_t)(_ccores_fp_ops  );
    core->mul_accesses     = (uint64_t)(_ccores_mult_ops);

    core->cdb_alu_accesses = (uint64_t)(_ccores_int_ops );
    core->cdb_fpu_accesses = (uint64_t)(_ccores_fp_ops  );
    core->cdb_mul_accesses = (uint64_t)(_ccores_mult_ops);

    accAccelEnergyEvents();
  }

  virtual double accel_leakage() override {
    //if(false) {
    //  float ialu = RegionStats::get_leakage(mc_accel->cores[0]->exu->exeu,lc)* nALUs_on;
    //  float mul  = RegionStats::get_leakage(mc_accel->cores[0]->exu->mul,lc) * nMULs_on;
    //  float imu  = RegionStats::get_leakage(mc_accel->nlas[0]->imu_window,lc)* imus_on;
    //  return ialu + mul + imu + net;
    //} else {
      return 0;
    //}
  }

  virtual double accel_region_en() override {
 
    float ialu  = mc_accel->cores[0]->exu->exeu->rt_power.readOp.dynamic; 
    float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
    float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 

    return ialu + fpalu + calu;
  }

  virtual int is_accel_on() {
    if(_fi_ccore) {
      return -_fi_ccore->id();
    } else if (_li_ccore) {
      return _li_ccore->id();
    } else {
      return 0;
    }
  }

  virtual void printMcPAT_Accel() {
    mc_accel->computeAccPower(); //need to compute the accumulated power

    float ialu  = mc_accel->ialu_acc_power.rt_power.readOp.dynamic; 
    float fpalu = mc_accel->fpu_acc_power.rt_power.readOp.dynamic; 
    float calu  = mc_accel->mul_acc_power.rt_power.readOp.dynamic; 

    float total = ialu + fpalu + calu;

    std::cout << _name << " accel(" << _nm << "nm)... ";
    std::cout << total << " (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu 
                       << ")\n";

  }

  virtual void accAccelEnergyEvents() {
    _ccores_int_ops_acc         +=  _ccores_int_ops        ;
    _ccores_fp_ops_acc          +=  _ccores_fp_ops         ;
    _ccores_mult_ops_acc        +=  _ccores_mult_ops       ;

    _ccores_int_ops         = 0;
    _ccores_fp_ops          = 0;
    _ccores_mult_ops        = 0;
  }






  // Handle enrgy events for McPAT XML DOC
  virtual void printAccelMcPATxml(std::string fname_base,int nm) {
    #include "mcpat-defaults.hh"
    pugi::xml_document accel_doc;
    std::istringstream ss(xml_str);
    pugi::xml_parse_result result = accel_doc.load(ss);

    //pugi::xml_parse_result result = doc.load_file("tony-ooo.xml");

    if(result) {
      pugi::xml_node system_node = accel_doc.child("component").find_child_by_attribute("name","system");
      //set the total_cycles so that we get power correctly
      sa(system_node,"total_cycles",numCycles());
      sa(system_node,"busy_cycles",0);
      sa(system_node,"idle_cycles",numCycles());

      sa(system_node,"core_tech_node",nm);
      sa(system_node,"device_type",0);

      pugi::xml_node core_node =
                system_node.find_child_by_attribute("name","core0");

      sa(core_node,"machine_type",1);	//<!-- inorder/OoO; 1 inorder; 0 OOO-->


      sa(core_node,"total_cycles",numCycles());
      sa(core_node,"busy_cycles",0);
      sa(core_node,"idle_cycles",numCycles());

      sa(core_node,"ALU_per_core", Prof::get().int_alu_count);
      sa(core_node,"MUL_per_core", Prof::get().mul_div_count);
      sa(core_node,"FPU_per_core", Prof::get().fp_alu_count);

      sa(core_node,"ialu_accesses",_ccores_int_ops);
      sa(core_node,"fpu_accesses",_ccores_fp_ops);
      sa(core_node,"mul_accesses",_ccores_mult_ops);
      sa(system_node,"",0);

    } else {
      std::cerr << "XML Malformed\n";
      return;
    }

    mcpat_xml_accel_fname = 
           std::string(mcpat_xml_fname);

    std::string fname=fname_base + std::string(".accel");
    accel_doc.save_file(fname.c_str());
  }

  virtual void calcAccelEnergy(std::string fname_base,int nm) {
    std::string fname=fname_base + std::string(".accel");
    std::string outf = fname + std::string(".out");

    std::cout << _name << " accel(" << nm << "nm)... ";
    std::cout.flush();

    execMcPAT(fname,outf);
    float ialu  = stof(grepF(outf,"Integer ALUs",7,4));
    float fpalu = stof(grepF(outf,"Floating Point Units",7,4));
    float calu  = stof(grepF(outf,"Complex ALUs",7,4));
    float total = ialu + fpalu + calu;
    std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ")\n";

  }

};





#endif //CP_CCORES
