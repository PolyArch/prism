#ifndef BERET_INST
#define BERET_INST

#include "cp_dep_graph.hh"
#include "loopinfo.hh"

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

  std::vector<std::weak_ptr<DynSubgraph>> dep_subgraphs; //things that i depend on
  std::vector<std::weak_ptr<DynSubgraph>> use_subgraphs; //things that use me

  int dyn_ind;
  uint64_t index;

  DynSubgraph(Subgraph *sg, int dyn_index, int inst_index) {
    startCFU=std::make_shared<T>();
    endCFU=std::make_shared<T>();
    startCFU->_index=inst_index; //for tracking errors
    static_sg=sg;
    dyn_ind=dyn_index;
    index=inst_index;
  }

  static void addDep(std::shared_ptr<DynSubgraph> a, 
                     std::shared_ptr<DynSubgraph> b, bool ignore_if_cycle,
                     const char* m, NLAInst& i1, NLAInst& i2);


  void setCumWeights(CumWeights* cum_weights) {
    startCFU->_cum_weights=cum_weights;
    endCFU->_cum_weights=cum_weights;
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
   
  int iter = -1;
  bool bypassed = false;

  NLAInst(Op* op) {
    this->_op=op;
  }

  virtual ~NLAInst() { 
    //std::cout << "deleting nla inst" << _index << "\n";
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  E* ex_edge, *st_edge;
  bool double_line=false;
  int lines_swapped=0;
  void updateImg(const CP_NodeDiskImage &img) {
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _true_cache_prod=img._true_cache_prod;
    _ex_lat=img._cc-img._ec;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._xc-img._wc;
    _floating=img._floating;
    _iscall=img._iscall;
    _hit_level=img._hit_level;
    _miss_level=img._miss_level;
    _eff_addr=img._eff_addr;
    _acc_size=img._acc_size;
  }


  NLAInst(const CP_NodeDiskImage &img, uint64_t index, Op* op):
              dg_inst_base<T,E>(index){
    updateImg(img);
    this->_op=op;
    isAccelerated=true;
  }

/*  void setCumWeights(CumWeights* cum_weights) {
    for (int i = 0; i < NumStages; ++i) {
      events[i]._cum_weights=cum_weights;
    }
  }*/

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

  virtual unsigned beginExecute() { //this is when to delay the begining of execution
    return Execute; 
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
