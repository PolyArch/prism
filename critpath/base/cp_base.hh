#ifndef DEFAULT_CPDG_HH
#define DEFAULT_CPDG_HH
#include "cp_dg_builder.hh"


class default_cpdg_t : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>>
{
public:
  dep_graph_t<Inst_t, dg_event, dg_edge_impl_t<dg_event>>* getCPDG()
  {
    return &cpdg;
  }

  dep_graph_impl_t<Inst_t, dg_event, dg_edge_impl_t<dg_event>> cpdg;

  void pushPipe(InstPtr &inst) {
    CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event> >::pushPipe(inst);
    if (getenv("MAFIA_DUMP_BASE_PIPE"))
      this->dumpInst(inst);
  }

};


#endif
