

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "op.hh"

namespace {
  class exec_profile {
  public:
    bool _initialized = false;
    void init();
    std::map<std::pair<uint64_t, int>, std::string> disasmMap;
  };
}

static exec_profile _prof_instance;



void exec_profile::init()
{
  if (_initialized)
    return ;
  _initialized = true;

  std::ifstream ifs;
  const char *prof_fname = getenv("EXEC_PROFILE");
  if (prof_fname) {
    ifs.open(prof_fname);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open " << prof_fname << "\n";
      return;
    }
  } else {
    // can we open a file named exec.profile.disasm
    ifs.open("exec.prof.disasm");
    if (!ifs.is_open())
      return;
  }

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

namespace ExecProfile {
  bool _hasProfileInternal(void);
  std::string _getDisasmInternal(uint64_t pc, int upc);

  bool _hasProfileInternal(void) {
    return (_prof_instance.disasmMap.size() > 0);
  }

  std::string _getDisasmInternal(uint64_t pc, int upc)
  {
    _prof_instance.init();

    std::pair<uint64_t, int> pc_upc = std::make_pair(pc, upc);
    return _prof_instance.disasmMap[pc_upc];
  }

}
