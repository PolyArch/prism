
#ifndef EXEC_PROFILE_HH
#define EXEC_PROFILE_HH

#include <stdint.h>
#include <stdlib.h>
#include <string>

namespace ExecProfile {

  __attribute__((__weak__))
  bool _hasProfileInternal(void);

  static inline bool hasProfile(void) {
    if (_hasProfileInternal)
      return _hasProfileInternal();
    return false;
  }

  __attribute__((__weak__))
  std::string _getDisasmInternal(uint64_t pc, int upc);

  static inline std::string getDisasm(uint64_t pc, int upc) {
    if (_getDisasmInternal)
      return _getDisasmInternal(pc, upc);
    char buf[64];
    sprintf(buf, "<%ld,%d>",pc, upc);
    return std::string(buf);
  }

}

#endif
