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

#include "cpu/crtpath/crtpathnode.hh"
class Op;
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
  enum { ISLOAD, ISSTORE, ISCALL, ISCTRL, ISRETURN };
  uint32_t _id;
  CPC _cpc;
  Deps _deps;
  Deps _uses; //forward use
  Deps _memDeps;
  Deps _cacheDeps;
  Deps _ctrlDeps;
  unsigned _opclass=0;
  std::bitset<8> _flags;
  BB* _bb; //saved in parent
  int _bb_pos;
  uint64_t _totLat; //total latency accross all executions
  uint64_t _times;  //total number of times this instruction was executed
  //Subgraph* _subgraph;
  uint64_t _effAddr = 0;
  int stride = 0;

  enum stride_value_type { st_Unknown, st_Constant, st_Variable, st_Cycle };
  enum stride_value_type stride_ty = st_Unknown;
  friend class boost::serialization::access;

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    uint8_t temp_flags = _flags.to_ulong();
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


    //ar & _subgraph;
    _flags = std::bitset<8>(temp_flags);

    for(auto i=_deps.begin(),e=_deps.end();i!=e;++i) {
      Op* dep_op = *i;
      dep_op->addUse(this);
    }
/*
    for(auto i=_uses.begin(),e=_uses.end();i!=e;++i) {
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
  Op (): _id(_idcounter++), _bb(NULL), _totLat(0), _times(0) /*,_subgraph(NULL)*/  {
   
  }
  
  uint32_t id() {return _id;}
  CPC cpc() {return _cpc;}
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
  unsigned _acc_size = 4;
  uint64_t _first_effAddr = 0;
  void initEffAddr(uint64_t addr, unsigned size, int iterCnt) {
    _first_effAddr = addr;
    _effAddr = addr;
    _acc_size = size;

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

  void setBB(BB* bb) {assert(bb); _bb=bb;}
  void setBB_pos(int i) {_bb_pos=i;}

  void addUse(Op* op) {
     //assert(op!=this);
     assert(op);
    _uses.insert(op);
  }

  void addJustDep(Op* op) {
    assert(op);
    _deps.insert(op);
  }

  unsigned opclass() {return _opclass;}
  void setOpclass(unsigned opclass) {
    _opclass=opclass;
  }

  void addDep(Op* op) {
    //assert(op!=this);
    assert(op);
    _deps.insert(op);
    op->addUse(this);
  }
  void addMemDep(Op* op) {
    assert(op->isLoad() || op->isStore());
    _memDeps.insert(op);
  }

  void addCacheDep(Op* op) {_cacheDeps.insert(op);}
  void addCtrlDep(Op* op) {_ctrlDeps.insert(op);}

  void executed(uint16_t lat) {
    assert(lat < 5000);
    _totLat+=lat; _times+=1;
  }

  bool dependsOn(Op* op) {return _deps.count(op)!=0;}

  Deps::iterator m_begin() { return _memDeps.begin(); }
  Deps::iterator m_end() { return _memDeps.end(); }
  Deps::iterator d_begin() {return _deps.begin();}
  Deps::iterator d_end() {return _deps.end();}

  Deps::iterator u_begin() {return _uses.begin();}
  Deps::iterator u_end() {return _uses.end();}
  unsigned numDeps()  { return _deps.size(); }
  unsigned numMemDeps()  { return _memDeps.size(); }
  unsigned numUses() {return _uses.size();}
  uint32_t avg_lat() {return _totLat / _times;}

};

#endif
