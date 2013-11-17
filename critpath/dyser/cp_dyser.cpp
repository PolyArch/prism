
#include "cp_dyser.hh"
#include "cp_vec_dyser.hh"
#include "cp_orig_dyser.hh"
#include "cp_registry.hh"


unsigned DySER::dyser_inst::Send_Recv_Latency = 1;

static RegisterCP<DySER::cp_dyser> XOut("dyser", false);
static RegisterCP<DySER::cp_dyser> XIn("dyser", true);

static RegisterCP<DySER::cp_vec_dyser> VecOut("vec-dyser", false);
static RegisterCP<DySER::cp_vec_dyser> VecIn("vec-dyser", true);

static RegisterCP<DySER::cp_orig_dyser> OrigOut("orig-dyser", false);
static RegisterCP<DySER::cp_orig_dyser> OrigIn("orig-dyser", true);


__attribute__((__constructor__))
static void init()
{

  CPRegistry::get()->register_argument("dyser-size", true, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-size", true, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-vec-len", true, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-vec-len", true, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("disallow-non-stride-vec", false,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("disallow-non-stride-vec", false,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("allow-non-stride-vec", false,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("allow-non-stride-vec", false,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("disallow-internal-control-in-dyser", false,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("disallow-internal-control-in-dyser", false,
                                       &VecIn.cp_obj);


  CPRegistry::get()->register_argument("dyser-config-fetch-latency", true,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-config-fetch-latency", true,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-config-switch-latency", true,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-config-switch-latency", true,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-ctrl-miss-config-penalty", true,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-ctrl-miss-config-penalty", true,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-count-depth-nodes-for-config", false,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-count-depth-nodes-for-config", false,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-fu-fu-latency", true,
                                       &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-fu-fu-latency", true,
                                       &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-use-reduction-tree",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-use-reduction-tree",
                                       false, &VecIn.cp_obj);
  CPRegistry::get()->register_argument("dyser-insert-ctrl-miss-penalty",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-insert-ctrl-miss-penalty",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-disallow-coalesce-mem-ops",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-disallow-coalesce-mem-ops",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-try-bundle-ops",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-try-bundle-ops",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-allow-merge-ops",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-allow-merge-ops",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-use-rpo-index-for-output",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-use-rpo-index-for-output",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-send-recv-latency",
                                       true, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-send-recv-latency",
                                       true, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-force-vectorize",
                                       false, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-force-vectorize",
                                       false, &VecIn.cp_obj);

  CPRegistry::get()->register_argument("dyser-inst-incr-factor",
                                       true, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-inst-incr-factor",
                                       true, &VecIn.cp_obj);


}

