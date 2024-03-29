
#ifndef CP_DEP_GRAPH_HH
#define CP_DEP_GRAPH_HH

#include <map>
#include <unordered_map>

#include <vector>

//#include "cpnode.hh"
#define STANDALONE_CRITPATH 1
#include "cpu/crtpath/crtpathnode.hh"

#include "op.hh"

#include <iostream>
#include <stdio.h>
#include <memory>
#include <set> 
#include <deque>
#include <unordered_set>
#include <iomanip>

// Terminology:
// "event" refers to individual instruction "stages" like fetch/decode/etc..
// "edge" is a dependency between events
// "inst" is simply a group of events


#include "edge_table.hh"

enum AnalysisType {
  Weighted=0,
  Unweighted=1,
  Offset=2
};

class CumWeights {
public:
  double edges_weighted[E_NUM]={0}; //acumulated edge weights
  double edges_unweighted[E_NUM]={0}; //acumulated edge weights
  double edges_offset[E_NUM]={0}; //acumulated edge weights

  void countEdge(int i,int time,float weight) {
    edges_weighted[i]+=time*weight;
    edges_unweighted[i]+=weight; //time is the weight
    edges_offset[i]+=(time+1)*weight; //some compromise to show 0-cycle edges
  }

  double weightOfEdge(int i, AnalysisType analType) {
    if(analType == AnalysisType::Weighted) {
      return edges_weighted[i];
    } else if (analType == AnalysisType::Unweighted) {
      return edges_unweighted[i];
    } else if (analType == AnalysisType::Offset) {
      return edges_offset[i];
    } 
    assert(0);
    return 123456789.10;
  }

  void print_edge_weights(std::ostream& out, AnalysisType analType) {
    double total_weight=0; //should roughly equal number of cycles in program
    double* arr=0;
    switch(analType) {
      case AnalysisType::Weighted:   arr=edges_weighted;   break;
      case AnalysisType::Unweighted: arr=edges_unweighted; break;
      case AnalysisType::Offset:     arr=edges_offset;     break;
    }
  
    for(int i = 0; i < E_NUM; ++i) {
      double weight=arr[i];
      total_weight+=weight;
    }
  
    for(int i = 0; i < E_NUM; ++i) {
      double weight = arr[i];
      if(weight!=0.0) {
        out << edge_name[i] << ":" << std::setprecision(3) << weight/total_weight << ",";
      }
    }
    out << " (total: " << total_weight << ")";
  }
};

template<typename E>
class CritEdgeListener {
public:
  virtual void listen(E* edge, float weight) = 0;
};

// single node
class dg_event_base {
public:
  virtual ~dg_event_base() {}
  virtual void setCycle(uint64_t) = 0;
};

// Dependency edge between events
template<typename T>
class dg_edge_impl_t {

public:
  typedef T NodeTy;
  typedef T * TPtr;

  TPtr _src,_dest;
  int _len;
  unsigned _type;
  
  dg_edge_impl_t(TPtr src, TPtr dest, unsigned len, unsigned type = E_NONE)
    : _src(src), _dest(dest), _len(len), _type(type) {}
  virtual ~dg_edge_impl_t() {}  
  TPtr dest() { return _dest;}
  TPtr src() { return _src;}
  int length() { return _len;}
  bool isDD() { return _type==E_RDep || _type==E_MDep ;}
  unsigned len() {return _len;}
  unsigned type() {return _type;}
};



template<typename T, typename E>
class dg_inst_base {
public:
  uint64_t _index=0;
  bool _done = false;
  uint16_t _opclass = 0;
  bool _isload = false;
  bool _isstore = false;
  Op* _op = NULL;
  bool isAccelerated=false;

  //Dynamic stuff:
  bool _isctrl = false;
  bool _ctrl_miss = false;
  uint16_t _prod[MAX_SRC_REGS] = {0,0,0,0,0,0,0};
  uint16_t _mem_prod = 0;
  uint16_t _cache_prod = 0;
  uint64_t _true_cache_prod=false;
  uint16_t _ex_lat = 0;
  bool _serialBefore = false;
  bool _serialAfter = false;
  bool _nonSpec = false;
  bool _floating = false;
  bool _iscall = false;
  uint16_t _st_lat = 0;
  uint64_t _eff_addr = 0;
  uint8_t _hit_level = 0, _miss_level = 0, _acc_size=0;
  uint8_t _type = 0; //type of instruction for debugging


  virtual T &operator[](const unsigned i) =0;

  virtual ~dg_inst_base() {
    //std::cout << "del: " << _index << " " << this << "\n";
/*
    static int warning_count=0;
    if(!done) {
      if(warning_count < 20) {
        std::cerr << "node never done-ed\n"
        warning_count++;
      }
    }
*/
  }

  virtual int adjustExecuteLatency(int lat) const {
    return lat;
  }

  virtual bool hasDisasm() const { return false; }
  virtual std::string getDisasm() const { return std::string(""); }
protected:

  dg_inst_base(uint64_t index):
    _index(index){
  }

  dg_inst_base() : _index(0) {}

  std::set<std::shared_ptr<dg_inst_base<T,E>>> _savedInsts;

public:
  void open_this_one() {
    for(unsigned j = 0; j < numStages(); ++j) {
     (*this)[j]._crit_path_opened=true;
    }
  }

//  virtual void open_for_backtrack() {
//    this->open_this_one();
//  }

  virtual void open_for_backtrack() {
    for(const auto& i : _savedInsts) {
      i->open_this_one();
    }
    this->open_this_one();
  }


  virtual bool savedIn(std::shared_ptr<dg_inst_base<T,E>> inst) {
    for(const auto& saved_inst : _savedInsts) {
      if(saved_inst == inst) {
        return true;
      }
      if(saved_inst->savedIn(inst)) {
        return true;
      }
    }
    return false;
  }

  virtual void saveInst(std::shared_ptr<dg_inst_base<T,E>> inst) {
    _savedInsts.insert(inst);
  }



  virtual uint64_t finalCycle() {
    uint64_t max=0;
    for(unsigned i = 0; i < numStages(); ++i) {
      if(max<cycleOfStage(i)) {
        max=cycleOfStage(i);
      }
    }
    return max;
  }
  virtual void setCumWeights(CumWeights* cm) {
    for(unsigned i = 0; i < numStages(); ++i) {
      (*this)[i]._cum_weights=cm;
      (*this)[i]._index=_index; //fun?
      (*this)[i]._op=_op; //fun?
    }
  }

  virtual void done(uint64_t n_index) {
     for(unsigned i = 0; i < numStages(); ++i) {
      (*this)[i].done(n_index); //fun?
    }
  }

  virtual uint64_t cycleOfStage(const unsigned i) = 0;
  virtual unsigned numStages() =0;
  virtual uint64_t index() {return _index;}
  virtual bool isPipelineInst() {return false;}

  virtual unsigned eventInception() {return 0;} //return the index of when the node's data is ready

  virtual unsigned beginExecute() {return (unsigned)-1;} //return the index of when the node's data begins executing
  virtual unsigned eventComplete() {return (unsigned)-1;} //return the index of when the node's data is ready
  virtual unsigned memComplete() {return (unsigned)-1;} //return the index of when the memory operation is ready
  virtual unsigned eventReady() {return (unsigned)-1;} //return the index of when the node's data is ready


  virtual unsigned eventCommit() {return numStages()-1;}

  typedef std::vector<std::shared_ptr< dg_inst_base<T, E> > > oplist_t;
  typedef typename oplist_t::iterator op_iterator;

protected:
  oplist_t _operands;
  oplist_t _mem_operands;

public:
  void addOperandInst(std::shared_ptr< dg_inst_base<T, E> > inst) {
    assert(inst);
    _operands.push_back(inst);
  }
  void addMemOperandInst(std::shared_ptr< dg_inst_base<T, E> > inst) {
    assert(inst);
    _mem_operands.push_back(inst);
  }

  void clearOperandInsts() {
    _operands.clear();
    _mem_operands.clear();
  }
  bool hasOperandInsts() const {
    return !_operands.empty();
  }
  bool hasMemOperandInsts() const {
    return !_mem_operands.empty();
  }

  op_iterator op_begin() { return _operands.begin(); }
  op_iterator op_end() { return _operands.end(); }

  op_iterator mem_op_begin() { return _mem_operands.begin(); }
  op_iterator mem_op_end()   { return _mem_operands.end();   }

  virtual bool isDummy() {return false;}
};



template<typename T, typename E>
// Dependence graph representation for instruction which flows through the pipeline
class dg_inst_dummy : public dg_inst_base<T,E> {
  typedef T* TPtr;
  typedef E* EPtr;
public:
  enum NodeTy {
    Dummy = 0,
    NumStages = 1
  };
  enum DummyType {
    DUMMY_MOVE=0,
    DUMMY_STACK_SLOT=1,
    DUMMY_CONST_LOAD=2
  };
  int _dtype;

protected:
  T events[NumStages];
  uint16_t _prod[MAX_SRC_REGS] = {0}; //all zeroes please : )
  uint16_t _mem_prod=0;

public:
  virtual ~dg_inst_dummy() {}

  virtual unsigned numStages() { return NumStages;}

  virtual unsigned beginExecute() {return 0;} 
  virtual unsigned eventComplete() {return 0;} 
  virtual unsigned eventReady()    {return 0;}
  virtual unsigned memComplete()   {return 0;}
  virtual unsigned eventCommit()   {return 0;}

  dg_inst_dummy(const CP_NodeDiskImage &img,uint64_t index,Op* op, int dtype):
    dg_inst_base<T,E>(index) {
    this->_opclass=img._opclass;
    this->_isload=img._isload;
    this->_isstore=img._isstore;
    this->_op=op;
    _mem_prod=img._mem_prod;
    _dtype=dtype;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
  }
 
  dg_inst_dummy(): dg_inst_base<T,E>() {}
  virtual uint64_t cycleOfStage(const unsigned i) {
    assert(i < NumStages);
    return events[i].cycle(); 
  }

  virtual T&operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void reset_inst() {
    for (int i = 0; i < NumStages; ++i) {
      events[i].remove_all_edges();
    }
  }

  bool getProd(unsigned& out_prod) {
    int count=0;
    for (int i = 0; i < MAX_SRC_REGS; ++i) {
      if(_prod[i]!=0) {
        if(count==0) {
          out_prod = _prod[i];
        }
        count++;
      }
    }
    return count > 1;
  }

  bool getMemProd(unsigned& out_prod) {
    out_prod=_mem_prod;
    return false;
  }

  bool getDataProdOfStore(unsigned& out_prod) {
    assert(this->_isstore);
    assert(this->_op->memHasDataOperand());
    int prod_index = this->_op->memDataOperandIndex();
    out_prod = _prod[prod_index];
    return false;
  }


  bool isMem() {return this->_isload || this->_isstore;}
  virtual bool isDummy() {return true;}
};



template<typename T, typename E>
// Dependence graph representation for instruction which flows through the pipeline
class dg_inst : public dg_inst_base<T,E> {
  typedef T* TPtr;
  typedef E* EPtr;
public:
  enum NodeTy {
    Fetch = 0,
    Dispatch = 1,
    Ready = 2,
    Execute = 3,
    Complete = 4,
    Commit = 5,
    Writeback = 6,
    NumStages = 7
  };

protected:
  T events[NumStages];

public:
  bool iqOpen=false;
  bool lqOpen=false;
  bool sqOpen=false;
  virtual ~dg_inst() { 
    /*for (int i = 0; i < NumStages; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  //Copy events from another Inst_t
  virtual void copyEvents(dg_inst<T,E>* from) {
    for (int i = 0; i < NumStages; ++i) {
      for (auto pi=from->events[i].pebegin(), 
                pe=from->events[i].peend(); pi!=pe; ++pi) {
        E* old_edge = *pi;
        E* edge = new E(old_edge->src(),&events[i], old_edge->len(), old_edge->type());
        old_edge->src()->add_edge(edge);
      }
    }
    this->_op=from->_op; //super fun?
    this->_index=from->_index; //super fun?
  }

  virtual unsigned numStages() {
    return NumStages;
  }

  virtual bool isPipelineInst() {
    return true;
  }
  virtual unsigned beginExecute() {
    return Execute;
  }
  virtual unsigned eventComplete() {
    return Complete;
  }
  virtual unsigned eventReady() {
    return Ready;
  }
  virtual unsigned memComplete() {
    if(this->_isload) {
      return Complete; 
    } else if (this->_isstore) {
      return Writeback;
    } else {
      assert(0 && "non-memory access not allowed");
    }
  }
  virtual unsigned eventCommit() {
    return Commit;
  }

  //Things from CP_Node image
  uint16_t _icache_lat = 0;
  uint8_t _numSrcRegs = 0;
  uint8_t _numFPSrcRegs = 0;
  uint8_t _numIntSrcRegs = 0;
  uint8_t _numFPDestRegs = 0;
  uint8_t _numIntDestRegs = 0;
  uint64_t _pc = 0;
  uint16_t _upc = 0;


  //Energy Counters, for validation
  //We could potentially delete this eventually
  //icache
  //uint8_t icache_read_accesses=0, icache_read_misses=0, icache_conflicts=0;

  ////ooo-pipeline
  //uint8_t regfile_reads=0, regfile_writes=0, regfile_freads=0, regfile_fwrites=0;
  //uint8_t rob_reads=0, rob_writes=0;
  //uint8_t iw_reads=0, iw_writes=0, iw_freads=0, iw_fwrites=0;
  //uint8_t rename_reads=0, rename_writes=0;

  //uint8_t btb_read_accesses=0, btb_write_accesses=0;


  dg_inst(const CP_NodeDiskImage &img,uint64_t index,Op* op=NULL):
    dg_inst_base<T,E>(index) {
    this->_opclass=img._opclass;
    this->_isload=img._isload;
    this->_isstore=img._isstore;
    this->_isctrl=img._isctrl;
    this->_ctrl_miss=img._ctrl_miss;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(this->_prod));
    this->_mem_prod=img._mem_prod;
    this->_cache_prod=img._cache_prod;
    this->_true_cache_prod=img._true_cache_prod;
    this->_ex_lat=img._ep_lat;
    this->_serialBefore=img._serialBefore;
    this->_serialAfter=img._serialAfter;
    this->_nonSpec=img._nonSpec;
    this->_st_lat=img._st_lat;
    this->_floating=img._floating;
    this->_iscall=img._iscall;
    this->_eff_addr=img._eff_addr;
    this->_hit_level=img._hit_level;
    this->_miss_level=img._miss_level;
    this->_acc_size=img._acc_size;


    _icache_lat=img._icache_lat;
    _numSrcRegs=img._numSrcRegs;
    _numFPSrcRegs=img._regfile_fread;
    _numIntSrcRegs=img._regfile_read;
    _numFPDestRegs=img._numFPDestRegs;
    _numIntDestRegs=img._numIntDestRegs;
    _pc=img._pc;
    _upc=img._upc;
    this->_op=op;
   
    for (int i = 0; i < NumStages; ++i) {
      events[i].set_inst(this);
     // if (img._isload)
     //   events[i].setLoad();
     // if (img._isstore)
     //   events[i].setStore();
     // if (img._isctrl)
     //   events[i].setCtrl();
     // events[i].prop_changed();
    }
    
  }

  void set_ex_lat(uint16_t lat) {this->_ex_lat=lat;}
 
  dg_inst(): dg_inst_base<T,E>() {}

  virtual uint64_t cycleOfStage(const unsigned i) {
    assert(i < NumStages);
    return events[i].cycle(); 
  }

  virtual T&operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void reset_inst() {
    for (int i = 0; i < NumStages; ++i) {
      events[i].remove_all_edges();
    }
  }

  bool isMem() {return this->_isload || this->_isstore;}

};

// Implementation of DYNOP Type, inerits from generic node
template <typename E>
class dg_event_impl_t: public dg_event_base
{
public:
  typedef E * EPtr;
  typedef typename E::TPtr TPtr;
  typedef typename E::NodeTy NodeTy;

  typedef typename std::vector<EPtr>::iterator EdgeIterator;
  typedef typename std::vector<EPtr>::iterator PredEdgeIterator;

  uint64_t _cycle;
  uint64_t _index=-2;
  uint64_t _n_index=0;
  Op* _op = NULL;
  dg_inst_base<NodeTy, E> *_inst;
  CumWeights* _cum_weights;
  bool _dirty=false;
  bool _crit_path_counted=false;
  bool _crit_path_opened=false;

  std::vector<EPtr> _edges; //forward edges
  //std::map<dg_event_base*, EPtr> _pred_edge_map;
  std::vector<EPtr> _pred_edges; //predecessor edges

  dg_event_impl_t():// _index(0), 
    _cycle(0), _inst(0), _cum_weights(0) { }

  dg_event_impl_t(uint64_t index, int ty,CumWeights* cw=NULL): 
                    _cycle(0), _inst(0), _cum_weights(cw) { }

  int numPredEdges() {
    return _pred_edges.size();
  }

  virtual ~dg_event_impl_t() {
    remove_all_edges();
    //_edges.clear();
  }

  void set_inst(dg_inst_base<NodeTy, E> *n){
    _inst = n;
  }

  bool isLastArrivingEdge(EPtr edge) {
    return edge->src()->cycle()+edge->len() == cycle();
  }

  //return an edge that explains the arrival time
  //bias given to earlier edges in EDGE_TYPE enum
  EPtr lastArrivingEdge() {
    for (auto I = _pred_edges.begin(), e=_pred_edges.end(); I!=e; ++I) {
      if ((*I)->src()->cycle()+(*I)->len() == cycle()) {
        return *I;
      }
    }
    return 0;
  }

  unsigned lastArrivingEdgeType() {
    EPtr edge = lastArrivingEdge();
    if(edge==0) {
      return E_NONE;
    } else {
      return edge->type();
    }
  }

  void reCalculate() {
    _cycle=0;
    for (auto i = _pred_edges.begin(), e = _pred_edges.end(); i!=e; ++i) {
      compute_cycle(*i); 
    }
  }

  typename std::vector<EPtr>::iterator pebegin() { return _pred_edges.begin(); }
  typename std::vector<EPtr>::iterator peend() { return _pred_edges.end(); }

  virtual bool has_pred(TPtr pred, unsigned type) {
    for(auto i = _pred_edges.begin(), e=_pred_edges.end();i!=e;++i){
      EPtr edge = *i;
      if(edge->src() == pred && edge->type()==type) {
        return true;
      }
    }
    return false;
  }

  void add_edge(EPtr e) {
    _edges.push_back(e);
    e->dest()->add_pred_edge(e);
    compute_cycle(e);
  }

  void done(uint64_t n_index) {
    _n_index=n_index;
  }

  static void compute_cycle(EPtr e) {

    static int num2000s=0;
    static int num5000s=0;
    if(e->length() > 5000 && num5000s < 10) {
      num5000s++;
      std::cerr << " WARNING: Edge:" //<< e->src()->_index << "->" << e->dest()->_index
                << " (type " << edge_name[e->type()] << ") is " << e->length() << " cycles long! \n";
    } else if(e->length() > 2000 && num2000s < 10) {
      num2000s++;
      std::cerr << " WARNING: Edge:" //<< e->src()->_index << "->" << e->dest()->_index
                << " (type " << edge_name[e->type()] << ") is " << e->length() << " cycles long! \n";
    } 
    e->dest()->setCycle(e->src()->cycle() + e->length());
  }

  /*
  virtual void compute_cycle_for_dep() {
    for (EdgeIterator I = _edges.begin(), e = _edges.end(); I != e; ++I) {
      compute_cycle(*I);
    }
  }

  virtual void compute_cycle_for_data_dep() {
    for (EdgeIterator I = _edges.begin(), e = _edges.end(); I != e; ++I) {
      if ( (*I)->isDD()) {
        compute_cycle(*I);
      } else {
        (*I)->dest()->setCycle(_cycle + 0);
      }
    }
  }
*/

  void add_pred_edge(EPtr e) {
    //dg_event_base *tmp = e->dest();
    _pred_edges.push_back(e);
  }

  void remove_pred_edge(EPtr rem_e) {
    _dirty=true;
    for (PredEdgeIterator I = _pred_edges.begin(), e=_pred_edges.end(); I != e; ++I) {
      if ((*I) == rem_e) {
        //delete (*I);
        _pred_edges.erase(I);
        return;
      }
    }
    assert(0);
  }

  void remove_edge(EPtr rem_e) {
    _dirty=true;
    for (EdgeIterator I = _edges.begin(), e=_edges.end(); I != e; ++I) {
      if ((*I) == rem_e) {
        //delete (*I);
        _edges.erase(I);
        return;
      }
    }
    assert(0);
  }

  void remove_all_edges() {
    //remove predecessor edges
    for (auto I = _pred_edges.begin(), e = _pred_edges.end(); I != e; ++I) {
      EPtr edge = *I;
      edge->src()->remove_edge(edge);
      delete (*I);
    }
    _pred_edges.clear();
    //remove dependent edges
    for (auto I = _edges.begin(), e = _edges.end(); I != e; ++I) {
      EPtr edge = *I;
      edge->dest()->remove_pred_edge(edge);
      delete (*I);
    }
    _edges.clear();
  }

  void print(const CP_NodeDiskImage &img) {
    std::cout << _cycle << " ";
  }

  //set if cycle is greater than current
  void setCycle(uint64_t cycle) {
    if (_cycle < cycle) {
      _cycle =  cycle;
    }
  }

  //void setLoad() { _load = true;}
  //void setStore() { _store = true;}
  //void setCtrl() { _ctrl =  true; }

  //bool isMem() { return isLoad() || isStore(); }
  //bool isLoad() { return _load; }
  //bool isStore() { return _store; }

  //bool isCtrl() { return _ctrl; }

  uint64_t cycle() { return _cycle; }
};



class dg_event: public dg_event_impl_t< dg_edge_impl_t<dg_event> >
{
public:
  dg_event(): dg_event_impl_t< dg_edge_impl_t<dg_event> >() {}

  dg_event(uint64_t index, int ty, CumWeights* cw=NULL): dg_event_impl_t< dg_edge_impl_t<dg_event> >(index, ty, cw)
  {
  }

};


// The abstract type for the entire graph
template<typename Inst_t, typename T, typename E>
class dep_graph_t {
public:
  virtual ~dep_graph_t() {};
  virtual void addInst(std::shared_ptr<dg_inst_base<T,E>> dg,uint64_t index) = 0;

  virtual void addInstChecked(std::shared_ptr<dg_inst_base<T,E>> dg,uint64_t index) = 0;

  virtual T* getHorizon() = 0;

  virtual E* insert_edge(T& event1, T& event2,
                         int len, unsigned etype=E_NONE) = 0;

  virtual E* insert_edge(T& event,
                         dg_inst_base<T,E>& destnodes,
                         const unsigned destTy,
                         int len, unsigned etype=E_NONE) = 0;

  virtual E* insert_edge(dg_inst_base<T,E>& srcnodes,
                           const unsigned srcTy,
                           T& event,
                           int len, unsigned etype=E_NONE) = 0;


  virtual E* insert_edge(dg_inst_base<T,E>& srcnodes,
                           const unsigned srcTy,
                           dg_inst_base<T,E>& destnodes,
                           const unsigned destTy,
                           int len, unsigned etype=E_NONE) = 0;


  virtual E* insert_edge(uint64_t srcIdx,
                           const unsigned srcTy,
                           dg_inst_base<T,E>& destnodes,
                           const unsigned destTy,
                           int len, unsigned etype=E_NONE) = 0;

  /*  virtual void insert_ff_edge(uint64_t srcIdx,
                              uint64_t destIdx, unsigned len) = 0;*/
  virtual dg_inst_base<T,E>& queryNodes(uint64_t idx) = 0;
  virtual bool hasIdx(uint64_t idx) = 0;
  virtual void setCritListener(CritEdgeListener<E>* cel) = 0;

  virtual CumWeights* cumWeights() = 0;

  virtual std::shared_ptr< dg_inst_base<T, E> > queryInst(uint64_t idx) = 0;
  virtual uint64_t getMaxCycles() =0;
  //virtual void printInstEvents(uint64_t index, std::ostream& out) = 0;

  virtual uint64_t getPipeLoc() const = 0;
  virtual Inst_t* peekPipe(int offset) = 0;
  virtual std::shared_ptr<Inst_t> peekPipe_sh(int offset) = 0;

  virtual void pushPipe(std::shared_ptr<Inst_t> dg) = 0;
  virtual void done(std::shared_ptr<Inst_t> dg) = 0;

  virtual void no_horizon() = 0;

  virtual void commitNode(uint64_t index)  = 0;
  virtual void finish() = 0;
  virtual void cleanup() = 0;
};

// Implementation for the entire graph
#define BSIZE 16384 //16384 //max number of insts to keep for data/mem dependence
#define HSIZE  6356 //6356
#define PSIZE   512 //max number in-flight instructions, biggest ROB size
#define RSIZE  1024
#define CSIZE (BSIZE-HSIZE-RSIZE)


template<typename Inst_t, typename T, typename E>
class dep_graph_impl_t : public dep_graph_t<Inst_t,T,E> {
public:
  dep_graph_impl_t(): _latestIdx(0), numCycles(0) {
    //_vec.resize(BSIZE);
    for (unsigned i = 0; i != BSIZE; ++i) {
      _vec[i] = 0;
    }
  }
  ~dep_graph_impl_t() { cleanup(); }
  //typedef dg_inst<T, E> Inst_t;

protected:
  //uint64_t _lastIdx;

  //InstVec _vec;
  typedef  std::shared_ptr<dg_inst_base<T, E> > dg_inst_base_ptr;
  dg_inst_base_ptr  _vec[BSIZE];
  uint64_t _latestIdx, _latestCleanIdx=0;


  CumWeights _cum_weights;
  CritEdgeListener<E>* _crit_edge_listener=NULL;

  // The "horizon" is the point beyond which no event may occur before, because
  // it is in the definite past.  This corresponds to a cycle number, and also
  // a node with which to attach dependencies to ensure that the horizon is not
  // crossed.
  //uint64_t _horizonCycle=0;
  uint64_t _horizonIndex=((uint64_t)-1);
  dg_inst_base_ptr _horizonInst;

  typename std::shared_ptr<Inst_t> _pipe[PSIZE];
  uint64_t _pipeLoc;

/*  void removeNodes(uint64_t idx) {
    IndexMapIterator I = _index2inst.find(idx);
    if (I == _index2inst.end())
      return;
    I->second.remove();
    _index2inst.erase(idx);
  }
*/

public:
  virtual void setCritListener(CritEdgeListener<E>* cel) {_crit_edge_listener=cel;}

  bool hasIdx(uint64_t idx) {
    if(idx > _latestIdx) {
      return false;  //don't have it -- looking into the future
    }
    if(idx >= BSIZE && idx <= _latestIdx - BSIZE) {
      return false;  //don't have it -- looking too far into the past
    }
    int vec_ind = idx % BSIZE;
    if(_vec[vec_ind]==NULL) {
      return false;
    }
    return true; //_vec[vec_ind]->_index == idx;  this is not necessarily true...
  }

  void no_horizon() {
    horizon_failed=true;
  }

  dg_inst_base<T,E>& queryNodes(uint64_t idx) {
    /*  IndexMapIterator I = _index2inst.find(idx);
    if (I != _index2inst.end()) {
      return I->second;
    }*/
    bool has = hasIdx(idx);
    if(!has) {
       //std::cout << "requested idx: " << idx << "(cur idx:" << _latestIdx << ")\n";
       assert(0);
    }

    int vec_ind = idx % BSIZE;
    return *_vec[vec_ind];
  }
  std::shared_ptr< dg_inst_base<T, E> > queryInst(uint64_t idx)
  {
    if (hasIdx(idx)) {
      return _vec[idx % BSIZE];
    }
    return 0;
  }

  bool horizon_failed=false;
  //update horizon takes the inst to be deleted
  virtual void updateHorizon(uint64_t del_index) {
    if(horizon_failed) {
      return;
    }

    /*
    //first check if we are warming up, and don't need to delete anything
    int del_ind=del_index%BSIZE;
    if(_vec[del_ind]==NULL) {
      return;
    }
    dg_inst_base_ptr del_inst = _vec[del_ind];
    if(!_horizonInst){ //initialize the existing horizon with del item
      _horizonInst=del_inst;
    }
*/
    //If we are deleting the horizon, or past the horizon, that's bad
    if(del_index >= _horizonInst->index()) {
      adjustHorizon(del_index+1);
    }
    return;
  }

   bool horizon_warning_printed=false;

   void adjustHorizon(uint64_t min_index) {
      uint64_t horizonCycle = _horizonInst->cycleOfStage(_horizonInst->eventInception());

      if(horizon_warning_printed==false) {
        //make sure we still have the instruction
        if(queryInst(_horizonInst->index()) != _horizonInst) {
          std::cerr << "ERROR: horizon inst lost\n";
          std::cerr << "min idx for func: " << min_index << "\n";
          std::cerr << "_horizonIdx: " << _horizonIndex << "\n";
          std::cerr << "Horizon Idx Inst op: " << queryInst(_horizonIndex)->_op->id() << "\n";
          std::cerr << "==" << queryInst(_horizonIndex)->_op->getUOPName() << "\n";
          std::cerr << "ptr: " << &*queryInst(_horizonIndex) << "\n";
          std::cerr << "index: " << queryInst(_horizonIndex)->_index << "\n";


          std::cerr << "Horizon Inst: " << _horizonInst->index() << "\n"
                    << "Horizon Inst op: " << _horizonInst->_op->id() << "\n"
                    << "==: " << _horizonInst->_op->getUOPName() << "\n"
                    << "ptr: " << &*_horizonInst << "\n";
          std::cerr << "Horizon Idx Inst: " << queryInst(_horizonInst->index())->index() << "\n";
          std::cerr << "Horizon Idx Inst op: " << queryInst(_horizonInst->index())->_op->id() << "\n";
           std::cerr << "==" << queryInst(_horizonInst->index())->_op->getUOPName() << "\n";
           std::cerr << "ptr: " << &*queryInst(_horizonInst->index()) << "\n";


         
        }
        assert(queryInst(_horizonInst->index()) == _horizonInst); 
      }

      uint64_t highest_hor_cand=0; //these two for debugging
      int num_iters=0;

      //iterate until we find a new horizon with appropriate inception time
      for(uint64_t horizonIndex = min_index; 
         horizonIndex < min_index + BSIZE; ++horizonIndex) {
         num_iters++; 
         dg_inst_base_ptr new_hor_inst = _vec[horizonIndex%BSIZE];
         if(!new_hor_inst || new_hor_inst->isDummy() || (horizonIndex!=new_hor_inst->_index)) {
           continue;
         }
         uint64_t new_hor_cycle = new_hor_inst->cycleOfStage(new_hor_inst->eventInception());
         if(new_hor_cycle >= highest_hor_cand) {
           highest_hor_cand=new_hor_cycle;
         }

         if(new_hor_cycle >= horizonCycle && _horizonInst != new_hor_inst) {
           if(0) { //debugging code
             std::cerr << "orig: 0, min: "
                       << (int64_t)min_index - (int64_t)_horizonInst->index();
             std::cerr << ", latest:" << (int64_t)_latestIdx - (int64_t)_horizonInst->index()
                       << ", new:" << new_hor_inst->index() - _horizonInst->index()
                       << "(" << horizonCycle << "to " << new_hor_cycle
                       << "; iters=" << num_iters << ")\n";
           }
           _horizonInst = new_hor_inst;
           _horizonIndex = horizonIndex;
           return;
         }
      }

      //Horizon Failed! -- Investigate! ------------------------------------
      if(horizon_warning_printed==false) {

        int num_iters=-1;
        for(uint64_t cur_idx = min_index-1;
           cur_idx < min_index + BSIZE; ++cur_idx, ++num_iters) {
           std::cerr << num_iters << "," <<cur_idx << ": ";
           dg_inst_base_ptr cur_inst = _vec[cur_idx%BSIZE];
           if(!cur_inst) {
             std::cerr << "null";
           } else if(cur_inst->isDummy()) {
             std::cerr << " " << cur_inst->_index << " ";
             std::cerr << "dummy\n";
             continue;
           } else {
              uint64_t incep_cycle = cur_inst->cycleOfStage(cur_inst->eventInception());
              uint64_t commit_cycle = cur_inst->cycleOfStage(cur_inst->eventCommit());

              std::cerr << " " << cur_inst->_index << " ";
              std::cerr << _vec[cur_idx%BSIZE]->_op->id() 
                        << " " << incep_cycle << " " << commit_cycle 
                        << " " << (cur_inst->isPipelineInst()?"pipe":"accel");
           }
           std::cerr << "\n";
        }

        std::cerr << "ERROR, no horizon instruction available! (cycle " 
                  << horizonCycle << ", highest_found:" << highest_hor_cand << ")\n";
        std::cerr << "orig horizon: " << _horizonInst->index() 
                  << ", latest:" <<_latestIdx 
                  << "(diff:" << _latestIdx - _horizonInst->index() << ")\n"
                  << "num iters: " << num_iters << "\n"
                  << "inst: " << _horizonInst->getDisasm() << "\n";
        horizon_warning_printed=true;
        //why do this?
        //_horizonInst=NULL;
        return;
      }

   }

   virtual T* getHorizon() {
     if(!_horizonInst) {
       return NULL; 
     } 

     uint64_t horizonIndex = _horizonInst->index();

     if(queryInst(horizonIndex) != _horizonInst) {
       std::cout << "horizon idx: " << _horizonInst->index() << "\n";
       std::cout << "op:" << _horizonInst->_op->id() << "\n";
     }

     assert(queryInst(horizonIndex) == _horizonInst); 
     //make sure we still have the instruction


     T* event = &(*_horizonInst)[_horizonInst->eventInception()];

     //if(_horizonIndex!=0 and event->cycle() != _horizonCycle) {
     //  assert(0);

     //  //adjustHorizon(--_horizonIndex);
     //  //hor_inst = _vec[_horizonIndex%BSIZE];
     //  //event = &(*hor_inst)[hor_inst->eventInception()];
     //}
     return event;
  }

  virtual CumWeights* cumWeights() {return &_cum_weights;}

  //detect if there is a cycle in the depedencegraph
  bool detectCycle(T* n, std::unordered_set<T*>& seen, 
                       std::unordered_set<T*>& temp, T*& cycle_start) {
    if(temp.count(n)) {
      std::cout << "cycle edges: \n";
      cycle_start=n;
      return true;
    }

    if(seen.count(n)==0) { //not seen n yet
      temp.insert(n);
      for(auto i=n->pebegin(),e=n->peend();i!=e;++i) {
        E* edge = *i;
        T* src = edge->src();
        bool is_cycling = detectCycle(src,seen,temp,cycle_start);
        if(cycle_start) {
          if(is_cycling) {
            if(edge->src()->_op) {
              std::cout << edge->src()->_op->func()->nice_name() << "\t";
              std::cout << std::setw(8) << edge->src()->_op->getUOPName();
            }

            std::cout << std::setw(5) << edge_name[edge->type()] 
                      << " index:" << std::setw(11) << edge->src()->_index
                      << " n_index:" << std::setw(11) << edge->src()->_n_index;


            if(edge->src()->_inst) {
              std::cout << "inst_ptr: " << std::setw(5) << &*edge->src()->_inst ;
            }
             
            if(edge->src()->_op) {
              std::cout << "\top:" << edge->src()->_op->id();
            }
            std::cout << "\n";

            if(n==cycle_start) {
              std::cout << "\n";
              return false;
            } else {
              return true;
            } 
          } else {
            return false;
          }
        }
      }
      //no cycle in children
      seen.insert(n);
      temp.erase(n);
    }
    return false;
  }

  virtual void critpath(T* first_node) {
    std::unordered_map<T*,float> weightOf;
    std::unordered_set<T*> node_done;

    std::unordered_set<T*> seen;
    std::unordered_set<T*> temp;

    T* cycle_start=NULL;
    bool cycling = detectCycle(first_node,seen,temp,cycle_start); //ERROR CHECKING, TODO: Comment
    if(cycling) {
      std::cout << "CYCLEING!\n";
    }
    if(cycle_start) {
      std::cout << "had a cycle!\n";
    }

    std::map<uint64_t,std::set<T*>> worklist;
    worklist[first_node->cycle()].insert(first_node);
    weightOf[first_node]=1;

    //bool warning = false;
    uint64_t highest = ((uint64_t)-1);

    while(!worklist.empty()) {
      //check weights
      auto workiter = worklist.rbegin();
      std::set<T*>& nodelist = workiter->second;
      
      if(workiter->first > highest) {
        std::cout << "ERROR: item" << workiter->first << "higher than" << highest << "\n";
      }
      highest=workiter->first;

      std::set<T*> done_list;
      while(!nodelist.empty()) {
        /* //For debugging total weight
        double total_weight=0;
        for(auto& i : worklist) {
          for(auto& n : i.second) {
            total_weight+=weightOf[n];
          }
        }
        if(total_weight < 0.999999 && !warning) {
          warning=true;
          std::cout << "WHAT: " << total_weight << "\n";
        }*/

        T* node = *(nodelist.begin());
        nodelist.erase(node);

        assert(weightOf.count(node));

        float weight=weightOf[node];

        std::set<T*> mini_worklist;
        for(auto i=node->pebegin(),e=node->peend();i!=e;++i) {
          E* edge = *i;
          if(node->isLastArrivingEdge(edge)) {
            T* src_node = edge->src();
            //if(node_done.count(src_node)) {
            //  std::cout << "Seen node with edge: " << edge_name[edge->type()] << "!\n";
            //}
            _cum_weights.countEdge(edge->type(),edge->len(),weight);
            if(node->_cum_weights) {
              node->_cum_weights->countEdge(edge->type(),edge->len(),weight);
            }
           
            if(_crit_edge_listener) {
              _crit_edge_listener->listen(edge,weight);
            }
            
            mini_worklist.insert(src_node);
          }
        }

        /*
        //For debugging last item
        if(mini_worklist.size()==0) {
          std::cout << "last" << node->cycle() << "\n";
        }*/

        float new_weight = weight / mini_worklist.size();
        
        for(auto i=mini_worklist.begin(),e=mini_worklist.end();i!=e;++i) {
          T* new_node = *i;

          if(!new_node->_crit_path_counted) {
            if(weightOf.count(new_node)){
              weightOf[new_node]+=new_weight;
            } else {
              weightOf[new_node]=new_weight;
              worklist[new_node->cycle()].insert(new_node);
            }
          }
        }
        
        //node_done.insert(node);
        done_list.insert(node);
        weightOf.erase(node);
      }
      for(auto& i : done_list) {
        i->_crit_path_counted=true;
      }
      worklist.erase(workiter->first);
    }
    assert(weightOf.size()==0);
  }


  T* backtrack_from(T* node, std::unordered_set<T*>& seen, 
                       std::unordered_set<T*>& temp, uint64_t stop,
                       T*& b_cycle_start, bool& is_cycling) {
    if(temp.count(node)) {
      b_cycle_start=node;
      is_cycling=true;
      return NULL;
    }

    if(node->_crit_path_opened) { //found one
      return node;
    }

    if(seen.count(node)==0) { //not seen n yet
      temp.insert(node);

      for(auto i=node->pebegin(),e=node->peend();i!=e;++i) {
        E* edge = *i;
        T* src_node = edge->src();
        if(node->isLastArrivingEdge(edge)) {
          T* new_node = backtrack_from(src_node,seen,temp,stop,b_cycle_start,is_cycling);

          if(is_cycling) {
            std::cout << "cycle edge: " << edge_name[edge->type()] << " "
                      << (uint64_t)src_node << "\n";
          }
          if(node==b_cycle_start) {
            is_cycling=false;
            std::cout << "\n";
          }
          if(b_cycle_start) {
            return NULL;
          } 

          if(new_node) {
            return new_node;
          }
        }
      }
      //no cycle in children
      seen.insert(node);
      temp.erase(node);
    }
    return NULL;
  }

  virtual void analyzeGraph(uint64_t start, uint64_t stop, uint64_t latest) {
    //First go forward from start to stop
    uint64_t start_ind = start%BSIZE;
    uint64_t reserve_ind = (stop-RSIZE)%BSIZE;
    uint64_t stop_ind = stop%BSIZE; 
    uint64_t latest_ind = latest%BSIZE;

    T* node = NULL;
    
    //Two ways to get start node:
    //1. Backtrack from latest idx to before stop_ind
    //2. Find last instruction between start and stop
    // Do 1 if vec[latest_ind] has instruction
    if(_vec[latest_ind] && _vec[latest_ind]->index() == latest) {
      for(uint64_t i = reserve_ind; i!=stop_ind; ++i,i=(i==BSIZE?0:i)) {
        if(!_vec[i]) {
          continue;
        }
        _vec[i]->open_for_backtrack();
      }

      T* cur_node = & (*_vec[latest_ind])[_vec[latest_ind]->eventComplete()];
      //std::cout << "latest" << cur_node->cycle() << "\n";

      T* cycle_start=NULL;
      std::unordered_set<T*> seen;
      std::unordered_set<T*> temp;
      bool is_cycling=false;
      node = backtrack_from(cur_node,seen,temp,stop_ind,cycle_start,is_cycling);
    }
     
    //Fall back on option 2 if it didn't work 
    if(!node) {
//      std::cout << "backtracking didn't work so well\n";

      for(uint64_t i = start_ind; i!=stop_ind; ++i,i=(i==BSIZE?0:i)) {
        dg_inst_base_ptr inst = _vec[i];
       
        if(inst) { 
          T* cur_node = & (*inst)[inst->eventComplete()];
          if(node==NULL || cur_node->cycle() > node->cycle()) {
            node=cur_node;
          }
        }
      }
    }

    assert(node);
    critpath(node);

    /*std::cout << "Range: " << start << "->" << stop << ", cycle: " << _vec[start_ind]->cycleOfStage(_vec[start_ind]->eventComplete()) << "->" << node->cycle() <<"; total_weight: " << total_weight <<"\n";
    */

//    std::cout << "clearing: " << start << " " << stop-RSIZE << " " << latest << "\n";
//    std::cout << "start_ind: " << start_ind << " " << reserve_ind << "\n";

    int j=0;
    for(uint64_t i = start_ind; i!=reserve_ind; ++i,i=(i==BSIZE?0:i),++j) {
/*      std::cout << "clear:" << i;
      if(_vec[i]) {
        std::cout << " " << _vec[i]->_index << "\n";
      } else {
        std::cout << " NULL VEC\n";
      }
*/
      _vec[i]=0;      
    }
//    std::cout << "elemennts deleted: " << j << "\n";
  }

  void check_update_horizon(std::shared_ptr<dg_inst_base<T,E>> dg, uint64_t index) {
    if(_latestIdx==0 && index > 1000) {
      _latestCleanIdx=index-1;
    }
//    bool debug_cond=index>790000;
//
//    if(debug_cond) {
//      std::cerr << "add to index: " << index << "   cycle: " 
//                << dg->cycleOfStage(dg->eventInception()) << "\n";
//    }

    //Not forcing this anymore
    //assert(index <= _latestIdx+1 && index + BSIZE >= _latestIdx);
    assert(index + BSIZE >= _latestIdx);
    //assert(index == dg->_index);

    //int vec_ind = index%BSIZE;
    //if (_vec[vec_ind].get() != 0) {
    //  remove_instr(_vec[vec_ind].get());
    //}
    if(!horizon_failed) {
      if(_horizonInst==NULL) {
        _horizonInst=dg;
      }

      if((index>=HSIZE && index > _latestIdx) ) {
        updateHorizon(index-HSIZE);


//        if(debug_cond) {
//          if(_horizonInst) {
//            uint64_t horizonCycle = _horizonInst->cycleOfStage(_horizonInst->eventInception());
//            uint64_t horizonIndex = _horizonInst->_index;
//            std::cerr << "horizon = c:" << horizonCycle << ", i:" << horizonIndex << " " << _latestIdx - horizonIndex << "\n";
//          } else {
//            std::cerr << "no horizon\n";
//          }
//        }
        //assert(_horizonInst->index() > (index-HSIZE));  -- we are allowing the horizon to get pushed out

        if(index>=_latestCleanIdx+CSIZE+RSIZE+HSIZE) {
           analyzeGraph(_latestCleanIdx, _latestCleanIdx+CSIZE+RSIZE,_latestIdx);
          _latestCleanIdx=_latestCleanIdx+CSIZE;
        }
        //I'm going to stomp on a node, time to analyze the graph so far
        //if(_vec[vec_ind]) {
        //  analyzeGraph(_vec[vec_ind]->index(), index-HSIZE, _latestIdx);
        //}
      }
    }

    if(index>_latestIdx) {
      _latestIdx=index;
    }
  }

  //If the vector values are the same, just push back
  virtual void addInstChecked(std::shared_ptr<dg_inst_base<T,E>> dg, uint64_t index) {
     assert(index!=0);

     check_update_horizon(dg,index);

     int vec_ind = index%BSIZE;
     if(_vec[vec_ind] && _vec[vec_ind]->index()==index) {
       if(_vec[vec_ind] == dg || _vec[vec_ind]->savedIn(dg)) { 
          //don't add twice -- do nothing
          return;
       } else {
          dg->saveInst(_vec[vec_ind]); //save old instruction
       }

     } 
     _vec[vec_ind]=dg;
  }


  virtual void addInst(std::shared_ptr<dg_inst_base<T,E>> dg, uint64_t index) {
//    std::cout << "index ---- " << index << " " << "type:" << (int)dg->_type << "  ptr:" << &*dg << "dg->_index: " << dg->_index << "\n";
    check_update_horizon(dg,index);

    int vec_ind = index%BSIZE;
    _vec[vec_ind]=dg;
  }

  virtual void remove_instr(dg_inst_base<T, E>* ptr) { }

  virtual uint64_t getPipeLoc() const { return _pipeLoc; }

  virtual std::shared_ptr<Inst_t> peekPipe_sh(int offset) {  
    if(_pipeLoc+offset > (uint64_t)-1000) {
      return NULL;  //base case -- no instructions yet
    }
    
    return _pipe[(_pipeLoc+offset)%PSIZE];
  }


  virtual Inst_t* peekPipe(int offset) {  
    if(_pipeLoc+offset > (uint64_t)-1000) {
      return NULL;  //base case -- no instructions yet
    }
    
    return _pipe[(_pipeLoc+offset)%PSIZE].get();
  }

  virtual void pushPipe(std::shared_ptr<Inst_t> dg) {
    _pipe[_pipeLoc%PSIZE]=dg;
    _pipeLoc+=1;
  }

  virtual void done(std::shared_ptr<Inst_t> dg) {
    //call done on each event   
  }



  //typedef typename std::vector<dg_inst<T,E> > KernelMarkerList;
  //KernelMarkerList  _kernel_markers;
  //typedef typename KernelMarkerList::iterator KernelMarkerListIterator;
  uint64_t maximum_cycle_seen=0;
  void updateMax(E* edge) {
    uint64_t newest_cycle=edge->dest()->cycle();
    if(newest_cycle > maximum_cycle_seen) {
      maximum_cycle_seen=newest_cycle;
    }
  }

public:
  E* insert_edge(T& event1, T& event2,
                     int len, 
                     unsigned etype=E_NONE) {
    E* edge = new E(&event1,&event2, len, etype);
    event1.add_edge(edge);
    updateMax(edge);
    return edge;
  }


  E* insert_edge(T& event,
                     dg_inst_base<T,E>& destnodes,
                     const unsigned destTy,
                     int len, 
                     unsigned etype=E_NONE) {
    E* edge = new E(&event,&destnodes[destTy], len, etype);
    event.add_edge(edge);
    updateMax(edge);
    return edge;
  }

  E* insert_edge(dg_inst_base<T,E>& srcnodes,
                     const unsigned srcTy,
                     T& event,
                     int len, 
                     unsigned etype=E_NONE) {
    E* edge = new E(&srcnodes[srcTy],&event, len, etype);
    srcnodes[srcTy].add_edge(edge);
    updateMax(edge);
    return edge;
  }

  E* insert_edge(dg_inst_base<T,E>& srcnodes,
                     const unsigned srcTy,
                     dg_inst_base<T,E>& destnodes,
                     const unsigned destTy,
                     int len, 
                     unsigned etype=E_NONE) {
    E* edge = new E(&srcnodes[srcTy],&destnodes[destTy], len, etype);
    srcnodes[srcTy].add_edge(edge);
    updateMax(edge);
    return edge;
  }

  E* insert_edge(uint64_t srcIdx,
                     const unsigned srcTy,
                     dg_inst_base<T,E>& destnodes,
                     const unsigned destTy,
                     int len, 
                     unsigned etype=E_NONE) {
    if(srcIdx > (uint64_t)-1000) {
      return NULL;  //base case -- no instructions yet
    }
    /*if (srcIdx < _lastIdx) {
      //warning here?
      return;
    }*/
    Inst_t &srcnodes = static_cast<Inst_t&>(queryNodes(srcIdx));
    //Inst_t &destnodes = queryNodes(destIdx);
    E* edge = new E(&srcnodes[srcTy],&destnodes[destTy], len, etype);
    srcnodes[srcTy].add_edge(edge);
    updateMax(edge);
    return edge;
  }

/*
  void insert_ff_edge(uint64_t srcIdx,
                      uint64_t destIdx, unsigned len) {
    dg_inst<T,E> &srcnodes = getNodes(srcIdx);
    dg_inst<T,E> &destnodes = getNodes(destIdx);

    srcnodes[0]->add_ff_edge(new E(destnodes[0], len));
  }
*/

  uint64_t numCycles;


  uint64_t getMaxCycles() {
    #if 0
      if (!_latestIdx)
        return -1;
 
      dg_inst_base<T,E> &inst = queryNodes(_latestIdx);
      return inst.cycleOfStage(inst.eventComplete());//inst.finalCycle();
    #else
      return maximum_cycle_seen;
    #endif
  }

  void cleanup() {
    //remove_nodes_until((uint64_t)-1);
  }

  void commitNode(uint64_t index) {}
  void finish() {
    //remove_nodes_until(index -1);
  }
protected:
/*
  virtual void process_nodes_until(uint64_t max_index) {
    remove_nodes_until(max_index);
  }
  void remove_nodes_until(uint64_t max_index) {
    //for (uint64_t index = _lastIdx; index <= max_index; ++index) {
    //  removeNodes(index);
    //}
    
    IndexMapIterator II,EE;
    for(II=_index2inst.begin(),EE=_index2inst.end();II!=EE;) {
      Inst_t& inst = II->second;
      if(inst.index()<=max_index) {
        inst.remove();
        II = _index2inst.erase(II);
      } else {
        ++II;
        //break
      }
    }
    _lastIdx = max_index;
  }
*/

};

#endif
