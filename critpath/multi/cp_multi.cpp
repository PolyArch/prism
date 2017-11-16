#include "cp_multi.hh"

static RegisterCP<cp_multi> MULTI_OOO("multi", true);
static RegisterCP<cp_multi> MULTI_In("multi", false);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_multi>*> models = {&MULTI_OOO,&MULTI_In};
  for(auto &model : models) {
    CPRegistry::get()->register_argument("multi-models",       true,&model->cp_obj);
  }
}


