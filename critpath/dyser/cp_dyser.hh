
#ifndef CP_DYSER_HH
#define CP_DYSER_HH

#include "cp_dg_builder.hh"
#include "cp_registry.hh"

namespace DySER {
  class dyser_inst : public dg_inst<dg_event,
                                    dg_edge_impl_t<dg_event> > {

    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef T* TPtr;
    typedef E* EPtr;

    T events[3];

  public:
    dyser_inst(const CP_NodeDiskImage &img, uint64_t index):
      dg_inst<T, E>(img, index) {

    }
    dyser_inst(): dg_inst<T, E>() {} 

    enum EventTy {
      OpReady = 0,
      Execute = 1,
      Complete = 2,
      NumStages = 3
    };

    T& operator[](const unsigned i) {
      if (i == dg_inst<T, E>::Ready)
        return events[OpReady];
      if (i == dg_inst<T, E>::Execute)
        return events[Execute];
      if (i == dg_inst<T, E>::Complete)
        return events[Complete];
      assert(0);
      return events[0];
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

  class cp_dyser : public CP_DG_Builder<dg_event,
                                        dg_edge_impl_t<dg_event> > {
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;
    typedef std::shared_ptr<dyser_inst> DyInstPtr;

    enum ModelState {
      IN_CPU,  // all instructions execute in CPU
      IN_DYSER // LoadSlice in CPU, compSlice in DYSER
    };

    LoopInfo *li;
    Op* curLoopHead;
    ModelState dyser_model_state;


    std::vector<InstPtr> CurInsts;
    std::vector<DyInstPtr> CurDySERInsts;
    //FIXME::This should be indexed by pc not the index
    std::map<uint64_t, bool> isInLS;
    void doSlice() {
      if (CurInsts.empty())
        return;
      for (int curIdx = CurInsts.size() - 1; curIdx >= 0; --curIdx) {
        InstPtr instPtr = CurInsts[curIdx];
        if (instPtr->_isload) {
          isInLS[instPtr->_index] = true;
        }
        // skip others for now
        //else if (instPtr->_isstore) {
        //}

        if (isInLS[instPtr->_index]) {
          // Marks its dependence also in LS
          for (int i = 0; i < 7; ++i) {
            int prod = instPtr->_prod[i];
            if ((uint64_t)prod >= instPtr->index())
              continue;
            int depIdx = curIdx - prod;
            if (depIdx < 0)
              continue; // dependence from outside
            InstPtr depInst = CurInsts[depIdx];
            isInLS[depInst->_index] = true;
          }
        }
        if (instPtr->_isstore) {
          isInLS[instPtr->_index] = true;
          // FIXME::Handle address computation
        }
        if (instPtr->_isctrl) {
          isInLS[instPtr->_index] = true;
        }
      }
    }
    void setDataDep(DyInstPtr &n) {

      for (int i = 0; i < 7; ++i) {
        int prod = n->_prod[i];
        if ((uint64_t)prod >= n->index()) {
          continue;
        }
        dg_inst_base<T,E>& depInst =
          getCPDG()->queryNodes(n->index()-prod);

        getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                               *n, Inst_t::Ready,
                               0,
                               E_RDep);
      }
    }
    void setReadyToExecute(DyInstPtr &n) {
      getCPDG()->insert_edge(*n, Inst_t::Ready,
                             *n, Inst_t::Execute, 0,
                             E_RE);
    }
    void setExecuteToComplete(DyInstPtr &n) {
      getCPDG()->insert_edge(*n, Inst_t::Execute,
                             *n, Inst_t::Complete,
                             n->_ex_lat,
                             E_EP);

    }

    bool isInCS(Op* op) { return false; }
  public:
    cp_dyser(): CP_DG_Builder<T, E> () {
      dyser_model_state = IN_CPU;
    }
    virtual ~cp_dyser() {}

   virtual void traceOut(uint64_t index,
                         const CP_NodeDiskImage &img, Op* op) {
    }

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }
    dep_graph_impl_t<Inst_t, T, E> cpdg;

    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {
      ModelState prev_model_state = dyser_model_state;

      switch (dyser_model_state) {
      default:
      case IN_CPU:
        //can we switch to dyser state?
        if (op->bb_pos() == 0) {
          // we are in top of basicblock
          li = op->func()->getLoop(op->bb());
          if (li && li->isInnerLoop()) {
            // We are inside a innerLoop, switch to dyser
            dyser_model_state = IN_DYSER;
            //std::cout << "Switched to DySER\n";
          }
        }
        break;
      case IN_DYSER:
        if (li != op->func()->getLoop(op->bb())) {
          //We are in a different loop??
          dyser_model_state = IN_CPU;
          li = 0;
        }
        break;
      }
#if 0
      Inst_t* inst = new Inst_t(img, index);
      std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);

      // add Instruction to the instruction trace <-- whatever I read from
      getCPDG()->addInst(sh_inst, index);

      // Adds Dependence Edges between events
      //   Different for IN_CPU and IN_DYSER
      addDeps(inst);

      // For the Pipeline -- should only happen for CPU Instruction
      getCPDG()->pushPipe(sh_inst);

      inserted(*inst, img);
#endif
      switch (dyser_model_state) {
      case IN_DYSER:  {
        //Create the instruction, but do not add to CPDG yet
        InstPtr inst = InstPtr(new Inst_t (img, index));
        CurInsts.push_back(inst);
        DyInstPtr dyinst = DyInstPtr(new dyser_inst(img, index));
        CurDySERInsts.push_back(dyinst);
        break;
      }
      case IN_CPU:
      default: {
        if (prev_model_state == IN_DYSER) {
          //std::cout << "Switch back to CPU\n";
          doSlice();
          for (unsigned i = 0, e = CurInsts.size(); i != e; ++i) {
            InstPtr sh_inst = CurInsts[i];
            if (isInLS[sh_inst->_index]) {
              getCPDG()->addInst(sh_inst, sh_inst->_index);
              addDeps(sh_inst);
              getCPDG()->pushPipe(sh_inst);
              inserted(sh_inst);
            } else {
              DyInstPtr dy_inst = CurDySERInsts[i];
              getCPDG()->addInst(dy_inst, dy_inst->_index);
              //Assumptions::
              // All Instruction in CS can be scheduled to DySER
              // Latency between complete->next instruction is zero
              //Honor data dependence
              setDataDep(dy_inst);
              //Honor ready to Execute
              setReadyToExecute(dy_inst);
              //Honor Execute->Complete (latency)
              setExecuteToComplete(dy_inst);
            }
          }
          CurInsts.clear();
          CurDySERInsts.clear();
          isInLS.clear();
        }
        Inst_t* inst = new Inst_t(img,index);
        std::shared_ptr<Inst_t> sh_inst = std::shared_ptr<Inst_t>(inst);
        getCPDG()->addInst(sh_inst,index);
        addDeps(sh_inst);
        getCPDG()->pushPipe(sh_inst);
        inserted(sh_inst);
      }
      }
    }
    uint64_t numCycles() {
      getCPDG()->finish(maxIndex);
      return getCPDG()->getMaxCycles();
    }

  };
}


#endif
