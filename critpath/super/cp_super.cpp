
#include "cp_super.hh"
#include "cp_registry.hh"

static RegisterCP<cp_super> cp_super("super",true);

/*
__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("simd-len", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-len", true, &In.cp_obj);
}*/

