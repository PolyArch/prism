
#ifndef CP_REGISTRY_HH
#define CP_REGISTRY_HH
#include <string>
#include <getopt.h>

#include "critpath.hh"
#include "map"

#include <vector>
#include <tuple>

class CPRegistry {
private:
  CPRegistry() {}
  static CPRegistry *_registry;
  std::map<std::string, CriticalPath*> cpmap;
  std::map<std::string, bool> cp2Enabled;
  CriticalPath* baselineCP=NULL;

public:
  static CPRegistry* get() {
    if (_registry)
      return _registry;
    _registry = new CPRegistry();
    return _registry;
  }

  void handleArgv(const char *argv, bool chkForBothModel = true) {
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
        handleArgv(inorder.c_str(), false);
        handleArgv(ooo.c_str(), false);
      }
    }
    if (!chkForBothModel)
      return;

    std::string inorder = std::string("inorder-") + std::string(argv);
    std::string ooo = std::string("ooo-") + std::string(argv);
    handleArgv(inorder.c_str(), false);
    handleArgv(ooo.c_str(), false);
  }

  void setWidth(int width, bool inorder) {
    for (auto i = cpmap.begin(); i != cpmap.end(); ++i) {
      if(i->second->isInOrder() == inorder) {
        i->second->setWidth(width);
      }
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
    std::string trace_out = std::string(fullname.c_str()) + ".txt";
    cp->setupOutFile(trace_out.c_str());
    cp2Enabled[fullname] = EnableByDefault;

    if(isBaseline) {
      baselineCP = cp;
    }
  }

  void insert(CP_NodeDiskImage img, uint64_t index, Op* op) {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->insert(img, index, op);
    }
  }

  void results() {
    if(!baselineCP) {
      baselineCP = cpmap.begin()->second;
    }
    double baseline = baselineCP->numCycles();

    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      std::cout << "Number of cycles [ " << I->first << " ]: "
                << I->second->numCycles() << "  "
                << (double)baseline/(double)I->second->numCycles();
      I->second->accelSpecificStats(std::cout);
      std::cout << "\n";
    }
  }
  void printMcPATFiles() {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->printMcPATxml( (std::string("mcpat/") +
                               I->first + std::string(".xml")).c_str() );
    }
  }
  void runMcPAT() {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      std::cout << I->first << " Dynamic Power(W)... ";
      std::cout.flush();

      std::string ms = std::string("mcpat -infile mcpat/") + I->first +
                       std::string(".xml 2>&1 > mcpat/") + I->first +
                       std::string(".out");

      system(ms.c_str());
      std::string gs = std::string("grep -ir \"Runtime Dynamic\" ") +
                       std::string("mcpat/") +  I->first + std::string(".out") +
                       std::string(" | head -1 | cut -d\" \" -f6");
      system(gs.c_str());

    }
  }

  void setupOptions(std::vector<struct option>& long_options,
                    struct option *static_long_options) {
    unsigned i = 0;
    while (static_long_options[i].name) {
      long_options.push_back(static_long_options[i]);
      ++i;
    }
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      struct option opt;
      opt.name = strdup(I->first.c_str());
      opt.has_arg = no_argument;
      opt.flag = 0; opt.val = 0;
      long_options.push_back(opt);
      std::string noOpt = std::string("no-") + I->first;
      opt.name = strdup(noOpt.c_str());
      long_options.push_back(opt);
      if (I->first.find("ooo-") == 0) {
        std::string nam = I->first.substr(4);
        opt.name = strdup(nam.c_str());
        long_options.push_back(opt);
        std::string noOpt = std::string("no-") + nam;
        opt.name = strdup(noOpt.c_str());
        long_options.push_back(opt);
      }
    }
    long_options.push_back(static_long_options[i]);
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
