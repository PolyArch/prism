
#include "cp_base.hh"
#include "cp_registry.hh"


static RegisterCP<default_cpdg_t> baseInorder("base",true);
static RegisterCP<default_cpdg_t> baseOOO("base",false);
