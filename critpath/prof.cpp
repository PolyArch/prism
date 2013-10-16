#include "prof.hh"
#include "lpanalysis.hh"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <iostream>
#include <utility>
#include <string> 


PathProf Prof::pathProf;

bool Prof::init(std::string& filename) {
  std::ifstream ifs(filename.c_str());

  if (!ifs.is_open()) {
    return false;
  }

  std::cout << "reading prof file: " << filename;
  std::cout.flush();
  boost::archive::binary_iarchive bia(ifs);
  //write class instance from archive
  bia >> pathProf;
  std::cout << "... done!\n";
  return true;
}

static std::string getCallStackTraceFileName(const std::string &traceFileName)
{
  size_t start_pos = traceFileName.find_last_of("/");
  if (start_pos == std::string::npos)
    return "callstack.out";
  return traceFileName.substr(0, start_pos) + std::string("/callstack.out");
}

#if 0
static std::string getLoopProfileFileName(const std::string &traceFileName)
{
  size_t start_pos = traceFileName.find_last_of("/");
  size_t dot_pos   = traceFileName.find(".", start_pos);
  std::string filename = (traceFileName.substr(0, dot_pos)
                          + std::string(".prof"));
  return filename;
}
#endif

void Prof::init_from_trace(const char *trace_fname,
                           uint64_t max_inst)
{
  uint64_t count = 0;
  std::cout << "Generating Loop Info from trace\n";
  std::cout.flush();


  pathProf.procStackFile(getCallStackTraceFileName(trace_fname).c_str());

  // loopprof does this :: critpath also does this ???
  // pathProf.procConfigFile(getConfigFilename(trace_fname).c_str());

  if (!doLoopProfAnalysis(trace_fname,
                          max_inst,
                          1024,  // winsize
                          false, // verbose
                          false, // no_gams
                          false, //gams_details
                          count,
                          pathProf)) {
    std::cerr << "Error Opening file: " << trace_fname << "\n";
    exit(-1);
  }
  std::cout << "... generating loop info ... done!!\n";

#if 0

  // IT DOES NOT WORK FOR SIMD, SINCE SOME INFO (eg. STRIDE) NOT
  // STORED IN THE ARCHIVE YET.

  // cache the analysis
  std::string prof_filename = getLoopProfileFileName(trace_fname);
  std::cout << "Creating .prof file: \"" << prof_filename << "\"\n";
  std::ofstream ofs(prof_filename.c_str());
  boost::archive::binary_oarchive oa(ofs);
  // write class instance to archive
  oa << pathProf;
#endif

}


PathProf& Prof::get() {
  return pathProf;
}

