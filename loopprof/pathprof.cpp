#include "pathprof.hh"
#include <iostream>
#include <map>

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



//Process op, determine if it is a bb or not
Op* StackFrame::processOp_phase2(uint32_t dId, CPC cpc, 
                                 uint64_t addr, uint8_t acc_size) {
  //get the bb for newCPC -- if this isn't a basic block head, return;
  BB* newBB = _funcInfo->getBB(cpc);

  //new I'm not in a basic block, then just get out... this only happens
  //during initializiation
  if(newBB==NULL && _prevBB==NULL) {
    return NULL;
  }

  if(newBB!=NULL) {
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
        cout << "HUGE PROBLEM -- EDGE NOT FOUND   (";
        if(!succfound) {
          cout << "no succ, ";
        }
        if(!predfound) {
          cout << "no pred, ";
        }
        cout << _prevBB->rpoNum() << "->" << newBB->rpoNum() 
             << ") dId=" << dId <<" \n";
        //assert(0);
      }
      _prevBB->succCount[newBB]+=1;
    }

    _prevBB=newBB;
    //cout << "setting prevBB to " << _prevBB->rpoNum() << "dId: " <<  dId << "\n";

    newBB->incFreq();
  }
  
  
  Op* op = _prevBB->getOp(cpc);
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
   if(second.isRecursing()) {
     top.setRecursing(true);
     if(top.funcInfo() == second.funcInfo()) {
       top.setDirectRecursing(true);
     } 
     return;
   }

   for(auto end_iter = _callStack.rend();iter!=end_iter;++iter) {
     StackFrame& next = *iter;
     assert(!next.isRecursing());
     if(next.funcInfo() == top.funcInfo()) {
        top.setRecursing(true);
        return;
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
  if(prevCPC.first!=0) {
    _callStack.back().processBB_phase1(_prevHead,prevCPC); 
  }
  adjustStack(newCPC,isCall,isRet);
  if(isCall) {
    _callStack.back().funcInfo()->got_called();
  }
  _prevHead=newCPC;
}

void PathProf::runAnalysis() {
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
        if(loopInfo->isInnerLoop()) {
          loopInfo->initializePathInfo();
        }
      }
    }
  } 
}

void PathProf::runAnalysis2(bool no_gams, bool gams_details) {

  std::multimap<uint64_t,LoopInfo*> loops;
 
  FuncMap::iterator i,e;
  for(i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    FunctionInfo::LoopList::iterator li,le;
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

    if(loopInfo->isInnerLoop()
       && hpi != -2 //no hot path
       && loopInfo->getLoopBackRatio(hpi) >= 0.5
       && loopInfo->getTotalIters() >= 10
       ) {
      stringstream part_gams_str;
      part_gams_str << "partition." << loopInfo->id() << ".gams";

      worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),gams_details,no_gams);
      if(worked) {
        cerr << " -- Beretized\n";
      } else {
        cerr << " -- NOT Beretized (Func Calls)\n";
      }
    } else {
      cerr << " -- NOT Beretized\n";
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
  _op_buf[_dId%MAX_OPS]=op;

  //check inst/execution statistics
  sf.funcInfo()->incInsts(sf.isLooping(),
                          sf.isDirectRecursing(),sf.isRecursing());

  //Check if op is new
  if(op->cpc().first==0) {
    op->setCPC(newCPC);
    op->setIsLoad(img._isload);
    op->setIsStore(img._isstore);
    op->setIsCtrl(img._isctrl);
    op->setIsCall(img._iscall);
    op->setIsReturn(img._isreturn);
  }

  op->executed(img._cc-img._ec);

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
  if(img._mem_prod!=0) {
    uint64_t full_ind = _dId - img._mem_prod;
    if(img._mem_prod<=_dId) {
      op->addMemDep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS]);
    }
  }

  if(img._cache_prod!=0) {
    uint64_t full_ind = _dId - img._cache_prod;
    if(img._cache_prod <= _dId) {
      op->addCacheDep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS]);
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
