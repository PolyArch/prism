#include "bb.hh"

using namespace std;

uint32_t BB::_idcounter=1;

void BB::rev_trace(BB* bb_prev) {
  _pred.push_back(bb_prev);
}

void BB::trace(BB* bb_next) {
  BBvec::iterator i,e;
  for(i=_succ.begin(),e=_succ.end();i!=e;++i) {
    BB* other = *i;
    if(bb_next == other) {
      return;
    }
  }
  _succ.push_back(bb_next);
  bb_next->rev_trace(this);
}



void BB::removePred(BB* bb) {
  _pred.erase(std::remove(_pred.begin(), _pred.end(), bb ), _pred.end() ); 
}

void BB::remove(BB* bb) {
  _succ.erase(std::remove(_succ.begin(), _succ.end(), bb ), _succ.end() ); 
  bb->removePred(this);
}

/*
void BB::setWeight(int i, int weight) {
  assert(i>=0);
  assert(weight>=0);
  _succ_weight.resize(i+1,-1);
  _succ_weight[i]=weight;
}*/

/*
int BB::weightOf(BB* bb) {
  BBvec::iterator ii,ee;
  int i=0;
  for(ii=_succ.begin(),ee=_succ.end();ii!=ee;++ii,++i) {
    BB* this_bb = *ii;
    if(this_bb==bb) {
      return _succ_weight[i];
    }
  }
  assert(0 && "no bb exists for bb");
}
*/

Op* BB::getOp(CPC cpc) {

  OpMap::iterator mapIter = _opMap.find(cpc);
  if(mapIter==_opMap.end()) {
    _ops.emplace_back(new Op());
    Op* op = _ops.back();
    _opMap[cpc]=op;
    op->setBB_pos(_ops.size()-1);
    return op;
  } else {
    return mapIter->second;
  }
}

//static funcs
// BB::Split
// split oldBB into oldBB->newBB
// transfer successors from oldBB to newBB
// attach oldBB to newBB
// transfer frequency from old to new
void BB::Split(BB* upperBB, BB* lowerBB) {
  BBvec::iterator i,e;
  for(i=upperBB->_succ.begin(),e=upperBB->_succ.end();i!=e;++i) {
    BB* bb = *i;
    lowerBB->_succ.push_back(bb);
    bb->removePred(upperBB);
    bb->_pred.push_back(lowerBB);
  }
  upperBB->_succ.clear();

  upperBB->_succ.push_back(lowerBB);
  lowerBB->_pred.push_back(upperBB);
  //newBB->_freq = oldBB->_freq;
}

// Greater
bool BB::taller(const BB* bb1, const BB* bb2) {
  if(bb1->head().first != bb2->head().first) {
    return bb1->head().first < bb2->head().first;
  } else {
    return bb1->head().second < bb2->head().second;
  }
/*
  if(bb1->head().first < bb2->head().first) {
    return true;
  } else if(bb1->head().first > bb2->head().first) {
    return false;
  } else {
    return bb1->head().second < bb2->head().second;
  }
*/
}

