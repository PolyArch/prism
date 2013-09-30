#ifndef OP_HH
#define OP_HH

#include <set>
#include <bitset>

#include <boost/serialization/set.hpp>
#include <boost/serialization/bitset.hpp>
#include <sstream>
#include <vector>
#include <iostream>

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

private:
  enum { ISLOAD, ISSTORE, ISCALL, ISCTRL, ISRETURN };
  uint32_t _id;
  CPC _cpc;
  Deps _deps;
  Deps _uses; //forward use
  Deps _memDeps;
  Deps _cacheDeps;
  Deps _ctrlDeps;
  std::bitset<8> _flags;
  BB* _bb; //saved in parent
  int _bb_pos;
  uint64_t _totLat; //total latency accross all executions
  uint64_t _times;  //total number of times this instruction was executed
  Subgraph* _subgraph;
  uint64_t _effAddr = 0;
  int stride = 0;
  enum stride_value_type { st_Unknown, st_Constant, st_Variable };
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
    ar & temp_flags;
    //ar & _bb; saved in bb, just add it back from bb
    ar & _bb_pos;
    ar & _totLat;
    ar & _times;
    ar & _subgraph;
    _flags = std::bitset<8>(temp_flags);
    for(auto i=_deps.begin(),e=_deps.end();i!=e;++i) {
      Op* dep_op = *i;
      dep_op->addUse(this);
    } 
  }

public:
  /*Op(CPC cpc,std::vector<Op*>& deps,Op* mem_dep, isLoad,isStore,isCtrl) :
    _cpc(cpc),_deps(deps),_mem_dep(mem_dep),_isLoad(isLoad),_isStore(isStore),
    _isCtrl(isCtrl)
  {
    
  }*/
  Op (): _id(_idcounter++), _bb(NULL), _totLat(0), _times(0), _subgraph(NULL)  {
   
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
  Subgraph*     subgraph() {return _subgraph;}
  void          setSubgraph(Subgraph* sg) {_subgraph=sg;}

  struct eainfo {
    uint64_t addr;
    int itercnt;
    stride_value_type sty;
    int stride;
    eainfo(uint64_t a, int i, stride_value_type ty, int s):
      addr(a), itercnt(i), sty(ty), stride(s) {}
  };

  std::vector<eainfo> effAddrAccessed;

  void printEffAddrs() {
    int i = 0;
    std::cout << "size: " << _size << "\n";
    for (auto I = effAddrAccessed.begin(), E = effAddrAccessed.end(); I != E; ++I) {
      std::cout << I->addr << ":" << I->itercnt << ":"
                << ((I->sty == st_Constant)?"C"
                    : (I->sty == st_Unknown)?"U"
                    : "V") << ":" << I->stride << " ";
      if ( (++i) % 8 == 0)
        std::cout << "\n";
    }
  }
  unsigned _size = 0;
  void initEffAddr(uint64_t addr, unsigned size, int iterCnt) {
    effAddrAccessed.emplace_back(eainfo(addr, iterCnt, stride_ty, stride));
    _effAddr = addr;
    _size = size;
  }
  void          computeStride(uint64_t addr, int iterCnt) {

    if (_effAddr == 0) {
      _effAddr = addr;
      return;
    }
    if (stride_ty == st_Unknown) {
      stride_ty = st_Constant;
      stride = ((int64_t)addr - (int64_t)_effAddr);
      _effAddr = addr;
    } else if (stride_ty == st_Constant) {
      int new_stride = (addr - _effAddr);
      _effAddr = addr;
      if ( new_stride != stride) {
        stride_ty = st_Variable;
      }
    }
    effAddrAccessed.emplace_back(eainfo(addr, iterCnt, stride_ty, stride));
  }
  bool getStride(int *strideLen) const {
    if (stride_ty == st_Constant) {
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
    _uses.insert(op);
  }

  void addDep(Op* op) {
    _deps.insert(op);
    op->addUse(this);
  }
  void addMemDep(Op* op) {_memDeps.insert(op);}
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
  int numUses() {return _uses.size();}
  uint32_t avg_lat() {return _totLat / _times;}

};

#endif
