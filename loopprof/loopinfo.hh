#ifndef LOOPINFO_HH
#define LOOPINFO_HH

#include <map>
#include <list>
#include <vector>

#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/tokenizer.hpp>
#include <fstream>
#include "bb.hh"

class Subgraph {
public:
  typedef std::set<Op*> OpSet;

private:
  OpSet _ops;
  static uint32_t _idCounter;
  uint32_t _id;

public:
friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version){
    ar & _ops;
  }

  Subgraph():_id(_idCounter++){}

  void insertOp(Op* op) {_ops.insert(op);}
  bool hasOp(Op* op) {return _ops.count(op);}
  OpSet::iterator op_begin() {return _ops.begin();}
  OpSet::iterator op_end() {return _ops.end();}
  unsigned size() {return _ops.size();}

  uint32_t id() {return _id;}
};

class FunctionInfo;
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
  
  typedef std::set<Subgraph*> SubgraphSet;
  typedef std::vector<Subgraph*> SubgraphVec;

  typedef std::set<Op*> OpSet;

static uint32_t _idcounter;

private:
  uint32_t _id=0;
  FunctionInfo* _funcInfo=NULL;
  BB* _head=NULL;
  BBset _loopLatches;
  BBset _loopBody;
  BBvec _rpo;
  PathMap _pathMap;
  int     _maxPaths=0;
  IntMap _iterCount; //path iteration count
  int    _totalIterCount=0; //total number of iters
  int    _loopCount=0; //total number of loops
  int    _curIter=0;

  uint64_t  _numInsts=0; //total number of instructions executed in this loop
  uint64_t  _numCycles=0; //total number of cycles (roughly) in this loop

  EdgeInt _edgeWeight; //weight of each transition to compute path index

  LoopSet _innerLoops, _outerLoops;
  LoopSet _immInnerLoops;
  LoopInfo* _immOuterLoop=NULL;

  OpAddr _prevOpAddr;
  OpInt  _prevOpIter;

  OpBool _opStriding;
  OpInt  _opStride;

  LoopDepSet _loopDepSet; //set of dependent loop iterations, specified relatively
  std::set<FunctionInfo*> _funcsCalled; //functions called, not yet used
  char _depth=-1;

  //Stuff for hot-path and beret
  OpSet _instsInPath;
  int _hotPathIndex=-1;
  SubgraphSet _subgraphSet;
  SubgraphVec _subgraphVec;

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
    ar & _loopDepSet;
    ar & _depth;
    ar & _instsInPath;
    ar & _hotPathIndex;
    ar & _subgraphSet;
    ar & _subgraphVec;
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


  void setFuncInfo(FunctionInfo* f) {_funcInfo=f;}
  bool hasSubgraphs() {
    return _subgraphSet.size()>0;
  }

  BBset::iterator       body_begin()  { return _loopBody.begin(); }
  BBset::iterator       body_end()    { return _loopBody.end(); }
  PathMap::iterator     paths_begin() { return _pathMap.begin(); }
  PathMap::iterator     paths_end()   { return _pathMap.end(); }
  LoopDepSet::iterator  ld_begin()    { return _loopDepSet.begin(); }
  LoopDepSet::iterator  ld_end()      { return _loopDepSet.end(); }
  SubgraphSet::iterator ss_begin()    { return _subgraphSet.begin(); }
  SubgraphSet::iterator ss_end()      { return _subgraphSet.end(); }

  BBset::iterator       rpo_begin()   { return _rpo.begin(); }
  BBset::iterator       rpo_end()     { return _rpo.end(); }

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

  void printGamsPartitionText(std::ostream& out,int count,
                              std::string resultfile, std::string fixes);

  bool printGamsPartitionProgram(std::string filename, bool gams_details,bool no_gams);


  void printSubgraphDot(std::ostream& out);

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

  int dynamicInstsOnPath(int i) {
    return instsOnPath(i) * _iterCount[i];
  }

  int instsInAllInnerLoops() {
    uint64_t total = numInsts();
    for(auto i = _immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
      LoopInfo* li = *i;
      total += li->instsInAllInnerLoops();
    }
    return total;
  }

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

  int loopSize() {return _loopBody.size();}

  int weightOf(BB* bb1, BB* bb2);

  void iterComplete(int pathIndex, BBvec& path);
  void endLoop();
  void beginLoop();
  int curIter() {return _curIter;}

  void incInstr() {_numInsts++;}
  uint64_t numInsts() {return _numInsts;} 
 
  void initializePathInfo(BB* bb, std::map<BB*,int>& numPaths);
  void initializePathInfo();

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

  void addRelativeLoopDep(LoopDep dep) {
    _loopDepSet.insert(dep);
  }

  //static Functions
  //Compute blind loop subset analysis
  static void checkNesting(LoopInfo* li1, LoopInfo* li2);

  SubgraphVec::iterator sg_begin() {return _subgraphVec.begin();}
  SubgraphVec::iterator sg_end() {return _subgraphVec.end();}

  bool canFlowOP(std::vector<Op*>& worklist, Op* dest_op, bool from);

};


//Class which describes properties of a dynamic iteration;
class LoopIter {
public:
  typedef std::vector<std::pair<LoopInfo*,int>> IterPos;
  typedef std::vector<LoopInfo*> LoopStack;

private:
  IterPos iterPos;
  
public:
  LoopIter(LoopStack& ls) {
    LoopStack::iterator i,e;
    for(i=ls.begin(),e=ls.end();i!=e;++i) {
      LoopInfo* li = *i;
      iterPos.push_back(std::make_pair(li,li->curIter()));
    }
  }
  
  LoopInfo* relevantLoop() {return iterPos.back().first;}
  int relevantLoopIter() {return iterPos.size() > 0 ? iterPos.back().second:-1;}

  unsigned psize() {return iterPos.size();}
  IterPos::iterator pbegin() {return iterPos.begin();}
  IterPos::iterator pend()   {return iterPos.end();}
};

#endif
