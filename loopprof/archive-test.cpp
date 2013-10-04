/*#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <iostream>
#include <utility>

#include "pathprof.hh"
#include <string>
#include <stdlib.h>*/

#include <stdint.h>
#include <sys/time.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <getopt.h>

#include "gzstream.hh"

#include "cpu/crtpath/crtpathnode.hh"
#include "pathprof.hh"
#include "lpanalysis.hh"
#include "stdlib.h"

#define INST_WINDOW_SIZE 1000

using namespace std;

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>




using namespace std;

int main(int argc, char** argv) {

  if(argc<1) {
    cout << "usage: " << argv[0] << " loop_profile.prof";
  }

/* OLD TEST
  //----------------
  ofstream ofs("testone");
  boost::archive::text_oarchive oa(ofs);

  Op op1, op2;
  op1.setIsLoad(true);
  op2.setIsStore(true);
  op1.addDep(&op2);

  CPC cpc1,cpc2;
  BB bb1(cpc1,cpc2);
  
  oa << bb1;

  ofs.close();
  //----------------



  //----------------
  BB bb2(cpc1,cpc2);

  ifstream ifs("testone");
  boost::archive::text_iarchive ia(ifs);

  ia >> bb2;
  //----------------
*/


  PathProf pathProf;
  std::ifstream ifs2(argv[1]);
  boost::archive::binary_iarchive ia2(ifs2);
  //write class instance to archive
  ia2 >> pathProf;

  system("mkdir archive-test-cfgdir");
  string st("archive-test-cfgdir");
  pathProf.printCFGs(st);
}
