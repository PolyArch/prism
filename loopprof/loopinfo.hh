#ifndef LOOPINFO_HH
#define LOOPINFO_HH

#include <map>
#include <list>
#include <vector>
#include <sstream>

#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/tokenizer.hpp>
#include <fstream>
#include "bb.hh"

#include "cfu.hh"

class Subgraph {
public:
  typedef std::set<Op*> OpSet;
  typedef std::vector<Op*> OpVec;

private:
  OpSet _ops;
  OpVec _opVec; //no need to save in serializer -- recomputed
  static uint32_t _idCounter;
  uint32_t _id;
  CFU* _cfu=NULL;
  std::map<Op*, CFU_node*> _opMap;

public:
friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version){
    ar & _id;
    ar & _ops;
    ar & _cfu;
    ar & _opMap;
    ar & _cfu;
    ar & _opMap;
  }

  Subgraph():_id(_idCounter++){
    _ops.clear();
    _opVec.clear();
  }

  void insertOp(Op* op) {assert(op); _ops.insert(op);}
  void checkVec();
  bool hasOp(Op* op) {
    return _ops.count(op);
  }
  OpSet::iterator op_begin() {return _ops.begin();}
  OpSet::iterator op_end() {return _ops.end();}

  OpVec::iterator opv_begin() {checkVec(); return _opVec.begin();}
  OpVec::iterator opv_end() {return _opVec.end();}
  unsigned size() {return _ops.size();}

  uint32_t id() {return _id;}

  void setCFU(CFU* cfu) {_cfu=cfu;}
  CFU* cfu() {return _cfu;}

  void setCFUNode(Op* op, CFU_node* node) {
    _opMap[op]=node;
  }

  CFU_node* getCFUNode(Op* op) {
    if(_opMap.count(op) == 0) {
      return NULL;
    }
    return _opMap[op];
  }

};

class SGSched {
public:
  typedef std::set<Subgraph*> SubgraphSet;
  typedef std::vector<Subgraph*> SubgraphVec;

  SubgraphSet _subgraphSet;
  SubgraphVec _subgraphVec;
  std::map<Op*,Subgraph*> _opSubgraphMap;
  CFU_set* _cfu_set = NULL;
  std::set<Op*> _opset;

  std::vector<int> _cfu_util;
  int _total_util=0;

  void add_util(int i) {
    if(_cfu_set) {
      if(_cfu_util.size()==0) {
        _cfu_util.resize(_cfu_set->numCFUs()+2);
      }
      _cfu_util[i]++;
      _total_util++;
    }
  }
  void calc_set() {
    for(auto i = sg_begin(), e = sg_end(); i!=e; ++i) {
      Subgraph* sg = *i;
      if(sg->cfu()) {
        add_util(sg->cfu()->ind());
      }
    }
  }

public:
friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version){
    ar & _subgraphSet;
    ar & _subgraphVec;
    ar & _opSubgraphMap;
    ar & _opset;
    ar & _cfu_set;
    calc_set();
  }

  void insertSG(Subgraph* subgraph) {
    _subgraphSet.insert(subgraph);
    _subgraphVec.push_back(subgraph);
    if(subgraph->cfu()) {
      add_util(subgraph->cfu()->ind());
    }
  }

  int maxUtil()     {return _total_util;}
  int utilOf(int i) {return _cfu_util[i];}

  void reset() {
    _subgraphSet.clear();
    _subgraphVec.clear();
  }

  Subgraph* subgraphOfOp(Op* op) {
    return _opSubgraphMap[op];
  }

  static bool depFromTo(Subgraph* sg1, Subgraph* sg2) {
    for(auto i1 = sg1->opv_begin(), e1=sg1->opv_end(); i1!=e1;++i1) {
      Op* op1 = *i1; //an op in sg1
      
      for(auto ui=op1->u_begin(), ue=op1->u_end(); ui!=ue; ++ui) {
        Op* op2 = *ui;
        if(sg2->hasOp(op2)) {
          return true;
        }
      }
    }
    return false;
  }

  void setMapping(Op* op, CFU_node* cfu_node, Subgraph* sg) {
    sg->setCFUNode(op,cfu_node);
    _opSubgraphMap[op]=sg;
  }

  void insertOp(Op* op) {assert(op); _opset.insert(op);}
  bool opScheduled(Op* op) {return _opset.count(op);}
  std::set<Op*>& opSet() {return _opset;}

  void setCFUSet(CFU_set* cfu_set) {_cfu_set = cfu_set;}
  CFU_set* cfu_set() {return _cfu_set;}

  SubgraphVec::iterator sg_begin() {return _subgraphVec.begin();}
  SubgraphVec::iterator sg_end()   {return _subgraphVec.end();  }

  SubgraphSet::iterator sg_set_begin() {return _subgraphSet.begin();}
  SubgraphSet::iterator sg_set_end()   {return _subgraphSet.end();  }

  int numSubgraphs() {return _subgraphSet.size();}
  bool valid() {return _subgraphSet.size() > 0;}
 
  void checkValid() {
    std::set<Op*> check_set;

    for(auto i = sg_begin(), e = sg_end(); i!=e; ++i) {
      Subgraph* sg = *i;
      
      for(auto opi = sg->opv_begin(),ope=sg->opv_end();opi!=ope;++opi) {
        Op* op = *opi;
        if(check_set.count(op)) {
          assert(0 && "already scheduled");
        }
        check_set.insert(op);
      }
    }       
    assert(_opset.size() == check_set.size());
  }

  Subgraph* sgForOp(Op* op) {
    return _opSubgraphMap[op];
  } 
};





class LoopInfo {
public:
  typedef std::vector<BB*> BBvec;
  typedef std::set<BB*> BBset;
  typedef std::map<int,BBvec> PathMap;
  typedef std::map<int,int> IntMap;
  typedef std::map<std::pair<BB*,BB*>,int> EdgeInt;
  typedef std::set<LoopInfo*> LoopSet;

  typedef std::map<Op*,uint64_t> OpAddr;
  typedef std::map<Op*,int> OpInt;
  typedef std::map<Op*,uint32_t> OpUInt;
  typedef std::map<Op*,int> OpBool;

  typedef std::vector<int> LoopDep;
  typedef std::set<LoopDep> LoopDepSet;

  typedef std::set<Op*> OpSet;

  typedef std::map<std::pair<Op*,FunctionInfo*>,int> CallToMap;
  typedef std::set<FunctionInfo*> CallToSet;

  static uint32_t _idcounter;

private:
  uint32_t _id = 0;
  FunctionInfo* _funcInfo = NULL;
  BB* _head = NULL;
  BBset _loopLatches;
  BBset _loopBody;
  BBvec _rpo;
  PathMap _pathMap;
  int     _maxPaths = 0;
  IntMap _iterCount; //path iteration count
  int    _totalIterCount = 0; //total number of iters
  int    _loopCount = 0; //total number of loops
  int    _curIter = 0;

  int _ninputs = -1;
  int _noutputs = -1;

  uint64_t  _numInsts = 0; //total number of instructions executed in this loop
  uint64_t  _numCycles = 0; //total number of cycles (roughly) in this loop

  EdgeInt _edgeWeight; //weight of each transition to compute path index

  LoopSet _innerLoops, _outerLoops;
  LoopSet _immInnerLoops;
  LoopInfo* _immOuterLoop=NULL;

  OpAddr _prevOpAddr;
  OpInt  _prevOpIter;

  OpBool _opStriding;
  OpInt  _opStride;

  LoopDepSet _loopDepSet_wr; //set of dependent loop iterations, write->read
  LoopDepSet _loopDepSet_ww; //set of dependent loop iterations, write->write
  LoopDepSet _loopDepSet_rw; //set of dependent loop iterations, read->write
  //std::set<FunctionInfo*> _funcsCalled; //functions called, not yet used
  char _depth=-1;

  //Stuff for hot-path and beret
  OpSet _instsInPath;
  int _hotPathIndex=-1;

  //SubgraphSet _subgraphSet;
  //SubgraphVec _subgraphVec;

  //SubgraphSet _subgraphSetNLA;
  //SubgraphVec _subgraphVecNLA;
  //std::map<Op*,Subgraph*> _opSubgraphMapNLA;
  //CFU_set* _cfu_set_NLA;

  SGSched _sgSchedBeret;
  SGSched _sgSchedNLA;

  //which op did the call, how many times
  CallToMap _calledToMap;
  CallToSet _calledTo;

friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _id;
    //ar & _funcInfo;
    ar & _head;
    ar & _loopLatches;
    ar & _loopBody;
    ar & _rpo;
    ar & _pathMap;
    ar & _maxPaths;
    ar & _iterCount;
    ar & _totalIterCount;
    ar & _edgeWeight;
    ar & _loopCount;
    ar & _numInsts; 
    ar & _numCycles;
    ar & _innerLoops;
    ar & _outerLoops;
    ar & _immInnerLoops;
    ar & _immOuterLoop;
    ar & _opStriding;
    ar & _opStride;
    ar & _loopDepSet_wr;
    ar & _loopDepSet_ww;
    ar & _loopDepSet_rw;
    ar & _depth;
    ar & _instsInPath;
    ar & _hotPathIndex;
    ar & _sgSchedBeret;
    ar & _sgSchedNLA;
    ar & _calledToMap;
  }

  void serializeSubgraphs();
  bool setContainsCallReturn(OpSet& opSet) {
    std::set<Op*>::iterator i,e;
    for(i=opSet.begin(),e=opSet.end();i!=e;++i) {
      Op* op = *i;
      if(op->isCall()) {
        return true;
      }
    }
    return false;
  }

public:
  static bool staticForwardDep(Op* dop, Op* op) {
    if(dop->bb() == op->bb()) { //only count forward ops
      if(dop->bb_pos() >= op->bb_pos()) {
        return false;
      }
    } else { //dop->bb() != op->bb() -- count forward bbs
      if(dop->bb()->rpoNum() >= op->bb()->rpoNum()) {
        return false;
      }
    }
    return true;
  }

  bool forwardDep(Op* dop, Op* op) {
    return staticForwardDep(dop,op);
  }

  void calcInsOuts() {
    std::set<Op*> inputOps;
    std::set<Op*> outputOps;

    for(auto bbi=_loopBody.begin(),bbe=_loopBody.end();bbi!=bbe;++bbi) {  
      BB* bb = *bbi;
      for(auto i=bb->op_begin(),e=bb->op_end();i!=e;++i) {
        Op* op = *i;

        //check each dependence
        for(auto dsi=op->d_begin(),dse=op->d_end();dsi!=dse;++dsi) {
          Op* dep_op = *dsi;
          if(!this->inLoop(dep_op->bb())&&dep_op->func() == func()) {
            inputOps.insert(dep_op);
          }
        }

        //check each use
        for(auto usi=op->u_begin(),use=op->u_end();usi!=use;++usi) {
          Op* use_op = *usi;
          if(!this->inLoop(use_op->bb())&&use_op->func() == func()) {
            outputOps.insert(use_op);
          }
        }

      }
    }
    _ninputs=inputOps.size();
    _noutputs=outputOps.size();
  }

  int ninputs() {
    if(_ninputs==-1) {
      calcInsOuts();
    }
    return _ninputs;
  }

  int noutputs() {
    if(_noutputs==-1) {
      calcInsOuts();
    }
    return _noutputs;
  }

  bool containsCallReturn() {
    for(auto bbi=_loopBody.begin(),bbe=_loopBody.end();bbi!=bbe;++bbi) {  
      BB* bb = *bbi;
      for(auto i=bb->op_begin(),e=bb->op_end();i!=e;++i) {
        Op* op = *i;
        if(op->isCall()) {
           return true;
        }
      }
    }
    return false;
  }

  BB* loop_head() {return _head;}
  LoopInfo():_funcInfo(NULL), _immOuterLoop(NULL){} //For Serializer >: (

  LoopInfo(FunctionInfo* funcInfo, BB* head, BB* latch) : 
    _id(_idcounter++),_funcInfo(funcInfo), _head(head), _loopCount(0), _immOuterLoop(NULL) {
    _loopLatches.insert(latch);
  }

  std::string nice_name();
  std::string nice_name_full();
  void nice_name_tree(std::stringstream& ss);

  void setFuncInfo(FunctionInfo* f) {_funcInfo=f;}
  bool hasSubgraphs(bool NLA=false) {
    if(NLA) {
      return _sgSchedNLA.valid();
    } else {
      return _sgSchedBeret.valid();
    }
  }

  unsigned              body_size()   { return _loopBody.size(); }
  BBset::iterator       body_begin()  { return _loopBody.begin(); }
  BBset::iterator       body_end()    { return _loopBody.end(); }
  PathMap::iterator     paths_begin() { return _pathMap.begin(); }
  PathMap::iterator     paths_end()   { return _pathMap.end(); }
  LoopDepSet::iterator  ld_wr_begin()    { return _loopDepSet_wr.begin(); }
  LoopDepSet::iterator  ld_wr_end()      { return _loopDepSet_wr.end(); }
  LoopDepSet::iterator  ld_ww_begin()    { return _loopDepSet_ww.begin(); }
  LoopDepSet::iterator  ld_ww_end()      { return _loopDepSet_ww.end(); }
  LoopDepSet::iterator  ld_rw_begin()    { return _loopDepSet_rw.begin(); }
  LoopDepSet::iterator  ld_rw_end()      { return _loopDepSet_rw.end(); }
  BBvec::iterator       rpo_begin()   { return _rpo.begin(); }
  BBvec::iterator       rpo_end()     { return _rpo.end(); }

  BBvec::reverse_iterator rpo_rbegin()   { return _rpo.rbegin(); }
  BBvec::reverse_iterator rpo_rend()     { return _rpo.rend(); }

  SGSched& sgSchedNLA() {return _sgSchedNLA;}
  SGSched& sgSchedBeret() {return _sgSchedBeret;}

  bool isOuterLoop() {
    return _immOuterLoop == NULL;
  }
  bool isInnerLoop() {
    return _immInnerLoops.size() == 0;
  }
  int pathFreq(int i) {
    return _iterCount[i];
  }
  int getTotalIters() { return _totalIterCount;}

  uint32_t id() {return _id;}
  FunctionInfo* func() {return _funcInfo;}

  bool dependenceInPath(Op* dop, Op* op); 
  bool dependenceInPath(std::set<Op*>& relevantOps,Op* dop, Op* op);

  bool dataDepBT(std::set<Op*>& relevantOps,Op* dop, Op* op, std::set<Op*>& seenOps);
  bool dataDepBT(std::set<Op*>& relevantOps,Op* dop, Op* op);

  void compatUse(Op* uop, std::set<Op*>& ops,
                 std::set<std::pair<Op*,Op*>>& closeSet, 
                 Op* orig_op, Op* cur_op, CFU_node* cur_fu,
                 std::set<Op*>& doneOps, std::set<CFU_node*>& doneCFUs);

  void compatDep(Op* dop, std::set<Op*>& ops,
                 std::set<std::pair<Op*,Op*>>& closeSet, 
                 Op* orig_op, Op* cur_op, CFU_node* cur_fu,
                 std::set<Op*>& doneOps, std::set<CFU_node*>& doneCFUs);


  void checkCompatible(std::set<Op*>& ops,
                     std::set<std::pair<Op*,Op*>>& closeSet, 
                     Op* orig_op, 
                     Op* cur_op,
                     CFU_node* cur_fu,
                     std::set<Op*> doneOps,
                     std::set<CFU_node*> doneCFUs);
  void calcPossibleMappings(std::set<Op*>& ops, CFU_set* cfu_set,
                            std::set<std::pair<Op*,Op*>>& closeSet);

  

  void printSGPartText(std::ostream& out,
                              std::string resultfile, 
                              std::string fixes, CFU_set* cfu_set);

  void printGamsPartitionText(std::ostream& out,int count,
                              std::string resultfile, 
                              std::string fixes,int nMemDepOps,
                              int max_beret_ops, int max_mem_ops);


  bool printGamsPartitionProgram(std::string filename, CFU_set* cfu_set=NULL, 
                                 bool gams_details=false, bool no_gams=false, 
                                 int max_beret_ops=6, int max_mem_ops=2);

  bool printGamsPartitionProgram(std::string filename, 
    BBvec& bbVec, SGSched& sgSched,
    CFU_set* cfu_set=NULL, bool gams_details=false, bool no_gams=false,
    int max_beret_ops=6, int max_mem_ops=2);

  void printSubgraphDot(std::ostream& out, bool NLA=false) {
    if(NLA) {
      printSubgraphDot(out,_sgSchedNLA,true);
    } else {
      printSubgraphDot(out,_sgSchedBeret,false);
    }
  }


  bool scheduleNLA(CFU_set* cfu_set, bool gams_details, bool no_gams); 
  bool scheduleNLA(CFU_set* cfu_set, SGSched& sg, bool gams_details, bool no_gams);


  void printSubgraphDot(std::ostream& out, 
                        SGSched& subgraphSet, 
                        bool NLA);

  void calledTo(Op* op, FunctionInfo* fi) {
    _calledToMap[std::make_pair(op,fi)]++;
    _calledTo.insert(fi);
  }

  bool is_revolverable();

  // does this loop call any recursive function?
  bool cant_inline_first_time=true;
  bool cant_inline_saved_answer=false;
  bool cantFullyInline(bool redo=false) {
    if(cant_inline_first_time || redo) {
      cant_inline_first_time=false;
      cant_inline_saved_answer = callsRecursiveFunc();
    }
    return cant_inline_saved_answer;
  }
  bool callsRecursiveFunc(); 

  int getHotPathIndex();
  BBvec& getHotPath();
  float getLoopBackRatio(int i){
    return _iterCount[i] / ((float)_totalIterCount);
  }

  int instsOnPath(int i) {
    int sumInsts=0;
    for(auto I = _pathMap[i].begin(),E=_pathMap[i].end();I!=E;++I) {
      BB* bb = *I;
      sumInsts+=bb->len();
    }
    return sumInsts;
  }

  bool isParentOf(LoopInfo* li) {
    return _immInnerLoops.count(li);
  }

  LoopSet::iterator iloop_begin() {return _immInnerLoops.begin();} 
  LoopSet::iterator iloop_end() {return _immInnerLoops.end();} 

  

  bool callsFunc(FunctionInfo* fi) {
    return _calledTo.count(fi);
  }

  uint64_t dynamicInstsOnPath(int i) {
    return (uint64_t)instsOnPath(i) * (uint64_t)_iterCount[i];
  }

  uint64_t getStaticInstCount() {
    uint64_t numStaticInsts = 0;
    for (auto I = body_begin(), E = body_end(); I != E; ++I) {
      numStaticInsts += (*I)->len();
    }
    return numStaticInsts;
  }

  double superBlockEfficiency() {
    uint64_t totalIterCount = getTotalIters();
    uint64_t totalDynamicInst = numInsts();

    uint64_t totalStaticInstCount = getStaticInstCount();

    uint64_t totalInstInSB = totalIterCount * totalStaticInstCount;
    // No instruction in the superblock ????
    // actually, we should assert here and debug the benchmarks...
    static bool messagePrinted = false;
    if (totalInstInSB == 0) {
      if (!messagePrinted) {
        std::cout << "Warning: totalInstInSB is zero. please investigate...\n";
        messagePrinted = true;
      }
      return false;
    }

    return (totalDynamicInst)/((double)totalInstInSB);
  }

  bool isSuperBlockProfitable(double factor) {
    double instrIncrFactor = superBlockEfficiency();
    return (instrIncrFactor >= factor);
  }


  bool calledOnlyFrom(std::set<FunctionInfo*>& fiSet,
                      std::set<LoopInfo*>& liSet,
                      bool first=true);

  uint64_t totalDynamicInlinedInsts();

  void printLoopBody(std::ostream& out) {
      out << loop_head()->rpoNum() << ": (";
      for(auto i = body_begin(), e = body_end();i!=e;++i) {
        out << (*i)->rpoNum() << " " ;
      }
      out << ")\n";
  }

  int numPaths() { return _iterCount.size(); } 
  BBvec& getPath(int path_index) { return _pathMap[path_index]; } 

  int depth() {return _depth;}
  LoopInfo* parentLoop() {return _immOuterLoop;}
 
  void mergeLoopBody(BB* latch_bb, BBset& newLoopBody) {
    _loopLatches.insert(latch_bb);
    _loopBody.insert(newLoopBody.begin(),newLoopBody.end());
  }

  bool isLatch(BB* bb) { return _loopLatches.count(bb)!=0;}
  bool inLoop(BB* bb) { return _loopBody.count(bb)!=0;}

  bool useOutsideLoop(Op* op) {
    for(auto di = op->u_begin(), de = op->u_end(); di!=de; ++di) {
      Op* dop = *di;
      BB* bb = dop->bb();
      if(!inLoop(bb)) {
        return true;
      }
    }
    return false;
  }

  int loopSize() {return _loopBody.size();}

  int weightOf(BB* bb1, BB* bb2);

  void incIter(bool profile);
  void iterComplete(int pathIndex, BBvec& path);
  void endLoop(bool profile);
  void beginLoop(bool profile);
  int curIter() {return _curIter;}

  void incInstr() {_numInsts++;}
  uint64_t numInsts() {return _numInsts;} 

  //Static Instructions in Loop Nest
  int staticInsts() {
    int static_insts=0;
    for(auto i=_loopBody.begin(),e=_loopBody.end();i!=e;++i) {
      BB* bb = *i;
      //static_insts+=bb->len();
      static_insts+=bb->static_estimate();
    }
    return static_insts;
  }

  //Static Instructions in *just* my BBs, not including deeper nests
  int myStaticInsts() {
    int static_insts=staticInsts();
    for(auto i=_immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
      LoopInfo* li=*i;
      static_insts-=li->staticInsts();
    }
    //assert(static_insts>0); I can have 0 static_insts, if some of the bbs 
    return static_insts;
  }

  int inlinedOnlyStaticInsts();

  //the whole thing, with inlining
  int inlinedStaticInsts() {
    if(cantFullyInline()) {
      return -1;
    }
    return staticInsts() + inlinedOnlyStaticInsts();
  }

  bool isLoopFullyParallelizable() {
    if(!isInnerLoop()) {
      return false;  //Only inner loops for now
    }

    //check all wr,ww,rw pairs for loop dependence
    for(int i = 0; i < 3; ++i) {
      auto ldsi = ld_wr_begin(), ldse = ld_wr_end();
      if(i==1) {
        ldsi = ld_ww_begin();
        ldse = ld_ww_end();
      } else if(i==2) {
        ldsi = ld_rw_begin();
        ldse = ld_rw_end();
      }

      for(;ldsi!=ldse;++ldsi) {
        LoopInfo::LoopDep dep = *ldsi;
        
        //Patterns:
        // 0 0  0:  Only dependences inside loop iter
        // 0 0 -1:  Inner loop dependence -- bad!
        // 0 1 -1:  Dependence between outer iterations, this is good still!
        for(auto di=dep.begin(),de=dep.end();di!=de;) {
          auto di_cur=di;
          ++di;

          if(di == de) { // this is the inner loop
            if(*di_cur !=0) { //depending on any other inner loop iteration is a no-no
              return false;
            }
          } else { //this is not the inner loop
            if(*di_cur==0) { //skip dependences that go between loop boundaries
              break; //go to next loop dependence
            }
          }
        }
      }
    }
    return true;
  }

  void printLoopDeps(std::ostream& out);

  void initializePathInfo(BB* bb, std::map<BB*,int>& numPaths);
  void initializePathInfo();
  void build_rpo();
  void build_rpo(BB* bb,std::set<BB*>& seen);

  //Compute precise nesting of loops, after loop subset analysis is determined
  int depthNest();

  void opAddr(Op* op, uint64_t addr, uint8_t acc_size);
 
  bool isStriding(Op* op) {
    if(_opStriding.count(op)) {
      return _opStriding[op];
    }
    return false;
  }

  int stride(Op* op) {
    if(_opStride.count(op)) {
      return _opStride[op];
    } 
    return -3; //this looks like a weird enough value to make me scared : )
  }

  void addRelativeLoopDep_wr(LoopDep dep) {_loopDepSet_wr.insert(dep);}
  void addRelativeLoopDep_ww(LoopDep dep) {_loopDepSet_ww.insert(dep);}
  void addRelativeLoopDep_rw(LoopDep dep) {_loopDepSet_rw.insert(dep);}

  //static Functions
  //Compute blind loop subset analysis
  static void checkNesting(LoopInfo* li1, LoopInfo* li2);

  //SubgraphVec::iterator sg_begin() {return _subgraphVec.begin();}
  //SubgraphVec::iterator sg_end() {return _subgraphVec.end();}
  bool canFlowOP(std::vector<Op*>& worklist, Op* dest_op, bool from);
};

//Class which describes properties of a dynamic iteration;
class LoopIter {
public:
  typedef std::vector<std::pair<LoopInfo*,int>> IterPos;
  typedef std::vector<LoopInfo*> LoopStack;

private:
  IterPos iterPos;
  uint64_t function_iter=0;
  
public:
  LoopIter(LoopStack& ls,uint64_t fi_iter) {
    function_iter=fi_iter;
    LoopStack::iterator i,e;
    for(i=ls.begin(),e=ls.end();i!=e;++i) {
      LoopInfo* li = *i;
      iterPos.push_back(std::make_pair(li,li->curIter()));
    }
  }
  
  LoopInfo* relevantLoop() {return iterPos.back().first;}
  int relevantLoopIter() {return iterPos.size() > 0 ? iterPos.back().second:-1;}
  uint64_t functionIter() {return function_iter;}

  unsigned psize() {return iterPos.size();}
  IterPos::iterator pbegin() {return iterPos.begin();}
  IterPos::iterator pend()   {return iterPos.end();}

  void print() {
    for(auto i1=pbegin(),e1=pend(); i1!=e1; ++i1) {
      std::cout << i1->second << " ";
    }
  }

};

#endif
