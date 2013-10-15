
#include "cp_simd.hh"
#include "cp_registry.hh"

static RegisterCP<simd::cp_simd> OOO("simd", false);
static RegisterCP<simd::cp_simd> In("simd", true);
std::map<LoopInfo*, bool> simd::cp_simd::li_printed;

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("simd-len", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-len", true, &In.cp_obj);

  CPRegistry::get()->register_argument("simd-use-inst-trace", false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-use-inst-trace", false, &In.cp_obj);

}
