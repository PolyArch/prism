

#include "cp_npu.hh"
#include "cp_registry.hh"

static RegisterCP<npu::cp_npu> OOO("npu", false);
static RegisterCP<npu::cp_npu> In("npu", true);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("npu-func", true,
                                       &OOO.cp_obj);
  CPRegistry::get()->register_argument("npu-func", true,
                                       &In.cp_obj);

  /*
  CPRegistry::get()->register_argument("npu-topology", true,
                                       &OOO.cp_obj);
  CPRegistry::get()->register_argument("npu-topology", true,
                                       &In.cp_obj);
  */

  CPRegistry::get()->register_argument("npu-exec-latency", true,
                                       &OOO.cp_obj);
  CPRegistry::get()->register_argument("npu-exec-latency", true,
                                       &In.cp_obj);

}


