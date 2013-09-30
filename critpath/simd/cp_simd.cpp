
#include "cp_simd.hh"

static RegisterCP<simd::cp_simd> OOO("simd", false);
static RegisterCP<simd::cp_simd> In("simd", true);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("simd-len", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-len", true, &In.cp_obj);
}
