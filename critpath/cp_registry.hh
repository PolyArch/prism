
#ifndef CP_REGISTRY_HH
#define CP_REGISTRY_HH
#include <string>
#include <getopt.h>

#include "critpath.hh"
#include "cp_args.hh"
#include "map"

#include <vector>
#include <tuple>


static  std::string exec(const char* cmd) {
  FILE* pipe = popen(cmd, "r");
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while(!feof(pipe)) {
      if(fgets(buffer, 128, pipe) != NULL)
              result += buffer;
  }
  pclose(pipe);
  return result;
}

static std::string grepF(std::string& fname, const char* sval, 
                  int lineoff, int field) {
  std::stringstream rss;
  rss << "grep -irA" << lineoff-1 <<  " \"" << std::string(sval) << "\" "
      << fname << " | tail -1 | tr -s \" \" | cut -d\" \" -f" << field;

  std::string rs = rss.str();
  //std::cout << rs << "\n";
  return exec(rs.c_str());
}

static void execMcPAT(std::string& inf, std::string& outf) {
  const char *mcpat = getenv("MCPAT");
  if (!mcpat) {
    mcpat = "mcpat";
    std::string ms = std::string(mcpat) + std::string(" -opt_for_clk 0 -print_level 5 -infile ") 
                   + inf + std::string(" > ") + outf;
    //std::cout << ms << "\n";
    system(ms.c_str());
  }
}


class CPRegistry {
private:
  CPRegistry() {}
  static CPRegistry *_registry;
  std::map<std::string, CriticalPath*> cpmap;
  std::map<std::string, std::vector<std::pair<bool, ArgumentHandler*>> >
    register_arg_map;
  std::map<std::string, bool> cp2Enabled;
  CriticalPath* baselineCP=NULL;

public:
  static CPRegistry* get() {
    if (_registry)
      return _registry;
    _registry = new CPRegistry();
    return _registry;
  }

  void handleModelArgv(const char *argv, bool chkForBothModel = true)
  {
    if (cp2Enabled.count(argv)) {
      cp2Enabled[argv] = true;
      return;
    }
    if (strncmp(argv, "no-", 3) == 0 && argv[3]) {
      if (cp2Enabled.count(&argv[3])) {
        cp2Enabled[&argv[3]] = false;
        return;
      }
      if (chkForBothModel) {
        std::string inorder = (std::string("no-inorder-")
                               + std::string(&argv[3]));
        std::string ooo = std::string("no-ooo-") + std::string(&argv[3]);
        handleModelArgv(inorder.c_str(), false);
        handleModelArgv(ooo.c_str(), false);
      }
    }
    if (!chkForBothModel)
      return;

    std::string inorder = std::string("inorder-") + std::string(argv);
    std::string ooo = std::string("ooo-") + std::string(argv);
    handleModelArgv(inorder.c_str(), false);
    handleModelArgv(ooo.c_str(), false);
  }

  void setWidth(int width, bool inorder) {
    for (auto i = cpmap.begin(); i != cpmap.end(); ++i) {
      if(i->second->isInOrder() == inorder) {
        i->second->setWidth(width);
      }
    }
  }

  void setGlobalParams(int nm, int maxEx, int maxMem) {
    for (auto i = cpmap.begin(); i != cpmap.end(); ++i) {
      i->second->set_nm(nm);
      i->second->set_max_mem_lat(maxMem);
      i->second->set_max_ex_lat(maxEx);
    }
  }


  void setTraceOutputs(bool t) {
    for (auto i = cpmap.begin(); i != cpmap.end(); ++i) {
      i->second->setTraceOutputs(t);
    }
  }

  void pruneCP(bool inorder, bool ooo) {
    assert((inorder || ooo) && "both inorder and ooo are false.");
    // erase inorder, ooo
    for (auto i = cpmap.begin(); i != cpmap.end(); ) {
      bool isInorder = i->second->isInOrder();
      if ((isInorder && !inorder) // no inorder allowed
          || (!isInorder && !ooo) // no ooo allowed
          || !cp2Enabled[i->first]) //specific
        i = cpmap.erase(i);
      else
        ++i;
    }
  }

  void setDefaults() {
    for (auto i = cpmap.begin(); i != cpmap.end(); ++i) {
      i->second->setDefaultsFromProf();
    }
  }


  void register_cp(std::string name, CriticalPath *cp,
                   bool EnableByDefault, bool isBaseline) {
    std::string fullname;
    if(cp->isInOrder()) {
       fullname="inorder-"+name;
    } else {
       fullname="ooo-"+name;
    }
    if (cpmap.find(fullname.c_str()) != cpmap.end()) {
      std::cerr << "CP :" << fullname << " is already defined" << "\n";
      assert(0);
    }
    cpmap[fullname.c_str()] = cp;
    cp->setName(fullname);
    cp2Enabled[fullname] = EnableByDefault;

    if (isBaseline) {
      baselineCP = cp;
    }
  }

  void register_argument(std::string name, bool has_arg,
                         ArgumentHandler *handler) {
    register_arg_map[name].push_back(std::make_pair(has_arg, handler));
  }

  void insert(CP_NodeDiskImage img, uint64_t index, Op* op) {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->insert(img, index, op);
    }
  }

  void results() {

    if (!baselineCP && cpmap.size() > 0) {
      baselineCP = cpmap.begin()->second;
    }
    uint64_t baselineCycles = baselineCP ? baselineCP->numCycles() : 0;

    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->printResults(std::cout, I->first, baselineCycles);
    }
  }

  void printMcPATFiles() {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->printMcPATxml((std::string("mcpat/") + _run_name +
                                I->first + std::string(".xml")).c_str() );
    }
  }

  void runMcPAT() {
    // look up env
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      std::cout << I->first << " Dynamic Power(W)... ";
      std::cout.flush();

      std::string inf = std::string("mcpat/") + _run_name + 
                         I->first + std::string(".xml");
      std::string outf = std::string("mcpat/") + _run_name + 
                         I->first + std::string(".out");

      execMcPAT(inf,outf);

      float tot_dyn_p = stof(grepF(outf,"Processor:",9,5));
      float tot_leak_p = stof(grepF(outf,"Processor:",4,5));

      std::cout << tot_dyn_p << " " << tot_leak_p << "\n";

      I->second->calcAccelEnergy();
    }
  }

  std::string _run_name = "";
  void setRunName(std::string run_name) {
    _run_name = run_name;
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->setRunName(_run_name);
    }
  }

  void setupOptions(std::vector<struct option>& long_options,
                    struct option *static_long_options) {
    unsigned i = 0;
    std::map<std::string, bool> optionMap;

    while (static_long_options[i].name) {
      long_options.push_back(static_long_options[i]);
      if (optionMap.count(static_long_options[i].name)) {
        std::cerr << "Warning: Duplicate option: "
                  << static_long_options[i].name << "\n";
      }
      optionMap[static_long_options[i].name] = true;
      ++i;
    }

    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      struct option opt;
      opt.name = strdup(I->first.c_str());
      opt.has_arg = no_argument;
      opt.flag = 0; opt.val = 0;
      long_options.push_back(opt);

      if (optionMap.count(I->first)) {
        std::cerr << "Warning: Duplicate option: "
                  << I->first << "\n";
      }

      optionMap[opt.name] = true;

      std::string noOpt = std::string("no-") + I->first;
      opt.name = strdup(noOpt.c_str());
      long_options.push_back(opt);

      // No need to check here
      if (optionMap.count(noOpt)) {
        std::cerr << "Warning: Duplicate option: "
                  << noOpt << "\n";
      }
      optionMap[opt.name] = true;
    }

    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      struct option opt;
      std::string nam = ((I->first.find("ooo-") == 0)
                         ? I->first.substr(4)
                         : ((I->first.find("inorder-") == 0)
                            ? I->first.substr(8)
                            : ""));
      if (nam == "")
        continue;
      // already defined option -- FIXME:: Warn if duplicate??
      if (optionMap.count(nam))
        continue;
      opt.name = strdup(nam.c_str());
      long_options.push_back(opt);
      optionMap[nam] = true;
      std::string noOpt = std::string("no-") + nam;
      opt.name = strdup(noOpt.c_str());
      long_options.push_back(opt);
      optionMap[noOpt] = true;
    }

    //
    for (auto I = register_arg_map.begin(), E = register_arg_map.end();
         I != E; ++I) {
      struct option opt;
      opt.name = strdup(I->first.c_str());
      opt.has_arg = I->second[0].first;
      opt.flag = 0; opt.val = 1;
      long_options.push_back(opt);
    }
    //
    long_options.push_back(static_long_options[i]);
  }

  void handleArgv(const char *name, const char *optarg) {
    auto I = register_arg_map.find(name);
    assert(I != register_arg_map.end());
    for (auto J = I->second.begin(), E = I->second.end(); J != E; ++J) {
      J->second->handle_argument(name, optarg);
    }
  }
};


template<typename T>
struct RegisterCP
{
  T cp_obj;
  RegisterCP(const char *N,
             bool attachInorder = false,
             bool EnableByDefault = false,
             bool isBaseline = false) {
    cp_obj.setInOrder(attachInorder);
    CPRegistry::get()->register_cp(std::string(N),
                                   &cp_obj,
                                   EnableByDefault,
                                   isBaseline);
  }
};

#endif
