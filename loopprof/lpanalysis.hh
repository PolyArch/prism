
#ifndef LP_ANALYSIS_HH
#define LP_ANALYSIS_HH

#include "pathprof.hh"

bool doLoopProfAnalysis(const char *fname,
                        uint64_t max_inst,
                        int winsize,
                        bool verbose,
                        bool size_based_cfus,
                        bool no_gams,
                        bool gams_details,
                        uint64_t &count,
                        bool extra_pass,
                        PathProf &pathProf);
#endif
