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
}


PathProf& Prof::get() {
  return pathProf;
}

