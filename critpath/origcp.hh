#ifndef ORIG_CRITICAL_PATH_HH
#define ORIG_CRITICAL_PATH_HH

#include "pugixml/pugixml.hpp"

#include "critpath.hh"
#include "prof.hh"

class OrigCP : public CriticalPath {


  class orig_dg_inst : public dg_inst<dg_event, dg_edge_impl_t<dg_event>> {
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;

    public:
    orig_dg_inst(const CP_NodeDiskImage &img, uint64_t index) :
      dg_inst(img, index) { }
    orig_dg_inst() : dg_inst() {}
  };

  //typedef dg_inst<dg_event, dg_edge_impl_t<dg_event>> Inst_t;
  typedef orig_dg_inst Inst_t;
  dep_graph_impl_t<orig_dg_inst, dg_event, dg_edge_impl_t<dg_event>> cpdg;

public:
  OrigCP() : CriticalPath() {}

  uint64_t numCycles() {
    return getCPDG()->getMaxCycles();
  }
  virtual ~OrigCP() { }

protected:
  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    Inst_t* inst = new Inst_t(img,index);
    std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
    getCPDG()->addInst(sh_inst,index);
    addDeps(*inst,img);
    getCPDG()->pushPipe(sh_inst);
    if (getenv("MAFIA_DUMP_ORIG_PIPE")) {
      dumpInst(sh_inst.get());
    }
  }

  virtual void addDeps(Inst_t& inst,const CP_NodeDiskImage &img) { 
    setFetchCycle(inst,img);
    setDispatchCycle(inst,img);
    setReadyCycle(inst,img);
    setExecuteCycle(inst,img);
    setCompleteCycle(inst,img);
    setCommittedCycle(inst,img);
    setWritebackCycle(inst,img);
  }

  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (!getTraceOutputs())
      return;

    Inst_t& inst = static_cast<Inst_t&>(getCPDG()->queryNodes(index));
    outs() << index + Prof::get().skipInsts/* << "(" << img._seq <<  ")" */ <<": ";
    outs() << inst.cycleOfStage(0) << " ";
    outs() << inst.cycleOfStage(1) << " ";
    outs() << inst.cycleOfStage(2) << " ";
    outs() << inst.cycleOfStage(3) << " ";
    outs() << inst.cycleOfStage(4) << " ";
    outs() << inst.cycleOfStage(5) << " ";

    if (img._isstore) {
      outs() << inst.cycleOfStage(6) << " ";
    }

    CriticalPath::traceOut(index,img,op);
    outs() << "\n";
  }


  dep_graph_impl_t<orig_dg_inst,dg_event,dg_edge_impl_t<dg_event>>* getCPDG(){
    return &cpdg;
  }

  virtual void setFetchCycle(Inst_t& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst.index()-1, Inst_t::Fetch,
                           inst, Inst_t::Fetch,
                           img._fc);
  }

  virtual void setDispatchCycle(Inst_t& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, Inst_t::Fetch,
                           inst, Inst_t::Dispatch,
                           /*img._dc*/4);
  }

  virtual void setReadyCycle(Inst_t& inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, Inst_t::Dispatch,
                           inst, Inst_t::Ready,
                           /*img._rc - img._dc*/1);
  }

  virtual void setExecuteCycle(Inst_t &inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, Inst_t::Ready,
                           inst, Inst_t::Execute,
                           /*img._ec - img._rc*/1);
  }
  virtual void setCompleteCycle(Inst_t &inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, Inst_t::Execute,
                           inst, Inst_t::Complete,
                           img._ep_lat);
  }
  virtual void setCommittedCycle(Inst_t &inst, const CP_NodeDiskImage &img) {
    getCPDG()->insert_edge(inst, Inst_t::Complete,
                           inst, Inst_t::Commit,
                           /*img._cmpc - img._cc*/2);
  }
  virtual void setWritebackCycle(Inst_t &inst, const CP_NodeDiskImage &img) {
    if(inst._isstore) {
      getCPDG()->insert_edge(inst, Inst_t::Commit,
                             inst, Inst_t::Writeback,
                             img._st_lat);
    }
  }
 
};

#endif
