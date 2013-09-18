#ifndef PROF_HH
#define PROF_HH

#include "pathprof.hh"
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <iostream>
#include <utility>
#include <string> 


class Prof {
private:
  static PathProf pathProf;

public:
  static void init(std::string& filename);
  static PathProf& get();
};

#endif
