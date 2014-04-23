#ifndef OP_HH
#define OP_HH

#include <set>
#include <map>
#include <bitset>

#include <boost/serialization/set.hpp>
#include <boost/serialization/bitset.hpp>
#include <sstream>
#include <vector>
#include <iostream>
#include <boost/tokenizer.hpp>

#include <boost/serialization/binary_object.hpp>
#include "cpu/crtpath/crtpathnode.hh"

class Op;

#include "exec_profile.hh"

class FunctionInfo;
class BB;
class Subgraph;

typedef std::pair<uint64_t,uint16_t> CPC;

//Some crappy code from stack overflow
template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
  template<typename S, typename T> struct hash<pair<S, T>>
  {
    inline size_t operator()(const pair<S, T> & v) const
    {
      size_t seed = 0;
      ::hash_combine(seed, v.first);
      ::hash_combine(seed, v.second);
      return seed;
    }
  };
}

/*
namespace std {
    template <>
        class hash<CPC>{
        public :
        size_t operator()(const CPC &cpc ) const{
            return hash<uint64_t>()(cpc.first) ^ hash<uint16_t>()(cpc.second);
        }
    };
}*/
class Op {
public:
  typedef std::set<Op*> Deps;
  static uint32_t _idcounter;
  CP_NodeDiskImage img;

private:

  enum { ISLOAD, ISSTORE, ISCALL, ISCTRL, ISRETURN, ISFLOATING };
  uint32_t _id;

  CPC _cpc;
  Deps _deps, _uses;
  Deps _memDeps,_cacheDeps,_ctrlDeps;
  Deps _adjDeps, _adjUses;

  std::map<Op*, std::set<unsigned> > _indOfDep;
  std::map<unsigned, Deps> _depsOfInd;

  unsigned _opclass=0;
  std::bitset<8> _flags;
  BB* _bb; //saved in parent
  int _bb_pos;
  uint64_t _totLat; //total latency accross all executions
  uint64_t _times;  //total number of times this instruction was executed
  //Subgraph* _subgraph;

  bool _is_stack = false;
  bool _is_const_load = false;

  uint64_t _effAddr = 0;
  int stride = 0;
  FunctionInfo *_calledFunc = 0;
  enum stride_value_type { st_Unknown, st_Constant, st_Variable, st_Cycle };
  enum stride_value_type stride_ty = st_Unknown;

  unsigned _acc_size = 4;
  uint64_t _first_effAddr = 0;




  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    uint8_t temp_flags = _flags.to_ulong();

    void* img_ptr = reinterpret_cast<void*>(&img);
    ar & boost::serialization::make_binary_object(img_ptr,
                                                  sizeof(CP_NodeDiskImage));

    ar & _id;
    ar & _cpc;
    ar & _deps;
    //ar & _uses;
    ar & _memDeps;
    ar & _cacheDeps;
    ar & _opclass;
    ar & temp_flags;
    //ar & _bb; saved in bb, just add it back from bb
    ar & _bb_pos;
    ar & _totLat;
    ar & _times;

    ar & stride;
    ar & stride_ty;
    ar & _sameEffAddrOpSet;
    ar & _nextEffAddrOpSet;
    ar & _calledFunc;

    ar & _acc_size;
    ar & _first_effAddr;
    ar & _indOfDep;
    ar & _is_stack;
    ar & _is_const_load;

    _flags = std::bitset<8>(temp_flags);

    for(auto i=_deps.begin(),e=_deps.end();i!=e;++i) {
      Op* dep_op = *i;
      dep_op->addUse(this);
    }

    for(const auto& i : _indOfDep) {
      for(int j : i.second) {
        _depsOfInd[j].insert(i.first); //restore the _depOfInd, if loading
      }
    }

/*  for(auto i=_uses.begin(),e=_uses.end();i!=e;++i) {
      Op* use_op = *i;
      use_op->addJustDep(this);
    }*/

  }

public:
  /*Op(CPC cpc,std::vector<Op*>& deps,Op* mem_dep, isLoad,isStore,isCtrl) :
    _cpc(cpc),_deps(deps),_mem_dep(mem_dep),_isLoad(isLoad),_isStore(isStore),
    _isCtrl(isCtrl)
  {
    
  }*/
  Op (): _id(_idcounter++), _bb(NULL), _totLat(0), _times(0)  {
   
  }
 
  void setImg(CP_NodeDiskImage& in_img) {img=in_img;}

  uint32_t id() {return _id;}
  CPC cpc() {return _cpc;}
  bool isFloating() {return _flags[ISFLOATING];}
  bool isLoad() {return _flags[ISLOAD];}
  bool isStore() {return _flags[ISSTORE];}
  bool isCtrl() {return _flags[ISCTRL];}
  bool isCall() {return _flags[ISCALL];}
  bool isReturn() {return _flags[ISRETURN];}
  bool isMem() {return _flags[ISLOAD] || _flags[ISSTORE];}


  bool isBBHead() {return _bb_pos==0; }
  BB*           bb()     {return _bb;}
  int           bb_pos() {return _bb_pos;}
  //Subgraph*     subgraph() {return _subgraph;}
  //void          setSubgraph(Subgraph* sg) {_subgraph=sg;}

  void setIsStack() { _is_stack=true; }
  bool isStack() { return _is_stack; }

  void setIsConstLoad() { 
    assert(!isSpcMem()); 

    //do this
    /*if(memHasDataOperand()) {
      _is_const_load=false;
      return;
    }*/

    //also check to make sure that the mem addr is "rdip" insruction
    for(auto addr_deps : _depsOfInd) {
      if(!isAddrOperandOfMem(addr_deps.first)) {
        continue;
      }

      for(Op* op : addr_deps.second) {
        if(!op->shouldIgnoreInAccel()) { //checks for rdip
          _is_const_load=false;
          return;
        }
      }
    }
    _is_const_load=true; 
  }
  bool isConstLoad() { return _is_const_load; }

  struct eainfo {
    uint64_t addr;
    int itercnt;
    stride_value_type sty;
    int stride;
    eainfo(uint64_t a, int i, stride_value_type ty, int s):
      addr(a), itercnt(i), sty(ty), stride(s) {}

    bool operator ==(const eainfo &that) const {
      return (this->addr == that.addr
              && this->itercnt == that.itercnt
              && this->sty == that.sty
              && this->stride == that.stride);
    }
    bool operator !=(const eainfo &that) const {
      return !(*this == that);
    }
  };

  //std::vector<eainfo> effAddrAccessed;

  void setCalledFunc(FunctionInfo *callFn) {
    _calledFunc = callFn;
  }

  std::string getCalledFuncName() const;
  FunctionInfo *getCalledFunc() const { return _calledFunc; }

  void printEffAddrs() {
    return ;
#if 0
    int i = 0;
    std::cout << "size: " << _acc_size << "\n";
    for (auto I = effAddrAccessed.begin(), E = effAddrAccessed.end(); I != E; ++I) {
      std::cout << I->addr << ":" << I->itercnt << ":"
                << ((I->sty == st_Constant)?"C"
                    : (I->sty == st_Unknown)?"U"
                    : (I->sty == st_Variable)?"V"
                    : "y") << ":" << I->stride << " ";
      if ( (++i) % 8 == 0)
        std::cout << "\n";
    }
#endif
  }
  void initEffAddr(uint64_t addr, unsigned size, int iterCnt) {
    _first_effAddr = addr;
    _effAddr = addr;
    // For old gem5, size can be zero.. 
    if (size) {
      _acc_size = size;
    }
    assert(_acc_size != 0);
  }

  uint64_t getCurEffAddr() const {
    return _effAddr;
  }

  void computeStride(uint64_t addr, int iterCnt) {
    if (_effAddr == 0) {
      _first_effAddr = addr;
      _effAddr = addr;
      return;
    }
    if (stride_ty == st_Unknown) {
      stride_ty = st_Constant;
      stride = ((int64_t)addr - (int64_t)_effAddr);
      _effAddr = addr;
    } else if (stride_ty == st_Constant || stride_ty == st_Cycle) {
      int new_stride = (addr - _effAddr);
      _effAddr = addr;
      if (new_stride != stride) {
        // can this be cycle through the address location...
        if (_first_effAddr  != addr) {
          stride_ty = st_Variable;
        } else {
          // Are we cycling through the address? I guess so -- a case in radar.
          // FIXME:: check the cycle stride as well...
          stride_ty = st_Cycle;
        }
      }
    }

  }

  std::set<Op*> _sameEffAddrOpSet; // keeps track of same addr
  std::set<Op*> _nextEffAddrOpSet; // keeps track of next addr for
                                   //coalescing purpose

  bool isSameEffAddrAccessed(Op *op) const {
    // check whether the same effective addr accessed map
    return (_sameEffAddrOpSet.count(op) != 0);
  }

  Op *getCoalescedOp() const {
    if (_nextEffAddrOpSet.size() == 0)
      return 0;
    if (_nextEffAddrOpSet.size() == 1)
      return *_nextEffAddrOpSet.begin();
    return 0;
  }

  void set_intersect_inplace(std::set<Op*> &this_set,
                             const std::set<Op*> &that_set) {
    std::set<Op*>::iterator it1 = this_set.begin();
    std::set<Op*>::const_iterator it2 = that_set.begin();
    while ( (it1 != this_set.end()) && (it2 != that_set.end()) ) {
      if (*it1 < *it2) {
        this_set.erase(it1++);
      } else if (*it2 < *it1) {
        ++it2;
      } else {
        ++it1;
        ++it2;
      }
    }
    this_set.erase(it1, this_set.end());
  }

  void iterComplete(std::map<uint64_t, std::set<Op*> > &_effAddr2Op)
  {
    // if (_sameEffAddrOpSet == nullset)
    //   _sameEffAddrOpSet = _effAddr2Op[getCurEffAddr]
    // else
    //   _sameEffAddrOpSet = _sameEffAddrOpSet \intersect
    //                            _effAddr2Op[getCurEffAddr]
    if (!isLoad() && !isStore())
      return;

    // atleast this op will be there.
    assert (_effAddr2Op.count(getCurEffAddr()));

    {
      const std::set<Op*> &opSet = _effAddr2Op[getCurEffAddr()];

      if (_sameEffAddrOpSet.size() == 0)
        _sameEffAddrOpSet.insert(opSet.begin(), opSet.end());
      else
        set_intersect_inplace(_sameEffAddrOpSet,
                              opSet);
      assert(_sameEffAddrOpSet.size() != 0);
    }

    {
      assert(_acc_size != 0);
      uint64_t nextEffAddr = getCurEffAddr() + _acc_size;

      //std::cout << "ca: " << getCurEffAddr() << ", size = " << _acc_size
      //<< " := na = " << nextEffAddr << "\n";


      if (_effAddr2Op.count(nextEffAddr) != 0) {
        const std::set<Op*> &nextOpSet = _effAddr2Op[nextEffAddr];
        if (_nextEffAddrOpSet.size() == 0)
          _nextEffAddrOpSet.insert(nextOpSet.begin(), nextOpSet.end());
        else
          set_intersect_inplace(_nextEffAddrOpSet, nextOpSet);
      }

      if (_nextEffAddrOpSet.size() == 0)
        _nextEffAddrOpSet.insert(0); // tombstone

      /*
      if (_nextEffAddrOpSet.size() == 1) {
        Op *nextOp = *_nextEffAddrOpSet.begin();
        std::cout << "mem:" << this << "--" << nextOp << "\n";
      }
      */
    }
  }

  bool getStride(int *strideLen) const {
    if (stride_ty == st_Constant || stride_ty == st_Cycle) {
      if (strideLen)
        *strideLen = stride;
      return true;
    }
    return false;
  }

  std::string   name() {
    std::stringstream ss;
    ss << "v" << _id;
    return ss.str();
  }
  FunctionInfo* func();

  void setCPC(CPC cpc) {_cpc=cpc;}
  void setIsLoad(bool isload)    {_flags.set(ISLOAD,isload);}
  void setIsStore(bool isstore)  {_flags.set(ISSTORE,isstore);}
  void setIsCtrl(bool isctrl)    {_flags.set(ISCTRL,isctrl);}
  void setIsCall(bool iscall)    {_flags.set(ISCALL,iscall);}
  void setIsReturn(bool isreturn){_flags.set(ISRETURN,isreturn);}
  void setIsFloating(bool isflt) {_flags.set(ISFLOATING,isflt);}

  void setBB(BB* bb) {assert(bb); _bb=bb;}
  void setBB_pos(int i) {_bb_pos=i;}

  void addUse(Op* op) {
     //assert(op!=this);
     assert(op);
    _uses.insert(op);
    updated();
  }

  void addJustDep(Op* op) {
    assert(op);
    _deps.insert(op);
    updated();
  }

  unsigned opclass() {return _opclass;}
  void setOpclass(unsigned opclass) {
    _opclass=opclass;
  }

  void addDep(Op* op, unsigned i) {
    assert(op);
    _deps.insert(op);
    _depsOfInd[i].insert(op);
    _indOfDep[op].insert(i);

    op->addUse(this);
    updated();
  }
  void addMemDep(Op* op) {
    assert(op->isLoad() || op->isStore());
    _memDeps.insert(op);
    updated();
  }

  void addCacheDep(Op* op) {_cacheDeps.insert(op); updated();}
  void addCtrlDep(Op* op) {_ctrlDeps.insert(op); updated();}

  void executed(uint16_t lat) {
    assert(lat < 5000);
    _totLat+=lat; _times+=1;
  }

  static bool checkDisasmHas(Op *op, const char *chkStr) {
      uint64_t pc = op->cpc().first;
      int upc = op->cpc().second;
      std::string disasm =  ExecProfile::getDisasm(pc, upc);
      if (disasm.find(chkStr) != std::string::npos)
        return true;
      return false;
  }

  bool shouldIgnoreInAccel() {
    if(!_cached_ignore_valid) {
      _cached_ignore=_shouldIgnoreInAccel();
      _cached_ignore_valid=true;
    }
    return _cached_ignore;
  }

  bool plainMove() {
    if(!_cached_plain_move_valid) {
      _cached_plain_move=_plainMove();
      _cached_plain_move_valid=true;
    }
    return _cached_plain_move;
  }  
  
  std::string getUOPName() {
    using namespace boost;

    uint64_t pc = _cpc.first;
    int upc = _cpc.second;
    std::string disasm =  ExecProfile::getDisasm(pc, upc);
    char_separator<char> sep(" ");
    tokenizer<char_separator<char>> tokens(disasm, sep);

    int i = 0;
    for (const auto& t : tokens) {
      ++i;
      if(i==4) {
        return t;
      }
    }
    return std::string("");
  }


private:
  void updated() {  //clear cached datastructs
    _adjDeps.clear();
    _adjUses.clear();
  }

  //Ignore op if it has no deps, or if it's a limm of some sort
  bool _cached_ignore=false;
  bool _cached_ignore_valid=false;
  bool _shouldIgnoreInAccel() {
    if(isStore() || isLoad() || isCtrl()) {
      return false;
    }
    //if(numUses()==0) {  //This is a bad idea, because some deps won't show
    //  return true;
    //}
    if(checkDisasmHas(this, "lfpimm") ||
       checkDisasmHas(this, "limm") ||
       checkDisasmHas(this, "rdip")
       ) {
      return true;
    }
    return false;
  }

  bool _cached_plain_move=false;
  bool _cached_plain_move_valid=false;

  bool _plainMove() {
     if(checkDisasmHas(this, "mov2fpsimp") ||
        checkDisasmHas(this, "movsimp")) {
       return true;
     }
     return false;
  }


public:

  static const char* opname(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return "nofu";
    case 1: //IntALU
      return "alu";

    case 2: //IntMult
      return "mul";
    case 3: //IntDiv
      return "div";

    case 4: //FloatAdd
      return "fadd";
    case 5: //FloatCmp
      return "fcmp";
    case 6: //FloatCvt
      return "fcvt";
    case 7: //FloatMult
      return "fmul";
    case 8: //FloatDiv
      return "fdiv";
    case 9: //FloatSqrt
      return "fsq";
    case 30: //FloatSqrt
      return "rdprt";
    case 31: //FloatSqrt
      return "wrprt";
    default:
      return "other";
    }
    return "?";
  }


  bool dependsOn(Op* op) {return _deps.count(op)!=0;}

  Deps::iterator m_begin() { return _memDeps.begin(); }
  Deps::iterator m_end() { return _memDeps.end(); }
  Deps::iterator d_begin() {return _deps.begin();}
  Deps::iterator d_end() {return _deps.end();}

  void uSet(Deps& uses,Deps& skipped) {
    //std::cout << "touch" << _id << "\n";
    for(auto& uop : _uses) {
      //std::cout << "use" << _id << " ";
      if(skipped.count(uop) || uses.count(uop)) {
        //std::cout << skipped.count(uop) << "," << uses.count(uop) << _id << "\n";
        continue;
      }
      if(uop->shouldIgnoreInAccel()) {
        skipped.insert(uop);
        return;
      }
      if(!uop->plainMove()) {
        uses.insert(uop);
        //std::cout << "inserted\n";
      } else {
        //std::cout << "skipped\n";
        skipped.insert(uop);
        uop->uSet(uses,skipped);
      }
    } 
  }

  Deps::iterator adj_u_begin() {
    if(_adjUses.empty()) {
      Deps skipped;
      //std::cout << "uset" << _id << "\n";
      uSet(_adjUses,skipped);
    }
    return _adjUses.begin();
  }
  Deps::iterator adj_u_end() {return _adjUses.end();}

  void dSet(Deps& deps,Deps& skipped) {
    for(auto& dop : _deps) {
      if(skipped.count(dop) || deps.count(dop)) {
        return;
      }
      if(dop->shouldIgnoreInAccel()) {
        skipped.insert(dop);
        return;
      }
      if(!dop->plainMove()) {
        deps.insert(dop);
      } else {
        skipped.insert(dop);
        dop->dSet(deps,skipped);
      }
    } 
  }

  Deps::iterator adj_d_begin() {
    if(_adjDeps.empty()) {
      Deps skipped;
      dSet(_adjDeps,skipped);
    }
    return _adjDeps.begin();
  }
  Deps::iterator adj_d_end() {return _adjDeps.end();}

  Deps::iterator u_begin() {return _uses.begin();}
  Deps::iterator u_end() {return _uses.end();}
  unsigned numDeps()  { return _deps.size(); }
  unsigned numMemDeps()  { return _memDeps.size(); }
  unsigned numUses() {return _uses.size();}
  uint32_t avg_lat() {return _totLat / _times;}

  std::string dotty_name() {
    std::stringstream out;
    out << id() << ":" << getUOPName();
    //out << "(";
    //if(op->isLoad()) {
    //  out << "ld";
    //} else if(op->isStore()) {
    //  out << "st";
    //} else 
    if(isCall()) {
      out << "(call)";
    } else if(isReturn()) {
      out << "(ret)";
    } else if(isCtrl()) {
      out << "(ctrl)";
    }

    if(isMem()) {
      out << "(x" << stride;
      
      if(isStack()) {
        out << ",stack";
      }
      if(isConstLoad()) {
        out << ",const";
      }

      out << ")";
    } 

    out << ":" << opname(opclass());
    //out << ")";
    return out.str();
  }

  std::string dotty_tooltip() {
    std::stringstream out;
    out <<  ExecProfile::getDisasm(cpc().first, cpc().second);
      //iterate through 

      out << "       ";
      out << "defs:";
      for(auto di=d_begin(),de=d_end();di!=de;++di) {
        Op* dep_op = *di;
        out << dep_op->id() << ",";
      }
      out << " uses:";
      for(auto ui=u_begin(),ue=u_end();ui!=ue;++ui) {
        Op* use_op = *ui;
        out << use_op->id() << ",";
      }

      out << "    ";
      out << "adj defs:";
      for(auto di=adj_d_begin(),de=adj_d_end();di!=de;++di) {
        Op* dep_op = *di;
        out << dep_op->id() << ",";
      }
      out << " adj uses:";
      for(auto ui=adj_u_begin(),ue=adj_u_end();ui!=ue;++ui) {
        Op* use_op = *ui;
        out << use_op->id() << ",";
      }

      out << " avg_lat: " << avg_lat(); 
      return out.str();
  }

  std::string dotty_name_and_tooltip() {
    std::stringstream out;
    out << "label=\"" << dotty_name();
    out << "\", ";
    out << "tooltip=\"" << dotty_tooltip() << "\"";
    return out.str();
  }

  void print() {
    std::cout << dotty_name() << dotty_tooltip() << "\n";
  }


  //TODO:FIXME:WARNING:
  //All of the below needs to change if gem5 uops change...
  
  static const int mem_data_operand_index = 2;

  bool memHasData=false;
  bool memHasData_cached=false;

  int memDataOperandIndex() {
    assert(isMem());
    assert(memHasDataOperand());
    return mem_data_operand_index;
  }

  //tell if this is a mem addr op
  bool isMemAddrOp(Op* op) {
    assert(_indOfDep.count(op)); //make sure i know it!
    for(int j : _indOfDep[op]) {
      if(isAddrOperandOfMem(j)) {
        return true;
      }
    }
    return false;
  }

  bool isDataOperandOfMem(int i) {
    if(memHasDataOperand()) {
      return i==mem_data_operand_index;
    } else {
      return false;
    }
  }

  bool isAddrOperandOfMem(int i) {
    if(memHasDataOperand()) {
      return (i==0) || (i==1) || (i==3);
    } else {
      return (i==0) || (i==1) || (i==2);
    }
  }

  bool memHasDataOperand() {
    assert(isMem()); // don't look at this if not memory
    if(!memHasData_cached) {
      memHasData_cached=true;
      std::string name = getUOPName();
      if(name == std::string("ldstbig") ||
         name == std::string("ldstlbig") ||
         name == std::string("ldfp") ||
         name == std::string("ldbig") ||
         name == std::string("cda") ){
        memHasData=false;
      }
      memHasData=true;
    }
    return memHasData;
  } 

  bool _is_spc_mem_cached=false;
  bool _is_spc_mem=false;

  bool isSpcMem() {
    static const std::string ldst("ldst");

    if(!_is_spc_mem_cached) {
      std::string name = getUOPName();
      _is_spc_mem_cached=true;    
      if(name.substr(0, ldst.size()) == ldst) {
        _is_spc_mem=true;
      }
    }
    return _is_spc_mem;
  }

  /*
  enum OperandType {None, Index, Base, Data, SegBase};
  static OperandType const test[2][4] = {
    {Index, Base, Data, SegBase}, 
    {Index, Base, Data, None}
  };*/


};


#if 0

Heres some notes! -- Ideally this would come from gem5s instruction generator, but
Im a bit too lazy for that.  If gem5 changes, this needs to change too.  Good luck
finding that bug if it ever happens! : )


Operand   0      1     2        3 

Loads:
Ldst      Index  Base  Data     SegBase  
LdstBig   Index  Base  SegBase
Ldstl     Index  Base  Data     SegBase
LdstlBig  Index  Base  SegBase
Ldfp      Index  Base  SegBase  
Ld        Index  Base  Data     SegBase 
LdBig     Index  Base  SegBase

Stores: 
St        Index  Base  Data     Segbase 
Stul      Index  Base  Data     Segbase
Stfp      Index  Base  FpData   Segbase
Cda       Index  Base  SegBase

#endif








#endif
