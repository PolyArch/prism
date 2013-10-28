
#include "cp_dyser.hh"
#include "cp_vec_dyser.hh"
#include "cp_registry.hh"

static RegisterCP<DySER::cp_dyser> XOut("dyser", false);
static RegisterCP<DySER::cp_dyser> XIn("dyser", true);

static RegisterCP<DySER::cp_vec_dyser> VecOut("vec-dyser", false);
static RegisterCP<DySER::cp_vec_dyser> VecIn("vec-dyser", true);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("dyser-vec-len", true, &VecOut.cp_obj);
  CPRegistry::get()->register_argument("dyser-vec-len", true, &VecIn.cp_obj);
}

