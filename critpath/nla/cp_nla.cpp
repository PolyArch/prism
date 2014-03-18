
#include "cp_nla.hh"

static RegisterCP<cp_nla> OOO("nla", true);
static RegisterCP<cp_nla> In("nla", false);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_nla>*> models = {&OOO,&In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("nla-serialize-sgs",        true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-issue-inorder",       true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-cfus-delay-writes",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-cfus-delay-reads",    true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-inorder-address-calc",true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-mem-dep-predictor",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-software-mem-alias",  true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-no-exec-speculation", true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-exclusive-cfus",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-pipelined-cfus",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-wb-networks",         true,&model->cp_obj);

    CPRegistry::get()->register_argument("no-gams",                false,&model->cp_obj);
  }

/*  CPRegistry::get()->register_argument("beret-max-seb", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-max-seb", true, &In.cp_obj);

  CPRegistry::get()->register_argument("beret-max-mem", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-max-mem", true, &In.cp_obj);

  CPRegistry::get()->register_argument("beret-max-ops", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-max-ops", true, &In.cp_obj);

  CPRegistry::get()->register_argument("beret-iops", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-iops", true, &In.cp_obj);

  CPRegistry::get()->register_argument("beret-config-time",true,&OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-config-time",true,&In.cp_obj);

  CPRegistry::get()->register_argument("beret-dataflow-pure",true,&OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-dataflow-pure",true,&In.cp_obj);

  CPRegistry::get()->register_argument("beret-dataflow-seb",true,&OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-dataflow-seb",true,&In.cp_obj);

  CPRegistry::get()->register_argument("no-gams", false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("no-gams", false, &In.cp_obj);
  */
}

