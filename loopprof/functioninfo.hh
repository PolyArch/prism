#ifndef FUNCTION_INFO_HH
#define FUNCTION_iNFO_HH

#include <map>
#include <vector>

#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>

#include "loopinfo.hh"
#include "bb.hh"

#include "elfparse.hh"

class FunctionInfo {

struct bb_compare {
  bool operator() (const BB* lhs, const BB* rhs) const{
    return BB::taller(rhs,lhs); 
  }
};

public:
  typedef std::map<CPC,BB*> BBMap;
  typedef std::map<CPC,std::set<BB*,bb_compare> > BBTailMap;
  typedef std::vector<BB*> BBvec;
  typedef std::vector<int> DOMvec;
  typedef std::map<BB*,LoopInfo*> LoopList;
  typedef std::set<FunctionInfo*> FuncSet;
  typedef std::map<Op*,int> CallByMap; //which op did the call, how many times
  typedef std::map<std::pair<Op*,FunctionInfo*>,int> CallToMap; //which op did the call, how many times

  static uint32_t _idcounter;

private:
  uint32_t _id=-1;
  BBMap _bbMap; //lists all basic blocks inside function
  BBTailMap _bbTailMap; //lists all basic blocks inside function
  FuncSet _calledBy;
  FuncSet _calledTo;
  CallByMap _calledByMap;
  CallToMap _calledToMap;

  BB* _firstBB=0;
  CPC _loc;

  //stats
  int _calls=0;

  uint64_t _insts=0;

  uint64_t _nonLoopInsts=0;

  uint64_t _directRecInsts=0;
  uint64_t _anyRecInsts=0;
  uint64_t _nonLoopDirectRecInsts=0;
  uint64_t _nonLoopAnyRecInsts=0;
  
  LoopList _loopList;
  DOMvec _dom;
  BBvec _rpo;
  prof_symbol* _sym=0;

  bool _canRecurse=false;
  bool _callsRecursiveFunc=false;

friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _id;
    ar & _bbMap;
//    ar & _bbTailMap; //not necessary?
    ar & _calledBy;
    ar & _calledTo;
    ar & _calledByMap;
    ar & _calledToMap;
    ar & _firstBB;
    ar & _loc;
    ar & _calls;
    ar & _insts;
    ar & _nonLoopInsts;
    ar & _directRecInsts;
    ar & _anyRecInsts;
    ar & _nonLoopDirectRecInsts;
    ar & _nonLoopAnyRecInsts;
    ar & _loopList;
    ar & _dom;
    ar & _rpo;
    ar & _sym;
    ar & _canRecurse;
    ar & _callsRecursiveFunc;

    for(auto i=_bbMap.begin(),e=_bbMap.end();i!=e;++i) {
      BB* bb = i->second;
      bb->setFuncInfo(this);
    }
    for(auto i=_loopList.begin(),e=_loopList.end();i!=e;++i) {
      LoopInfo* li = i->second;
      li->setFuncInfo(this);
    }
  }

  void calculateRPO(BB* bb);

  int intersectDOM(int finger1,int finger2);  
  void getLoopBody(BB* latch_bb, std::set<BB*>& loopBody);
  void createLoop(BB* head_bb, BB* latch_bb);
  bool dominates(BB* bb1,BB* bb2);

public:
  FunctionInfo(CPC cpc) : _id(_idcounter++), _firstBB(NULL), _calls(0), _sym(NULL)
                           {_loc=cpc;}
  FunctionInfo() : _id(_idcounter++), _firstBB(NULL), _calls(0),_sym(NULL)
                       {} //For serializer ... >: (

  BBMap::iterator bb_begin() {return _bbMap.begin();}
  BBMap::iterator bb_end() {return _bbMap.end();}
  LoopList::iterator li_begin() {return _loopList.begin();}
  LoopList::iterator li_end() {return _loopList.end();}

  uint32_t id() {return _id;}
  int nBBs() {return _bbMap.size();}

  void setSymbol(prof_symbol* sym) { _sym=sym;}
  prof_symbol* symbol() {return _sym;}

  std::string nice_name() {
    if(_sym!=0) {
      return ELF_parser::demangle(_sym->name.c_str());
    } else {
      std::stringstream ss;
      ss << _id;
      return ss.str();
    }
  }

  void setCanRecurse(bool b) {
    /*if(!_canRecurse) {
      std::cout<<nice_name() << " can recurse\n";
    }*/
    _canRecurse=b;
  }

  //this means that you either recurse, or call a recursive function
  void propogateCallsRecursiveFunc(bool propogate=false, int depth=0) {
    //this node already processed
    //for(int i = 0 ; i < depth; ++i) {
    //  std::cout << "->";
    //}
    //std::cout << nice_name() 
      //        << " propogate=" << propogate 
        //      << " _canRecurse=" << _canRecurse 
          //    << " _callsRec=" << _callsRecursiveFunc << "\n";

    if(_callsRecursiveFunc) {
      return;
    }

    if(_canRecurse || propogate) {
      _callsRecursiveFunc=true;
      for(auto i = _calledBy.begin(), e= _calledBy.end(); i!=e;++i) {
        FunctionInfo* funcInfo = *i;
        funcInfo->propogateCallsRecursiveFunc(true, depth+1);
      }
    }
  }

/*
  void figureOutIfLoopsCallRecursiveFunctions() {

  }*/

  bool canRecurse() {return _canRecurse;}
  bool callsRecursiveFunc() {return _callsRecursiveFunc;}
  bool isLeaf() {return _calledTo.size()==0;}
  bool callsFunc(FunctionInfo* fi) {return _calledTo.count(fi);}

  void got_called() {_calls++;}
  void calledBy(FunctionInfo* fi) {
    _calledBy.insert(fi);
    fi->_calledTo.insert(this);  
    //std::cout << fi->nice_name() << " called " << this->nice_name() << "\n";
  }

  void calledByOp(FunctionInfo* fi,Op* op) {
    _calledByMap[op]++;
    fi->_calledToMap[std::make_pair(op,this)]++;

    //std::cout << fi->nice_name() << " called " << this->nice_name() << "\n";

    //do the same to the loop
    LoopInfo* li = fi->innermostLoopFor(op->bb());
    if(li) {
      li->calledTo(op,this);
      //std::cout << "--------------found called to loop";
    } else {
      //std::cout << "--------------no loop for";
    }
    //std::cout << "bb" << op->bb()->rpoNum() << " " << op->func()->nice_name() << "\n";
  }

  CallByMap::iterator callby_begin() {return _calledByMap.begin();}
  CallByMap::iterator callby_end()   {return _calledByMap.end();}
  CallToMap::iterator callto_begin() {return _calledToMap.begin();}
  CallToMap::iterator callto_end()   {return _calledToMap.end();}

  int calls() {return _calls;}

  bool hasFuncCalls() {
    return _calledToMap.size() > 0;
  /*
    for(auto i = _bbMap.begin(), e = _bbMap.end(); i!=e; ++i) {
      BB* bb = i->second;
      for(auto ii = bb->op_begin(), ee=bb->op_end();ii!=ee;++ii) {
        Op* op = *ii;
        if(op->isCall()) {
          return true;
        }
      }
    }
    return false;
    */
  }

  uint64_t insts() {return _insts;}

  void incInsts(bool loop, bool directRec, bool anyRec) {
    _insts++;
    if(!loop) {
      _nonLoopInsts++;
      if(directRec) {
        _nonLoopDirectRecInsts++; 
      }
      if(anyRec) {
        _nonLoopAnyRecInsts++;
      }
    } 

    if(directRec) {
      _directRecInsts++;      
    }
    if(anyRec) {
      _anyRecInsts++;
    }
  }

  uint64_t nonLoopInsts() {return _nonLoopInsts;}
  uint64_t directRecInsts() {return _directRecInsts;}
  uint64_t anyRecInsts() {return _anyRecInsts;}
  uint64_t nonLoopDirectRecInsts() {return _nonLoopDirectRecInsts;}
  uint64_t nonLoopAnyRecInsts() {return _nonLoopAnyRecInsts;}
  
  LoopInfo* innermostLoopFor(BB* bb) {
    //Find loop with Smallest size which contains BB
    LoopInfo* loop_for_bb=NULL;
    LoopList::iterator il,el;
    for(il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
      LoopInfo* li = il->second;
      if(li->inLoop(bb)) {
        if(loop_for_bb==NULL || li->loopSize() < loop_for_bb->loopSize()) {
          loop_for_bb=li;
        }
      }
    }
    return loop_for_bb;
  }

  int staticInsts() {
    int static_insts=0;
    for(auto i=_bbMap.begin(),e=_bbMap.end();i!=e;++i) {
      BB* bb = i->second;
      static_insts+=bb->len();
    }
    return static_insts;
  }

  //remove loops
  int myStaticInsts() {
    int static_insts=staticInsts();
    if(static_insts==0) {
      return 0;
    }
    for(auto i=_loopList.begin(),e=_loopList.end();i!=e;++i) {
      LoopInfo* li = i->second;
      if(li->isOuterLoop()) {
        static_insts-=li->staticInsts();
      }
      assert(static_insts>=0);
    }
    assert(static_insts>=0);
    return static_insts;
  }

  bool cantFullyInline() {
    return (_insts==0 || _callsRecursiveFunc);
  }

  //the whole thing, with inlining
  int inlinedStaticInsts() {
    if(cantFullyInline()) {
      return -1;
    }
    int static_insts=staticInsts();
    if(static_insts==0) {
      return 0;
    }
    // pair<pair<Op*,FunctionInfo*>,int>
    for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
      FunctionInfo* fi = i->first.second;
      static_insts+=fi->inlinedStaticInsts();
    }
    return static_insts;
  }
 

  //the whole thing, with inlining
  uint64_t totalDynamicInlinedInsts() {
    if(cantFullyInline()) {
      return 0; //zero means recursive
    }

    uint64_t total=insts(); //this already includes loop insts
    // pair<pair<Op*,FunctionInfo*>,int>
    for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
      FunctionInfo* fi = i->first.second;
      float ratio = i->second / fi->calls();  //divide dynamic insts up by calls
      total+=fi->totalDynamicInlinedInsts() * ratio;
    }

    return total;
  }


#if 0
  static std::pair<LoopInfo*,FunctionInfo*> findEntry(
                                            std::set<FunctionInfo*>& fiSet, 
                                            std::set<LoopInfo*>& liSet) {
     if(fiSet.count(this)) {
       return make_pair(NULL,this);
     }
  }


  static std::pair<LoopInfo*,FunctionInfo*> findEntry(Op* op,
                                            std::set<FunctionInfo*>& fiSet, 
                                            std::set<LoopInfo*>& liSet) {
    //if this function is contained, and not the orginal item, we are done
    if(!first && fiSet.count(this)) {
      return true;
    }

    // bail on recursion, or if we've reached the first func
    if(_callsRecursiveFunc || _calledByMap.size()==0 ) {
      return false;
    }

    //since we're not contained, we must check all predecessor calls,
    //and make sure they are contained
    for(auto i = _calledByMap.begin(), e=_calledByMap.end(); i!=e;++i) {
      Op* call_op = i->first;
      //if this op is associated with the loop, check the loop
      if(LoopInfo* li = call_op->func()->innermostLoopFor(call_op->bb())) {
        if(!li->calledOnlyFrom(fiSet,liSet,false)) {
          return false;
        }
      } else {
      //if this op is not associated with a loop, check the next function up
        if(!call_op->func()->calledOnlyFrom(fiSet,liSet,false)) {
          return false;
        }
      }
    }
    
    return true;
  }
#endif

  bool calledOnlyFrom(std::set<FunctionInfo*>& fiSet,
                              std::set<LoopInfo*>& liSet, 
                              bool first=true) {
    //if this function is contained, and not the orginal item, we are done
    if(!first && fiSet.count(this)) {
      return true;
    }

    // bail on recursion, or if we've reached the first func
    if(_callsRecursiveFunc || _calledByMap.size()==0 ) {
      return false;
    }

    //since we're not contained, we must check all predecessor calls,
    //and make sure they are contained
    for(auto i = _calledByMap.begin(), e=_calledByMap.end(); i!=e;++i) {
      Op* call_op = i->first;
      //if this op is associated with the loop, check the loop
      if(LoopInfo* li = call_op->func()->innermostLoopFor(call_op->bb())) {
        if(!li->calledOnlyFrom(fiSet,liSet,false)) {
          return false;
        }
      } else {
      //if this op is not associated with a loop, check the next function up
        if(!call_op->func()->calledOnlyFrom(fiSet,liSet,false)) {
          return false;
        }
      }
    }
    
    return true;
  }



  BB* getBB(CPC cpc);
  BB* addBB(BB* prevBB, CPC headCPC, CPC tailCPC);

  BB* firstBB() {return _firstBB;}
  CPC loc() {return _loc;}
  void ascertainBBs();

  void calculateRPO() {
    //iterate over bbs, and find the ones with no predecessors
    for(auto i=_bbMap.begin(),e=_bbMap.end();i!=e;++i) {
      BB* bb = i->second;
      if(bb->pred_size()==0) {
        calculateRPO(bb);
      }
    }
    if(_rpo.size()==0 && _bbMap.size()!=0) {
      calculateRPO(_firstBB);
    }

    std::reverse(_rpo.begin(), _rpo.end());
    if(_bbMap.size() != _rpo.size()) {
      std::cerr << "bbmapsize: " << _bbMap.size() 
                << " rposize " << _rpo.size() << "\n";
      assert(_bbMap.size() == _rpo.size()); //make sure we've got everyone once
    }
  }

  void calculateDOM();
  void detectLoops();
  void loopNestAnalysis();

  void printAllLoops() {
    std::cout << "Func<" << this << ">::\n";
    for (auto I = _loopList.begin(), E = _loopList.end(); I != E; ++I) {
      std::cout << "\tBB<" << I->first << "> : Loop<" << I->second << ">\n";
    }
  }

  LoopInfo* getLoop(BB* bb) {
    LoopList::iterator i = _loopList.find(bb);
    if(i==_loopList.end()) {
      return NULL;
    } else {
      return i->second;
    }
  }

  void toDotFile(std::ostream& out);
  void toDotFile_detailed(std::ostream& out);
  void toDotFile_record(std::ostream& out);

};

#endif
