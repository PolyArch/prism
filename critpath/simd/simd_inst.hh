
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
  };

}

#endif
