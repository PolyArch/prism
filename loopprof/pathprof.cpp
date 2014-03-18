#include "pathprof.hh"
#include <iostream>
#include <map>
#include <unordered_set>
using namespace std;

// -------------------------------- StackFrame ---------------------------------

Op* StackFrame::processOp_phase3(uint32_t dId, CPC cpc) {
  BB* newBB = _funcInfo->getBB(cpc);

  if(newBB==NULL && _prevBB==NULL) {
    return NULL;
  }

  if(newBB!=NULL) {
    processBB_phase2(dId, newBB,false); 
    //intentionally using phase 2, but do not profile again,
    //b/c this info is already saved.
    _prevBB=newBB;
  }
  Op* op = _prevBB->getOp(cpc);
  return op;
}

Op* StackFrame::getOp(CPC cpc) {
  if(_prevBB) {
    return _prevBB->getOp(cpc);
  } else {
    return NULL;
  }
}


std::unordered_set<std::pair<BB*,BB*>> error_pairs;

//Process op, determine if it is a bb or not
Op* StackFrame::processOp_phase2(uint32_t dId, CPC cpc, 
                                 uint64_t addr, uint8_t acc_size) {
  //get the bb for newCPC -- if this isn't a basic block head, return;
  BB* newBB = _funcInfo->getBB(cpc);
  
  
  //if I'm not in a basic block, then just get out... this only happens
  //during initializiation
  if(newBB==NULL && _prevBB==NULL) {
    return NULL;
  }


  if(newBB!=NULL) {
    //cout << "BB" << newBB->rpoNum() 
    //  << " (" << newBB->head().first << "." << newBB->head().second << ")"
    //  << "\n";

    //cout << "\nBB" << newBB->rpoNum();
    //cout << "\nBB" << newBB->head().first << "." << newBB->head().second;

    processBB_phase2(dId, newBB);

    //Check if the CFG is consistent
    //Can turn this off if we need to go faster.
    if(_prevBB!=NULL) {
      bool succfound=false;
      for(auto i = _prevBB->succ_begin(), e = _prevBB->succ_end(); i!=e; ++i) {
        if(*i == newBB) {
          if(succfound) {
            assert("double find" && 0);
          }
          succfound=true;
        }
      }
      bool predfound=false;
      for(auto i = newBB->pred_begin(), e = newBB->pred_end(); i!=e; ++i) {
        if(*i == _prevBB) {
          if(predfound) {
            assert("double find" && 0);
          }
          predfound=true;
        }
      }

      if(!succfound || !predfound) {
        auto pair = make_pair(_prevBB,newBB);

        if(error_pairs.count(pair)==0) {
          error_pairs.insert(pair);
          cout << "HUGE PROBLEM -- EDGE NOT FOUND   (";
          if(!succfound) {
            cout << "no succ, ";
          }
          if(!predfound) {
            cout << "no pred, ";
          }

          cout << _prevBB->rpoNum() << "->" << newBB->rpoNum() 
               << ") dId=" << dId <<" \n";

          cout << "*****BB1:\n";
          _prevBB->print();
          cout << "*****BB2:\n";
          newBB->print();
        }
        //assert(0);
      }

      _prevBB->succCount[newBB]+=1;
    }

    _prevBB=newBB;
    //cout << "setting prevBB to " << _prevBB->rpoNum() << "dId: " <<  dId << "\n";

    newBB->incFreq();
  }
  
  
  Op* op = _prevBB->getOp(cpc);

  //cout << "OP" << " (" << cpc.first << "." << cpc.second << ")"
  //    << (op->isCtrl()?"   ctrl":"") << "\n";


  op->setBB(_prevBB);
  if(op->isMem() && _loopStack.size()>0) {
    _loopStack.back()->opAddr(op,addr,acc_size);

    /*
    cout << "iter" << _loopStack.back()->curIter() << ": ";
    if(op->isLoad()) {
      cout << "load: \t" << addr;
    } else {
      cout << "store:\t" << addr;
    }
    //cout << "\n";
    */
  }

  //What loop did this get executed in?
  if(_loopStack.size() > 0) {
    _loopStack.back()->incInstr();
  }

  if(op->isStore()) {
    giganticMemDepTable[addr].dId=dId;
  }
  if(op->isLoad()) {
    uint32_t dep_dId = giganticMemDepTable[addr].dId;
    dyn_dep(op,dep_dId,true);
  }


/*
  if(_loopStack.size()>0) {
    //cout << _iterStack.back()->relevantLoopIter();
    LoopIter* curIter = _iterStack.back().get();

    std::map<uint32_t,std::shared_ptr<LoopIter>>::iterator i;
    i = --_iterMap.upper_bound(dId);

    if(_iterStack.size()>0) {
      cout << curIter->relevantLoopIter();
    } else {
      cout << "?";
    }
    cout << ", ";
    if(i != _iterMap.end()) { 
      LoopIter* depIter = i->second.get();
      cout << depIter->relevantLoopIter();
    } else {
      cout <<"?";
    }
    cout  << "\n";
  }
*/
  //return or create Op in BB
  return op;
}


//Determines current loop nests, and can profile iterations
void StackFrame::processBB_phase2(uint32_t dId, BB* bb, bool profile) {
  bool stackChanged = false;

  //check if we need to iter complete
  if(profile) {
    if(_loopStack.size() > 0 && _loopStack.back()->loop_head() == bb) {
      _loopStack.back()->iterComplete(_pathIndex,_loopPath);
      //cout<<"iter complete\n";
    }
  }
 
  //now update the loop stack
  while(_loopStack.size()>0&&!_loopStack.back()->inLoop(bb)) {
    if(profile) {
      _loopStack.back()->endLoop();
    }
    _loopStack.pop_back();
    _iterStack.pop_back();  //get rid of iter too
    stackChanged=true;
  }
 
  //now check if we need to push a loop
  if(LoopInfo* li = _funcInfo->getLoop(bb)) {
    //make sure we didn't loop back!
    if(_loopStack.size() != 0 && _loopStack.back() == li) {
      _iterStack.pop_back();
    } else {
      _loopStack.push_back(li);
      if(profile) {
        li->beginLoop();
      }
    }
    _iterStack.emplace_back(new LoopIter(_loopStack)); //this gets deconstructed in itermap, don't worry!
    stackChanged=true;   
  }

  if(stackChanged) {
    //these aren't stack-ified yet because they don't work on non-inner loops anyways
    _loopPath.clear();
    _pathIndex=0; //going inside the loop should clear the path index
    if(profile) {
      _iterMap.emplace(std::piecewise_construct,
                       forward_as_tuple(dId),
                       forward_as_tuple(_iterStack.back())); 
    }
  } else if( _loopStack.size() > 0) { //loop stack didn't change
    _pathIndex+=_loopStack.back()->weightOf(_loopPath.back(),bb);
  }

  //update path
  _loopPath.push_back(bb);

  // never seen any loop depths this big -- relax if not true
  assert(_loopStack.size() < 20); 
  if(_loopStack.size() +1 != _iterStack.size()) {
    cout << "LoopStack Size: " << _loopStack.size() << "  "
         << "IterStack Size: " << _iterStack.size() << "\n";
    assert(_loopStack.size() +1 == _iterStack.size());
  }
  
}



#if 0
//Determines current loop nests, and can profile iterations
void StackFrame::processBB_phase2(uint32_t dId, BB* bb, bool profile) {
  LoopInfo* curLoop = NULL;
  if(_loopStack.size() > 0) {
    curLoop = _loopStack.back();
  }

  /*cout << bb->func()->nice_name() << ":  ";
  cout << bb->rpoNum();
  cout << " (" << _iterStack.size() << " " << _loopStack.size() << ")\n";*/


  //Did we get a new loop head?
  if(LoopInfo* li = _funcInfo->getLoop(bb)) {
    //Different Loop?
    if(curLoop!=li) { 
      //cout << curLoop << " " << li << "\n";
      if(curLoop && li->parentLoop()!=curLoop) {
         //cout << "back to back ";

        //when loops are butted up next to eachother, this shoud happen rarely
        if(profile) {
          _loopStack.back()->iterComplete(_pathIndex,_loopPath);
          curLoop->endLoop();
        }
        _loopStack.pop_back();
        _iterStack.pop_back();

        assert(li->parentLoop()==NULL || 
               li->parentLoop()==_loopStack.back()); //make sure we have right loop
      }
      /*cout << "inner loop! (";
      cout << li->loop_head()->rpoNum() << ": ";
      for(auto i = li->body_begin(), e = li->body_end();i!=e;++i) {
        cout << (*i)->rpoNum() << " " ;
      }
      cout << ")\n";*/

      //starting new loop, push it onto our stack
      
      _loopStack.push_back(li);
      _iterStack.emplace_back(new LoopIter(_loopStack)); //this gets deconstructed in itermap, don't worry!
      if(profile) {
        li->beginLoop();
      }


      _iterMap.emplace(std::piecewise_construct,
                       forward_as_tuple(dId),
                       forward_as_tuple(_iterStack.back())); 
                       //shared ptr now owns loopiter object
      _loopPath.clear();
      _pathIndex=0; //going inside the loop should clear the path index

    } else {
      //cout << " same loop! \n";
      //Same Loop as top of stack, so all is good, but need to make new iter marker
      if(profile) {
        _loopStack.back()->iterComplete(_pathIndex,_loopPath);
      }
      _loopPath.clear();
      _pathIndex=0;
      _iterStack.pop_back();
      _iterStack.emplace_back(new LoopIter(_loopStack)); 
      _iterMap.emplace(std::piecewise_construct,
                       forward_as_tuple(dId),
                       forward_as_tuple(_iterStack.back()));
    }

  } else if(curLoop!=NULL) {
    //_pathIndex+=//_loopPath.back()->weightOf(bb);

    //If we are no longer in our loop, roll back stack
    if(!curLoop->inLoop(bb)) {
      //cout << "exit loop! \n";
      //KNOWN_HOLE -- does not count loop exit paths

      while(_loopStack.size()>0&&!_loopStack.back()->inLoop(bb)) {
        if(profile) {
          _loopStack.back()->endLoop();
        }
        _loopStack.pop_back();
        _iterStack.pop_back();  //get rid of iter too
      }
      //place new marker
      _iterMap.emplace(std::piecewise_construct,
                         forward_as_tuple(dId),
                         forward_as_tuple(_iterStack.back()));
      _loopPath.clear();
      _pathIndex=0;
    }

    if(_loopStack.size()>0) {
      if(_loopPath.size()>0) {
        _pathIndex+=_loopStack.back()->weightOf(_loopPath.back(),bb);
      }
    }
  }

  assert(_loopStack.size() < 20); // never seen any loop depths this big -- relax if not true

  if(_loopStack.size()>0) {
    _loopPath.push_back(bb);
/*    if(_loopStack.back()->isLatch(bb)) {
       _loopStack.back()->iterComplete(_pathIndex,_loopPath);
       _loopPath.clear();
       _pathIndex=0;
       return;
     }*/
  }

}
#endif

void StackFrame::dyn_dep(Op* op, uint32_t dId, bool isMem) {
  if(_loopStack.size()==0) {
    return; // leave if we are not dealing with a loop memory dep
  }

  if(!isMem) {
    return; // for now, just get rid of non-memory loop deps
  }

  LoopIter* curIter = _iterStack.back().get();
  //get the iteration this corresponds to
  LoopIter* depIter = (--_iterMap.upper_bound(dId))->second.get();

  bool theSameLoops=curIter->psize()==depIter->psize();
  LoopInfo::LoopDep relDep;
  //Check if loops in question are the same (zippered iterator)
  LoopIter::IterPos::iterator i1,i2,e1,e2;
  for(i1=curIter->pbegin(),i2=depIter->pbegin(),e1=curIter->pend(),e2=depIter->pend();
      theSameLoops && i1!=e1 && i2!=e2; ++i1, ++i2) {
      theSameLoops &= i1->first == i2->first;
      relDep.push_back(i2->second - i1->second);
  }
  
  if(theSameLoops) {
    curIter->relevantLoop()->addRelativeLoopDep(relDep);
  } else {
    //More complex dependency, must handle this seperately
    //This shouldn't interfere with anything though.
  }
}


// -------------------------------- PathProf -----------------------------------

FunctionInfo*  PathProf::getOrAddFunc(CPC newCPC) {
    FuncMap::iterator funcInfoIter = _funcMap.find(newCPC);

    if(funcInfoIter == _funcMap.end()) {
      funcInfoIter = _funcMap.emplace(newCPC,new FunctionInfo(newCPC)).first;

      if(sym_tab.size() > 0) {
        prof_symbol* sym= &((--sym_tab.upper_bound(newCPC.first))->second);//.get()
        if(sym->isFunc()) {
          funcInfoIter->second->setSymbol(sym);
        }
      }
    }
    return funcInfoIter->second;
}

//Checks recursion on top of stack, and tells top stackframe
void PathProf::checkRecursion() {
   if(_callStack.size() < 2) {
     return;  
   }

   auto iter = _callStack.rbegin();
   StackFrame& top = *iter;
   ++iter; //skip this guy before starting search

   StackFrame& second = *iter;

   //if the same, direct recursing
   if(top.funcInfo() == second.funcInfo()) {
     top.setDirectRecursing(true);
   }

   //the previousis recursing, so am i
   if(second.isRecursing()) {
     top.setRecursing(true);
   }


   bool found=false;
   for(auto end_iter = _callStack.rend();iter!=end_iter;++iter) {
     StackFrame& next = *iter;
     if(next.funcInfo() == top.funcInfo()) {
        top.setRecursing(true);
        found=true;
        break;
     }
   }

   if(found) {
     //std::cout << "marking recursion at \"" << iter->funcInfo()->nice_name() << "\"\n";
     auto fwd_iter = (++iter).base();
     for(auto end_iter = _callStack.end(); fwd_iter!=end_iter;++fwd_iter) {
     //std::cout << " --> " << fwd_iter->funcInfo()->nice_name() << "\n";

       StackFrame& next = *fwd_iter;
       next.funcInfo()->setCanRecurse(true);
     }
   }

   return;
}



bool PathProf::adjustStack(CPC newCPC, bool isCall, bool isRet ) {
  if(isCall) {
    //If call, add function if it's not there, and add an item to the call stack
    FunctionInfo* funcInfo = getOrAddFunc(newCPC);
    _callStack.emplace_back(funcInfo,_dId);
    checkRecursion(); 
    return true;
  }

  if(isRet) {
    _callStack.pop_back();
    assert(_callStack.size()>0);
    return true;
  }
  return false;
}


void PathProf::processOpPhase1(CPC prevCPC, CPC newCPC, bool isCall, bool isRet)
{
  FunctionInfo* call_fi=NULL;

  if(_prevHead.first!=0&&prevCPC.first!=0) {
    call_fi=_callStack.back().funcInfo();
    _callStack.back().processBB_phase1(_prevHead,prevCPC); 
  }
  adjustStack(newCPC,isCall,isRet);

  /* //print the stack 
  if(isCall || isRet) {
    for(auto i = _callStack.begin(), e= _callStack.end();i!=e;++i) {
      FunctionInfo* fi = i->funcInfo();
      cout << fi->nice_name() << ",";
    }
    cout << "\n";
  }*/


  if(isCall) {
    _callStack.back().funcInfo()->got_called();
    if(call_fi) {
      _callStack.back().funcInfo()->calledBy(call_fi);
    }
  } else if(isRet) {
    if(call_fi) {
      call_fi->calledBy(_callStack.back().funcInfo());
    }
  }

  _prevHead=newCPC;
}

void PathProf::runAnalysis() {
  //force the last bb just in case
  _callStack.back().processBB_phase1(_prevHead,std::make_pair(-1,-1)); 


  FuncMap::iterator i,e;
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    if(fi.nBBs() != 0) {
      //order of functions is very important : )
      fi.ascertainBBs();
      fi.calculateRPO(); 
      fi.calculateDOM();
      fi.detectLoops();
      fi.loopNestAnalysis();
      FunctionInfo::LoopList::iterator li,le;
      for(li=fi.li_begin(),le=fi.li_end();li!=le;++li) {
        LoopInfo* loopInfo = li->second;
        loopInfo->build_rpo();
        if(loopInfo->isInnerLoop()) {
          loopInfo->initializePathInfo();
        }
      }
    }
  }
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    if(fi.nBBs() != 0) {
      fi.propogateCallsRecursiveFunc(false);
    }
  }
}

void PathProf::runAnalysis2(bool no_gams, bool gams_details, bool size_based_cfus) {

  std::multimap<uint64_t,LoopInfo*> loops;
 
  FuncMap::iterator i,e;
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    FunctionInfo::LoopList::iterator li,le;

    // loop calls recursive func an
    //fi->figureOutIfLoopsCallRecursiveFunctions();

    for(li=fi.li_begin(),le=fi.li_end();li!=le;++li) {
      LoopInfo* loopInfo = li->second;
      loops.insert(std::make_pair(loopInfo->numInsts(),loopInfo));
    }
  } 

  std::multimap<uint64_t,LoopInfo*>::reverse_iterator I;
  for(I=loops.rbegin();I!=loops.rend();++I) {
    LoopInfo* loopInfo = I->second;
    /*
    cout << "loopinfo: " << loopInfo->id() << " ";
    cout << loopInfo->getLoopBackRatio(loopInfo->getHotPathIndex()) << " ";
    cout << loopInfo->getTotalIters() << "\n"; */

    //generate this only for 
    //1. Inner Loops
    //2. >50% Loop-Back
    //3. Executed >= 10 Times

    int hpi = loopInfo->getHotPathIndex();
    cerr << "func: " << loopInfo->func()->nice_name() 
         << "(" << loopInfo->func()->id() << ")"
         << " loop: " << loopInfo->id()
         << "(depth:" << loopInfo->depth() << " hpi:" << hpi
         << "hp_len: " << loopInfo->instsOnPath(hpi)  
         << (loopInfo->isInnerLoop() ? " inner " : " outer ")
         << " lbr:" << loopInfo->getLoopBackRatio(hpi)
         << " iters:" << loopInfo->getTotalIters()
         << " insts:" << loopInfo->numInsts()
         << ")";

    bool worked=false;

    // BERET Scheduling
    if(loopInfo->isInnerLoop()
       && hpi != -2 //no hot path
       && loopInfo->getLoopBackRatio(hpi) >= 0.7
       && loopInfo->getTotalIters() >= 10
       ) {
      stringstream part_gams_str;
      part_gams_str << "partition." << loopInfo->id() << ".gams";

      if(size_based_cfus) {
        worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
                   NULL,
                   gams_details,no_gams);
      } else {
        worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
                   &_beret_cfus,
                   gams_details,no_gams);
      }
      if(worked) {
        cerr << " -- Beretized\n";
      } else {
        cerr << " -- NOT Beretized (Func Calls?)\n";
      }
    } else {
      cerr << " -- NOT Beretized\n";
    }

    //NLA Scheduling
//    if(!loopInfo->cantFullyInline()) {
    if(!loopInfo->containsCallReturn()) {
       if(size_based_cfus) {
         worked = loopInfo->scheduleNLA(NULL, gams_details, no_gams);
       } else {
         worked = loopInfo->scheduleNLA(&_beret_cfus, gams_details, no_gams);
       }

      if(worked) {
        cerr << " -- NLA'D\n";
      } else {
        cerr << " -- NOT NLA'd\n";
      }    
    }

    //update stats
    if(worked) {
      insts_in_beret += loopInfo->dynamicInstsOnPath(hpi); 
    }
    if(loopInfo->isInnerLoop() && !loopInfo->containsCallReturn()) {
      insts_in_simple_inner_loop += loopInfo->numInsts();
    }
    if(loopInfo->isInnerLoop()) {
      insts_in_inner_loop += loopInfo->numInsts();
    }

    insts_in_all_loops += loopInfo->numInsts(); 
  }

  std::multimap<uint64_t,FunctionInfo*> funcs;
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    funcs.insert(std::make_pair(fi.nonLoopInsts(),&fi));
    
    non_loop_insts_direct_recursion += fi.nonLoopDirectRecInsts();
    non_loop_insts_any_recursion += fi.nonLoopAnyRecInsts();
  }
  
  cerr << "Funcs by non-loop insts:\n";
  for(auto I = funcs.rbegin(), E = funcs.rend(); I!=E; ++I) {
    FunctionInfo* fi = I->second;
    cerr << fi->nice_name() << " (" << fi->id() << "): " << fi->nonLoopInsts() << "\n";
    
    //don't bother printing the small functions 
    if(fi->nonLoopInsts() < 10000) {
      break;    
    }
  }

}


void PathProf::processOpPhase2(CPC prevCPC, CPC newCPC, bool isCall, bool isRet, CP_NodeDiskImage& img) {

  FunctionInfo* call_fi = NULL;
  Op* call_op = NULL;

  if(isCall && _callStack.size()>0) {
    call_op = _callStack.back().getOp(prevCPC);
    call_fi = _callStack.back().funcInfo();
  }

  adjustStack(newCPC,isCall,isRet);
  /*if(isChanged) {
    //cout << "WOOO!\n";
    FunctionInfo* fi = _callStack.back().funcInfo();
    cout << fi->firstBB()->head().first << " " << fi->firstBB()->head().second << ", ";
  }*/
  StackFrame& sf = _callStack.back();
  Op* op = sf.processOp_phase2(_dId,newCPC,img._eff_addr,img._acc_size);
  if(op==NULL) {
    return;
  }
  op->setImg(img);

  _op_buf[_dId%MAX_OPS]=op;

  if(isCall && call_fi && call_op) {
    sf.funcInfo()->calledByOp(call_fi,call_op);
    call_op->setCalledFunc(sf.funcInfo());
  }

  //check inst/execution statistics
  sf.funcInfo()->incInsts(sf.isLooping(),
                          sf.isDirectRecursing(),sf.isRecursing());

  //Check if op is new
  if (op->cpc().first == 0) {
    op->setCPC(newCPC);
    op->setIsLoad(img._isload);
    op->setIsStore(img._isstore);
    op->setIsCtrl(img._isctrl);
    op->setIsCall(img._iscall);
    op->setIsReturn(img._isreturn);
  }

  //cout << "," << op->cpc().first << "." << op->cpc().second;
  //cout << "," << op->id();


  op->executed(img._cc-img._ec);
  op->setOpclass(img._opclass);
  //We must add in all the dependencies
  for(int i = 0; i < 7; ++i) {
    int dep_ind = img._prod[i];
    if(dep_ind==0) {
      break;
    }
    uint64_t full_ind = _dId -dep_ind;
    if(dep_ind<=_dId) {
      op->addDep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS]);
      sf.dyn_dep(op,full_ind,false);
    }
  }

  if (img._mem_prod != 0) {
    uint64_t full_ind = _dId - img._mem_prod;
    if (img._mem_prod <= _dId) {
      op->addMemDep(_op_buf[(full_ind + MAX_OPS) % MAX_OPS]);
    }
  }

  if(img._cache_prod!=0) {
    uint64_t full_ind = _dId - img._cache_prod;
    if(img._cache_prod <= _dId) {
      op->addCacheDep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS]);
    }
  }
  if (op->isLoad() || op->isStore()) {
    int iterNum = sf.getLoopIterNum();
    if (iterNum == 0) {
      op->initEffAddr(img._eff_addr, img._acc_size, iterNum);
    } else if (iterNum > 0) {
      op->computeStride(img._eff_addr, iterNum);
    }
  }

/*
  if(op->isLoad()) {
    uint32_t store_dId = sf.getStoreFor(img._eff_addr);

    if(store_dId != 0) {
      sf.dyn_dep(op,_dId-img._mem_prod,true);
    }
  }
*/

  _dId++;
}

Op* PathProf::processOpPhase3(CPC newCPC, bool wasCalled, bool wasReturned){
  adjustStack(newCPC,wasCalled,wasReturned);
  StackFrame& sf = _callStack.back();
  Op* op = sf.processOp_phase3(_dId,newCPC);

  if(op==NULL) {
    return NULL;
  }

  //  _op_buf[_dId%MAX_OPS]=op;
  _dId++;
  return op;
}


void PathProf::printInfo() {
  FuncMap::iterator i,e;
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    CPC cpc = i->first;
    FunctionInfo& fi = *i->second;
    cout << "function at" << cpc.first << " " << cpc.second << "\n";
    FunctionInfo::BBMap::iterator ib,eb;
    for(ib=fi.bb_begin(),eb=fi.bb_end();ib!=eb;++ib) {
      BB& bb= *(ib->second);
      cout << "\tbasic block\t" << bb.head().first << " " << bb.head().second 
           << "(" << bb.freq() << ")\n";
      
    }
  } 
}


void PathProf::procConfigFile(const char* filename) {
  std::string line,tag,val;
  std::ifstream ifs(filename);

  if(!ifs.good()) {
    std::cerr << filename << " doesn't look good";
    return;
  }

  while(std::getline(ifs, line)) {
    if(line.find("[system]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("cache_line_size",tag,val,cache_line_size);
        }
      } 
    } else if(line.find("[system.cpu.dcache]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("assoc",tag,val,dcache_assoc, true);
          if (!getStat("hit_latency",tag,val,dcache_hit_latency)) {
            if (getStat("latency", tag, val, dcache_hit_latency, true)) {
              dcache_hit_latency /= 500;
              if (dcache_hit_latency < 0)
                dcache_hit_latency = 1;
            }
          }
          getStat("mshrs",tag,val,dcache_mshrs, true);
          getStat("response_latency",tag,val, dcache_response_latency, true);
          getStat("size",tag,val,dcache_size, true);
          getStat("tgts_per_mshr",tag,val,dcache_tgts_per_mshr, true);
          getStat("write_buffers",tag,val,dcache_write_buffers, true);
          if (cache_line_size == 0) {
            cache_line_size = 64; // override for old gem5
          }
        }
      } 
    } else if(line.find("[system.cpu.icache]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("assoc",tag,val,icache_assoc, true);
          if (!getStat("hit_latency",tag,val,icache_hit_latency, true)) {
            if (getStat("latency",tag, val, icache_hit_latency, true)) {
              icache_hit_latency /= 500;
              if (icache_hit_latency == 0)
                icache_hit_latency = 1;
            }
          }
          getStat("mshrs",tag,val,icache_mshrs, true);
          getStat("response_latency",tag,val,icache_response_latency, true);
          getStat("size",tag,val,icache_size, true);
          getStat("tgts_per_mshr",tag,val,icache_tgts_per_mshr, true);
          getStat("write_buffers",tag,val,icache_write_buffers, true);
        }
      } 
    } else if(line.find("[system.l2]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("assoc",tag,val,l2_assoc, true);
          if (!getStat("hit_latency",tag,val,l2_hit_latency, true)) {
            if (getStat("latency", tag, val, l2_hit_latency, true)) {
              l2_hit_latency /= 500;
              if (l2_hit_latency == 0)
                l2_hit_latency = 1;
            }
          }
          getStat("mshrs",tag,val,l2_mshrs, true);
          getStat("response_latency",tag,val,l2_response_latency, true);
          getStat("size",tag,val,l2_size, true);
          getStat("tgts_per_mshr",tag,val,l2_tgts_per_mshr, true);
          getStat("write_buffers",tag,val,l2_write_buffers, true);
        }
      } 
    } else if(line.find("[system.switch_cpus]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("LQEntries",tag,val,LQEntries);
          getStat("LSQDepCheckShift",tag,val,LSQDepCheckShift);
          getStat("SQEntries",tag,val,SQEntries);
          getStat("cachePorts",tag,val,cachePorts);
          getStat("commitToDecodeDelay",tag,val,commitToDecodeDelay);
          getStat("commitToFetchDelay",tag,val,commitToFetchDelay);
          getStat("fetchWidth",tag,val,fetchWidth);
          getStat("decodeWidth",tag,val,decodeWidth);
          getStat("dispatchWidth",tag,val,dispatchWidth);
          getStat("renameWidth",tag,val,renameWidth);
          getStat("issueWidth",tag,val,issueWidth);
          getStat("commitWidth",tag,val,commitWidth);
          getStat("squashWidth",tag,val,squashWidth);
          getStat("decodeToFetchDelay",tag,val,decodeToFetchDelay);
          getStat("decodeTorenameDelay",tag,val,decodeToRenameDelay);
          getStat("fetchToDecodeDelay",tag,val,fetchToDecodeDelay);
          getStat("fetchTrapLatency",tag,val,fetchTrapLatency);
          getStat("iewToDecodeDelay",tag,val,iewToDecodeDelay);
          getStat("iewToFetchDelay",tag,val,iewToFetchDelay);
          getStat("iewToRenameDelay",tag,val,iewToRenameDelay);
          getStat("issueToExecuteDelay",tag,val,issueToExecuteDelay);
          getStat("needsTSO",tag,val,needsTSO);
          getStat("numIQEntries",tag,val,numIQEntries);
          getStat("numROBEntries",tag,val,numROBEntries);
          getStat("numPhysFloatRegs",tag,val,numPhysFloatRegs);
          getStat("numPhysIntRegs",tag,val,numPhysIntRegs);
          getStat("renameToDecodeDelay",tag,val,renameToDecodeDelay);
          getStat("renameToFetchDelay",tag,val,renameToFetchDelay);
          getStat("renameToIEWDelay",tag,val,renameToIEWDelay);
          getStat("renameToROBDelay",tag,val,renameToROBDelay);
          getStat("wbDepth",tag,val,wbDepth);
          getStat("wbWidth",tag,val,wbWidth);
          getStat("BTBEntries", tag, val, BTBEntries);
          getStat("BTBTagSize",tag,val,BTBTagSize);
          getStat("RASSize",tag,val,RASSize);
        }
      }
    } else if(line.find("[system.switch_cpus.branchPred]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("BTBEntries",tag,val,BTBEntries);
          getStat("BTBTagSize",tag,val,BTBTagSize);
          getStat("RASSize",tag,val,RASSize);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList0]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("count",tag,val,int_alu_count);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList0.opList]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,int_alu_issueLat);
          getStat("opLat",tag,val,int_alu_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList1]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("count",tag,val,mul_div_count);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList1.opList0]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,mul_issueLat);
          getStat("opLat",tag,val,mul_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList1.opList1]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,div_issueLat);
          getStat("opLat",tag,val,div_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList2]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("count",tag,val,fp_alu_count);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList2.opList0]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fadd_issueLat);
          getStat("opLat",tag,val,fadd_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList2.opList1]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fcmp_issueLat);
          getStat("opLat",tag,val,fcmp_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList2.opList2]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fcvt_issueLat);
          getStat("opLat",tag,val,fcvt_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList3]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("count",tag,val,fp_mul_div_sqrt_count);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList3.opList0]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fmul_issueLat);
          getStat("opLat",tag,val,fmul_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList3.opList1]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fdiv_issueLat);
          getStat("opLat",tag,val,fdiv_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList3.opList2]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,fsqrt_issueLat);
          getStat("opLat",tag,val,fsqrt_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList7]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("count",tag,val,read_write_port_count);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList7.opList0]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,read_port_issueLat);
          getStat("opLat",tag,val,read_port_opLat);
        }
      } 
    } else if(line.find("[system.switch_cpus.fuPool.FUList7.opList1]")!= string::npos) {
      while(std::getline(ifs, line) && !line.empty()) {
        std::istringstream iss(line);
        if( getToken(iss, tag,'=') && getToken(iss,val) ) {
          getStat("issueLat",tag,val,write_port_issueLat);
          getStat("opLat",tag,val,write_port_opLat);
        }
      } 
    }
  }
}

void PathProf::procStatsFile(const char* filename) {
  std::string line;
  std::ifstream ifs(filename);

  if(!ifs.good()) {
    std::cerr << filename << " doesn't look good";
    return;
  }

  while(std::getline(ifs, line)) {
    std::istringstream iss(line);
   
    std::string tag,val;
    if( getToken(iss, tag) && getToken(iss,val) ) {
      if( tag.find("switch_cpus") != std::string::npos ) {
        statMap[tag] = val;
        getStat("numCycles",tag,val,numCycles);
        getStat("idleCycles",tag,val,idleCycles);

        getStat("iq.FU_type_0::total",tag,val,totalInsts);

        getStat("iq.FU_type_0::MemRead",tag,val,loadOps);
        getStat("iq.FU_type_0::MemWrite",tag,val,storeOps);

        getStat("iq.FU_type_0::No_OpClass",tag,val,nOps);
        getStat("iq.FU_type_0::IntAlu",tag,val,aluOps);
        getStat("iq.FU_type_0::IntMult",tag,val,multOps);
        getStat("iq.FU_type_0::IntDiv",tag,val,divOps);

        getStat("iq.FU_type_0::FloatAdd",tag,val,faddOps);
        getStat("iq.FU_type_0::FloatCmp",tag,val,fcmpOps);
        getStat("iq.FU_type_0::FloatCvt",tag,val,fcvtOps);
        getStat("iq.FU_type_0::FloatMult",tag,val,fmultOps);
        getStat("iq.FU_type_0::FloatDiv",tag,val,fdivOps);
        getStat("iq.FU_type_0::FloatSqrt",tag,val,fsqrtOps);

        if (!getStat("branchPred.lookups",tag,val,branchPredictions)) {
          getStat("BPredUnit.lookups",tag,val,branchPredictions);
        }
        if (!getStat("branchPred.condIncorrect",tag,val,mispredicts)) {
          getStat("BPredUnit.condIncorrect",tag,val,mispredicts);
        }

        getStat("rob.rob_reads",tag,val,rob_reads);
        getStat("rob.rob_writes",tag,val,rob_writes);

        getStat("rename.int_rename_lookups",tag,val,rename_reads);
        getStat("rename.int_rename_operands",tag,val,rename_writes);

        getStat("rename.fp_rename_lookups",tag,val,fp_rename_reads);
        getStat("rename.fp_rename_operands",tag,val,fp_rename_writes);

        getStat("iq.int_inst_queue_reads",tag,val,int_iw_reads);
        getStat("iq.int_inst_queue_writes",tag,val,int_iw_writes);
        getStat("iq.int_inst_queue_wakeup_accesses",tag,val,int_iw_wakeups);

        getStat("iq.fp_inst_queue_reads",tag,val,fp_iw_reads);
        getStat("iq.fp_inst_queue_writes",tag,val,fp_iw_writes);
        getStat("iq.fp_inst_queue_wakeup_accesses",tag,val,fp_iw_wakeups); 

        getStat("int_regfile_reads",tag,val,int_regfile_reads);
        getStat("int_regfile_writes",tag,val,int_regfile_writes);

        getStat("fp_regfile_reads",tag,val,fp_regfile_reads);
        getStat("fp_regfile_writes",tag,val,fp_regfile_writes);

        getStat("function_calls",tag,val,func_calls);

        getStat("iq.int_alu_accesses",tag,val,ialu_ops);
        getStat("iq.fp_alu_accesses",tag,val,fp_alu_ops);

        getStat("fetch.CacheLines",tag,val,icacheLinesFetched);

        getStat("commit.int_insts",tag,val,commitIntInsts);
        getStat("commit.fp_insts",tag,val,commitFPInsts);

        if (!getStat("committedOps",tag,val,commitInsts)) {
          // try to use switch_cpus.commit.count
          if (!getStat("commit.count", tag, val, commitInsts)) {
            // use int+fp
            commitInsts = commitIntInsts + commitFPInsts;
          }
        }
        getStat("commit.branches",tag,val,commitBranches);
        if (!getStat("commit.branchMispredicts",tag,val,commitBranchMispredicts)) {
          getStat("BPredUnit.condIncorrect", tag, val, commitBranchMispredicts);
        }
        getStat("commit.loads",tag,val,commitLoads);
        getStat("commit.refs",tag,val,commitMemRefs);
      } else {
        getStat("icache.overall_misses",tag,val,icacheMisses);
        getStat("icache.replacements",tag,val,icacheReplacements);

        getStat("dcache.ReadReq_accesses",tag,val,dcacheReads);
        getStat("dcache.WriteReq_accesses",tag,val,dcacheWrites);
        getStat("dcache.ReadReq_misses",tag,val,dcacheReadMisses);
        getStat("dcache.WriteReq_misses",tag,val,dcacheWriteMisses);
        getStat("dcache.replacements",tag,val,dcacheReplacements);

        getStat("l2.ReadReq_accesses",tag,val,l2Reads); 
        getStat("l2.WriteReq_accesses",tag,val,l2Writes); 
        getStat("l2.ReadReq_misses",tag,val,l2ReadMisses); 
        getStat("l2.WriteReq_misses",tag,val,l2WriteMisses); 
        getStat("l2.replacements",tag,val,l2Replacements); 
      }
    }
  }
  intOps=nOps+aluOps+multOps+divOps;
  fpOps=faddOps+fcmpOps+fcvtOps+fmultOps+fdivOps+fsqrtOps;
  commitStores=commitMemRefs-commitLoads;

  #if 0
  std::cout << "Example " << int_iw_reads << ", ";
  std::cout << dcacheReads << ", ";
  std::cout << storeOps << ","; 
  std::cout << "\n";
  #endif
}


void PathProf::procStackFile(const char* filename) {
  std::string line,line1,line2;
  std::ifstream ifs(filename);

  if(!ifs.good()) {
    std::cerr << filename << " doesn't look good";
    return;
  }

  std::getline(ifs,line1);
  std::getline(ifs,line2);
  _origPrevCPC = std::make_pair(std::stoul(line1), (uint16_t)std::stoi(line2));

  std::getline(ifs,line1);
  _origPrevCtrl = to_bool(line1);

  std::getline(ifs,line1);
  _origPrevCall = to_bool(line1);

  std::getline(ifs,line1);
  _origPrevRet = to_bool(line1);

  if(ifs) {
    while(std::getline(ifs, line)) {
      _origstack.push_back(std::stoul(line)); 
    }
  }
}



