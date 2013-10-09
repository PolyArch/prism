
#include "cp_beret.hh"

static RegisterCP<cp_beret> OOO("beret", true);
static RegisterCP<cp_beret> In("beret", false);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("beret-max-size", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-max-size", true, &In.cp_obj);

  CPRegistry::get()->register_argument("beret-max-mem", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("beret-max-mem", true, &In.cp_obj);

}

