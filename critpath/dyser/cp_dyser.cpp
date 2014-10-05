
#include "cp_dyser.hh"
#include "cp_vec_dyser.hh"
#include "cp_orig_dyser.hh"
#include "cp_registry.hh"


unsigned DySER::dyser_inst::Send_Recv_Latency = 1;

static RegisterCP<DySER::cp_dyser> XOut("scalardyser", false);
static RegisterCP<DySER::cp_dyser> XIn("scalardyser", true);

static RegisterCP<DySER::cp_vec_dyser> VecOut("dyser", false);
static RegisterCP<DySER::cp_vec_dyser> VecIn("dyser", true);

static RegisterCP<DySER::cp_orig_dyser> OrigOut("orig-dyser", false);
static RegisterCP<DySER::cp_orig_dyser> OrigIn("orig-dyser", true);


__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<DySER::cp_vec_dyser>*> models = {&VecOut,&VecIn};
  for(auto &m : models) {
    CPRegistry::get()->register_argument("dyser-size", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-vec-len", true, &m->cp_obj);
    CPRegistry::get()->register_argument("disallow-non-stride-vec", false, &m->cp_obj);
    CPRegistry::get()->register_argument("allow-non-stride-vec", false, &m->cp_obj);
    CPRegistry::get()->register_argument("disallow-internal-control-in-dyser", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-config-fetch-latency", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-config-switch-latency", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-ctrl-miss-config-penalty", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-count-depth-nodes-for-config", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-fu-fu-latency", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-use-reduction-tree", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-insert-ctrl-miss-penalty", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-disallow-coalesce-mem-ops", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-try-bundle-ops", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-allow-merge-ops", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-use-rpo-index-for-output", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-send-recv-latency", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-force-vectorize", false, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-inst-incr-factor", true, &m->cp_obj);
    CPRegistry::get()->register_argument("dyser-full-dataflow", false, &m->cp_obj);
  }

}

