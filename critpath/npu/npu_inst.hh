
#ifndef NPU_INST_HH
#define NPU_INST_HH

#include "cp_dg_builder.hh"

namespace npu {

  class npu_inst : public dg_inst<dg_event,
                                  dg_edge_impl_t<dg_event> > {
  public:
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    unsigned _exec_latency = 32;
    npu_inst(unsigned lat): dg_inst<T, E>(),
                            _exec_latency(lat)
    {
      this->isAccelerated = true;

    }

    bool hasDisasm() const { return true;}
    std::string getDisasm() const {
      return "npu_exec";
    }

    virtual int adjustExecuteLatency(int lat) const {
      return _exec_latency;
    }

  };

} // end namespace NPU

#endif
