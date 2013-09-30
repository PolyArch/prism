#ifndef CP_CCORES
#define CP_CCORES

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "cp_ccores_all.hh"

extern int TraceOutputs;

class cp_ccores : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
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


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if(TraceOutputs) {
      dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);  
      if(inst.isPipelineInst()) {
        CP_DG_Builder::traceOut(index,img,op);
      } else {  
        out << index + Prof::get().skipInsts << ": ";
        out << inst.cycleOfStage(0) << " ";
        out << inst.cycleOfStage(1) << " ";
        out << inst.cycleOfStage(2) << " ";
        CriticalPath::traceOut(index,img,op);
        out << "\n";
      }
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
      ccores_inst* cc_inst = new ccores_inst(img,index);
      std::shared_ptr<ccores_inst> sh_inst(cc_inst);
      getCPDG()->addInst(sh_inst,index);

      if(transitioned) {
        Inst_t* prevInst = getCPDG()->peekPipe(-1);
        assert(prevInst);
        T* event_ptr = new T();
        cur_bb_end.reset(event_ptr);
        getCPDG()->insert_edge(*prevInst, Inst_t::Commit,
                               *cur_bb_end, 8, E_CXFR);
      }
  
      if(op->isBBHead() || op->isMem()) {
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
  typedef std::vector<std::shared_ptr<ccores_inst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;


  virtual void addCCoreDeps(ccores_inst& inst,const CP_NodeDiskImage &img) { 
    setBBReadyCycle_cc(inst,img);
    setExecuteCycle_cc(inst,img);
    setCompleteCycle_cc(inst,img);
  }

  //This node when current ccores BB is active
  virtual void setBBReadyCycle_cc(ccores_inst& inst, const CP_NodeDiskImage &img) {
    CCoresBB::iterator I,E;
    /*for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        ccores_inst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, ccores_inst::Complete,
                               inst, ccores_inst::BBReady, 0);
    }*/
    if(prev_bb_end) {
      inst.startBB=prev_bb_end;
      getCPDG()->insert_edge(*prev_bb_end,
                               inst, ccores_inst::BBReady, 0);
    }
  }

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_cc(ccores_inst &inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, ccores_inst::BBReady,
                           inst, ccores_inst::Execute, 0, true);

    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst.index()) {
        continue;
      }
      dg_inst_base<T,E>& dep_inst = getCPDG()->queryNodes(inst.index()-prod);
      getCPDG()->insert_edge(dep_inst, dep_inst.eventComplete(),
                             inst, ccores_inst::Execute, 0, true);
    }
    //Memory dependence enforced by BB ordering -- if this is going to be
    //relaxed, then go ahead and implement mem dependence
/*
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
*/
  }

  virtual void setCompleteCycle_cc(ccores_inst& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, ccores_inst::Execute,
                           inst, ccores_inst::Complete, inst._ex_lat);
    if(cur_bb_end) {
      inst.endBB = cur_bb_end; // have instruction keep
      getCPDG()->insert_edge(inst, ccores_inst::Complete,
                               *cur_bb_end, 0);
    }

  }



  uint64_t numCycles() {
    getCPDG()->finish(maxIndex);
    return getCPDG()->getMaxCycles();
  }



};

static RegisterCP<cp_ccores> cp_ccores1("ccores",false);
static RegisterCP<cp_ccores> cp_ccores2("ccores",true);


#endif //CP_CCORES
