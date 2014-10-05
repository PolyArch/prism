

#include "cp_npu.hh"
#include "cp_registry.hh"

static RegisterCP<npu::cp_npu> NPU_OOO("npu", false);
static RegisterCP<npu::cp_npu> NPU_In("npu", true);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<npu::cp_npu>*> models = {&NPU_OOO,&NPU_In};

  for(auto &model : models) {
    CPRegistry::get()->register_argument("npu-func",         true, &model->cp_obj);
  //CPRegistry::get()->register_argument("npu-topology",     true, &model->cp_obj);  */
    CPRegistry::get()->register_argument("npu-exec-latency", true, &model->cp_obj);
  }

}


