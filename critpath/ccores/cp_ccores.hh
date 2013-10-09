#ifndef CP_CCORES
#define CP_CCORES

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "cp_ccores_all.hh"
#include "pathprof.hh"
//#include "functioninfo.hh"
//#include "loopinfo.hh"
#include "mutable_queue.hh"


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

  unsigned _ccores_num_mem=1, _ccores_bb_runahead=1, _ccores_max_ops=1000;
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

  }

  class CTarget {
    double computeValue();
    std::set<LoopInfo*> lichildren;
    std::set<FunctionInfo*> fichildren;
  };

  class CTargetFunc {
    FunctionInfo* fi;
    double computeValue() {
      return 0; 
    }
  };

  class CTargetLoop {
    LoopInfo* li;
    double computeValue() {
         return 0;
    }

  };



  std::map<LoopInfo*,bool> liAccel;
  std::map<FunctionInfo*,bool> fiAccel;

  virtual void setDefaultsFromProf() {
    CP_DG_Builder::setDefaultsFromProf();

    //set up the subgraphs
    std::multimap<uint64_t,LoopInfo*> loops;
    PathProf::FuncMap::iterator i,e;
    for(i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo& fi = *i->second;
      FunctionInfo::LoopList::iterator li,le;
      for(li=fi.li_begin(),le=fi.li_end();li!=le;++li) {
        //LoopInfo* loopInfo = li->second;
        //loops.insert(std::make_pair(loopInfo->numInsts(),loopInfo));
      }
    } 
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

  bool prevCall = false;
  bool prevRet = false;
  bool inCCore = false;

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) {

    bool transitioned=false;

    if(!inCCore) {
      if((prevCall) && !op->func()->hasFuncCalls()) {
        inCCore=true;
        transitioned=true;
      }

      prevCall = op->isCall();
    }

    if(inCCore) {
      CCoresInst* cc_inst = new CCoresInst(img,index);
      std::shared_ptr<CCoresInst> sh_inst(cc_inst);
      getCPDG()->addInst(sh_inst,index);

      if(transitioned) {
        Inst_t* prevInst = getCPDG()->peekPipe(-1);
        assert(prevInst);
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
        getCPDG()->insert_edge(*prevInst, Inst_t::Commit,
                               *cur_bb_end, 8, E_CXFR);
      }
  
      if(endOfBB(op,img)) {
        //only one memory instruction per basic block
        prev_bb_end=cur_bb_end;
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
      }
      addCCoreDeps(*cc_inst,img);

      prevRet = op->isReturn();
      if(prevRet) {
        inCCore=false;
      }
    } else {


      Inst_t* inst = new Inst_t(img,index);
      std::shared_ptr<Inst_t> sh_inst(inst);
      getCPDG()->addInst(sh_inst,index);
      if(prevRet) {
        getCPDG()->insert_edge(*cur_bb_end,
                               *inst, Inst_t::Fetch, 2, E_CXFR);
      }
      addDeps(sh_inst);
      pushPipe(sh_inst);
      inserted(sh_inst);
    }

  }

private:
  typedef std::vector<std::shared_ptr<CCoresInst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;


  virtual void addCCoreDeps(CCoresInst& inst,const CP_NodeDiskImage &img) { 
    setBBReadyCycle_cc(inst,img);
    setExecuteCycle_cc(inst,img);
    setCompleteCycle_cc(inst,img);
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

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_cc(CCoresInst &inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, CCoresInst::BBReady,
                           inst, CCoresInst::Execute, 0, true);

    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst.index()) {
        continue;
      }
      dg_inst_base<T,E>& dep_inst = getCPDG()->queryNodes(inst.index()-prod);
      getCPDG()->insert_edge(dep_inst, dep_inst.eventComplete(),
                             inst, CCoresInst::Execute, 0, true);
    }

    //Memory dependence enforced by BB ordering, in the restricted case
    //when when bb-runahead is on, or num-mem is >1, then we should enforce this
    //of course, this means that CCORES would need dependence resolution hardware

    //memory dependence
     //memory dependence
    if (inst._mem_prod > 0) {
      Inst_t& prev_node = static_cast<Inst_t&>( 
                          getCPDG()->queryNodes(inst.index()-inst._mem_prod));

      if (prev_node._isstore && inst._isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  inst, CCoresInst::Execute, 0, true);
      } else if (prev_node._isstore && inst._isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  inst, CCoresInst::Complete, 0, true);
      } else if (prev_node._isload && inst._isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node.index(), prev_node.eventComplete(),
                                  inst, CCoresInst::Complete, 0, true);
      }
    }
  }

  virtual void setCompleteCycle_cc(CCoresInst& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, CCoresInst::Execute,
                           inst, CCoresInst::Complete, inst._ex_lat);

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



};



#endif //CP_CCORES
