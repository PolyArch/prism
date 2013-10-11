
#ifndef EXEC_PROFILE_HH
#define EXEC_PROFILE_HH

#include <stdint.h>
#include <stdlib.h>
#include <string>


namespace ExecProfile {

  static inline bool hasProfile(void) {
    return (getenv("EXEC_PROFILE") != 0);
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
