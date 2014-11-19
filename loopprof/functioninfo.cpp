#include "functioninfo.hh"
#include "exec_profile.hh"

#include <boost/tokenizer.hpp>


using namespace std;

uint32_t FunctionInfo::_idcounter=1;

void FunctionInfo::inlinedBBs(std::set<FunctionInfo*>& funcsSeen,
                              std::vector<BB*>& bbs) {
  if(funcsSeen.count(this)) {
    return;
  }
  funcsSeen.insert(this);
  for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii) {
    BB* bb = *ii;
    bbs.push_back(bb);
  }
  for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
    FunctionInfo* fi = i->first.second;
    fi->inlinedBBs(funcsSeen,bbs);
  }
}

bool FunctionInfo::no_loops_in_inlined_callgraph() {
  if(cantFullyInline()) {
    return false; //zero means recursive
  }
  if(_loopList.size() > 0) {
    return false;
  }
  for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
    FunctionInfo* fi = i->first.second;
    if(!fi->no_loops_in_inlined_callgraph()) {
      return false;
    }
  }
  return true;
}


void FunctionInfo::ascertainBBs() {
  //iterate through each tail, see if there are issues
  for(auto ti = _bbTailMap.begin(), te=_bbTailMap.end();ti!=te;++ti) {
    std::set<BB*,bb_compare>& tail = ti->second;
    if(tail.size()==1) {
      continue;
    } else if(tail.size()==0) {
      assert(0);
    }
    
    BB* smallBB=NULL;
    BB* nextSmallestBB;
    //for each bb with the same tail
    for(auto bbi = tail.begin(),bbe=tail.end();bbi!=bbe;++bbi) {
      if(smallBB==NULL) {
        smallBB = *bbi;
      } else {
        //fall through bbs must be corrected
        BB* fallBB = *bbi;
        //iterate through successors of fallBB, and put them on smallBB
        for(auto si=fallBB->succ_begin(),se=fallBB->succ_end();si!=se;++si) {
          BB* succ_bb = *si;
          succ_bb->removePred(fallBB);
          smallBB->trace(succ_bb);
        }
        fallBB->clear_succ();
        fallBB->trace(nextSmallestBB);
      }

      //set this up for nexet bb
      nextSmallestBB=*bbi;
    }
  }
  deleteUnreachableStaticBBs(); //doing this here makes sense.
}

BB* FunctionInfo::addBB(BB* prevBB, CPC headCPC, CPC tailCPC, bool dynamic) {

   //std::cout << headCPC.first << "." << headCPC.second << " " << tailCPC.first << tailCPC.second << "\n"; 

   BB* bb;
   BBMap::iterator bbMapIter = _bbMap.find(headCPC);
   if(bbMapIter == _bbMap.end()){ //add bb to both maps
     //KNOWN_ISSUE -- THIS BB NEVER DELETED
     bbMapIter = _bbMap.emplace(std::piecewise_construct,
                               forward_as_tuple(headCPC),
                               forward_as_tuple(new BB(this,headCPC,tailCPC))).first;
      bb = bbMapIter->second;
     _bbTailMap[tailCPC].insert(bb); //add to the tail map
     
   } else {
     bb = bbMapIter->second;
   }

   if(prevBB!=NULL) {
     prevBB->trace(bb);
   }

   if(_firstBB==NULL || bb->head().first < _firstBB->head().first) {
     _firstBB=bb;
   }

   if(dynamic) {
     bb->setNotStatic();
   }

   return bb;
/*
    BB* bb;  
    BB* returnBB=NULL;
    bool bb_is_new=false;

    BBMap::iterator bbMapIter = _bbMap.find(headCPC);
    if(bbMapIter == _bbMap.end()){ //add bb to both maps
      //KNOWN_ISSUE -- THIS BB NEVER DELETED
      bbMapIter = _bbMap.emplace(std::piecewise_construct,
                                forward_as_tuple(headCPC),
             forward_as_tuple(new BB(this,headCPC,tailCPC))).first;
      bb_is_new=true;
    }

    bb=bbMapIter->second;

    BBTailMap::iterator bbMapTailIter = _bbTailMap.find(tailCPC);
    if(bbMapTailIter == _bbTailMap.end()) {
       _bbTailMap[tailCPC].insert(bb);
       returnBB = bb;
    } else {
       BB* tallestBB=NULL;
       BB* nextTallestBB=NULL;
       BB* smallestBB=bb;

       //1. find the basic block head in the tail map which is the next tallest
       std::set<BB*>& tailSet = bbMapTailIter->second; 
       std::set<BB*>::iterator i,e;
       for(i=tailSet.begin(),e=tailSet.end();i!=e;++i) {
         BB* someBB = *i;
         if(!tallestBB||BB::taller(someBB,tallestBB)) {
           tallestBB=someBB;
         }
         if(!smallestBB||BB::taller(smallestBB,someBB)) {
           smallestBB=someBB;
         }
         if(BB::taller(someBB,bb) &&
           ( !nextTallestBB || (BB::taller(nextTallestBB,someBB)) )) {
           nextTallestBB=someBB;
         }
       }
       //Split the BB
       if(bb_is_new) {
         if(BB::taller(bb,tallestBB)){
           BB::Split(bb,tallestBB);
         } else {
           BB::Split(nextTallestBB,bb);
         }
       }
       _bbTailMap[tailCPC].insert(bb);
       returnBB = smallestBB;
    }

    if(prevBB!=NULL) {
      prevBB->trace(bb);
    }

    if(_firstBB==NULL) {
      _firstBB=bb;
    }

    return returnBB;
*/
}


BB* FunctionInfo::getBB(CPC cpc) {
    BBMap::iterator bbiter = _bbMap.find(cpc);
    if(bbiter == _bbMap.end()){
      return NULL;
    }
    return ((*bbiter).second);
}

void FunctionInfo::deleteBB(BB* bb) {
   for(auto i=bb->succ_begin(),e=bb->succ_end();i!=e;++i) {
     BB* succ_bb = *i;
     bb->remove(succ_bb);
   }
   for(auto i=bb->pred_begin(),e=bb->pred_end();i!=e;++i) {
     BB* pred_bb = *i;
     pred_bb->remove(bb);
   }
   _bbMap.erase(bb->head());
   //TODO: Memory Leak (bb)
}

bool FunctionInfo::findNonStaticOnlyBB(BB* bb, std::set<BB*>& seen_bbs) {
  if(seen_bbs.count(bb)) {
    return false;
  }
  seen_bbs.insert(bb);
  for(auto i=bb->succ_begin(),e=bb->succ_end();i!=e;++i) {
    BB* succ_bb = *i;
    if(!succ_bb->static_only()) {
      return true;
    }
    bool nsr = findNonStaticOnlyBB(succ_bb,seen_bbs);
    if(nsr) {
      return true;
    }
  }
  return false;
}


void FunctionInfo::markReachable(BB* bb, std::set<BB*>& seen_bbs) {
  if(seen_bbs.count(bb)) {
    return;
  }
  seen_bbs.insert(bb);
  for(auto i=bb->succ_begin(),e=bb->succ_end();i!=e;++i) {
    BB* succ_bb = *i;
    markReachable(succ_bb,seen_bbs);
  }
}

//Delete any BBs which are not reachable by a real bb or the _firstBB
void FunctionInfo::deleteUnreachableStaticBBs() {
  std::set<BB*> seen_bbs;
  bool count_first=true;

  //first determine if _firstBB can reach anyone
  if(_firstBB->static_only()) {
    count_first = findNonStaticOnlyBB(_firstBB,seen_bbs);
  }

  seen_bbs.clear();
  for(auto i = _bbMap.begin(),e=_bbMap.end();i!=e;++i) {
    BB* bb = i->second;
    if(!bb->static_only() || (count_first && bb==_firstBB)) {
      markReachable(bb,seen_bbs);
    }
  }

  //This deletes any unreachable fake nodes.
  vector<BB*> delete_list;
  for(auto i = _bbMap.begin(),e=_bbMap.end();i!=e;++i) {
    BB* bb = i->second;
    if(bb->static_only() && seen_bbs.count(bb)==0 && bb!=_firstBB) {
      delete_list.push_back(bb);
    }
  }

  for(BB* i : delete_list) {
    deleteBB(i);
  }
 
  bool all_reachable=false;

  for(int i = 0; i < 1000; ++i) {
    seen_bbs.clear();
    markReachable(_firstBB,seen_bbs);
    
    if(seen_bbs.size() != _bbMap.size()) {
    
      BB* nBB=NULL;
      for(auto& i : _bbMap) {
        BB* bb = i.second;
        if(bb!=_firstBB && seen_bbs.count(bb)==0 && (nBB==NULL || nBB->head().first > bb->head().first)) {
          nBB=bb;
        }
      }
      assert(nBB);
      if(nBB) {
        addBB(_firstBB,nBB->head(),nBB->tail());
      }
    } else {
      all_reachable=true;
      break;
    }
  }

  assert(all_reachable);
  

}

void FunctionInfo::calculateRPO() {

  if(_firstBB) { //do first BB first
    calculateRPO(_firstBB);
  }

  //iterate over bbs, and find the ones with no predecessors
  for(auto i=_bbMap.begin(),e=_bbMap.end();i!=e;++i) {
    BB* bb = i->second;
    if(bb->pred_size()==0  && _firstBB!=bb) {
      calculateRPO(bb);
    }
  }
  //Not needed
 /*    if(_rpo.size()!=_bbMap.size()) {
    if( _firstBB->rpoNum() == -1) {
      calculateRPO(_firstBB);
    }  else {
      assert(0 && "rpo calc error: this should never happen");
    }
  }*/

  std::reverse(_rpo.begin(), _rpo.end());
  if(_bbMap.size() != _rpo.size()) {
    std::cerr << nice_name() << " ";
    std::cerr << "bbmapsize: " << _bbMap.size() 
              << " rposize " << _rpo.size() << "\n";

    for(auto& i : _rpo) {
      std::cerr << i->head().first << "\n";
      if(_bbMap.count(i->head())==0) {
        std::cerr << "^ Not in bb map\n";
        for(auto& j : _bbMap) {
          BB* bb = j.second;
          for(auto ii = bb->pred_begin(), ie = bb->pred_end();ii!=ie;++ii) {
            BB* check_bb = *ii;
            if(check_bb == i) {
              std::cerr << bb->head().first << "was responsible (had preD)\n";
            }
          }
          for(auto ii = bb->succ_begin(), ie = bb->succ_end();ii!=ie;++ii) {
            BB* check_bb = *ii;
            if(check_bb == i) {
              std::cerr << bb->head().first << "was responsible (had succ)\n";
            }
          }

        }
      }
    }
    std::ofstream err_ofs;
    err_ofs.open ("rpo_bb_mismatch.dot", std::ofstream::out | std::ofstream::trunc);
    toDotFile(err_ofs);
    assert(0);
    //assert(_bbMap.size() == _rpo.size()); //make sure we've got everyone once
  }
}



void FunctionInfo::calculateRPO(BB* bb) {
  bb->setRPONum(-2);
  BB::BBvec::iterator i,e;
  for(i=bb->succ_begin(),e=bb->succ_end();i!=e;++i) {
    BB* succ_bb = *i;
    if(succ_bb->rpoNum()==-1) {
      calculateRPO(succ_bb);
    }
  }
  _rpo.push_back(bb);
  bb->setRPONum(_bbMap.size()-_rpo.size());
}


void FunctionInfo::hookupExitBB(std::set<BB*>& exitBBs) {
  assert(exitBBs.size()>0);
  BB* newExitBB=NULL;
  for(auto i=exitBBs.begin(),e=exitBBs.end();i!=e;++i) {
    BB* exitBB = *i;
    newExitBB = addBB(exitBB,make_pair(-2,-2),make_pair(-3,-3));
  }
  newExitBB->setRPONum(_rpo.size());
  newExitBB->setFakeExit();
  _unique_exit=newExitBB;
}

void FunctionInfo::back_remove(BB* bb, std::set<BB*>& remainingBBs) {
  if(remainingBBs.count(bb)==0) {
    return;
  }
  remainingBBs.erase(bb);
  for(auto i=bb->pred_begin(),e=bb->pred_end();i!=e;++i) {
    BB* pred_bb = *i;
    back_remove(pred_bb,remainingBBs);
  }
}

//Add's a new BB if there isn't a unique exit.
//Returns true if BB is added
bool FunctionInfo::addUniqueExitBB() {
  std::set<BB*> exitBBs;
  std::set<BB*> remainingBBs;
  //bool
 
  for(auto ii=_rpo.rbegin(),ee=_rpo.rend();ii!=ee;++ii) { //reverse reverse post order
    BB* bb = *ii;
    if(bb->succ_size()==0 || (bb->lastOp() && bb->lastOp()->isReturn())) {
      exitBBs.insert(bb);
    }
    remainingBBs.insert(bb);
  }

  for(auto ii=exitBBs.begin(),ee=exitBBs.end();ii!=ee;++ii) { 
    back_remove(*ii,remainingBBs);   
  }

  //eliminate any remaining weird loops
  while(remainingBBs.size()!=0) {
    BB* highestRPO=NULL;
    for(auto ii=remainingBBs.begin(),ee=remainingBBs.end();ii!=ee;++ii) {
      BB* remBB = *ii;
      if(highestRPO==NULL || highestRPO->rpoNum() < remBB->rpoNum()) {
        highestRPO=remBB;
      }
    }
    //Fake Exit
    assert(highestRPO!=NULL);
    exitBBs.insert(highestRPO);
    back_remove(highestRPO,remainingBBs);
  }

/* No longer needed
  if(exitBBs.size() == 0) {
    //FUDGE! we must have stopped recording in a loop.  Only thing to do is
    //create a fake exit node.
    exitBBs.insert(*_rpo.rbegin());
    hookupExitBB(exitBBs);
    return true;
  }
*/
  BB* first_exit_bb=*(exitBBs.begin());
  if(exitBBs.size() == 1 && first_exit_bb->rpoNum()==(int)(_rpo.size()-1)) {
    //we can skip creating a unique exit, because we already have one
    _unique_exit=first_exit_bb;
    return false;
  }
  
  hookupExitBB(exitBBs);
  return true;
}

void FunctionInfo::calculatePDOM() {
  if(_bbMap.size()==0) {
    return;
  } 
  //For later analysis, add a fake BB as a unique exit node.
  bool added_one = addUniqueExitBB();
  bool changed=true;
  
  int pdom_size = _rpo.size()+added_one;

  _pdom.resize(pdom_size,-1);
  _pdom[pdom_size-1]=pdom_size-1; // this *has* to be the unique exit node

  while(changed) {
    changed=false;

    for(auto ii=_rpo.rbegin(),ee=_rpo.rend();ii!=ee;++ii) { //reverse reverse post order
      BB* bb = *ii;
     
      int new_ipdom_rpo=-1;
      for(auto ip=bb->succ_begin(),ep=bb->succ_end();ip!=ep;++ip) {
        BB* succ_bb = *ip;
        int succ_rpo = succ_bb->rpoNum();

        int succ_ipdom_rpo = _pdom[succ_rpo];
        if(succ_ipdom_rpo==-1) {
          continue;
        }

        if(new_ipdom_rpo==-1) {
          new_ipdom_rpo=succ_rpo;
        } else {
          new_ipdom_rpo=intersectPDOM(new_ipdom_rpo,succ_rpo);
        }
      }

      if(new_ipdom_rpo == -1) {
        continue;
      }

      if(new_ipdom_rpo != _pdom[bb->rpoNum()]) {
        changed=true;
        _pdom[bb->rpoNum()]=new_ipdom_rpo;
      }
    }
  }

  calculatePDOM_Frontier();
}

// I <3 Cooper / Harvey / Kennedy (& Ferrante)
/*for all nodes, b
 * if the number of predecessors of b ≥ 2
 *   for all predecessors, p, of b
 *     runner ← p
 *     while runner != doms[b]
 *       add b to runner’s dominance frontier set
 *       runner = doms[runner] 
 */
void FunctionInfo::calculateDOM_Frontier() {
  assert(0 && "To be done");  
}

/*for all nodes, b
 * if the number of successors of b ≥ 2
 *   for all succecessors, s, of b
 *     runner ← s
 *     while runner != pdoms[b]
 *       add b to runner’s post_dominance frontier set
 *       runner = pdoms[runner] 
 */
void FunctionInfo::calculatePDOM_Frontier() {
  for(auto ii=_rpo.rbegin(),ee=_rpo.rend();ii!=ee;++ii) { //reverse reverse post order
    BB* bb = *ii;
    for(auto ip=bb->succ_begin(),ep=bb->succ_end();ip!=ep;++ip) {
      BB* succ_bb = *ip;
      BB* runner = succ_bb;

      set<BB*> originBBs;

      while (runner->rpoNum() != _pdom[bb->rpoNum()]) {
        originBBs.insert(runner);

        unsigned pdom_ind = _pdom[runner->rpoNum()];
        if(pdom_ind==_rpo.size()) {
          runner=_unique_exit;
        } else {
          runner=_rpo[pdom_ind];
        }
      }

      for(auto i=originBBs.begin(), e=originBBs.end();i!=e;++i) {
        BB* originBB = *i;
        _pdomFrontier[originBB].insert(make_pair(bb,succ_bb));
        _pdomReverseFrontier[bb].insert(make_pair(succ_bb,originBB));
      }


    }
  }
}

int FunctionInfo::intersectPDOM(int finger1, int finger2) {
  //if(finger1==-1) return _dom[finger2];
  //if(finger2==-1) return _dom[finger1];

  while (finger1 != finger2) {
    while (finger1 < finger2) {
      finger1 = _pdom[finger1];
    }
    while (finger2 < finger1) {
      finger2 = _pdom[finger2];
    }
  }

  return finger1;
}

bool FunctionInfo::post_dominates(BB* bb1, BB* bb2) {
  int finger1=bb1->rpoNum();
  int finger2=bb2->rpoNum();

  while (finger2 > finger1) {
    finger2 = _pdom[finger2];
  }
  
  return finger1==finger2;
}

void FunctionInfo::calculateDOM() {
  bool changed=true;

  _dom.resize(_rpo.size(),-1);
  _dom[0]=0;
  //std::fill (_dom.begin(),_dom.end(),-1);


  while(changed) {
    changed=false;

    for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii) {
      BB* bb = *ii;
     
      int new_idom_rpo=-1;
      for(auto ip=bb->pred_begin(),ep=bb->pred_end();ip!=ep;++ip) {
        BB* pred_bb = *ip;
        int pred_rpo = pred_bb->rpoNum();

        int pred_idom_rpo = _dom[pred_rpo];
        if(pred_idom_rpo==-1) {
          continue;
        }

        if(new_idom_rpo==-1) {
          new_idom_rpo=pred_rpo;
        } else {
          new_idom_rpo=intersectDOM(new_idom_rpo,pred_rpo);
        }
      }

      if(new_idom_rpo == -1) {
        continue;
      }

      if(new_idom_rpo != _dom[bb->rpoNum()]) {
        changed=true;
        _dom[bb->rpoNum()]=new_idom_rpo;
      }
    }
  }
}


int FunctionInfo::intersectDOM(int finger1, int finger2) {
  //if(finger1==-1) return _dom[finger2];
  //if(finger2==-1) return _dom[finger1];

  while (finger1 != finger2) {
    while (finger1 > finger2) {
      finger1 = _dom[finger1];
    }
    while (finger2 > finger1) {
      finger2 = _dom[finger2];
    }
  }

  return finger1;
}

bool FunctionInfo::dominates(BB* bb1, BB* bb2) {
  int finger1=bb1->rpoNum();
  int finger2=bb2->rpoNum();

  while (finger2 > finger1) {
    finger2 = _dom[finger2];
  }
  
  return finger1==finger2;
}

void FunctionInfo::getLoopBody(BB* cur_bb, set<BB*>& loopBody) {
  loopBody.insert(cur_bb);

  BB::BBvec::iterator ip,ep;
  for(ip=cur_bb->pred_begin(),ep=cur_bb->pred_end();ip!=ep;++ip) {
    BB* pred_bb = *ip;
    if(loopBody.count(pred_bb)==0) {
      getLoopBody(pred_bb,loopBody);
    }
  }
}

void FunctionInfo::createLoop(BB* head_bb, BB* latch_bb) {
  LoopList::iterator loopInfoIter;
  loopInfoIter = _loopList.find(head_bb);

  if(loopInfoIter == _loopList.end()){ //add bb to both maps
    loopInfoIter = _loopList.emplace(std::piecewise_construct,
                            forward_as_tuple(head_bb),
                            forward_as_tuple(new LoopInfo(this,head_bb,latch_bb))).first;
  }

  LoopInfo* loopInfo = loopInfoIter->second;

  set<BB*> loopBody;
  loopBody.insert(head_bb);
  if(head_bb!=latch_bb) {
    getLoopBody(latch_bb,loopBody);
  }

  loopInfo->mergeLoopBody(latch_bb,loopBody);
}

//This function detects loops, calls CreateLoop!
void FunctionInfo::detectLoops() {
  BBvec::iterator ii,ee;
  for(ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii) {
    BB* bb = *ii;
     
    for(auto ip=bb->pred_begin(),ep=bb->pred_end();ip!=ep;++ip) {
      BB* pred_bb = *ip;
      if(dominates(bb,pred_bb)) {
        //found a loop!
        createLoop(bb,pred_bb);
      }
    }
 
  }
}

void FunctionInfo::loopNestAnalysis() {
  LoopList::iterator li1,le1;
  LoopList::iterator li2,le2;
  for(li1=li_begin(),le1=li_end();li1!=le1;++li1) {
    LoopInfo* loopInfo1 = li1->second;
    for(li2=li_begin(),le2=li_end();li2!=le2;++li2) {
      LoopInfo* loopInfo2 = li2->second;
      if(loopInfo1 == loopInfo2) {
        continue;
      }
      LoopInfo::checkNesting(loopInfo1,loopInfo2);
    }
  }
  //do depth analysis
  for(li1=li_begin(),le1=li_end();li1!=le1;++li1) {
    LoopInfo* loopInfo1 = li1->second;
    loopInfo1->depthNest();
  }
}

//static std::string gcolors[] = {"azure", "cornflowerblue", "lightgoldenrodyellow", "palegreen3", "ybrown1", "powderblue", "oldlace", "lavenderblush1", "grey89", "deepskyblue1", "darkolivegreen1", "darksalmon", "mediumorchid","yellowgreen","lightgoldenrod" };
        
static std::vector<std::string> gcolors = {"azure", "cornflowerblue", "lightgoldenrodyellow", "palegreen3", "rosybrown1", "powderblue", "oldlace", "lavenderblush1", "grey89", "deepskyblue1", "darkolivegreen1", "darksalmon", "mediumorchid","yellowgreen","lightgoldenrod" };

void FunctionInfo::toDotFile(std::ostream& out) {
    out << "digraph GA{\n";

    out << " graph [fontname = \"helvetica\"];\n";
    out << " node  [fontname = \"helvetica\"];\n";
    out << " edge  [fontname = \"helvetica\"];\n";  
 
    out << "labelloc=\"t\";\n";
    if(_sym!=0) {
      out <<  "label=\"" << ELF_parser::demangle(_sym->name.c_str());
    } else {
      out <<  "label=\"Func Name Not Known";
    }

    out << "\\n";
    out << "ins=" << ninputs() << ", outs=" << noutputs() << ";\\n";

    if(_canRecurse) {
      out << "can-recurse";
    } else if(_callsRecursiveFunc){
      out << "calls-recurisve";
    } else if(isLeaf()) {
      out << "pure";
    } else {
      out << "calls-funcs";
    }

    if(cantFullyInline()) {
      out << ",cant-inline";
    } else {
      out << ",can-inline";
    }


    out << "\\nst<" << myStaticInsts() 
        << "," << staticInsts()
        << "," << inlinedStaticInsts() << ">\\n";
    out << "dy<" << insts() << "," << totalDynamicInlinedInsts() << ">"; 

    out << "\";\n"; //end label

    BBMap::iterator bbi,bbe;
    for(bbi=_bbMap.begin(),bbe=_bbMap.end();bbi!=bbe;++bbi) {
      BB& bb = *(bbi->second);
      out << "\"" << bb.head().first << "x" << bb.head().second << "\"" 
     
          << "[label=\"" << bb.rpoNum();
        //                 << "\\n" << bb.head().first << "_" << bb.tail().first

      LoopInfo* smallest_li=innermostLoopFor(&bb);
      /*for(auto il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
        LoopInfo* li = il->second;
        if(li->inLoop(&bb)) {
          if(smallest_li == NULL || smallest_li->loopSize() > li->loopSize()) {
            smallest_li=li;
          }
        }
      }*/
      if(smallest_li) {
        out << " L" << smallest_li->id();
      }

      //out << hex << " " << bb.head().first << " " << hex << bb.tail().first;
      //out << dec;

      if(bb.fake_unique_exit()) {
        out << "(fake exit)";
      }
     
      Op* last_op = bb.lastOp();
      if(last_op) {
        if(FunctionInfo* fi = funcCallForOp(last_op)) {
          out << " ->" << fi->nice_name();
        }
      }

      out << "\"";
      
      if(bb.static_only()) {
        out << ",style=\"dashed,filled\"";
      } else {
        out << ",style=\"filled\"";
      }

      if(smallest_li) {
        out << ", fillcolor=" << gcolors[smallest_li->id() % gcolors.size()];
      } else {
        out << ", fillcolor=white";
      }

      
      out << "]\n;";

      for(auto si=bb.succ_begin(),  se=bb.succ_end(); si!=se;++si) {
        BB* succ_bb = *si;
        out << "\"" <<       bb.head().first << "x" <<       bb.head().second 
            << "\"->"
            << "\"" << succ_bb->head().first << "x" << succ_bb->head().second 
            << "\" [label=\"";
        
/*
       for(auto il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
         LoopInfo& li =*(il->second);
         int weight = li.weightOf(&bb,succ_bb);
         if(weight != -1) {
           out << weight;
           break;
         }
       }
*/
        out << bb.succCount[succ_bb]; 

        out << "\"];\n";
      }
    }

    /*
    for(bbi=bbMap.begin(),bbe=bbMap.end();bbi!=bbe;++bbi) {
      BB& bb = bbi->second;
      out << "\"" << bb.head().first << "x" << bb.head().second << "S\"" 
     
          << "[label=\"" << bb.rpoNum()
                         << "\\n" << bb.head().first << "_" << bb.tail().first
                         << "\"]\n;";
      BB::BBvec::iterator si,se;
      for(si=bb.pred_begin(),se=bb.pred_end();si!=se;++si) {
        BB* pred_bb = *si;
        out << "\"" << pred_bb->head().first << "x" << pred_bb->head().second 
            << "S\"->"
            << "\"" << bb.head().first << "x" << bb.head().second 
            << "S\";\n";
      }
    }

    */

    out << "rpo [shape=rectangle,label=\"";
    out << "doms:\\n";
    for(unsigned i = 0; i < _dom.size(); ++i) {
      out << i << " " << _dom[i] << "\\n";
    }
    out << "\"];\n";

    out << "po [shape=rectangle,label=\"";
    out << "pdoms:\\n";
    for(unsigned i = 0; i < _pdom.size(); ++i) {
      out << i << " " << _pdom[i] << "\\n";
    }
    out << "\"];\n";

    out << "pof [shape=rectangle,label=\"";
    out << "pdomFrontier:\\n";
    for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii) { 
      BB* bb = *ii;
      out << bb->rpoNum() << ": "; 
      for(auto i=_pdomFrontier[bb].begin(), e=_pdomFrontier[bb].end(); i!=e; ++i) {
        BB* frontBB = (*i).first;
        BB* toBB = (*i).second;
        out << frontBB->rpoNum() <<"->"<< toBB->rpoNum()<< " ";
      }
      out << "\\n";
    }
    out << "\"];\n";

    out << "porf [shape=rectangle,label=\"";
    out << "Reverse pdomFrontier:\\n";
    for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii) { 
      BB* bb = *ii;
      if(_pdomReverseFrontier[bb].size()==0) continue;
      out << bb->rpoNum() << ": "; 
      for(auto i=_pdomReverseFrontier[bb].begin(), 
               e=_pdomReverseFrontier[bb].end(); i!=e; ++i) {
        BB* frontBB = (*i).first;
        BB* toBB = (*i).second;
        out << frontBB->rpoNum() <<"->"<< toBB->rpoNum()<< " ";
      }
      out << "\\n";
    }
    out << "\"];\n";


    LoopList::iterator il,el;
    for(il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
      LoopInfo& li = *(il->second);
  
      LoopInfo* pli = li.parentLoop();
      if(pli) {
        out << "\"loop_" << pli->loop_head()->head().first << "\"->"
            << "\"loop_" << li.loop_head()->head().first << "\";";
      }
  
      out << "\"loop_" << li.loop_head()->head().first <<  "\" [label=\"";
      out << "L" << li.id(); 
      out << " (depth = " << li.depth();
      if(li.callsRecursiveFunc()) {
        out << ",calls-rec";
      } else if(li.containsCallReturn()){
        out << ",calls-funcs";
      } else {
        out << ",pure";
      }
      if(li.cantFullyInline()) {
        out << ",cant-inline";
      } else {
        out << ",can-inline";
      }

      out << ") \\n";

      out << "ins=" << li.ninputs() << ", outs=" << li.noutputs() << ";";

      out << "\\nst<" << li.myStaticInsts() 
          << "," << li.staticInsts()
          << "," << li.inlinedStaticInsts() << ">"
          << "\\ndy<" << li.numInsts() << "," << li.totalDynamicInlinedInsts() << ">"

          << ";\\n";

      out << "BBs:"; 
      for(auto ib=li.body_begin(),eb=li.body_end();ib!=eb;++ib) {
        BB* bb = *ib;
        out << bb->rpoNum() << ", ";
      }
      out << "\\n";
     
      li.printLoopDeps(out);

      out << "Paths: ";
      for(auto ip=li.paths_begin(),ep=li.paths_end();ip!=ep;++ip) {
        int pathNum = ip->first;
        LoopInfo::BBvec& bbvec = ip->second;
        int freq = li.pathFreq(pathNum);
        out << pathNum << ": ";
  
        for(auto ib=bbvec.begin(),eb=bbvec.end();ib!=eb;++ib) {
          BB* bb = *ib;
          out << bb->rpoNum() <<" ";
        }
        out << "(" << freq << " times)\\n";
      }
  
      out << "\",";
      
      out << "fillcolor=" << gcolors[li.id() % gcolors.size()];

      out << ", style=filled, shape=box, margin=\"0.2,0.2\"];\n";     
    }


    out << "}\n";
    

}

bool canFlowTo(BB* bb, BB* to_bb,set<BB*>& seen) {
  if(to_bb == bb) {
    return true;
  }
  seen.insert(bb);
  bool canFlow = false;
  for(auto i=bb->succ_begin(),e=bb->succ_end();i!=e;++i) {
    BB* succ_bb = *i;
    if(seen.count(succ_bb) == 0 ) {
      canFlow |= canFlowTo(succ_bb,to_bb,seen);
      if(canFlow) {
        break;
      }
    }
  }
  return canFlow;
}


bool forwardDep(Op* dop, Op* op) {
  if(dop->bb() == op->bb()) { //only count forward ops
    return dop->bb_pos() < op->bb_pos();
  } else { //dop->bb() != op->bb() -- count forward bbs
    set<BB*> bbset;
    return canFlowTo(dop->bb(),op->bb(),bbset);
  }
}


void FunctionInfo::toDotFile_detailed(std::ostream& out) {
  out << "digraph GB{\n";

  out << "compound=true\n";

  out << "labelloc=\"t\";\n";
  if(_sym!=0) {
    out <<  "label=\"" << ELF_parser::demangle(_sym->name.c_str()) << "\";\n";
  } else {
    out <<  "label=\"Func Name Not Known\";\n";
  }


  BBMap::iterator bbi,bbe;
  for(bbi=_bbMap.begin(),bbe=_bbMap.end();bbi!=bbe;++bbi) {
    BB& bb = *(bbi->second);
    out << "subgraph"
        << "\"cluster_" << bb.head().first << "x" << bb.head().second << "\"{" 
   
        << "label=\"BB " << bb.rpoNum()
      //                 << "\\n" << bb.head().first << "_" << bb.tail().first
                       << "\"\n";

    out << "style=\"filled,rounded\"\n";
    out << "color=lightgrey\n";

    BB::OpVec::iterator oi,oe;
    int i;
    for(oi=bb.op_begin(),oe=bb.op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;
      out << "\"" << op->cpc().first << "x" << op->cpc().second << "\" "
          << "[";

      out << op->dotty_name_and_tooltip();
          
      out << ",style=filled, color=white]\n";
    }
    out << "}\n";
 
    

   //Iterate through Ops
    for(oi=bb.op_begin(),oe=bb.op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;

      //out << "\"" << op->cpc().first << "x" << op->cpc().second << "\" "
      //    << "[label=\"op" << i << "\" style=filled, color=white]\n";


/*
      if(op!=bb.lastOp() && !bb.lastOp()->dependsOn(op)) {
      out << "\"" << op->cpc().first << "x" << op->cpc().second << "\""
          << " -> "
          << "\"" << bb.lastOp()->cpc().first << "x" << bb.lastOp()->cpc().second 
          << "\"[weight=0.0, constraint=false, style=\"invis\"]\n";
      }

      if(op!=bb.firstOp() && !op->dependsOn(bb.firstOp()) ) {
      out << "\"" << bb.firstOp()->cpc().first << "x" << bb.firstOp()->cpc().second 
          << "\"" << " -> "
          << "\"" << op->cpc().first << "x" << op->cpc().second 
          << "\"[weight=0.0, constraint=false, style=\"invis\"]\n";
      }
*/

      for(auto di=op->d_begin(),de=op->d_end();di!=de;++di) {
        Op* dep_op = *di;
        if(dep_op->func()==op->func()) {
          out << "\"" << dep_op->cpc().first << "x" << dep_op->cpc().second << "\""
              << " -> "
              << "\"" << op->cpc().first << "x" << op->cpc().second 
              << "\"[";
          if(!forwardDep(dep_op,op)) {
            out << "constraint=false color=red ";
          }

          out << "weight=0.5]\n";
        } else {
          //input dependences
        }
      }
    }   


    BB::BBvec::iterator si,se;
    BB::IntEdge::iterator ii,ie;
    for(si=bb.succ_begin(),  se=bb.succ_end(); si!=se; ++si) {
      BB* succ_bb = *si;
      if(bb.len() > 0 && succ_bb->len() > 0) {
        out << "\"" << bb.lastOp()->cpc().first << "x" <<  bb.lastOp()->cpc().second
            << "\"->"
            << "\"" << succ_bb->firstOp()->cpc().first << "x" 
            << succ_bb->firstOp()->cpc().second  << "\" [";
      }
      
      out << "ltail=\"cluster_" << bb.head().first << "x" 
                        << bb.head().second << "\" ";
      out << "lhead=\"cluster_" << succ_bb->head().first << "x" 
                        << succ_bb->head().second << "\" ";

      out << "arrowhead=open arrowsize=1.5 weight=1 penwidth=4 color=blue ];\n"; //minlen=2 : (


      /*
      BB::OpVec::iterator oi,oe;
      for(oi=succ_bb->op_begin(),oe=succ_bb->op_end(),i=0;oi!=oe;++oi,++i) {
        Op* succ_op = *oi;
 
        out << "\"" << bb.lastOp()->cpc().first << "x" <<  bb.lastOp()->cpc().second
            << "\"->"
            << "\"" << succ_op->cpc().first << "x" 
                  << succ_op->cpc().second  << "\" [label=\"";

        out << "\" ";
        
        out << "ltail=\"cluster_" << bb.head().first << "x" 
                          << bb.head().second << "\" ";
        out << "lhead=\"cluster_" << succ_bb->head().first << "x" 
                          << succ_bb->head().second << "\" ";       
        out << "weight=1 style=\"invis\"];\n";
      }
      */
      
    }

  }


  LoopList::iterator il,el;
  for(il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
    LoopInfo& li = *(il->second);

    LoopInfo* pli = li.parentLoop();
    if(pli) {
      out << "\"loop_" << pli->loop_head()->head().first << "\"->"
          << "\"loop_" << li.loop_head()->head().first << "\";";
    }


    out << "\"loop_" << li.loop_head()->head().first <<  "\" [fillcolor=pink,label=\"";
    out << "depth = " << li.depth() << ";\\n";
    LoopInfo::BBset::iterator ib,eb;
    for(ib=li.body_begin(),eb=li.body_end();ib!=eb;++ib) {
      BB* bb = *ib;
      out << bb->rpoNum() << ", ";
    }
    out << "\\n";
    
    LoopInfo::PathMap::iterator ip,ep;
    for(ip=li.paths_begin(),ep=li.paths_end();ip!=ep;++ip) {
      int pathNum = ip->first;
      LoopInfo::BBvec& bbvec = ip->second;
      int freq = li.pathFreq(pathNum);
      out << pathNum << ": ";

      LoopInfo::BBvec::iterator ib,eb;
      for(ib=bbvec.begin(),eb=bbvec.end();ib!=eb;++ib) {
        BB* bb = *ib;
        out << bb->rpoNum() <<" ";
      }
      out << "(" << freq << " times)\\n";
    }

    out << "\"];\n";     
  }


  out << "}\n";

}


  

void FunctionInfo::toDotFile_record(std::ostream& out) {
  out << "digraph GB{\n";
  out << "rankdir=\"LR\"\n";

  out << "labelloc=\"t\";\n";
  if(_sym!=0) {
    out <<  "label=\"" << ELF_parser::demangle(_sym->name.c_str()) << "\";\n";
  } else {
    out <<  "label=\"Func Name Not Known\";\n";
  }

  BBMap::iterator bbi,bbe;
  for(bbi=_bbMap.begin(),bbe=_bbMap.end();bbi!=bbe;++bbi) {
    BB& bb = *(bbi->second);

    //Find loop with Smallest size which contains BB
    LoopInfo* loop_for_bb=innermostLoopFor(&bb);

    out << "bb" << bb.head().first << "x" << bb.head().second << " [" 
   
        << "label=\" <begin> BB " << bb.rpoNum();
      //                 << "\\n" << bb.head().first << "_" << bb.tail().first
     //                  << "\n";

    //Iterate through Ops

    BB::OpVec::iterator oi,oe;
    int i;
    for(oi=bb.op_begin(),oe=bb.op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;

      assert(i == op->bb_pos());

      out << "| <" << i << ">";
      out << op->dotty_name();

/*        << op->id() << ":";

      //print node name
      if(op->isLoad()) {
        out << "ld";
      } else if(op->isStore()) {
        out << "st";
      } else if(op->isCall()) {
        out << "call";
      } else if(op->isReturn()) {
        out << "ret";
      } else if(op->isCtrl()) {
        out << "ctrl";
      }*/

      if(op->isMem()&&loop_for_bb) {
        if(loop_for_bb->isStriding(op)) {
          out << " x" << loop_for_bb->stride(op);
        } else {
          out << " x?";
        }
      }


      out << " (";

      Op::Deps::iterator di,de;
      for(di=op->d_begin(),de=op->d_end();di!=de;++di) {
        Op* dep_op = *di;
        if(dep_op->bb()==op->bb()) {
          out << dep_op->id() << " ";
          //input dependences
        }
      }
      out << ")";
    }
    
    out << "\" shape=\"record\"]\n";
    

    for(oi=bb.op_begin(),oe=bb.op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;

      Op::Deps::iterator di,de;
      for(di=op->d_begin(),de=op->d_end();di!=de;++di) {
        Op* dep_op = *di;
        if(op->bb() != dep_op->bb() && dep_op->func()==op->func()) {
          out << "bb" << dep_op->bb()->head().first  << "x" 
                        << dep_op->bb()->head().second << ":" << dep_op->bb_pos() << ""
              << " -> "
              << "bb" << op->bb()->head().first << "x" 
                        << op->bb()->head().second << ":" << op->bb_pos() << ""

              << " [";
          out << "weight=0.5]\n";
        } 
      }
    }
    
    BB::BBvec::iterator si,se;
    BB::IntEdge::iterator ii,ie;
    for(si=bb.succ_begin(),  se=bb.succ_end(); si!=se; ++si) {
      BB* succ_bb = *si;

      out << "bb" << bb.head().first << "x" << bb.head().second << " "
          << " -> "
          << "bb" << succ_bb->head().first << "x" 
                  << succ_bb->head().second << " [";

      out << "arrowhead=open arrowsize=1.5 weight=1 penwidth=4 color=blue minlen=2];\n";

    }

  }

  LoopList::iterator il,el;
  for(il=_loopList.begin(),el=_loopList.end();il!=el;++il) {
    LoopInfo& li = *il->second;

    LoopInfo* pli = li.parentLoop();
    if(pli) {
      out << "\"loop_" << pli->loop_head()->head().first << "\"->"
          << "\"loop_" << li.loop_head()->head().first << "\";";
    }

    out << "\"loop_" << li.loop_head()->head().first <<  "\" [fillcolor=pink,label=\"";
    out << "depth = " << li.depth() << ";\\n";
    LoopInfo::BBset::iterator ib,eb;
    for(ib=li.body_begin(),eb=li.body_end();ib!=eb;++ib) {
      BB* bb = *ib;
      out << bb->rpoNum() << ", ";
    }
    out << "\\n";

    li.printLoopDeps(out);

    LoopInfo::PathMap::iterator ip,ep;
    for(ip=li.paths_begin(),ep=li.paths_end();ip!=ep;++ip) {
      int pathNum = ip->first;
      LoopInfo::BBvec& bbvec = ip->second;
      int freq = li.pathFreq(pathNum);
      out << pathNum << ": ";

      LoopInfo::BBvec::iterator ib,eb;
      for(ib=bbvec.begin(),eb=bbvec.end();ib!=eb;++ib) {
        BB* bb = *ib;
        out << bb->rpoNum() <<" ";
      }
      out << "(" << freq << " times)\\n";
    }

    out << "\"];\n";     
  }


  out << "}\n";

}



