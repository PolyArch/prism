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

void Prof::init(std::string& filename) {
  std::ifstream ifs(filename.c_str());

  if (!ifs.is_open()) {
    std::cerr << "Cannot open file: \"" << filename << "\"\n";
    exit(1);
  }

  boost::archive::binary_iarchive bia(ifs);
  //write class instance to archive
  bia >> pathProf;
}

void Prof::init_from_trace(const char *trace_fname,
                           uint64_t max_inst)
{
  uint64_t count = 0;
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
}


PathProf& Prof::get() {
  return pathProf;
}

