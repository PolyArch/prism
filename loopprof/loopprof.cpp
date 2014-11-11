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


int main(int argc, char *argv[])
{
  bool verbose = false;
  bool gams_details = false;
  bool no_gams = false;
  bool size_based_cfus = false;
  bool extra_pass = false;

  //Parse the Options
  uint64_t max_inst= (uint64_t)-1;
  int winsize = 1000;

  static struct option long_options[] =
  {
    {"help", no_argument, 0, 'h'},
    {"verbose",no_argument, 0, 'v'},
    {"gams-details",no_argument, 0, 'g'},
    {"no-gams",no_argument, 0, 'n'},
    {"extra-pass",extra_pass, 0, 'e'},
    {"size-based-cfus",no_argument, 0, 's'},
    {"max-insts", required_argument, 0, 'm'},
    {"cfgdir", required_argument, 0, 'd'},
    {0,0,0,0}
  };
    
  string cfgdir;
  bool  print_cfgs=false;

  while (1) {
    int option_index = 0;

    int c = getopt_long(argc, argv, "hvm:d:",
                        long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
    case 0:
      break;
    case 'd':
      cfgdir = optarg;
      print_cfgs=true;
      break;
    case 'e': extra_pass = true; break;
    case 'h':
      std::cout << argv[0] << " [-v -m] file\n";
      return(0);
    case 'g': gams_details = true; break;
    case 'n': no_gams = true; break;
    case 's': size_based_cfus = true; break;
    case 'v': verbose = true; break;
    case 'm': max_inst = atoi(optarg); break;
    case '?': break;
    default:
      abort();
    }
  }
  
  
  // Begin Program
  uint64_t count;

  struct timeval start;
  struct timeval end;
  gettimeofday(&start, 0);


  PathProf pathProf;
  if(argc>optind+1) {
    pathProf.procSymTab(argv[optind+1]);
    pathProf.procStaticCFG((string(argv[optind+1])+".cfg").c_str()); 
  }

   //prepare the output filename
  std::string filename(argv[optind]);
   /*  std::string filename;
    if(pos != std::string::npos) {
    filename.assign(path.begin() + pos + 1, path.end());
  } else {
    filename = path;
  }*/


  size_t start_pos = filename.find_last_of("/");

  //Process Stack File
  string stackfile;
  if(start_pos != string::npos) {
    string dir = filename.substr(0, start_pos);
    stackfile = dir + "/callstack.out";
  } else {
    stackfile = "callstack.out";
  }
  pathProf.procStackFile(stackfile.c_str());

  //Process Config File
  string configfile;
  if(start_pos != string::npos) {
    string dir = filename.substr(0, start_pos);
    stackfile = dir + "/m5out/config.ini";
  } else {
    stackfile = "m5out/config.ini";
  }
  pathProf.procConfigFile(stackfile.c_str());


  // 
  system("mkdir -p gams");
  if(print_cfgs) {
    system((string("mkdir -p ") + cfgdir).c_str());
  }

  if (!doLoopProfAnalysis(argv[optind],
                          max_inst, winsize, verbose,
                          size_based_cfus,
                          no_gams, gams_details,
                          count, extra_pass, pathProf))
    return 1;

  if(print_cfgs) {
    pathProf.printCFGs(cfgdir);
  }

  gettimeofday(&end, 0);
  uint64_t start_time = start.tv_sec*1000000 + start.tv_usec;
  uint64_t end_time   = end.tv_sec * 1000000 + end.tv_usec;
  std::cout << "runtime : " << (double)(end_time - start_time)/1000000 << "  seconds\n";
  std::cout << "rate....: "
            <<  1000000*(double)count/((double)(end_time - start_time))
            << " recs/sec\n";

  std::cout << "Num of records              :" << count << "\n";
  
  uint64_t all_insts = pathProf.stopInst-pathProf.skipInsts;

  std::cout << "Loop/Rec Info: "
            << pathProf.insts_in_beret << " Beretized, "
            << pathProf.insts_in_simple_inner_loop << " Flat-Inner, "
            << pathProf.insts_in_inner_loop << " Inner, "
            << pathProf.insts_in_all_loops << " All-Loop, "
            << pathProf.non_loop_insts_direct_recursion << " Direct-Rec, "
            << pathProf.non_loop_insts_any_recursion << " Any Rec, "
            << all_insts << " All\n";

  if(extra_pass) {


    //We have to fix up everything!
    for(auto i=pathProf.cs_begin(),e=pathProf.cs_end();i!=e;++i) {
      StackFrame& sf = *i;
      sf.returning(all_insts,&pathProf);
    }

    cout << "Durations\n";
    for(int i=0;i<PathProf::ST_NUM;++i) {
//      cout << i << ": ";
      pathProf._gran_duration[i].printCDF(cout,i,1.0/(all_insts));
//      cout << "\n";
    }
    for(auto i : pathProf.longer_loops_d16384_s256) {
      if(((double)i.second)/(all_insts) < 0.005) {
        continue;
      }
      cout << "d16384_s256:   " << i.first->nice_name_full() << " " << ((double)i.second)/(all_insts) << " src: ";
      pathProf.print_loop_loc(cout,i.first);  
      cout << "\n";
    }

    for(auto i : pathProf.longer_loops_d16384_s256_i) {
      if(((double)i.second)/(all_insts) < 0.005) {
        continue;
      }
      cout << "d16384_s256_i: " << i.first->nice_name_full() << " " << ((double)i.second)/(all_insts) << " src: ";
      pathProf.print_loop_loc(cout,i.first);  
      cout << "\n";
    }

  }

  //prepare output filename ... continued
  size_t lp_start_pos=start_pos;
  if(lp_start_pos == string::npos) {
    lp_start_pos=0;
  }
  size_t dot_pos =  filename.find(".",lp_start_pos);
  filename = (string::npos == dot_pos)? filename : filename.substr(0, dot_pos);
  filename += string(".prof");
  std::cout << "created .prof file: \"" << filename << "\"\n";
  std::ofstream ofs(filename.c_str());
  boost::archive::binary_oarchive oa(ofs);
  // write class instance to archive
  oa << pathProf;

  //pathProf.printInfo();
  return 0;
}
