
#include "cp_beret.hh"

static RegisterCP<cp_beret> OOO("beret", true);
static RegisterCP<cp_beret> In("beret", false);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("beret-max-seb", true, &OOO.cp_obj);
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

}

