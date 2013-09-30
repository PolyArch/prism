#ifndef CP_BERET
#define CP_BERET

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

extern int TraceOutputs;

class beret_inst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
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
      NumStages
  };
private:
  T events[NumStages];

public:
  virtual ~beret_inst() { 
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }*/
  }


  uint16_t _opclass=0;
  bool _isload=false;
  bool _isstore=false;
  bool _isctrl=false;
  uint16_t _prod[7];
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint16_t _ex_lat=0;

  E* ex_edge;

  beret_inst(const CP_NodeDiskImage &img, uint64_t index):
              dg_inst_base<T,E>(index){
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _ex_lat=img._cc-img._ec;
  }

  beret_inst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void updateLat(uint16_t lat) {
    ex_edge->_len=lat;
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
};


// CP_BERET !!!
class cp_beret : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
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


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if(TraceOutputs) {
      out << index + Prof::get().skipInsts << ": ";
  
      dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);  
  
      for(unsigned i = 0; i < inst.numStages();++i) { 
        out << inst.cycleOfStage(i) << " ";
      }
  
      CriticalPath::traceOut(index,img,op);
  
      out << "\n";
    }
  }

  virtual void accelSpecificStats(std::ostream& out) {
    //out << "Beret Cycles = " << totalBeretCycles << "\n";
    out << " (beret-only " << totalBeretCycles << ")";
  }

  /*
   *  Beret Iteration State
   */
  unsigned beret_state;
  std::map<Op*,beret_inst*> binstMap;
  beret_inst* first_inst=NULL;
  beret_inst* last_inst=NULL;
  LoopInfo* li;
  Op* curLoopHead;
  std::vector<std::pair<CP_NodeDiskImage,uint64_t>> replay_queue;
  unsigned whichBB;
  uint64_t curBeretStartCycle;
  uint64_t totalBeretCycles;

  void addLoopIteration(dg_inst_base<T,E>* inst, unsigned eventInd) {
    Subgraph* prev_sg=NULL;
    first_inst=NULL;
    binstMap.clear();
    replay_queue.clear();
    whichBB=0;
    //create instruction for each SEB
    for(auto i =li->sg_begin(),e =li->sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        beret_inst* b_inst = new beret_inst();
	if(first_inst==NULL) {
          first_inst=b_inst;
        }
	last_inst=b_inst;
        binstMap[op]=b_inst;
        getCPDG()->insert_edge(*b_inst, beret_inst::SEBReady,
                               *b_inst, beret_inst::Execute, 0, E_SEBE);

	b_inst->ex_edge = 
	    getCPDG()->insert_edge(*b_inst, beret_inst::Execute,
	                           *b_inst, beret_inst::Complete,op->avg_lat(), E_SEBC);
/*        std::cout << "X " << b_inst->cycleOfStage(0) << " "
                  << b_inst->cycleOfStage(1) << " "
                  << b_inst->cycleOfStage(2) << "\n";*/
      }
      prev_sg=sg;
    }

    prev_sg=NULL;
    //Add Deps
    for(auto i =li->sg_begin(),e =li->sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        beret_inst* b_inst = binstMap[op];
        //add subgraph deps to previous subgraph
        if(prev_sg!=NULL) {
          for(auto si=prev_sg->op_begin(),se=prev_sg->op_end();si!=se;++si){
            Op* ps_op = *si;
            beret_inst* ps_beret_inst = binstMap[ps_op];
            getCPDG()->insert_edge(*ps_beret_inst, beret_inst::Complete,
                                   *b_inst, beret_inst::SEBReady, 0, E_SEBA);
          } 
        } else {
          getCPDG()->insert_edge(*inst, eventInd,
                                 *b_inst,beret_inst::SEBReady,0,E_SEBS);
          /*std::cout << inst->cycleOfStage(eventInd) << " "
                    << b_inst->cycleOfStage(beret_inst::SEBReady) << "\n";*/
        }
        for(auto di = op->d_begin(),de = op->d_end();di!=de;++di) {
          Op* dop = *di;
          if(li->dependenceInPath(op,dop)) {
            beret_inst* dep_beret_inst = binstMap[dop];
            getCPDG()->insert_edge(*dep_beret_inst, beret_inst::Complete,
                       *b_inst, beret_inst::Execute, 0,E_SEBD);
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

  void reCalcBeretLoop() {
    for(auto i =li->sg_begin(),e =li->sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->op_begin(),ope=sg->op_end();opi!=ope;++opi) {
        Op* op = *opi;
        beret_inst* b_inst = binstMap[op];
	b_inst->reCalculate();
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
          if(li && li->hasSubgraphs()) {
            //std::cout << " .. and it's beret-able!\n";
            curLoopHead=op;
            beret_state=BERET;
            Inst_t* inst = new Inst_t();

            //This instruction is created to simulate transfering live inputs
            //like loop constants, initial values for induction vars, etc
            inst->set_ex_lat(5);
            std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
            addDeps(sh_inst);
            pushPipe(sh_inst);
            inserted(sh_inst);
            //inserted(inst,img);

            curBeretStartCycle=inst->cycleOfStage(inst->eventComplete());
	    addLoopIteration(inst,Inst_t::Commit);
          }
        }
        break;
      case BERET:
        if(op==curLoopHead) { //came back into beret
	  //`beret_inst* binst = binstMap[op];
          reCalcBeretLoop(); //get correct beret timing
	  addLoopIteration(last_inst,beret_inst::Complete);
	} else if(op->bb_pos()==0) {
          if(li->getHotPath()[++whichBB]!=op->bb()) {
            beret_state = CPU;  //WRONG PATH - SWITCH TO CPU
     	    //beret_inst* binst = binstMap[op];
            beret_inst* binst = binstMap[li->getHotPath()[whichBB-1]->firstOp() ];

            reCalcBeretLoop(); //get correct beret wrong-path timing
            //get timing for beret loop
            uint64_t endBeretCyc=binst->cycleOfStage(binst->eventComplete());
            totalBeretCycles+=endBeretCyc-curBeretStartCycle;

	    //Replay BERET on CPU
	    for(unsigned i=0; i < replay_queue.size();++i) {
	      CP_NodeDiskImage& img = replay_queue[i].first;
	      uint64_t& index = replay_queue[i].second;
              Inst_t* inst = new Inst_t(img,index);
	      if(i==0) {
                getCPDG()->insert_edge(*binst, beret_inst::Complete,
	                               *inst, Inst_t::Fetch,0,E_BREP); 
	      }
              std::shared_ptr<Inst_t> sh_inst(inst);
              getCPDG()->addInst(sh_inst,index);
              addDeps(sh_inst);
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
        /*beret_inst* b_inst = new beret_inst(img,index);
        std::shared_ptr<beret_inst> sh_inst = std::shared_ptr<beret_inst>(inst);
        getCPDG()->addInst(sh_inst,index);
        addBeretDeps(*inst);*/

	beret_inst* b_inst = binstMap[op];
        assert(b_inst);
	std::shared_ptr<beret_inst> sh_inst(b_inst);
	getCPDG()->addInst(sh_inst,index);
	b_inst->updateLat(img._cc-img._ec);
	replay_queue.push_back(std::make_pair(img,index));
        break;
      } default:
        assert(0); //not sure what to do
        break;
    }
  }

private:
  typedef std::vector<std::shared_ptr<beret_inst>> CCoresBB;
  CCoresBB prev_bb,current_bb;

/*
  virtual void addBeretDeps(beret_inst& inst) { 
    setSEBReadyCycle_beret(inst);
    setExecuteCycle_beret(inst);
    setCompleteCycle_beret(inst);
  }

  //This node when current SEB is active
  virtual void setSEBReadyCycle_beret(beret_inst& inst) {
    CCoresBB::iterator I,E;
    for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        beret_inst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, beret_inst::Complete,
                               inst, beret_inst::BBReady, 0);
    }
  }

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_beret(beret_inst &inst) {
    getCPDG()->insert_edge(inst, beret_inst::SEBReady,
                           inst, beret_inst::Execute, 0, true);
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

  virtual void setCompleteCycle_beret(beret_inst& inst) {
    getCPDG()->insert_edge(inst, beret_inst::Execute,
                           inst, beret_inst::Complete, inst._ex_lat);
  }
*/
  uint64_t numCycles() {
    getCPDG()->finish(maxIndex);
    return getCPDG()->getMaxCycles();
  }



};



#endif //CP_BERET
