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
  static uint32_t _idcounter;

private:
  uint32_t _id=-1;
  BBMap _bbMap; //lists all basic blocks inside function
  BBTailMap _bbTailMap; //lists all basic blocks inside function
  FuncSet _calledBy;
  FuncSet _calledTo;

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

friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _id;
    ar & _bbMap;
//    ar & _bbTailMap; //not necessary?
    ar & _calledBy;
    ar & _calledTo;
    ar & _firstBB;
    ar & _loc;
    ar & _calls;
    ar & _nonLoopInsts;
    ar & _directRecInsts;
    ar & _anyRecInsts;
    ar & _nonLoopDirectRecInsts;
    ar & _nonLoopAnyRecInsts;
    ar & _loopList;
    ar & _dom;
    ar & _rpo;
    ar & _sym;
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

  void got_called() {_calls++;}
  void calledBy(FunctionInfo* fi) {
    _calledBy.insert(fi);
    fi->_calledTo.insert(this);  
  }
  int calls() {return _calls;}

  bool hasFuncCalls() {
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
  
  int staticInsts() {
    int static_insts=0;
    for(auto i=_bbMap.begin(),e=_bbMap.end();i!=e;++i) {
      BB* bb = i->second;
      static_insts+=bb->len();
    }
    return static_insts;
  }

  int myStaticInsts() {
    int static_insts=staticInsts();
    for(auto i=_loopList.begin(),e=_loopList.end();i!=e;++i) {
      LoopInfo* li = i->second;
      if(li->isOuterLoop()) {
        static_insts-=li->staticInsts();
      }
      assert(static_insts>0);
    }
    assert(static_insts>0);
    return static_insts;
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
