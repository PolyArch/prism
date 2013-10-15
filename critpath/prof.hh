#ifndef PROF_HH
#define PROF_HH

#include "pathprof.hh"


class Prof {
private:
  static PathProf pathProf;

public:
  static bool init(std::string& filename);
  static void init_from_trace(const char *trace_fname,
                              uint64_t max_inst);

  static PathProf& get();
};

#endif
