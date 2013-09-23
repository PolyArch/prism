
#ifndef CP_SIMD_HH
#define CP_SIMD_HH

#include "cp_dg_builder.hh"
#include "cp_registry.hh"

#include "simd_inst.hh"

namespace simd {


  class cp_simd : public CP_DG_Builder<dg_event,
                                       dg_edge_impl_t<dg_event> > {

    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;
    typedef std::shared_ptr<simd_inst> SimdInstPtr;

    dep_graph_impl_t<Inst_t, T, E> cpdg;

  public:
    cp_simd() : CP_DG_Builder<T, E> () {
    }

    virtual ~cp_simd() {}

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

  };

};

#endif
