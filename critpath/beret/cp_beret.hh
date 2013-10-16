#ifndef CP_BERET
#define CP_BERET

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "loopinfo.hh"

class BeretInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      SEBReady = 0,
      Execute = 1,
      Complete = 2,
      Writeback = 3,
      NumStages
  };



private:
  T events[NumStages];

public:
  virtual ~BeretInst() { 
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  bool _isctrl=0;
  bool _ctrl_miss=0;
  uint16_t _icache_lat=0;
  uint16_t _prod[7]={0,0,0,0,0,0,0};
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint64_t _true_cache_prod=false;
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
  E* ex_edge;

  BeretInst(const CP_NodeDiskImage &img, uint64_t index):
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
    _true_cache_prod=img._true_cache_prod;
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

  BeretInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void updateLat(uint16_t lat) {
    ex_edge->_len=lat;
  }

  uint16_t ex_lat() {
    return ex_edge->_len;
  }


  void reCalculate() {
    for(int i = 0; i < NumStages; ++i) {
      events[i].reCalculate();
    }
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


// CP_BERET !!!
class cp_beret : public ArgumentHandler,
      public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef dg_inst<T, E> Inst_t;

 /* 
  BeretOpInfo *bOpArr;
  BeretOpInfo* bOpI(Op* li) {
    return bOpArr[op->id()];
  }

  void analyzeInnerLoop(LoopInfo* li) {
    LoopInfo::BBvec& hotPath = li->getHotPath();
    set<Op*> opsInTrace;
   
    LoopInfo::BBvec::iterator bi,be;
    for(bi=hotPath.begin(),be=hotPath.end();bi!=be;++bi) {
      BB* bb = *I;
      
      BB::OpVec::iterator io,eo;
      for(io=bb.op_begin(),eo=bb.op_end();io!=eo;++io) {
        Op* op = *io;
        opsInTrace.insert(op); 
      }
    }     
  }


  void analyzeFunction(FunctionInfo* fi) {
    FuncInfo::LoopList::iterator I,E;
    for(I=prof.fbegin(),E=prof.fend();I!=E;++I) {
      LoopInfo* li = I->second;
      if(li->parentLoop()==NULL) {
        analyzeInnerLoop(li);
      }
    }
  }

  void analyzeProfile() {
    PathProf& prof=Prof::get()
    
    //initialize all analysis info
    bOpArr = new BeretOpInfo[prof.maxOps];

    PathProf::FuncMap::iterator I,E;
    for(I=prof.fbegin(),E=prof.fend();I!=E;++I) {
      FunctionInfo* fi = I->second;
      analyzeFunction(fi);
    }
  }
*/

enum BERET_STATE {
  CPU,
  BERET
};

public:
  cp_beret() : CP_DG_Builder<T,E>() {
    beret_state=CPU;
  }

  virtual ~cp_beret(){
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;

  std::map<LoopInfo*,LoopInfo::SubgraphVec> li2sgmap;
  std::map<LoopInfo*,LoopInfo::SubgraphSet> li2ssmap;
  LoopInfo* _prevLoop=NULL;

  unsigned _beret_max_seb=6,_beret_max_mem=2,_beret_max_ops=80;
  unsigned _beret_config_time=1,_beret_iops=2;

  void handle_argument(const char *name, const char *optarg) {
    if (strcmp(name, "beret-max-seb") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_seb = temp;
      }  else {
        std::cerr << "ERROR: \"" << name << "\" arg value\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-max-mem") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_mem = temp;
      } else {
        std::cerr << "ERROR: \"" << name << "\" arg value\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-max-ops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_ops = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-config-time") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_config_time = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg\"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-iops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_iops = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg\"" << optarg << "\" is invalid\n";
      }
    }


  }

  virtual void setDefaultsFromProf() {
    CP_DG_Builder::setDefaultsFromProf();
    //set up the subgraphs
    std::multimap<uint64_t,LoopInfo*> loops;
    PathProf::FuncMap::iterator i,e;
    for(i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo& fi = *i->second;
      FunctionInfo::LoopList::iterator li,le;
      for(li=fi.li_begin(),le=fi.li_end();li!=le;++li) {
        LoopInfo* loopInfo = li->second;
        loops.insert(std::make_pair(loopInfo->numInsts(),loopInfo));
      }
    } 
 

    std::multimap<uint64_t,LoopInfo*>::reverse_iterator I;
    for(I=loops.rbegin();I!=loops.rend();++I) {
      LoopInfo* loopInfo = I->second;
  
      //generate this only for 
      //1. Inner Loops
      //2. >50% Loop-Back
      //3. Executed >= 10 Times
  
      int hpi = loopInfo->getHotPathIndex();
      std::cerr << "func: " << loopInfo->func()->nice_name() 
           << "(" << loopInfo->func()->id() << ")"
           << " loop: " << loopInfo->id()
           << "(depth:" << loopInfo->depth() << " hpi:" << hpi
           << "hp_len: " << loopInfo->instsOnPath(hpi)  
           << (loopInfo->isInnerLoop() ? " inner " : " outer ")
           << " lbr:" << loopInfo->getLoopBackRatio(hpi)
           << " iters:" << loopInfo->getTotalIters()
           << " insts:" << loopInfo->numInsts()
           << ")";
  
      bool worked=false;
  
      if(loopInfo->isInnerLoop()
         && hpi != -2 //no hot path
         && loopInfo->getLoopBackRatio(hpi) >= 0.6
         && loopInfo->getTotalIters() >= 2
         && loopInfo->instsOnPath(hpi) <= (int)_beret_max_ops
         ) {
        std::stringstream part_gams_str;
        part_gams_str << "partition." << loopInfo->id() << ".gams";
  
        bool gams_details=false;
        bool no_gams=false;
        std::cout << _beret_max_seb << " " << _beret_max_mem << "\n";
        worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
            li2ssmap[loopInfo],li2sgmap[loopInfo],
            gams_details,no_gams,_beret_max_seb,_beret_max_mem);
        if(worked) {
          std::cerr << " -- Beretized\n";
        } else {
          std::cerr << " -- NOT Beretized (Probably had Func Calls)\n";
        }
      } else {
        std::cerr << " -- NOT Beretized -- did not satisfy criteria\n";
      }
    }
    
  }

virtual void printEdgeDep(BaseInst_t& inst, int ind,
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


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (!getTraceOutputs())
      return;

    outs() << index + Prof::get().skipInsts << ": ";

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);

    for (unsigned i = 0; i < inst.numStages(); ++i) {
      outs() << inst.cycleOfStage(i) << " ";
      printEdgeDep(inst,i,E_FF);

    }
    CriticalPath::traceOut(index,img,op);
    outs() << "\n";
  }
  

  virtual void accelSpecificStats(std::ostream& out) {
    //out << "Beret Cycles = " << _totalBeretCycles << "\n";
    out << " (beret-only " << _totalBeretCycles 
        << " beret-insts " << _totalBeretInsts
       << ")";
  }

  /*
   *  Beret Iteration State
   */
  unsigned beret_state;
  std::map<Op*,std::shared_ptr<BeretInst>> binstMap;
  BeretInst* first_inst=NULL;
  BeretInst* last_inst=NULL;
  LoopInfo* li;
  Op* curLoopHead;
  std::vector<std::pair<CP_NodeDiskImage,uint64_t>> replay_queue;
  unsigned _whichBB;
  uint64_t _curBeretStartCycle=0, _curBeretStartInst=0;
  uint64_t _totalBeretCycles=0, _totalBeretInsts=0;

  void addLoopIteration(dg_inst_base<T,E>* inst, unsigned eventInd, unsigned xfer=8) {
    Subgraph* prev_sg=NULL;
    first_inst=NULL;
    binstMap.clear();
    replay_queue.clear();
    _whichBB=0;
    //create instruction for each SEB
    
    assert(li2sgmap[li].size()>0);
    for(auto i =li2sgmap[li].begin(),e =li2sgmap[li].end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        BeretInst* b_inst = new BeretInst();
	if(first_inst==NULL) {
          first_inst=b_inst;
        }
	last_inst=b_inst;

        binstMap.emplace(std::piecewise_construct,
                         std::forward_as_tuple(op),
                         std::forward_as_tuple(b_inst));

        getCPDG()->insert_edge(*b_inst, BeretInst::SEBReady,
                               *b_inst, BeretInst::Execute, 0, E_SEBE);

	b_inst->ex_edge = 
	    getCPDG()->insert_edge(*b_inst, BeretInst::Execute,
	                           *b_inst, BeretInst::Complete,/*op->avg_lat()*/1, E_SEBC);

        getCPDG()->insert_edge(*b_inst, BeretInst::Complete,
                               *b_inst, BeretInst::Writeback, 0, E_SEBE);

/*        std::cout << "X " << b_inst->cycleOfStage(0) << " "
                  << b_inst->cycleOfStage(1) << " "
                  << b_inst->cycleOfStage(2) << "\n";*/
      }
      prev_sg=sg;
    }

    prev_sg=NULL;
    //Add Deps
    for(auto i =li2sgmap[li].begin(),e =li2sgmap[li].end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        //add subgraph deps to previous subgraph
        if(prev_sg!=NULL) {
          for(auto si=prev_sg->op_begin(),se=prev_sg->op_end();si!=se;++si){
            Op* ps_op = *si;
            std::shared_ptr<BeretInst> ps_BeretInst = binstMap[ps_op];
            getCPDG()->insert_edge(*ps_BeretInst, BeretInst::Complete,
                                   *b_inst, BeretInst::SEBReady, 0, E_SEBA);
          } 
        } else {
          getCPDG()->insert_edge(*inst, eventInd,
                                 *b_inst,BeretInst::SEBReady,xfer/_beret_iops,E_SEBS);
          /*std::cout << inst->cycleOfStage(eventInd) << " "
                    << b_inst->cycleOfStage(BeretInst::SEBReady) << "\n";*/
        }
        for(auto di = op->d_begin(),de = op->d_end();di!=de;++di) {
          Op* dop = *di;
          if(binstMap.count(dop) && li->forwardDep(dop,op)) {
            std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
            getCPDG()->insert_edge(*dep_BeretInst, BeretInst::Complete,
                       *b_inst, BeretInst::Execute, 0,E_SEBD);
          }
        }
        b_inst->reCalculate();
/*        std::cout << b_inst->cycleOfStage(0) << " "
                  << b_inst->cycleOfStage(1) << " "
                  << b_inst->cycleOfStage(2) << "\n";*/
      }

      prev_sg=sg;
    }
  }

  
  void reCalcBeretLoop(bool commit_iter) {
    //recalc loop, and get last cycle
    uint64_t last_cycle=0;
    for(auto i =li2sgmap[li].begin(),e =li2sgmap[li].end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
	b_inst->reCalculate();
        if(b_inst->_isload) {
          //get the MSHR resource here!
          checkNumMSHRs(b_inst); 
        }
	b_inst->reCalculate();  //TODO: check this! : 

        uint64_t cycle=b_inst->cycleOfStage(BeretInst::Complete);
        if(last_cycle < cycle) {
          last_cycle=cycle;
        }
      }
    }

    //write stores from store buffer
    int iseb=0;
    for(auto i=li2sgmap[li].begin(), e=li2sgmap[li].end(); i!=e;++i,++iseb) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        if(commit_iter && b_inst->_isstore) {
          // stores must come after all computation is complete
          checkNumMSHRs(b_inst, last_cycle); 
        }
        //done with this instruction
        //getCPDG()->done(sh_inst);
/*        if (getTraceOutputs()) {
          std::cout << "SEB" << iseb << ":";
          for (unsigned i = 0; i < b_inst->numStages(); ++i) {
            std::cout << b_inst->cycleOfStage(i) << " ";
          }
          std::cout << b_inst->ex_lat() << "\n";*/
        //`}
      }
    }
  }

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    switch(beret_state) {
      case CPU:
        //Started BERET Loop
        if(op->bb_pos()==0) {
	  li = op->func()->getLoop(op->bb());
          if(li) {
            //std::cout << "found a loop\n";
          }
         
          if(li && li2sgmap.count(li)!=0 && li2sgmap[li].size()!=0) {
            //std::cout << " .. and it's beret-able!\n";
            curLoopHead=op;
            beret_state=BERET;
            Inst_t* inst = new Inst_t();
            unsigned config_time = _beret_config_time*li2sgmap[li].size();
            if(li==_prevLoop) {
              config_time=0;
            }
            //This instruction is created to simulate transfering live inputs
            //like loop constants, initial values for induction vars, etc
            inst->set_ex_lat(8/_beret_iops + config_time); //TODO: make this real num vars
            std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
            addDeps(sh_inst);
            pushPipe(sh_inst);
            inserted(sh_inst);
            //inserted(inst,img);

            _curBeretStartCycle=inst->cycleOfStage(inst->eventComplete());
            _curBeretStartInst=index;
	    addLoopIteration(inst,Inst_t::Commit,8);
            _prevLoop=li;
          }
        }
        break;
      case BERET:
        if(op==curLoopHead) { //came back into beret
          reCalcBeretLoop(true); //get correct beret timing
	  addLoopIteration(last_inst,BeretInst::Complete,0);
	} else if(op->bb_pos()==0) {
          if(li->getHotPath()[++_whichBB]!=op->bb()) {
            beret_state = CPU;  //WRONG PATH - SWITCH TO CPU
            std::shared_ptr<BeretInst> binst = 
                           binstMap[li->getHotPath()[_whichBB-1]->firstOp() ];

            reCalcBeretLoop(false); //get correct beret wrong-path timing
            //get timing for beret loop
            uint64_t endBeretCyc=binst->cycleOfStage(binst->eventComplete());
            _totalBeretCycles+=endBeretCyc-_curBeretStartCycle;
            _totalBeretInsts+=index-_curBeretStartInst;

	    //Replay BERET on CPU
	    for(unsigned i=0; i < replay_queue.size();++i) {
	      CP_NodeDiskImage& img = replay_queue[i].first;
	      uint64_t& index = replay_queue[i].second;
              Inst_t* inst = new Inst_t(img,index);
	      if(i==0) {
                getCPDG()->insert_edge(*binst, BeretInst::Complete,
	                               *inst, Inst_t::Fetch,4/_beret_iops,E_BREP); 
	      }
              std::shared_ptr<Inst_t> sh_inst(inst);
              getCPDG()->addInst(sh_inst,index);
              addDeps(sh_inst); //regular add deps
              pushPipe(sh_inst);
              inserted(sh_inst);
	    }
	    //Done Replaying up to the instruction before the bad instruction!
	    //The CPU execution is now caught up.
	    replay_queue.clear();
          }
	}
        break;
      default:
        assert(0); //not sure what to do
        break;
    }

    switch(beret_state) {
      case CPU: {
        //base cpu model
        Inst_t* inst = new Inst_t(img,index);
        std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
        getCPDG()->addInst(sh_inst,index);
        addDeps(sh_inst);
        pushPipe(sh_inst);
        inserted(sh_inst);
        break;
      } case BERET: {
        //make beret instruction
        /*BeretInst* b_inst = new BeretInst(img,index);
        std::shared_ptr<BeretInst> sh_inst = std::shared_ptr<BeretInst>(inst);
        getCPDG()->addInst(sh_inst,index);
        addBeretDeps(*inst);*/

	std::shared_ptr<BeretInst> b_inst = binstMap[op];
        assert(b_inst);
	std::shared_ptr<BeretInst> sh_inst(b_inst);
	getCPDG()->addInst(sh_inst,index);

        //this sets the latency for a beret instruction
        int lat=epLat(img._cc-img._ec,img._opclass,img._isload,
               img._isstore,img._cache_prod,img._true_cache_prod);
        b_inst->updateLat(lat);

	replay_queue.push_back(std::make_pair(img,index));
        break;
      } default:
        assert(0); //not sure what to do
        break;
    }
  }

private:

  virtual void checkNumMSHRs(std::shared_ptr<BeretInst>& n, uint64_t minT=0) {
    int ep_lat=n->ex_lat();
 //epLat(n->_ex_lat,n->_opclass,n->_isload,n->_isstore,
               //   n->_cache_prod,n->_true_cache_prod);

    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,n->_st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 

    int rechecks=0;
    uint64_t extraLat=0;

    uint64_t access_time=reqDelayT + n->cycleOfStage(BeretInst::Execute);

    if (minT > access_time) {
      minT=access_time;
    } 

    if(n->_isload) {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, BeretInst::Execute, mshrT+respDelayT, E_MSHR);
      }
    } else {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, BeretInst::Execute, mshrT+respDelayT, E_MSHR);
          //TODO: Execute is the wrong stage here.... need to add a node here for writeback...
      }
    }
  }


/*
  virtual void addBeretDeps(BeretInst& inst) { 
    setSEBReadyCycle_beret(inst);
    setExecuteCycle_beret(inst);
    setCompleteCycle_beret(inst);
  }

  //This node when current SEB is active
  virtual void setSEBReadyCycle_beret(BeretInst& inst) {
    CCoresBB::iterator I,E;
    for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        BeretInst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, BeretInst::Complete,
                               inst, BeretInst::BBReady, 0);
    }
  }

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_beret(BeretInst &inst) {
    getCPDG()->insert_edge(inst, BeretInst::SEBReady,
                           inst, BeretInst::Execute, 0, true);
    for (int i = 0; i < 7; ++i) {
      int prod = inst._prod[i];
      if (prod <= 0) {
        continue;
      }
      dg_inst_base<T,E>& depInst =
            getCPDG()->queryNodes(inst.index()-prod);

      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             inst, Inst_t::Ready, 0,E_RDep);
    }
    
    //Memory dependence enforced by BB ordering -- if this is going to be
    //relaxed, then go ahead and implement mem dependence

    //memory dependence
    if (n._mem_prod > 0) {
      Inst_t& prev_node = static_cast<Inst_t&>( 
                          getCPDG()->queryNodes(n.index()-n._mem_prod));

      if (prev_node._isstore && n._isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node.index(), dg_inst::Complete,
                                  n, dg_inst::Ready, 0, true);
      } else if (prev_node._isstore && n._isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node.index(), dg_inst::Complete,
                                  n, dg_inst::Complete, 0, true);
      } else if (prev_node._isload && n._isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node.index(), dg_inst::Complete,
                                  n, dg_inst::Complete, 0, true);
      }
    }

  }

  virtual void setCompleteCycle_beret(BeretInst& inst) {
    getCPDG()->insert_edge(inst, BeretInst::Execute,
                           inst, BeretInst::Complete, inst._ex_lat);
  }
*/
  uint64_t numCycles() {
    getCPDG()->finish(maxIndex);
    return getCPDG()->getMaxCycles();
  }



};



#endif //CP_BERET
