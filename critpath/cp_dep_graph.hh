
#ifndef CP_DEP_GRAPH_HH
#define CP_DEP_GRAPH_HH

#include <map>
#include <unordered_map>

#include <vector>

#include "cpnode.hh"
#include "op.hh"

#include <iostream>
#include <stdio.h>
#include <memory>
#include <set> 
#include <deque>
#include <unordered_set>

// Terminology:
// "event" refers to individual instruction "stages" like fetch/decode/etc..
// "edge" is a dependency between events
// "inst" is simply a group of events

//
#define EDGE_TABLE                                      \
  X(DEF,  "DEF", "Default Type")                        \
  X(NONE, "NONE","Type Not Specified")                  \
  X(FF,   "FF",  "Fetch to Fetch")                      \
  X(IC,   "IC",  "ICache Miss")                         \
  X(FBW,  "FBW", "Fetch Bandwidth")                     \
  X(FPip, "FPip","Frontend Pipe BW")                    \
  X(LQTF, "LQTF","Load/Store Queue To Fetch")           \
  X(BP,   "BP",  "Branch Predict (in-order)")           \
  X(CM,   "CM",  "Control Miss (OoO)")                  \
  X(IQ,   "IQ",  "Instruction Queue Full")              \
  X(LSQ,  "LSQ", "LoadStore Queue Full")                \
  X(FD,   "FD",  "Fetch to Dispatch")                   \
  X(DBW,  "DBW", "Dispatch Bandwidth")                  \
  X(DD,   "DD",  "Dispatch Inorder")                    \
  X(ROB,  "ROB", "ROB Full")                            \
  X(DR,   "DR",  "Dispatch to Ready")                   \
  X(NSpc, "NSpc","non-speculative inst")                \
  X(RDep, "RDep","Register Dependence")                 \
  X(MDep, "MDep","Memory Dependnece")                   \
  X(EPip, "EPip","Execution Pipeline BW")               \
  X(EE,   "EE",  "Execute to Execute")                  \
  X(IBW,  "IBW", "Issue Width")                         \
  X(RE,   "RE",  "Ready to Execute")                    \
  X(FU,   "FU",  "Functional Unit Hazard")              \
  X(EP,   "EP",  "Execute to Complete")                 \
  X(WBBW, "WBBW","WriteBack BandWidth")                 \
  X(MSHR, "MSHR","MSHR Resource")                       \
  X(WB,   "WB",  "Writeback")                           \
  X(PP,   "PP",  "Cache Dep (w/e)")                     \
  X(PC,   "PC",  "Complete To Commit")                  \
  X(SQUA, "SQUA","Squash Penalty")                      \
  X(CC,   "CC",  "Commit to Commit")                    \
  X(CBW,  "CBW", "Commit B/W")                          \
  X(SER,  "SER", "serialize this instruction")          \
  X(CXFR, "CXFR","CCores Control Transfer")             \
  X(CSBB, "CSBB","CCores Seralize BB Activation")       \
  X(BBA,  "BBA", "CCores Activate Basic Block")         \
  X(BBE,  "BBR", "CCores Basic Block Ready")            \
  X(BBC,  "BBC", "CCores Basic Block Complete")         \
  X(SEBB, "SEBB", "SEB Region Begin")                   \
  X(SEBA, "SEBA", "SEB Activate")                       \
  X(SEBW, "SEBW", "SEB Writeback")                      \
  X(SEBS, "SEBS", "SEB Serialization")                  \
  X(SEBL, "SEBL", "SEB Execute to Complete Latency")    \
  X(SEB,  "SEB", "???")                                 \
  X(SEBD, "SEBD", "Intra-SEB Data Dependence")                \
  X(SEBX, "SEBX", "Inter-SEB Data Dependence")                \
  X(BREP, "BREP", "Beret Replay")                               \
  X(BXFR, "BXFR", "Beret Data Transfer")                        \
  X(CHT,  "CHT",  "cheat edge for super insts")                 \
  X(DyDep, "DyDep", "DySER Dependence")                         \
  X(DyRR,  "DyRR", "DySER Functional unit ready queue")         \
  X(DyRE,  "DyRE",  "DySER Ready to Execute")                   \
  X(DyFU,  "DyFU", "DySER Functional unit pipelined")           \
  X(DyEP,  "DyEP", "DySER Functional Execute to complete")      \
  X(DyPP,  "DyPP", "DySER Complete To Complete")                \
  X(DyCR,  "DyCR", "DySER Commit to ready")                     \
  X(NPUPR, "NPUCR", "NPU Complete to ready")                    \
  X(NPUFE, "NPUFE", "NPU Fake Edges")                    \
  X(HORZ,  "HORZ",  "The HORIZON")                    \
  X(NUM,   "NUM",  "LAST (should not see)")                    \



#define X(a, b, c) E_ ## a,
enum EDGE_TYPE {
  EDGE_TABLE
};
#undef X

#include "edge_table.hh"

/*
static const char* ename(uint8_t ind) {
  return edge_name[ind];
}
*/

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
  uint64_t _index;
  bool done = false;
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
  uint64_t _eff_addr;
  uint8_t _hit_level = 0, _miss_level = 0;

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


public:
  virtual uint64_t finalCycle() {
    uint64_t max=0;
    for(int i = 0; i < numStages(); ++i) {
      if(max<cycleOfStage(i)) {
        max=cycleOfStage(i);
      }
    }
    return max;
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
  void addOperandInst(std::shared_ptr< dg_inst_base<T, E> > op) {
    _operands.push_back(op);
  }
  void addMemOperandInst(std::shared_ptr< dg_inst_base<T, E> > op) {
    _mem_operands.push_back(op);
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
  std::set<std::shared_ptr<dg_inst_base<T,E>>> _savedInsts;

public:
  bool iqOpen=false;
  bool lqOpen=false;
  bool sqOpen=false;
  virtual ~dg_inst() { 
    /*for (int i = 0; i < NumStages; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  virtual void saveInst(std::shared_ptr<dg_inst_base<T,E>> inst) {
    _savedInsts.insert(inst);
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
      assert(0 && "no mem access allowed");
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
    this->_ex_lat=img._cc-img._ec;
    this->_serialBefore=img._serialBefore;
    this->_serialAfter=img._serialAfter;
    this->_nonSpec=img._nonSpec;
    this->_st_lat=img._xc-img._wc;
    this->_floating=img._floating;
    this->_iscall=img._iscall;
    this->_eff_addr=img._eff_addr;
    this->_hit_level=img._hit_level;
    this->_miss_level=img._miss_level;

    _icache_lat=img._icache_lat;
    _numSrcRegs=img._numSrcRegs;
    _numFPSrcRegs=img._regfile_fread;
    _numIntSrcRegs=img._regfile_read;
    _numFPDestRegs=img._numFPDestRegs;
    _numIntDestRegs=img._numIntDestRegs;
    _pc=img._pc;
    _upc=img._upc;
    this->_op=op;
   /* 
    for (int i = 0; i < NumStages; ++i) {
      events[i].set_inst(this);
      if (img._isload)
        events[i].setLoad();
      if (img._isstore)
        events[i].setStore();
      if (img._isctrl)
        events[i].setCtrl();
      events[i].prop_changed();
    }
    */
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

/*  void compute_cycle() {
    for (int i = 0; i < 6; ++i) {
      events[i]->compute_cycle_for_dep();
    }
  }

  void compute_cycle_data_dep() {
    for (int i = 0; i < 6; ++i) {
      events[i]->compute_cycle_for_data_dep();
    }
  }*/

/*  void remove() {
    remove_edges();
  }*/

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

  uint64_t _index;
  uint64_t _cycle;
  //int _ty;
  //EPtr ff_edge;
  //bool _load, _store, _ctrl;
  bool _dirty=false;
  bool _crit_path_counted=false;
  bool _crit_path_opened=false;


  dg_inst<NodeTy, E> *_inst;

  std::vector<EPtr> _edges; //forward edges
  //std::map<dg_event_base*, EPtr> _pred_edge_map;
  std::vector<EPtr> _pred_edges; //predecessor edges

  dg_event_impl_t():// _index(0), 
    _cycle(0),
    //_ty(0), //ff_edge(0),
    //                  _load(false), _store(0), _ctrl(false),
                      _inst(0) 
  {
  }
  dg_event_impl_t(uint64_t index, int ty): 
                   //_index(index), 
                    _cycle(0),
                    //_ty(ty), //ff_edge(0),
    //             _load(false), _store(0), _ctrl(false), 
                   _inst(0)
  {
  }

  virtual int numPredEdges() {
    return _pred_edges.size();
  }

/*  virtual uint64_t index() {
    return _index;
  }*/

  virtual ~dg_event_impl_t() {
    //std::cout << "deleting event" << _index << "\n";
    remove_all_edges();
    //_edges.clear();
  }

  virtual void set_inst(dg_inst<NodeTy, E> *n){
    _inst = n;
  }

  virtual bool isLastArrivingEdge(EPtr edge) {
    return edge->src()->cycle()+edge->len() == cycle();
  }

  //return an edge that explains the arrival time
  //bias given to earlier edges in EDGE_TYPE enum
  virtual EPtr lastArrivingEdge() {
    for (auto I = _pred_edges.begin(), e=_pred_edges.end(); I!=e; ++I) {
      if ((*I)->src()->cycle()+(*I)->len() == cycle()) {
        return *I;
      }
    }
    return 0;
  }

  virtual unsigned lastArrivingEdgeType() {
    EPtr edge = lastArrivingEdge();
    if(edge==0) {
      return E_NONE;
    } else {
      return edge->type();
    }
  }

  virtual void reCalculate() {
    _cycle=0;
    for (auto i = _pred_edges.begin(), e = _pred_edges.end(); i!=e; ++i) {
      compute_cycle(*i); 
    }
  }

  virtual void prop_changed(){}

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

  virtual void add_edge(EPtr e) {
    _edges.push_back(e);
    e->dest()->add_pred_edge(e);
    compute_cycle(e);
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

  virtual void add_pred_edge(EPtr e) {
    //dg_event_base *tmp = e->dest();
    _pred_edges.push_back(e);
  }

  virtual void remove_pred_edge(EPtr rem_e) {
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

  virtual void remove_edge(EPtr rem_e) {
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

  virtual void remove_all_edges() {
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

  virtual void print(const CP_NodeDiskImage &img) {
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

  dg_event(uint64_t index,
            int ty): dg_event_impl_t< dg_edge_impl_t<dg_event> >(index, ty)
  {
  }

/*
  void create_pred_edge(EPtr e)
  {
    EPtr pred_edge = new dg_edge_impl_t<dg_event>(this, e->length(),
                                                     e->type());
    e->dest()->add_pred_edge(pred_edge);
  }
*/

};

enum AnalysisType {
  Weighted=0,
  Unweighted=1,
  Offset=2
};

// The abstract type for the entire graph
template<typename Inst_t, typename T, typename E>
class dep_graph_t {
public:
  virtual ~dep_graph_t() {};
  virtual void addInst(std::shared_ptr<dg_inst_base<T,E>> dg,uint64_t index) = 0;

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

  virtual double weightOfEdge(int i, AnalysisType analType) =0;

  virtual std::shared_ptr< dg_inst_base<T, E> > queryInst(uint64_t idx) = 0;
  virtual uint64_t getMaxCycles() =0;
  //virtual void printInstEvents(uint64_t index, std::ostream& out) = 0;

  virtual uint64_t getPipeLoc() const = 0;
  virtual Inst_t* peekPipe(int offset) = 0;
  virtual void pushPipe(std::shared_ptr<Inst_t> dg) = 0;
  virtual void done(std::shared_ptr<Inst_t> dg) = 0;

  virtual void no_horizon() = 0;

  virtual void commitNode(uint64_t index)  = 0;
  virtual void finish(uint64_t index) = 0;
  virtual void cleanup() = 0;
};


// Implementation for the entire graph
#define BSIZE 16384 //max number of insts to keep for data/mem dependence
#define HSIZE  6356
#define PSIZE   512 //max number in-flight instructions, biggest ROB size

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
  uint64_t _latestIdx;

  double edges_weighted[E_NUM]={0}; //acumulated edge weights
  double edges_unweighted[E_NUM]={0}; //acumulated edge weights
  double edges_offset[E_NUM]={0}; //acumulated edge weights


  // The "horizon" is the point beyond which no event may occur before, because
  // it is in the definite past.  This corresponds to a cycle number, and also
  // a node with which to attach dependencies to ensure that the horizon is not
  // crossed.
  //uint64_t _horizonCycle=0;
  //uint64_t _horizonIndex=((uint64_t)-1);
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
        assert(queryInst(_horizonInst->index()) == _horizonInst); 
      }

      uint64_t highest_hor_cand=0; //these two for debugging
      int num_iters=0;

      //iterate until we find a new horizon with appropriate inception time
      for(uint64_t horizonIndex = min_index; 
         horizonIndex < min_index + BSIZE; ++horizonIndex) {
         num_iters++; 
         dg_inst_base_ptr new_hor_inst = _vec[horizonIndex%BSIZE];
         if(!new_hor_inst || new_hor_inst->isDummy()) {
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
           return;
         }
      }
      //Horizon Failed! -- Investigate!
      if(horizon_warning_printed==false) {
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

  virtual double weightOfEdge(int i, AnalysisType analType) {
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

  void countEdge(int i,int time,float weight) {
    edges_weighted[i]+=time*weight;
    edges_unweighted[i]+=weight; //time is the weight
    edges_offset[i]+=(time+1)*weight; //some compromise to show 0-cycle edges
  }

  T* detectCycle(T* n, std::unordered_set<T*>& seen, 
                       std::unordered_set<T*>& temp) {
    if(temp.count(n)) {
      return n;
    }

    if(seen.count(n)==0) { //not seen n yet
      temp.insert(n);
      for(auto i=n->pebegin(),e=n->peend();i!=e;++i) {
        E* edge = *i;
        T* src = edge->src();
        T* cycle_node = detectCycle(src,seen,temp);
        if(cycle_node) {
          std::cout << "cycle edge: " << edge_name[edge->type()] << "\n";
          if(src == cycle_node) {
            return NULL;
          } else {
            return cycle_node;
          }
        }
      }
      //no cycle in children
      seen.insert(n);
      temp.erase(n);
    }
    return NULL;
  }

  virtual void critpath(T* first_node) {
    std::unordered_map<T*,float> weightOf;
    std::unordered_set<T*> node_done;

    std::unordered_set<T*> seen;
    std::unordered_set<T*> temp;
    detectCycle(first_node,seen,temp); //ERROR CHECKING, TODO: Uncomment

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
            countEdge(edge->type(),edge->len(),weight);
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

#define RSIZE 1024

  T* backtrack_from(T* node, uint64_t stop) {
    if(node->_crit_path_opened) { //found one
      return node;
    }

    for(auto i=node->pebegin(),e=node->peend();i!=e;++i) {
      E* edge = *i;
      T* src_node = edge->src();
      if(node->isLastArrivingEdge(edge)) {
        T* new_node = backtrack_from(src_node,stop);
        if(new_node) {
          return new_node;
        }
      }
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
     for(uint64_t i = reserve_ind; i!=stop_ind; ++i) {
        if(i==BSIZE) {
          i=0;
        }
        if(!_vec[i]) {
          continue;
        }
        for(int j = 0; j < _vec[i]->numStages(); ++j) {
          (*_vec[i])[j]._crit_path_opened=true;
        }
      }

      T* cur_node = & (*_vec[latest_ind])[_vec[latest_ind]->eventComplete()];
      node = backtrack_from(cur_node,stop_ind);
    }
     
    //Fall back on option 2 if it didn't work 
    if(!node) {
      for(uint64_t i = start_ind; i!=stop_ind; ++i) {
        if(i==BSIZE) {
          i=0;
        }
  
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

    //debug:
    double total_weight=0; //should roughly equal number of cycles in program
    for(int i = 0; i < E_NUM; ++i) {
      double weight =weightOfEdge(i,Weighted);
      total_weight+=weight;
    }
    std::cout << "Range: " << start << "->" << stop << ", cycle: " << _vec[start_ind]->cycleOfStage(_vec[start_ind]->eventComplete()) << "->" << node->cycle() <<"; total_weight: " << total_weight <<"\n";



    for(uint64_t i = start_ind; i!=reserve_ind; ++i) {
      if(i==BSIZE) {
        i=0;
      }

      _vec[i]=0;      
    }
  }
 
  virtual void addInst(std::shared_ptr<dg_inst_base<T,E>> dg,
                       uint64_t index) {
    //Not forcing this anymore
    //assert(index <= _latestIdx+1 && index + BSIZE >= _latestIdx);
    assert(index + BSIZE >= _latestIdx);
    //assert(index == dg->_index);

    int vec_ind = index%BSIZE;
    //if (_vec[vec_ind].get() != 0) {
    //  remove_instr(_vec[vec_ind].get());
    //}
    if(!horizon_failed) {
      if(_horizonInst==NULL) {
        _horizonInst=dg;
      }

      if((index>=HSIZE && index > _latestIdx) ) {
        updateHorizon(index-HSIZE);
        //std::cout << "horizon = " << _horizonCycle << "\n";
        //assert(_horizonInst->index() > (index-HSIZE));  -- we are allowing the horizon to get pushed out

        //I'm going to stomp on a node, time to analyze the graph so far
        if(_vec[vec_ind]) {
          analyzeGraph(_vec[vec_ind]->index(), index-HSIZE, _latestIdx);
        }

      }
    }

    if(index>_latestIdx) {
      _latestIdx=index;
    }
    _vec[vec_ind]=dg;
  }

  virtual void remove_instr(dg_inst_base<T, E>* ptr) { }

  virtual uint64_t getPipeLoc() const { return _pipeLoc; }

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
  void finish(uint64_t index) {
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
