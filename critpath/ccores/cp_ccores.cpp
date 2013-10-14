

//#include "cp_ccores_all.hh"
#include "cp_ccores.hh"


static RegisterCP<cp_ccores> OOO("ccores",false);
static RegisterCP<cp_ccores> In("ccores",true);

//static RegisterCP<cp_ccores_all> All("ccores-all",true);


__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("ccores-num-mem", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("ccores-num-mem", true, &In.cp_obj);

  CPRegistry::get()->register_argument("ccores-bb-runahead", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("ccores-bb-runahead", true, &In.cp_obj);
  
  CPRegistry::get()->register_argument("ccores-max-ops", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("ccores-max-ops", true, &In.cp_obj);

  CPRegistry::get()->register_argument("ccores-iops", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("ccores-iops", true, &In.cp_obj);

}

