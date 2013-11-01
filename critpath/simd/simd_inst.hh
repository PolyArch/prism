
#ifndef CP_SIMD_INST_HH
#define CP_SIMD_INST_HH

#include "cp_dg_builder.hh"

namespace simd {

  class simd_inst : public dg_inst<dg_event,
                                   dg_edge_impl_t<dg_event> > {
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef T* TPtr;
    typedef E* EPtr;

  public:
    simd_inst() : dg_inst<T, E> () {}
  };

  class shuffle_inst: public simd_inst {
  public:
    shuffle_inst() {
      _opclass = 0;
      // not a memory or ctrl
      _isload = _isstore = _isctrl = _ctrl_miss = false;
      // no icache_miss
      _icache_lat = 0;

      // Its op is just a node added...
      _prod[0] = 1;

      // no memory
      _mem_prod = _cache_prod = 0;
      // executes in 1 cycle
      _ex_lat = 1;
      // not a serialization instruction
      _serialBefore =  _serialAfter = _nonSpec = false;

      _st_lat = 0;
      _pc = -1; // what is the pc
      _upc = -1; // what is the upc
      _floating = true;
      _iscall = false;
      _numSrcRegs = 1;
      _numFPSrcRegs = 1;
      _numIntSrcRegs = 0;
      _numFPDestRegs = 2;
      _numIntDestRegs = 0;
      _eff_addr = 0;

      for (int i = 0; i < NumStages; ++i) {
        events[i].set_inst(this);
        events[i].prop_changed();
      }
    }

    virtual bool hasDisasm() const { return true; }
    virtual std::string getDisasm() const {
      static char buf[256];
      sprintf(buf, "shuffle_inst %d", _prod[0]);
      return std::string(buf);
    }

  };

  class pack_inst: public simd_inst {
  public:
    pack_inst(int prod1, int prod2) {
      _opclass = 0;
      // not a memory or ctrl
      _isload = _isstore = _isctrl = _ctrl_miss = false;
      // no icache_miss
      _icache_lat = 0;

      // Its op is just a node added...
      _prod[0] = prod1;
      _prod[1] = prod2;

      // no memory
      _mem_prod = _cache_prod = 0;
      // executes in 1 cycle
      _ex_lat = 1;
      // not a serialization instruction
      _serialBefore =  _serialAfter = _nonSpec = false;

      _st_lat = 0;
      _pc = -1; // what is the pc
      _upc = -1; // what is the upc
      _floating = true;
      _iscall = false;
      _numSrcRegs = 1;
      _numFPSrcRegs = 1;
      _numIntSrcRegs = 0;
      _numFPDestRegs = 2;
      _numIntDestRegs = 0;
      _eff_addr = 0;

      for (int i = 0; i < NumStages; ++i) {
        events[i].set_inst(this);
        events[i].prop_changed();
      }
    }

    bool hasDisasm() const { return true; }
    std::string getDisasm() const {
      static char buf[256];
      sprintf(buf, "pack_inst %d %d", _prod[0], _prod[1]);
      return std::string(buf);
    }
  };
}

#endif
