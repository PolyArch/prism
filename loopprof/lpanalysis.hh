
#ifndef LP_ANALYSIS_HH
#define LP_ANALYSIS_HH

#include "pathprof.hh"

bool doLoopProfAnalysis(const char *fname,
                        uint64_t max_inst,
                        int winsize,
                        bool verbose,
                        bool no_gams,
                        bool gams_details,
                        uint64_t &count,
                        PathProf &pathProf);
#endif
