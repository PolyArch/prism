#include <stdint.h>
#include <sys/time.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <getopt.h>

#include "gzstream.hh"

#define STANDALONE_CRITPATH 1
#include "cpu/crtpath/crtpathnode.hh"
#include "pathprof.hh"

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

  //Parse the Options
  uint64_t max_inst= (uint64_t)-1;
  int winsize = 1000;

  static struct option long_options[] =
  {
    {"help", no_argument, 0, 'h'},
    {"verbose",no_argument, 0, 'v'},
    {"gams-details",no_argument, 0, 'g'},
    {"no-gams",no_argument, 0, 'n'},
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
    case 'h':
      std::cout << argv[0] << " [-v -m] file\n";
      return(0);
    case 'g': gams_details = true; break;
    case 'n': no_gams = true; break;
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

  CP_NodeDiskImage* cp_array = new CP_NodeDiskImage[winsize];

  PathProf pathProf;
  if(argc>optind+1) {
    pathProf.procSymTab(argv[optind+1]);
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

  // 
  system("mkdir -p gams");
  if(print_cfgs) {
    system((string("mkdir -p ") + cfgdir).c_str());
  }

  //Two Passes
  for(int pass = 1; pass <= 2; ++pass) {
    igzstream inf(argv[optind], std::ios::in | std::ios::binary);

    if (!inf.is_open()) {
      std::cerr << "Cannot open file " << argv[optind] << "\n";
      return 1;
    }

    bool prevCtrl=true;
    bool prevCall=true;
    bool prevRet=false;

    CPC prevCPC;
    prevCPC.first=0;
    prevCPC.second=0;

    pathProf.resetStack(prevCPC,prevCtrl,prevCall,prevRet);
  
    printf("LoopProf PASS %d\n",pass);

    count=0;
    while (!inf.eof()) {
      int ind=count%winsize;
      cp_array[ind] = CP_NodeDiskImage::read_from_file(inf);
      CP_NodeDiskImage& img = cp_array[ind];

      CPC cpc = make_pair(cp_array[ind]._pc,cp_array[ind]._upc);
      ++count;
  
      if (verbose&&pass==2) {
        std::cout << count << ":  ";
        img.write_to_stream(std::cout);
        std::cout << "\n";
      }
      
      if(pass==1) {
        static int skipInsts=0;
        if(prevCtrl) {
           if(skipInsts>0) {
             //cout << "------------------skip ---------" << skipInsts << "\n";
             pathProf.setSkipInsts(skipInsts);
             skipInsts=-1;
           }
          //Only pass control instructions to phase 1
          pathProf.processOpPhase1(prevCPC,cpc,prevCall,prevRet);
        } else if (skipInsts!=-1) {
          skipInsts++;
        }
      } else if (pass==2) {
        //Pass all instructions to phase 2
        pathProf.processOpPhase2(prevCPC,cpc,prevCall,prevRet,img);
      } else if (pass==3) {
        //Pass all instructions to phase 3 -- this is what will be
	//used in the transform pass, used for debugging purposes only
        pathProf.processOpPhase3(cpc,prevCall,prevRet);
      }

      prevCPC=cpc;
      prevCtrl=img._isctrl;
      prevCall=img._iscall;
      prevRet=img._isreturn;
      
      if (count == max_inst) {
        break;
      }
      if ( (count % 100000 == 0) && (count!=0) ) {
        std::cout << "processed " << count << "\n";
      }
    }

    if(pass==1) {
      pathProf.runAnalysis();
    } else if(pass==2) {
      pathProf.runAnalysis2(no_gams,gams_details);
    }

    inf.close();
  }
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
  
  delete[] cp_array;


  pathProf.setStopInst(count);
  std::cout << "Loop/Rec Info: "
            << pathProf.insts_in_beret << " Beretized, "
            << pathProf.insts_in_simple_inner_loop << " Flat-Inner, "
            << pathProf.insts_in_inner_loop << " Inner, "
            << pathProf.insts_in_all_loops << " All-Loop, "
            << pathProf.non_loop_insts_direct_recursion << " Direct-Rec, "
            << pathProf.non_loop_insts_any_recursion << " Any Rec, "
            << pathProf.stopInst-pathProf.skipInsts << " All\n";

  //prepare output filename ... continued

  size_t dot_pos =  filename.find(".",start_pos);
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
