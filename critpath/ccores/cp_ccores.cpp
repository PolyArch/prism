

//#include "cp_ccores_all.hh"
#include "cp_ccores.hh"


static RegisterCP<cp_ccores> CCORES_OOO("ccores",false);
static RegisterCP<cp_ccores> CCORES_In("ccores",true);

//static RegisterCP<cp_ccores_all> All("ccores-all",true);


__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<cp_ccores>*> models = {&CCORES_OOO,&CCORES_In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("ccores-num-mem", true, &model->cp_obj);
    CPRegistry::get()->register_argument("ccores-bb-runahead", true, &model->cp_obj);
    CPRegistry::get()->register_argument("ccores-max-ops", true, &model->cp_obj);
    CPRegistry::get()->register_argument("ccores-iops", true, &model->cp_obj);
    CPRegistry::get()->register_argument("ccores-bb-runahead",true,&model->cp_obj);
    CPRegistry::get()->register_argument("ccores-full-dataflow",true,&model->cp_obj);
  }

}

