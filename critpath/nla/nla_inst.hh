#ifndef BERET_INST
#define BERET_INST

#include "cp_dep_graph.hh"

class NLAInst;

class DynSubgraph {
public:
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  int mark = 0;
  Subgraph* static_sg;

  std::shared_ptr<T> startCFU;
  std::shared_ptr<T> endCFU;

  std::vector<std::weak_ptr<NLAInst>> insts;
  std::set<Op*> ops_in_subgraph;

  std::vector<std::weak_ptr<DynSubgraph>> dep_subgraphs;
  std::vector<std::weak_ptr<DynSubgraph>> use_subgraphs;

  int ind;
  DynSubgraph(Subgraph *sg, int index) {
    startCFU=std::make_shared<T>();
    endCFU=std::make_shared<T>();
    static_sg=sg;
    ind=index;
  }

  static void addDep(std::shared_ptr<DynSubgraph> use, 
                     std::shared_ptr<DynSubgraph> dep) {
    assert(dep!=use);
    use->use_subgraphs.push_back(dep);
    dep->dep_subgraphs.push_back(use);
  }

  int critCycles; 
  std::shared_ptr<NLAInst> calcCritCycles();
};

class NLAInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      Execute = 0,
      Complete = 1,
      Forward = 2,
      Writeback = 3,
      NumStages
  };

private:
  T events[NumStages];

public:

  std::shared_ptr<DynSubgraph> dynSubgraph;
  
  std::shared_ptr<T>& startCFU() {return dynSubgraph->startCFU;}
  std::shared_ptr<T>& endCFU() {return dynSubgraph->endCFU;}
   


  NLAInst(Op* op) {
    this->_op=op;
  }

  virtual ~NLAInst() { 
    //std::cout << "deleting nla inst" << _index << "\n";
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  bool _isctrl=0;
  bool _ctrl_miss=0;
  uint16_t _icache_lat=0;
  uint16_t _prod[MAX_SRC_REGS]={0,0,0,0,0,0,0};
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint64_t _true_cache_prod=false;
  uint16_t _ex_lat=0;
  bool _serialBefore=0;
  bool _serialAfter=0;
  bool _nonSpec=0;
  uint16_t _st_lat=0;
  uint64_t _pc=0;
  uint16_t _upc=0;
  uint64_t _eff_addr;
  bool _floating=false;
  bool _iscall=false;
  E* ex_edge, *st_edge;

  void updateImg(const CP_NodeDiskImage &img) {
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    _icache_lat=img._icache_lat;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _true_cache_prod=img._true_cache_prod;
    _ex_lat=img._cc-img._ec;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._xc-img._wc;
    _pc=img._pc;
    _upc=img._upc;
    _floating=img._floating;
    _iscall=img._iscall;
  }


  NLAInst(const CP_NodeDiskImage &img, uint64_t index, Op* op):
              dg_inst_base<T,E>(index){
    updateImg(img);
    this->_op=op;
  }

  NLAInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void updateLat(uint16_t lat) {
    ex_edge->_len=lat;
  }

  void updateStLat(uint16_t lat) {
    st_edge->_len=lat;
  }

  uint16_t ex_lat() {
    return ex_edge->_len;
  }

  void reCalculate() {
    startCFU()->reCalculate();
    for(int i = 0; i < NumStages; ++i) {
      events[i].reCalculate();
    }
    endCFU()->reCalculate();
  }

  void reCalculate(int i) {
    events[i].reCalculate();
  }

  virtual unsigned numStages() {
    return NumStages;
  }

  virtual uint64_t cycleOfStage(const unsigned i) {
    return events[i].cycle(); 
  }

  virtual unsigned eventComplete() {
    return Complete; 
  }

  virtual unsigned eventReady() { //this is when to delay the begining of execution
    return Execute; 
  }

  virtual unsigned memComplete() {
    if(this->_isload) {
      return Complete; //TODO: Complete or commit?
    } else if (this->_isstore) {
      return Writeback;
    } else {
      assert(0 && "no mem access allowed");
    }
  }


/*  virtual unsigned memComplete() {
    return Complete;
  }*/

};

#endif
