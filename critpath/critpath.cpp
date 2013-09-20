
#include <sys/time.h>
#include <getopt.h>
#include <stdint.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "cp_dep_graph.hh"
#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include "critpath.hh"
#include "gzstream.hh"

#include "origcp.hh"
#include "default_cpdg.hh"

#include "prof.hh"

#include <fstream>
#include "cp_ccores_all.hh"
#include "cp_beret.hh"
#include "cp_ccores.hh"

int FETCH_WIDTH = 4;
int D_WIDTH = 4;
int ISSUE_WIDTH = 4;
int FETCH_TO_DISPATCH_STAGES = 4;
int EXECUTE_WIDTH =2;
int WRITEBACK_WIDTH = 4;
int COMMIT_WIDTH = 4;
int SQUASH_WIDTH = 4;
int IQ_WIDTH = 64;
int ROB_SIZE = 192;
int LQ_SIZE = 32;
int SQ_SIZE = 32;

int IN_ORDER_BR_MISS_PENALTY = (1 //squash width (rename+decode+fetch+dispatch)
                       + 7 //restarting fetch
                       );


int BR_MISS_PENALTY = (1 //squash width (rename+decode+fetch+dispatch)
                       + 4 //ROB squashing (16 instruction to squash)
                       + 2 //restarting fetch
                       );

unsigned DySER_Size = 32;
unsigned CCA_Size = 3;
unsigned DySER_Concurrency = 8;
int DySER_Loop  = 0;
int CCA_Ctrl = 1;
int DySER_LDOnly = 0;
int TraceOutputs = 0;
unsigned GPU_LD_Latency = 0;

CPRegistry* CPRegistry::_registry = 0;
int HighBW = 0;



using namespace std;



OrigCP orig;
static RegisterCP<default_cpdg_t> baseInorder("base",true);
static RegisterCP<default_cpdg_t> baseOOO("base",false);


int main(int argc, char *argv[])
{
  orig.setupOutFile("orig_cp.txt");
  bool inorder_model=true;
  bool ooo_model=true;
  int  noMcPAT=0;

  static struct option static_long_options[] =
    {
      {"help", no_argument, 0, 'h'},
      {"verbose",no_argument, 0, 'v'},
      {"count", no_argument, 0, 't'},
      {"no-registry", no_argument, 0, 'n'},
      {"fetch-width", required_argument, 0, 'f'},
      {"commit-width", required_argument, 0, 'c'},
      {"rob-size", required_argument, 0, 'r'},
      {"branch-miss-penalty", required_argument, 0, 'b'},
      {"iq-size", required_argument, 0, 'i'},
      {"max-insts", required_argument, 0, 'm'},
      {"dy-size", required_argument, 0, 's'},
      {"cca-size", required_argument, 0, 'a'},
      {"bandwidth", no_argument, &HighBW, 1},
      {"concurrency", required_argument, 0, 'y'},
      {"dyser-loop", no_argument, &DySER_Loop, 1},
      {"no-cca-ctrl", no_argument, &CCA_Ctrl, 0},
      {"dyser-load-only", no_argument, &DySER_LDOnly, 1},
      {"gpu-ld-lat", required_argument, 0, 'l'},
      {"trace-out", no_argument, &TraceOutputs, 1},
      {"models", required_argument, 0, 'x'}, //inorder, ooo, both
      {"no-mcpat", no_argument, &noMcPAT, 1},
      {0,0,0,0}
    };

  std::vector<struct option> long_options;
  CPRegistry::get()->setupOptions(long_options, static_long_options);


  uint64_t max_inst = (uint64_t)-1;
  bool verbose =false;
  bool registry_off = false;
  bool count_nodes = false;
  while (1) {
    int option_index = 0;

    const struct option *longopts = &*long_options.begin();
    int c = getopt_long(argc, argv, "hvntf:c:r:b:i:m:s:a:y:l:x",
                        longopts, &option_index);
    if (c == -1)
      break;

    switch(c) {
    case 0:
      CPRegistry::get()->handleArgv(long_options[option_index].name);
      //std::cout << "Saw " << long_options[option_index].name << "\n";
      break;
    case 'h':
      std::cout << argv[0] << " [-f F|-c C|-r R|-b B|-h|-n] file\n";
      return(0);
    case 'v': verbose = true; break;
    case 'f': FETCH_WIDTH = atoi(optarg); break;
    case 'c': COMMIT_WIDTH = atoi(optarg); break;
    case 'r': ROB_SIZE = atoi(optarg); break;
    case 'b': BR_MISS_PENALTY = atoi(optarg); break;
    case 'i': IQ_WIDTH = atoi(optarg); break;
    case 'm': max_inst = atoi(optarg); break;
    case 'n': registry_off = true; break;
    case 't': count_nodes = true; break;
    case 's': DySER_Size = atoi(optarg); break;
    case 'a': CCA_Size = atoi(optarg); break;
    case 'y': DySER_Concurrency = atoi(optarg); break;
    case 'l': GPU_LD_Latency = atoi(optarg); break;
    case 'x':
      if(strcmp(optarg,"inorder")==0) {
        inorder_model=true;
        ooo_model=false;
      } else if(strcmp(optarg,"ooo")==0) {
        inorder_model=false;
        ooo_model=true;
      } else if(strcmp(optarg,"both")==0) {
        inorder_model=true;
        ooo_model=true;
      } else {
        std::cerr << "option: \"" << optarg << "\" is not valid for --models";
      }
    case '?': break;
    default:
      abort();
    }
  }

  if (GPU_LD_Latency <= 0) {
    GPU_LD_Latency = 0;
  }
  if (DySER_Concurrency == 0)
    DySER_Concurrency = 8;

  if (FETCH_WIDTH <= 0) {
    FETCH_WIDTH = 4;
  }
  if (COMMIT_WIDTH <= 0) {
    COMMIT_WIDTH = 4;
  }
  if (ROB_SIZE <= 0) {
    ROB_SIZE = 192;
  }
  if (BR_MISS_PENALTY <= 0) {
    BR_MISS_PENALTY = 6;
  }
  if (IQ_WIDTH <= 0) {
    IQ_WIDTH = 64;
  }
  if (max_inst == 0) {
    max_inst = (uint64_t)-1;
  }
  if (DySER_Size == 0) {
    DySER_Size = 32;
  }
  if (CCA_Size == 0) {
    CCA_Size = 3;
  }
  if (argc - optind != 1) {
    std::cerr << "Requires one argument.\n";
    return 1;
  }
  igzstream inf(argv[optind], std::ios::in | std::ios::binary);

  if (!inf.is_open()) {
    std::cerr << "Cannot open file: \"" << argv[optind] << "\"\n";
    return 1;
  }

  CPRegistry::get()->pruneCP(inorder_model, ooo_model);

  //determine the prof file name
  std::string prof_file(argv[optind]);
  size_t start_pos = prof_file.find_last_of("/");

  //read in m5out/stats.txt file
  //Process m5out/stats.txt for events
  string statsfile;
  if(start_pos != string::npos) {
    string dir = prof_file.substr(0, start_pos);
    statsfile = dir + "/m5out/stats.txt";
  } else {
    statsfile = "m5out/stats.txt";
  }

  Prof::get().procStatsFile(statsfile.c_str());



  //open prof file
  size_t dot_pos =  prof_file.find(".",start_pos);
  prof_file = (string::npos == dot_pos)? prof_file : prof_file.substr(0, dot_pos);
  prof_file += string(".prof");

  std::cout << "reading prof file: " << prof_file;
  std::cout.flush();
  Prof::init(prof_file);
  std::cout << "... done!\n";
  
  uint64_t count = 0;
  uint64_t numCycles =  0;

  bool prevCall=true;
  bool prevRet=false;
  bool notused=false;
  CPC notusedCPC; 

  //this sets the prevCall/prevRet based on the information
  //stored in the profile.  Originally from the file containing
  //the stack.
  Prof::get().resetStack(notusedCPC,notused,prevCall,prevRet);

  struct timeval start;
  struct timeval end;
  gettimeofday(&start, 0);

  CP_NodeDiskImage img;
  while (!inf.eof()) {
    CP_NodeDiskImage::read_from_file_into(inf,img);
    if (inf.eof()) {
      break;
    }
    if (count == 0) {
      img._fc = 0;
    }

    if (verbose) {
      std::cout << count << ":  ";
      img.write_to_stream(std::cout);
      std::cout << "\n";
    }
    if (!count_nodes) {
      CPC cpc = make_pair(img._pc,img._upc);
      Op* op = Prof::get().processOpPhase3(cpc,prevCall,prevRet);
      prevCall=img._iscall;
      prevRet=img._isreturn;

      if(op!=NULL) {
        orig.insert(img, count, op);
        //base_ooo.insert(img, count, op);

        if (!registry_off) {
          CPRegistry::get()->insert(img, count, op);
          //if (verbose) {
          //  CPRegistry::get()->verbose();
          //}
        }

        numCycles += img._fc;
        //assert(numCycles + img._cmpc == orig.numCycles());
        ++count;

      }
    }
    
    if (count_nodes) {
      count++;
    }

    if (count == max_inst || count == Prof::get().stopInst) {
      break;
    }
    if (count && count % 100000 == 0) {
      std::cout << "processed " << count << "\n";
    }
  }
  numCycles += img._cmpc;

  gettimeofday(&end, 0);
  uint64_t start_time = start.tv_sec*1000000 + start.tv_usec;
  uint64_t end_time   = end.tv_sec * 1000000 + end.tv_usec;
  std::cout << "runtime : " << (double)(end_time - start_time)/1000000 << "  seconds\n";
  std::cout << "rate....: "
            <<  1000000*(double)count/((double)(end_time - start_time))
            << " recs/sec\n";

  std::cout << "numCycles " << numCycles << "\n";
  inf.close();
  std::cout << "Num of records              :" << count << "\n";
  if (!count_nodes) {
    std::cout << "Number of cycles [ original ]: " << orig.numCycles() << "\n";
    //std::cout << "Number of cycles [cpdg]    :" << base_ooo.numCycles()
    //          << "  " << (double)orig.numCycles()/base_ooo.numCycles() << "\n";

    if (!registry_off) {
      CPRegistry::get()->results(orig.numCycles());

      if(!noMcPAT) {
        system("mkdir -p mcpat/");

        orig.printMcPATxml("mcpat/orig.xml");
        CPRegistry::get()->printMcPATFiles();

        std::cout << "Calculating orig Energy/Power...";
        std::cout.flush();

        system("mcpat -infile mcpat/orig.xml 2>&1 > mcpat/orig.out");
        system("grep -ir \"Runtime Dynamic\" mcpat/orig.out | head -1 | cut -d\" \" -f6");
        CPRegistry::get()->runMcPAT();
      }
    }
    std::cout << "--------------------\n";
  }
  return 0;
}
