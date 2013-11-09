
#ifndef DYSER_INST_HH
#define DYSER_INST_HH

#include "cp_dg_builder.hh"


namespace DySER {

  class dyser_inst : public dg_inst<dg_event,
                                    dg_edge_impl_t<dg_event> > {
  public:
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef T* TPtr;
    typedef E* EPtr;

    dyser_inst(): dg_inst<T, E>() {}

    dyser_inst(const CP_NodeDiskImage &img, uint64_t index):
      dg_inst<T, E>(img, index) {}

    bool hasDisasm() const { return true; }
    std::string getDisasm() const {
      return "dyser_inst";
    }

  };

  // Instructions that execute inside dyser.
  class dyser_compute_inst: public dyser_inst {

  public:
    dyser_compute_inst(const CP_NodeDiskImage &img, uint64_t index):
      dyser_inst(img, index) { }

    dyser_compute_inst(): dyser_inst() {}

    enum EventTy {
      DyReady = 0,
      DyExecute = 1,
      DyComplete = 2,
      DyNumStages = 3
    };

    T& operator[](const unsigned i) {
      if (i == dyser_compute_inst::DyReady)
        return events[DyReady];
      if (i == dyser_compute_inst::DyExecute)
        return events[DyExecute];
      if (i == dyser_compute_inst::DyComplete)
        return events[DyComplete];
      assert(0);
      return events[0];
    }


    void reCalculate() {
      for(int i = 0; i < DyNumStages; ++i) {
        events[i].reCalculate();
      }
    }


    virtual unsigned numStages() {
      return DyNumStages;
    }
    virtual uint64_t cycleOfStage(const unsigned i) {
      return events[i].cycle();
    }
    virtual unsigned eventComplete() {
      return DyComplete;
    }

    bool hasDisasm() const { return true; }
    std::string getDisasm() const {
      static char buf[256];
      sprintf(buf, "dyser_compute %p: %s",
              (void*)this,
              ExecProfile::getDisasm(this->_pc, this->_upc).c_str());
      return std::string(buf);
    }

  };

  class dyser_sincos_inst : public dyser_compute_inst {
  public:
    dyser_sincos_inst() : dyser_compute_inst() {}

    std::string getDisasm() const {
      return "dyser_sincos";
    }
  };

  class dyser_pipe_inst : public dyser_inst {
  public:
    dyser_pipe_inst(): dyser_inst() {

      // not a memory or ctrl
      _isload = _isstore = _isctrl = _ctrl_miss = false;

      // no icache_miss
      _icache_lat = 0;

      // no memory
      _mem_prod = _cache_prod = 0;

      // executes in 1 cycle
      _ex_lat = 1;

      // not a serialization instruction
      _serialBefore =  _serialAfter = _nonSpec = false;

      _st_lat = 0;
      _pc = -1; // what is the pc
      _upc = -1; // what is the upc
      _floating = false;
      _iscall = false;
      _numSrcRegs = 0;
      _numFPSrcRegs = 0;
      _numIntSrcRegs = 0;
      _numFPDestRegs = 0;
      _numIntDestRegs = 0;
      _eff_addr = 0;

      for (int i = 0; i < NumStages; ++i) {
        events[i].set_inst(this);
        events[i].prop_changed();
      }
    }
    bool hasDisasm() const { return true; }

    std::string getDisasm() const {
      return "dyser_pipe_inst";
    }

  };

  // DySER send instruction.
  class dyser_send : public dyser_pipe_inst {

  public:
    dyser_send(): dyser_pipe_inst() {
      // Its operand is just the node added before
      _prod[0] = 1;
      // one source reg
      _numSrcRegs = 1;
    }

    std::string getDisasm() const {
      return "dyser_send 1";
    }

  };

  class dyser_recv : public dyser_pipe_inst {

  public:
    dyser_recv(): dyser_pipe_inst() {
      // It producer is in dyser??
      _numIntDestRegs = 1;
    }

    std::string getDisasm() const {
      return "dyser_recv";
    }

  };

  class dyser_config : public dyser_pipe_inst {
  public:
    dyser_config(unsigned numCyclesToFetch): dyser_pipe_inst() {
      _icache_lat = numCyclesToFetch;
    }
    std::string getDisasm() const {
      return "dyser_config";
    }

  };


} // end namespace dyser

#endif
