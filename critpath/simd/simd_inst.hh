
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
    simd_inst(const CP_NodeDiskImage &img, uint64_t index):
      dg_inst<T, E>(img, index) {}
    simd_inst() : dg_inst<T, E> () {}
  };

}

#endif
