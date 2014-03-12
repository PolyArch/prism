
#include "cp_nla.hh"

static RegisterCP<cp_nla> OOO("nla", true);
static RegisterCP<cp_nla> In("nla", false);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_nla>*> models = {&OOO,&In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("nla_serialize_sg",    true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_issue_inorder",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_cfus_delay_writes",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_cfus_delay_reads",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_inorder_address_calc",true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_mem_dep_predictor",   true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_software_mem_alias",  true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_no_exec_speculation", true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_exclusive_cfus",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_pipelined_cfus",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla_wb_networks",         true,&model->cp_obj);

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

