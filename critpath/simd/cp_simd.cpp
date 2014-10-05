
#include "cp_simd.hh"
#include "cp_registry.hh"

static RegisterCP<simd::cp_simd> SIMD_OOO("simd", false);
static RegisterCP<simd::cp_simd> SIMD_In("simd", true);

__attribute__((__constructor__))
static void init()
{
  std::vector<RegisterCP<simd::cp_simd>*> models = {&SIMD_OOO,&SIMD_In};
  for(auto &model : models) {
    CPRegistry::get()->register_argument("simd-len",              true, &model->cp_obj);
    CPRegistry::get()->register_argument("simd-use-inst-trace",  false, &model->cp_obj);
    CPRegistry::get()->register_argument("allow-non-stride-vec", false, &model->cp_obj);
    CPRegistry::get()->register_argument("unaligned-vec-access", false, &model->cp_obj);
    CPRegistry::get()->register_argument("use-reduction-tree",   false, &model->cp_obj);
    CPRegistry::get()->register_argument("disallow-splitted-op", false, &model->cp_obj);
    CPRegistry::get()->register_argument("disallow-merge-op",    false, &model->cp_obj);
    CPRegistry::get()->register_argument("simd-exec-width",       true, &model->cp_obj);
    CPRegistry::get()->register_argument("simd-inst-incr-factor", true, &model->cp_obj);
    CPRegistry::get()->register_argument("simd-full-dataflow",   false, &model->cp_obj);
  }

}
