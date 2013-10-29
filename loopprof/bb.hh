#ifndef BB_HH
#define BB_HH

#include "op.hh"

#include <vector>
#include <map>
#include <list>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/list.hpp>

class BB;
class BB {
public:
  typedef std::vector<BB*> BBvec;
  typedef std::vector<int> IntEdge;
  typedef std::list<Op*> OpVec;
  typedef std::map<CPC,Op*> OpMap;

  typedef  std::map<BB*,unsigned> EdgeCount;

  static uint32_t _idcounter;

private:
  uint32_t _id;
  FunctionInfo* _funcInfo;
  CPC _head, _tail;
  BBvec _pred, _succ;
  //IntEdge _succ_weight; removed, delete this
  int _freq;  
  int _rpo_num;
  OpVec _ops;
  OpMap _opMap;

  //EdgeCount predCount;

friend class boost::serialization::access;
template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _id;
    //ar & _funcInfo; saved by parent
    ar & _head;
    ar & _tail;
    ar & _pred;
    ar & _succ;
    //ar & _succ_weight;  removed, delete this soon
    ar & _freq;
    ar & _rpo_num;
    ar & _ops;
    ar & _opMap;
    for(auto i=_ops.begin(),e=_ops.end();i!=e;++i) {
      Op* op = *i;
      op->setBB(this);
    }
  }

public:
  EdgeCount succCount;

  //member funcs
  BB() : _funcInfo(NULL) {} // For serializer >: (

  BB(FunctionInfo* fi,CPC cpc,CPC tailCPC) : _id(_idcounter++), 
     _funcInfo(fi), _head(cpc), _tail(tailCPC), _rpo_num(-1) {}
 
  //getters
  CPC head() const {return _head;}
  CPC tail() const {return _tail;}

  int len() {return _ops.size();} 
  Op* firstOp() {return _ops.front();}
  Op* lastOp() {return _ops.back();}

  bool freq() {return _freq;}
  void incFreq() {++_freq;}

  int rpoNum() {return _rpo_num;}
  FunctionInfo* func() {return _funcInfo;}

  //setters
  void setFuncInfo(FunctionInfo* f) {_funcInfo=f;}
  void setRPONum(int num) {_rpo_num=num;}

  unsigned succ_size() {return _succ.size();}
  BBvec::iterator succ_begin() {return _succ.begin();}
  BBvec::iterator succ_end() {return _succ.end();}
  void clear_succ() {_succ.clear();}

  unsigned pred_size() {return _pred.size();}
  BBvec::iterator pred_begin() {return _pred.begin();}
  BBvec::iterator pred_end() {return _pred.end();}

  //IntEdge::iterator weight_begin() {return _succ_weight.begin();}
  //IntEdge::iterator weight_end() {return _succ_weight.end();}

  OpVec::iterator op_begin() {return _ops.begin();}
  OpVec::iterator op_end() {return _ops.end();}

  OpVec::reverse_iterator op_rbegin() {return _ops.rbegin();}
  OpVec::reverse_iterator op_rend() {return _ops.rend();}



  void rev_trace(BB* bb_prev);
  void trace(BB* bb_next);
  void remove(BB* bb);
  void removePred(BB* bb);
  //void setWeight(int i, int weight);  delete these two later
  //int weightOf(BB* bb);

  Op* getOp(CPC cpc);

//static funcs
  // BB::Split
  static void Split(BB* upperBB, BB* lowerBB); 

  // Decide which one came first.
  static bool taller(const BB* bb1, const BB* bb2);

};

#endif


