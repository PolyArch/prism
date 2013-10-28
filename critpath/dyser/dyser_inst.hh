
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
  };

  // Instructions that execute inside dyser.
  class dyser_compute_inst: public dyser_inst {

  public:
    dyser_compute_inst(const CP_NodeDiskImage &img, uint64_t index):
      dyser_inst(img, index) { }

    dyser_compute_inst(): dyser_inst() {}

    enum EventTy {
      OpReady = 0,
      Execute = 1,
      Complete = 2,
      NumStages = 3
    };

    T& operator[](const unsigned i) {
      if (i == dyser_inst::Ready)
        return events[OpReady];
      if (i == dyser_inst::Execute)
        return events[Execute];
      if (i == dyser_inst::Complete)
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
  };

  class dyser_recv : public dyser_pipe_inst {

  public:
    dyser_recv(): dyser_pipe_inst() {
      // It producer is in dyser??
      _numIntDestRegs = 1;
    }

  };


} // end namespace dyser

#endif
