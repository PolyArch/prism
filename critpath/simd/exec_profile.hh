
#ifndef EXEC_PROFILE_HH
#define EXEC_PROFILE_HH

#include <stdint.h>
#include <map>
#include <string>

class exec_profile {
  exec_profile() {}

  static exec_profile *instance;

  void init();
  std::map<std::pair<uint64_t, int>, std::string> disasmMap;
public:
  static bool hasProfile();
  static std::string getDisasm(uint64_t pc, int upc);
};

#endif
