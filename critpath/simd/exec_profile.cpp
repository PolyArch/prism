
#include "exec_profile.hh"

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

exec_profile *exec_profile::instance = 0;

void exec_profile::init()
{
  const char *prof_fname = getenv("EXEC_PROFILE");
  if (!prof_fname)
    return;

  std::ifstream ifs;
  ifs.open(prof_fname);
  std::string str;
  while (ifs.good()) {
    uint64_t pc;
    int upc;
    std::string disasm;
    ifs >> pc;
    ifs >> upc;
    getline(ifs, disasm);
    std::pair<uint64_t, int> pc_upc = std::make_pair(pc, upc);
    disasmMap[pc_upc] = disasm;
  }
}


bool exec_profile::hasProfile() {
  return (getenv("EXEC_PROFILE") != 0);
}

std::string exec_profile::getDisasm(uint64_t pc, int upc)
{
  if (!exec_profile::instance) {
    exec_profile::instance = new exec_profile();
    exec_profile::instance->init();
  }

  std::pair<uint64_t, int> pc_upc = std::make_pair(pc, upc);
  return exec_profile::instance->disasmMap[pc_upc];
}
