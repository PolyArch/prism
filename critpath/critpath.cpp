
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
#include "plugin.hh"

#include "prof.hh"

#include <fstream>

CPRegistry* CPRegistry::_registry = 0;


using namespace std;


int main(int argc, char *argv[])
{
  bool inorder_model=true;
  bool ooo_model=true;
  int  noMcPAT=0;
  int  allModels=0;
  int inorderWidth=0;
  int oooWidth=0;
  bool traceOutputs = false;
  int gen_loop_prof = 0;

  load_plugins(argv[0]);

  static struct option static_long_options[] =
    {
      {"help", no_argument, 0, 'h'},
      {"verbose",no_argument, 0, 'v'},
      {"count", no_argument, 0, 't'},
      {"no-registry", no_argument, 0, 'n'},
      {"max-insts", required_argument, 0, 'm'},
      {"loop-prof-max-insts", required_argument, 0, 'l'},
      {"models", required_argument, 0, 'x'}, //inorder, ooo, both
      {"no-mcpat", no_argument, &noMcPAT, 1},
      {"inorder-width", required_argument, 0, 2},
      {"ooo-width", required_argument, 0, 3},
      {"trace-out", no_argument, 0, 4},
      {"all-models", no_argument, &allModels, 1},
      {"gen-loop-prof", no_argument, &gen_loop_prof, 1},
      {0,0,0,0}
    };

  std::vector<struct option> long_options;
  CPRegistry::get()->setupOptions(long_options, static_long_options);


  uint64_t max_inst = (uint64_t)-1;
  uint64_t loop_prof_max_inst = max_inst;
  bool verbose =false;
  bool registry_off = false;
  bool count_nodes = false;
  while (1) {
    int option_index = 0;

    const struct option *longopts = &*long_options.begin();
    int c = getopt_long(argc, argv, "hvtnm:x:",
                        longopts, &option_index);
    if (c == -1)
      break;

    switch(c) {
    case 0:
      CPRegistry::get()->handleModelArgv(long_options[option_index].name);
      //std::cout << "Saw " << long_options[option_index].name << "\n";
      break;
    case 1:
      CPRegistry::get()->handleArgv(long_options[option_index].name,
                                    optarg);
      break;
    case 2:
      inorderWidth = atoi(optarg);
      if (!inorderWidth)
        inorderWidth = 2;
      break;
    case 3:
      oooWidth = atoi(optarg);
      if (!oooWidth)
        oooWidth = 4;
      break;
    case 4:
      traceOutputs = true;
      break;
    case 'h':
      std::cout << argv[0] << " [options] file\n";
      return(0);
    case 'v': verbose = true; break;
    case 'm': max_inst = atoi(optarg); break;
    case 'l': loop_prof_max_inst = atoi(optarg); break;
    case 'n': registry_off = true; break;
    case 't': count_nodes = true; break;
    case 'x':
      if (strcmp(optarg, "inorder") == 0) {
        inorder_model = true;
        ooo_model = false;
      } else if (strcmp(optarg,"ooo") == 0) {
        inorder_model = false;
        ooo_model = true;
      } else if (strcmp(optarg, "both") == 0) {
        inorder_model = ooo_model = true;
      } else {
        std::cerr << "option: \""
                  << optarg << "\" is not valid for --models."
                  << "Options are inorder, ooo, or both.\n";
      }
    case '?': break;
    default:
      abort();
    }
  }

  if (argc - optind != 1) {
    std::cerr << "Requires one argument.\n";
    return 1;
  }

  //determine the prof file name
  std::string prof_file(argv[optind]);
  size_t start_pos = prof_file.find_last_of("/");

  //open prof file
  if (!gen_loop_prof) {
    size_t dot_pos =  prof_file.find(".", start_pos);
    prof_file = ((dot_pos == string::npos)
                 ? prof_file
                 : prof_file.substr(0, dot_pos));
    prof_file += string(".prof");

    std::cout << "reading prof file: " << prof_file;
    std::cout.flush();
    Prof::init(prof_file);
    std::cout << "... done!\n";
  } else {
    if (loop_prof_max_inst == (uint64_t)-1)
      // use max_inst
      loop_prof_max_inst = max_inst;

    std::cout << "Generating Loop Info from trace\n";
    std::cout.flush();
    Prof::init_from_trace(argv[optind], loop_prof_max_inst);
    std::cout << "... generating loop info ... done!!\n";
  }


  //Process m5out/stats.txt for events
  string statsfile;
  if(start_pos != string::npos) {
    string dir = prof_file.substr(0, start_pos);
    statsfile = dir + "/m5out/stats.txt";
  } else {
    statsfile = "m5out/stats.txt";
  }
  Prof::get().procStatsFile(statsfile.c_str());

  //Process m5out/config.ini for events
  string configfile;
  if(start_pos != string::npos) {
    string dir = prof_file.substr(0, start_pos);
    configfile = dir + "/m5out/config.ini";
  } else {
    configfile = "m5out/config.ini";
  }
  Prof::get().procConfigFile(configfile.c_str());

  if(!allModels) {
    CPRegistry::get()->pruneCP(inorder_model, ooo_model);
  }
  CPRegistry::get()->setDefaults();

  if(inorderWidth > 0) {
    CPRegistry::get()->setWidth(inorderWidth, true);
  }
  if(oooWidth > 0) {
    CPRegistry::get()->setWidth(oooWidth, false);
  }
  CPRegistry::get()->setTraceOutputs(traceOutputs);

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
  igzstream inf(argv[optind], std::ios::in | std::ios::binary);

  if (!inf.is_open()) {
    std::cerr << "Cannot open file: \"" << argv[optind] << "\"\n";
    return 1;
  }

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
        if (!registry_off) {
          CPRegistry::get()->insert(img, count, op);
        }

        numCycles += img._fc;
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
    if (!registry_off) {
      CPRegistry::get()->results();

      system("mkdir -p mcpat/");
      CPRegistry::get()->printMcPATFiles();

      if(!noMcPAT) {
        CPRegistry::get()->runMcPAT();
      }
    }
    std::cout << "--------------------\n";
  }
  return 0;
}
