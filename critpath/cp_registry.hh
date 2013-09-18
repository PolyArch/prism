
#ifndef CP_REGISTRY_HH
#define CP_REGISTRY_HH
#include<string>

#include "critpath.hh"
#include "map"

#include<vector>
#include<tuple>

class CPRegistry {
private:
  CPRegistry() {}
  static CPRegistry *_registry;
  std::map<std::string, CriticalPath*> cpmap;

public:
  static CPRegistry* get() {
    if (_registry)
      return _registry;
    _registry = new CPRegistry();
    return _registry;
  }

  bool inorder_model;
  bool ooo_model;

  void setModels(bool inorder,bool ooo) {
    inorder_model=inorder;
    ooo_model=ooo; 

    if(inorder_model==false) {
      for(auto i=cpmap.begin();i!=cpmap.end();) {
        CriticalPath* cp = i->second;
        if(cp->isInOrder()) {
          i = cpmap.erase(i); 
        } else {
          ++i;
        }
      }
    } else if(ooo_model==false) {
      for(auto i=cpmap.begin();i!=cpmap.end();) {
        CriticalPath* cp = i->second;
        if(!cp->isInOrder()) {
          i = cpmap.erase(i); 
        } else {
          ++i;
        }
      }
    }
  }

  void register_cp(std::string name, CriticalPath *cp) {
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
  }

  void insert(CP_NodeDiskImage img, uint64_t index, Op* op) {
    for (auto I = cpmap.begin(), E = cpmap.end(); I != E; ++I) {
      I->second->insert(img, index, op);
    }
  }

/*
  void verbose() {
    for (std::map<const char*, CriticalPath *>::iterator I = cpmap.begin(),
           E = cpmap.end(); I != E; ++I) {
      std::cout << I->first << " :: ";
      I->second->_nodes.back().print_to_stream(std::cout);
    }
  }
*/


  void results(uint64_t baseline) {
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
      std::cout << "Calculating " << I->first << " Energy/Power...";
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
};


template<typename T>
struct RegisterCP
{
  T cp_obj;
  RegisterCP(const char *N, bool attachInorder=false) {
    cp_obj.setInOrder(attachInorder);
    CPRegistry::get()->register_cp(std::string(N), &cp_obj);
  }
};


#endif
