#ifndef CP_CCORES_ALL
#define CP_CCORES_ALL

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>


class CCoresInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      BBReady = 0,
      Execute = 1,
      Complete = 2,
      NumStages
  };
private:
  T events[NumStages];

public:
 std::shared_ptr<T>   endBB;
 std::shared_ptr<T> startBB;

  virtual ~CCoresInst() { 
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }
    endBB->remove_all_edges();
    startBB->remove_all_edges();*/
  }


  int16_t _opclass=0;
  bool _isload=false;
  bool _isstore=false;
  bool _isctrl=false;
  uint16_t _icache_lat=0;
  uint16_t _prod[7];
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint16_t _ex_lat=0;

  CCoresInst(const CP_NodeDiskImage &img, uint64_t index):
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

  CCoresInst() : dg_inst_base<T,E>() {}

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

};

static bool endOfBB(Op* op, const CP_NodeDiskImage& img) {
  return img._serialBefore || img._serialAfter || 
      op->isBBHead()|| op->isMem();
}


class cp_ccores_all : public ArgumentHandler,
    public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;

  typedef dg_inst<T, E> Inst_t;

public:
  cp_ccores_all() : CP_DG_Builder<T,E>() {
  }

  virtual ~cp_ccores_all() {
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (!getTraceOutputs())
      return;

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);

    outs() << index + Prof::get().skipInsts << ": ";
    outs() << inst.cycleOfStage(0) << " ";
    outs() << inst.cycleOfStage(1) << " ";
    outs() << inst.cycleOfStage(2) << " ";

    CriticalPath::traceOut(index,img,op);
    outs() << "\n";
  }


  void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) {
/*    Inst_t* inst = new Inst_t(img,index);
    std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
    getCPDG()->addInst(sh_inst,index);
    addDeps(*inst,img);
    getCPDG()->pushPipe(sh_inst);
    inserted(*inst,img);*/
    CCoresInst* cc_inst = new CCoresInst(img,index);
    std::shared_ptr<CCoresInst> sh_inst(cc_inst);
    getCPDG()->addInst(sh_inst,index);


    if (op->isBBHead() || op->isMem()) {
      //only one memory instruction per basic block
      prev_bb_end=cur_bb_end;
      T* event_ptr = new T();
      cur_bb_end.reset(event_ptr);
    }
    addDeps(*cc_inst,img);
  }

private:
  typedef std::vector<std::shared_ptr<CCoresInst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;


  virtual void addDeps(CCoresInst& inst,const CP_NodeDiskImage &img) { 
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
      getCPDG()->insert_edge(inst.index()-prod, CCoresInst::Complete,
                             inst, CCoresInst::Execute, 0, true);
    }
    //Memory dependence enforced by BB ordering, in the restricted case
    //when when bb-runahead is on, then 

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



#endif //CP_CCORES_ALL
