
#include "cp_super.hh"
#include "cp_registry.hh"

static RegisterCP<cp_super> cp_super("super",true);


__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("super-no-spec", true, &cp_super.cp_obj);
  CPRegistry::get()->register_argument("super-dataflow-no-spec", true, &cp_super.cp_obj);
  CPRegistry::get()->register_argument("inorder-per-instruction", true, &cp_super.cp_obj);
  CPRegistry::get()->register_argument("model-sq", true, &cp_super.cp_obj);
}

