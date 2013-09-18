#include "prof.hh"

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

PathProf& Prof::get() {
  return pathProf;
}

