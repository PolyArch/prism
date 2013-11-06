
#include "cp_simd.hh"
#include "cp_registry.hh"

static RegisterCP<simd::cp_simd> OOO("simd", false);
static RegisterCP<simd::cp_simd> In("simd", true);

__attribute__((__constructor__))
static void init()
{
  CPRegistry::get()->register_argument("simd-len", true, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-len", true, &In.cp_obj);

  CPRegistry::get()->register_argument("simd-use-inst-trace",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("simd-use-inst-trace",
                                       false, &In.cp_obj);

  CPRegistry::get()->register_argument("allow-non-stride-vec",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("allow-non-stride-vec",
                                       false, &In.cp_obj);

  CPRegistry::get()->register_argument("unaligned-vec-access",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("unaligned-vec-access",
                                       false, &In.cp_obj);

  CPRegistry::get()->register_argument("use-reduction-tree",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("use-reduction-tree",
                                       false, &In.cp_obj);

  CPRegistry::get()->register_argument("disallow-splitted-op",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("disallow-splitted-op",
                                       false, &In.cp_obj);

  CPRegistry::get()->register_argument("disallow-merge-op",
                                       false, &OOO.cp_obj);
  CPRegistry::get()->register_argument("disallow-merge-op",
                                       false, &In.cp_obj);

}
