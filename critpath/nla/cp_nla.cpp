
#include "cp_nla.hh"

static RegisterCP<cp_nla> NLA_OOO("nla", true);
static RegisterCP<cp_nla> NLA_In("nla", false);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_nla>*> models = {&NLA_OOO,&NLA_In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("nla-serialize-sgs",       true,&model->cp_obj);
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
    CPRegistry::get()->register_argument("nla-dataflow-ctrl",       true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-inorder-per-sg",      true,&model->cp_obj);

    CPRegistry::get()->register_argument("nla-ser-loops",           true,&model->cp_obj);
    CPRegistry::get()->register_argument("nla-loop-iter-dist",      true,&model->cp_obj);

    CPRegistry::get()->register_argument("no-gams",                false,&model->cp_obj);
  }

}

