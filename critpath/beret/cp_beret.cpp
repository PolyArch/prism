
#include "cp_beret.hh"

static RegisterCP<cp_beret> BERET_OOO("beret", true);
static RegisterCP<cp_beret> BERET_In("beret", false);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_beret>*> models = {&BERET_OOO,&BERET_In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("beret-max-seb",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-max-mem",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-max-ops",      true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-iops",         true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-config-time",  true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-dataflow-pure",true,&model->cp_obj);
    CPRegistry::get()->register_argument("beret-dataflow-seb", true,&model->cp_obj);
    CPRegistry::get()->register_argument("no-gams",           false,&model->cp_obj);
    CPRegistry::get()->register_argument("gams-details",      false,&model->cp_obj);
    CPRegistry::get()->register_argument("size-based-cfus",   false,&model->cp_obj);
  }


}

