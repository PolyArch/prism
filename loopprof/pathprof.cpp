#include "pathprof.hh"
#include <iostream>
#include <map>
#include <unordered_set>
using namespace std;

// --------------------------------- Dur Histo -------------------------------
std::vector<int> DurationHisto::sizes = {8,16,32,64,128,256,512,1024,2048,4096,8192,2147483647};

// -------------------------------- StackFrame ---------------------------------

Op* StackFrame::processOp_phase3(uint32_t dId, CPC cpc,PathProf* prof, bool extra) {
  BB* newBB = _funcInfo->getBB(cpc);

  if(newBB==NULL && _prevBB==NULL) {
    return NULL;
  }

  if(newBB!=NULL) {
    processBB_phase2(dId, newBB,(extra?2:0)/*which one?*/,prof); 
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
          cout << "HUGE PROBLEM -- EDGE NOT FOUND in func: \"" << _funcInfo->nice_name() << "\"   (";
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

void StackFrame::check_for_stack(Op* op, uint64_t dId, uint64_t addr, uint8_t acc_size) {

/*  if(op->isLoad()) {
    cout << "load ";
  }
  if(op->isStore()) {
    cout << "store ";
  }

  if(op->isMem()) {
    cout << op->id() << " ";
    cout << addr << "\n"; 
  }*/

  if(op->isStore()) {
    if(giganticMemDepTable.count(addr)) { //output dep
      uint32_t dep_dId = giganticMemDepTable[addr].dId;
      Op*      dep_op  = giganticMemDepTable[addr].op;
      dyn_dep(dep_op,op,dep_dId,dId,true);
    }
    if(giganticMemLoadTable.count(addr)) { // anti dep
      uint32_t dep_dId = giganticMemLoadTable[addr].dId;
      Op*      dep_op  = giganticMemLoadTable[addr].op;
      dyn_dep(dep_op,op,dep_dId,dId,true);
    }
    giganticMemDepTable[addr].dId=dId;
    giganticMemDepTable[addr].op=op;
  }
  if(op->isLoad()) {
    if(giganticMemDepTable.count(addr)) {
      uint32_t dep_dId = giganticMemDepTable[addr].dId;
      Op*      dep_op  = giganticMemDepTable[addr].op;

      dyn_dep(dep_op,op,dep_dId,dId,true);

      checkIfStackSpill(giganticMemDepTable[addr].op,op,addr);
    } else {
      //This op is not a candidate b/c it is reading value before writing
      //TODO: don't do this if this is the first_function we are running
      _funcInfo->not_stack_candidate(op);
    }
    giganticMemLoadTable[addr].dId=dId;
    giganticMemLoadTable[addr].op=op;
  }
}

void StackFrame::hot_trace_gran(uint64_t dId,PathProf* prof,bool loop_changed) {
  if(_trace_duration &&
      (loop_changed || _pathIndex!=_loopStack.back()->getHotPathIndex()) ) {
    prof->_gran_duration[PathProf::ST_TRACE].addHist(_trace_duration, 
                                                     _prev_trace_interval,
                                                     false /*not considering inlining*/);
    _trace_duration=0;
  }
  
  if(!loop_changed && _loopStack.size() >0 && 
     _pathIndex==_loopStack.back()->getHotPathIndex() && 
     !_loopStack.back()->containsCallReturn() ) {
    uint64_t trace_interval = dId-_trace_start;
    assert(trace_interval < 100000);
    _trace_duration += trace_interval;
    _prev_trace_interval = trace_interval;
  }

  _trace_start=dId;
}

void StackFrame::checkLoopGranBegin(uint64_t dId, LoopInfo* li, PathProf* prof) {
  if(_cur_i_iloop==NULL && li->is_revolverable() && li->isInnerLoop()  ) {
    _cur_i_iloop=li;
    _cur_i_iloop_start=dId;
  }

  for(int i : DurationHisto::sizes) {
    //No-inlining case
    {HistoItem& item = prof->_gran_duration[PathProf::ST_OLOOP].histo_map[i];
    if(item.li==NULL && !li->containsCallReturn() && li->staticInsts() < item.size) {
      item.li=li;
      item.start=dId;
    }}
    //inline-away!
    {HistoItem& itemi = prof->_gran_duration[PathProf::ST_OLOOP].histo_map_i[i];
    if(itemi.li==NULL&& !li->cantFullyInline() && li->inlinedStaticInsts() < itemi.size){
      itemi.li=li;
      itemi.start=dId;
    }}
  }
}

void StackFrame::checkLoopGranComplete(uint64_t dId, LoopInfo* li, PathProf* prof) {
  if(_cur_i_iloop==li) {
    uint64_t duration = dId-_cur_i_iloop_start;
    prof->_gran_duration[PathProf::ST_ILOOP].addHist(duration,li->inlinedStaticInsts(),li->containsCallReturn());
    _cur_i_iloop=NULL;
  }
  for(int i : DurationHisto::sizes) {
    {HistoItem& item = prof->_gran_duration[PathProf::ST_OLOOP].histo_map[i];
    if(item.li == li) {
      uint64_t duration = dId-item.start;
      prof->_gran_duration[PathProf::ST_OLOOP].addSpecificHist(duration,item);
      item.li=NULL;

      if(item.size==256 && duration >= 16384) {
        prof->longer_loops_d16384_s256[li]+=duration;
      }
    }}
    {HistoItem& itemi = prof->_gran_duration[PathProf::ST_OLOOP].histo_map_i[i];
    if(itemi.li == li) {
      uint64_t duration = dId-itemi.start;
      prof->_gran_duration[PathProf::ST_OLOOP].addSpecificHist(duration,itemi);
      itemi.li=NULL;

      if(itemi.size==256 && duration >= 16384) {
        prof->longer_loops_d16384_s256_i[li]+=duration;
      }
    }}
  }
}

//Determines current loop nests, and can profile iterations
void StackFrame::processBB_phase2(uint32_t dId, BB* bb, int profile, PathProf* prof) {
  bool stackChanged = false, loopChanged=false;

  //check if we need to iter complete
  if(profile) {
    //only track paths for inner loops
    if(_loopStack.size() > 0 && _loopStack.back()->loop_head() == bb) {
      if(profile==1) {
        _loopStack.back()->iterComplete(_pathIndex,_loopPath);
      } else if(profile==2) {
        hot_trace_gran(dId,prof,false);
      }
    } 
  }

  for(auto i =_loopStack.begin(), e=_loopStack.end(); i!=e; ++i) {
    LoopInfo* li = *i;
    if(li->loop_head() == bb) {
      li->incIter(profile==1);
    }
  }
 
  //now update the loop stack
  while(_loopStack.size()>0&&!_loopStack.back()->inLoop(bb)) {
    _loopStack.back()->endLoop(profile==1);
    if(profile==2) {
      checkLoopGranComplete(dId,_loopStack.back(),prof);
    }
    _loopStack.pop_back();
    _iterStack.pop_back();  //get rid of iter too
    stackChanged=true;
    loopChanged=true;
  }
 
  //now check if we need to push a loop
  if(LoopInfo* li = _funcInfo->getLoop(bb)) {
    //make sure we didn't loop back!
    if(_loopStack.size() != 0 && _loopStack.back() == li) {
      _iterStack.pop_back();
    } else {
      loopChanged=true;
      _loopStack.push_back(li);
      li->beginLoop(profile==1);
      if(profile==2) {
        checkLoopGranBegin(dId,_loopStack.back(),prof);
      }
    }
    _iterStack.emplace_back(new LoopIter(_loopStack,_funcInfo->instance_num())); //this gets deconstructed in itermap, don't worry!
    stackChanged=true; 
  }

  if(stackChanged) {
    //these aren't stack-ified yet because they don't work on non-inner loops anyways
    _loopPath.clear();
    _pathIndex=0; //going inside the loop should clear the path index
    if(profile==1) {
      _iterMap.emplace(std::piecewise_construct,
                       forward_as_tuple(dId),
                       forward_as_tuple(_iterStack.back())); 
    }
    if(profile==2)
     if(loopChanged) {
      //_cur_trace_id=-1; //reset trace
      hot_trace_gran(dId,prof,true);
    }
    _trace_start=dId;

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

void StackFrame::dyn_dep(Op* dep_op, Op* op, 
                         uint64_t dep_dId, uint64_t dId, bool isMem) {
  if(_loopStack.size()==0) {
    return; // leave if we are not dealing with a loop memory dep
  }

  if(!isMem) {
    return; // for now, just get rid of non-memory loop deps
  }

  LoopIter* curIter = _iterStack.back().get();
  //get the iteration this corresponds to
  LoopIter* depIter = (--_iterMap.upper_bound(dep_dId))->second.get();

  bool theSameLoops=curIter->functionIter()==depIter->functionIter()
                    && curIter->psize()==depIter->psize();
  LoopInfo::LoopDep relDep;
  //Check if loops in question are the same (zippered iterator)
  LoopIter::IterPos::iterator i1,i2,e1,e2;
  for(i1=curIter->pbegin(),i2=depIter->pbegin(),e1=curIter->pend(),e2=depIter->pend();
      theSameLoops && i1!=e1 && i2!=e2; ++i1, ++i2) {
      theSameLoops &= i1->first == i2->first;
      relDep.push_back(i2->second - i1->second);
  }
  
  if(theSameLoops) {
    if(dep_op->isStore() && op->isLoad()) {
      curIter->relevantLoop()->addRelativeLoopDep_wr(relDep); 
    } else if(dep_op->isStore() && op->isStore()) {
      curIter->relevantLoop()->addRelativeLoopDep_ww(relDep);
    } else if(dep_op->isLoad() && op->isStore()) {
      curIter->relevantLoop()->addRelativeLoopDep_rw(relDep);
    }

    op->check_nearest_dep(dep_op, relDep);

    //cout << "dep dId:" << dep_dId << " dId:" << dId << " " << curIter->functionIter() << " ";

    //curIter->print();
    //cout << " -- ";
    //depIter->print();
    //cout << "\n";
  } else {
    //More complex dependency, must handle this seperately
    //This shouldn't interfere with anything though.
  }
}


// -------------------------------- PathProf -----------------------------------

FunctionInfo*  PathProf::getOrAddFunc(CPC newCPC) {
    FuncMap::iterator funcInfoIter = _funcMap.find(newCPC);


    if(funcInfoIter == _funcMap.end()) {
      FunctionInfo* func = new FunctionInfo(newCPC);
      funcInfoIter = _funcMap.emplace(newCPC,func).first;

      if(sym_tab.size() > 0) {
        prof_symbol* sym= &((--sym_tab.upper_bound(newCPC.first))->second);//.get()
        if(sym->isFunc()) {
          funcInfoIter->second->setSymbol(sym);
        }
      }

      StaticFunction* sfunc = NULL;
      if(static_cfg.static_funcs.count(newCPC.first)) {
        sfunc = static_cfg.static_funcs[newCPC.first];
      }
      if(sfunc) {
        //cout << func->nice_name() << " found!\n";
        func->setPinInfo(sfunc->name, sfunc->filename, sfunc->line);

        //Iterate through static function, and do build the initial BBs!
        for(auto i=sfunc->static_bbs.begin(),e=sfunc->static_bbs.end();i!=e;++i) {
          StaticBB* sbb = i->second;
          CPC sbb_cpc = std::make_pair(sbb->head,0);
          CPC sbb_cpc_t = std::make_pair(sbb->tail,0);
   
          BB* bb = func->addBB(NULL,sbb_cpc,sbb_cpc_t,false);
          bb->set_line_number(sbb->line,sbb->srcno); 
        }

        for(auto i=sfunc->static_bbs.begin(),e=sfunc->static_bbs.end();i!=e;++i) {
          StaticBB* sbb = i->second;
          CPC sbb_cpc = std::make_pair(sbb->head,0);

          //get this BB if we have it
          BB* bb = func->bbOfCPC(sbb_cpc);
          assert(bb);

          //Iterate through next pointers, hook up BBs
          for(auto ii=sbb->next_static_bb.begin(),
                   ee=sbb->next_static_bb.end();ii!=ee;++ii) {
            uint64_t next_bb_head = ii->first;
            uint64_t next_bb_tail = ii->second;
            CPC headCPC = std::make_pair(next_bb_head,0);
            CPC tailCPC = std::make_pair(next_bb_tail,0);
            func->addBB(bb,headCPC,tailCPC,false/*not dynamic*/);
            //cout << "added bb\n";
          }
        }
      } else {
        //cout << func->nice_name() << " not found!\n";
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
    _callStack.back().returning(_dId,this);
    _callStack.pop_back();
    assert(_callStack.size()>0);
    return true;
  }
  return false;
}

void PathProf::processAddr(CPC cpc, uint64_t addr, bool is_load, bool is_store) {
  uint64_t word_addr = addr & 0xFFFFFFFFFFFFFFFC;

  if(is_load && !StackFrame::onStack(word_addr) ) {
    if(const_loads.count(cpc)) {
      uint64_t recorded_addr = const_loads[cpc];
      if(recorded_addr != 0 && word_addr != recorded_addr) {
        //cout << "found bad cpc:" 
        //     << cpc.first << "." << cpc.second << " " << word_addr <<"\n";
        const_loads_backwards.erase(recorded_addr);
        const_loads[cpc]=0;
      }
    } else if (word_addr != 0) {
      const_loads[cpc]=word_addr;
      const_loads_backwards[word_addr]=cpc;
      //cout << "const load candidate:" << cpc.first << "." << cpc.second << "\n";
    }
  }
}

bool PathProf::isBB(CPC cpc) {
  FunctionInfo* fi = _callStack.back().funcInfo();
  if(fi) {
    return fi->bbOfCPC(cpc)!=0;
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

void PathProf::runAnalysis2(bool no_gams, bool gams_details, bool size_based_cfus,
                            uint64_t total_dyn_insts) {
  for(auto i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    fi.calculatePDOM();
    fi.setStackOps();
  }

  for(auto iter : const_load_ops) {
    Op* op = iter.second;
    op->setIsConstLoad();
  }

  std::multimap<uint64_t,LoopInfo*> loops;
  for(auto i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    FunctionInfo::LoopList::iterator li,le;

    // loop calls recursive func an
    //fi->figureOutIfLoopsCallRecursiveFunctions();

    for(li=fi.li_begin(),le=fi.li_end();li!=le;++li) {
      LoopInfo* loopInfo = li->second;

      //std::vector<Op*> long_loop_recs;
      //loopInfo->calcRecurrences(long_loop_recs);
      loops.insert(std::make_pair(loopInfo->numInsts(),loopInfo));
    }
  } 

  std::ofstream sched_stats;
  sched_stats.open("stats/sched-stats.out", std::ofstream::out | std::ofstream::trunc);

  std::ofstream sched_nla;
  sched_nla.open("stats/sched-nla.out", std::ofstream::out | std::ofstream::trunc);
  //sched_nla << "#loopname   entries exits iters cinsts    dyn_insts   Includes\n";
  sched_nla << "#LoopName  loopid entries exits iters static_insts dyn_insts cinsts  Includes\n";

  cout << "CFU Scheduling";
  cout.flush();

  for(auto I=loops.rbegin();I!=loops.rend();++I) {
    LoopInfo* loopInfo = I->second;
    /*
    cout << "loopinfo: " << loopInfo->id() << " ";
    cout << loopInfo->pathHeatRatio(loopInfo->getHotPathIndex()) << " ";
    cout << loopInfo->getTotalIters() << "\n"; */

    //generate this only for 
    //1. Inner Loops
    //2. >50% Loop-Back
    //3. Executed >= 10 Times

    int hpi = loopInfo->getHotPathIndex();
    sched_stats << "func: " << loopInfo->func()->nice_name() 
         << "(" << loopInfo->func()->id() << ")"
         << " loop: " << loopInfo->id()
         << "(depth:" << loopInfo->depth() << " hpi:" << hpi
         << "hp_len: " << loopInfo->instsOnPath(hpi)  
         << (loopInfo->isInnerLoop() ? " inner " : " outer ")
         << " hot_path_heat:" << loopInfo->pathHeatRatio(hpi)
         << " iters:" << loopInfo->getTotalIters()
         << " insts:" << loopInfo->numInsts()
         << ")";

    bool worked=false;

    // BERET Scheduling
    if(loopInfo->isInnerLoop()
       && hpi != -2 //no hot path
       && loopInfo->pathHeatRatio(hpi) >= 0.55
       && loopInfo->getTotalIters() >= 10
       && !loopInfo->containsCallReturn(hpi)
       ) {
      stringstream part_gams_str;
      part_gams_str << "partition." << loopInfo->id();

      worked = false;
      if(size_based_cfus) {
        //worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
        //           NULL,
        //           gams_details,no_gams);
      } else {
        worked = loopInfo->scheduleBERET(part_gams_str.str(),
                   &_beret_cfus,
                   gams_details,no_gams);
      }
      if(worked) {
        sched_stats << " -- Beretized";
        cout << ".";
      } else {
        sched_stats << " -- NOT Beretized (Func Calls?)";
        cout << "x";
      }
      cout.flush();
    } else {
      sched_stats << " -- NOT Beretized";
    }

    //update stats
    if(worked) {
      insts_in_beret += loopInfo->dynamicInstsOnPath(hpi); 
    }

    //NLA Scheduling
    if(!loopInfo->cantFullyInline()) {
//    if(!loopInfo->containsCallReturn()) {
      bool attempted=false;
      if(size_based_cfus) {
        //worked = loopInfo->scheduleNLA(NULL, gams_details, no_gams, 
        //                               attempted, total_dyn_insts);
      } else {
        worked = loopInfo->scheduleNLA(&_beret_cfus, gams_details, no_gams,
                                       attempted, total_dyn_insts);
      }

      SGSched& sgSchedNLA = loopInfo->sgSchedNLA();

      if(worked) {
        sched_stats << " -- NLA'D\n";
        cout << loopInfo->id() << ",";

        std::set<FunctionInfo*> funcsSeen;
        std::vector<BB*> totalVec;
        std::set<LoopInfo*> loopsSeen;
        for(auto ii=loopInfo->rpo_begin(),ee=loopInfo->rpo_end();ii!=ee;++ii) {
          BB* bb = *ii;  
          totalVec.push_back(bb);
        }
        loopInfo->inlinedBBs(funcsSeen,totalVec);
        for(BB* bb : totalVec) {
          LoopInfo* cli = bb->func()->innermostLoopFor(bb);
          if(cli!=NULL && cli != loopInfo) {
            loopsSeen.insert(cli);
          }
        }


        //sched_nla << "#LoopName loopid entries exits iters static_insts dyn_insts cinsts  Includes\n";
        sched_nla << loopInfo->nice_name_full_quoted()
           << " " << loopInfo->id()
           << " " << loopInfo->getLoopEntries()
           << " " << loopInfo->getLoopCount()
           << " " << loopInfo->getTotalIters()
           << " " << loopInfo->inlinedStaticInsts()
           << " " << loopInfo->totalDynamicInlinedInsts()
           << " " << sgSchedNLA.numSubgraphs();

        for(FunctionInfo* fi : funcsSeen) {
          sched_nla << " " << fi->nice_name_quoted();
        }
        for(LoopInfo* li : loopsSeen) {
          sched_nla << " " << li->nice_name_full_quoted();
        }
        sched_nla << "\n";

      } else if(!attempted) {
        sched_stats << " -- Skip NLAt\n";
        cout << "s";      
      } else {
        sched_stats << " -- NOT NLA'd\n";
        cout << "z";
      }
      cout.flush();
    } else {
      sched_stats << "\n";
    }
  
    if(loopInfo->isInnerLoop() && !loopInfo->containsCallReturn()) {
      insts_in_simple_inner_loop += loopInfo->numInsts();
    }
    if(loopInfo->isInnerLoop()) {
      insts_in_inner_loop += loopInfo->numInsts();
    }
  
    insts_in_all_loops += loopInfo->numInsts(); 
  }
  cout << "\n";
  
  std::multimap<uint64_t,FunctionInfo*> funcs;
  for(auto i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
    FunctionInfo& fi = *i->second;
    funcs.insert(std::make_pair(fi.nonLoopInsts(),&fi));
    
    non_loop_insts_direct_recursion += fi.nonLoopDirectRecInsts();
    non_loop_insts_any_recursion += fi.nonLoopAnyRecInsts();
  }
  
  
  std::ofstream func_stats;
  func_stats.open("stats/func-stats.out", std::ofstream::out | std::ofstream::trunc);
  func_stats << "Funcs by non-loop insts:\n";
  for(auto I = funcs.rbegin(), E = funcs.rend(); I!=E; ++I) {
    FunctionInfo* fi = I->second;
    func_stats << fi->nice_name() << " (" << fi->id() << "): " << fi->nonLoopInsts() << "\n";
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

  //if(isChanged) {
  //  //cout << "WOOO!\n";
  //  FunctionInfo* fi = _callStack.back().funcInfo();
  //  cout << fi->firstBB()->head().first << " " << fi->firstBB()->head().second << ", ";
  //}
  

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
    sf.funcInfo()->inc_instance();
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
    op->setIsCondCtrl(img._iscondctrl);
    op->setIsIndirectCtrl(img._isindctrl);
    op->setIsCall(img._iscall);
    op->setIsReturn(img._isreturn);
    op->setIsFloating(img._floating);
  }

  //cout << "," << op->cpc().first << "." << op->cpc().second;
  //cout << "," << op->id();


  op->executed(img._ep_lat);
  op->setOpclass(img._opclass);
  //We must add in all the dependencies
  for(unsigned i = 0; i < MAX_SRC_REGS; ++i) {
    int dep_ind = img._prod[i];
    if(dep_ind==0) {
      continue;
    }
    uint64_t full_ind = _dId -dep_ind;
    if(dep_ind<=_dId) {
      op->addDep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS],i);
      sf.dyn_dep(_op_buf[(full_ind+MAX_OPS)%MAX_OPS],op,full_ind,_dId,false);
    }
  }

  if(op->isCondCtrl() || op->isIndirectCtrl() || op->isCall() || op->isReturn()) {
    _ctrlFreeMem.clear();
  }

  if (img._mem_prod != 0) {
    uint64_t full_ind = _dId - img._mem_prod;
    if (img._mem_prod <= _dId) {
      Op* md_op = _op_buf[(full_ind + MAX_OPS) % MAX_OPS];
      if(_ctrlFreeMem.count(md_op)) {
        op->addMemDep(md_op,true);
      } else {
        op->addMemDep(md_op,false);
      }
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

  sf.check_for_stack(op,_dId,img._eff_addr,img._acc_size);

  //check for const loads:
  if(op->isLoad()) {
    if(const_loads.count(op->cpc())) { // i'm a little unhappy with having to do 
    //this, but oh well. I should have probably created the Ops in stage 1, 
    //so that the const_load maps could have used ops directly.
      if(const_loads[op->cpc()]!=0 && !op->isSpcMem()) {
        const_load_ops[op->cpc()]=op;
      }
    }
  }
  if(op->isStore()) {
    //check if we have written over something we thought was a constant
    uint64_t word_addr = img._eff_addr & 0xFFFFFFFFFFFFFFFC;
    if(const_loads_backwards.count(word_addr)) {
      CPC cpc = const_loads_backwards[word_addr];
      const_loads.erase(cpc);
      const_load_ops.erase(cpc);
      const_loads_backwards.erase(word_addr);
    }
  }
  if(op->isLoad() || op->isStore()) {
    _ctrlFreeMem.insert(op); 
  }

  _dId++;
}

Op* PathProf::processOpPhase3(CPC newCPC, bool wasCalled, bool wasReturned){
  adjustStack(newCPC,wasCalled,wasReturned);
  StackFrame& sf = _callStack.back();
  Op* op = sf.processOp_phase3(_dId,newCPC,this,false/*don't do any extra profiling*/);

  if(op==NULL) {
    return NULL;
  }

  //  _op_buf[_dId%MAX_OPS]=op;
  _dId++;
  return op;
}

Op* PathProf::processOpPhaseExtra(CPC newCPC, bool wasCalled, bool wasReturned){
  adjustStack(newCPC,wasCalled,wasReturned);
  StackFrame& sf = _callStack.back();
  Op* op = sf.processOp_phase3(_dId,newCPC,this,true/*profile regions*/);

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

void PathProf::procStaticCFG(const char* filename) {
  std::string line,tag,val,val2,val3,val4,garbage;
  std::ifstream ifs(filename);

  if(!ifs.good()) {
    std::cerr << filename << " doesn't look good\n";
    return;
  }

  std::unordered_map<uint64_t,StaticFunction*>& fmap = static_cfg.static_funcs;
  StaticFunction* sfunc = NULL;

  while(std::getline(ifs, line)) {

    if(line.find("Section:") == 0 || line.empty()) {
      continue; //do nothing
    } else if(line.find("FNAME:")== 0) {
      std::istringstream iss(line);
      if( getToken(iss, tag,' ') && getToken(iss,val,'"') ) {
        static_func_names.push_back(val);
      } else {
        cout << "fname line not parsable" << line << "\n";
      }
    } else if(line.find("Function:")== 0) {
      std::istringstream iss(line);
      uint64_t func_addr;
      if( getToken(iss, tag,' ') && getToken(iss,val) && getToken(iss,val2,'"')
          && getToken(iss,garbage,'"') 
          && getToken(iss,val3,'"') && getToken(iss,val4)) {
         func_addr = stoul(val,0,16);
         sfunc=new StaticFunction();
         sfunc->name=val2;
         sfunc->filename=val3;
         sfunc->line=stoul(val4);

         fmap[func_addr]=sfunc;
      } else {
         cout << "line not parsable" << line << "\n";
 //        cout << "got" << tag << "-" << val << "-" << val2 << "-" << garbage 
 //                             << "-" << val3<< "-" << val4 << ";\n";
         assert(0);
      }

    } else if(sfunc!=NULL) {
      std::istringstream iss(line);
      uint64_t bb_begin, bb_end;
      int lineno,srcno;
      getToken(iss, val2,',');
      getToken(iss, val3,':');
      getToken(iss, tag,'.');
      getToken(iss, val,' ');

      lineno    = std::stoul(val2);
      srcno     = std::stoul(val3);
      bb_begin = std::stoul(tag,0,16);
      bb_end   = std::stoul(val,0,16);

      StaticBB* sbb = new StaticBB(bb_begin,bb_end,lineno,srcno);
      sfunc->static_bbs[bb_begin]=sbb;

      //cout << "got BB: \"" << tag << " " << val << "\"\n";
      getToken(iss, val2,'>');
      //cout << "junk" << val2 << "\n";
      
      //iterate through rest of line to find BB successors    
      while(getToken(iss, tag, '.') && getToken(iss, val, ',') ) {
        //cout << tag << "|" << val << "\n";
        uint64_t next_bb_begin = stoul(tag,0,16);
        uint64_t next_bb_end =   stoul(val,0,16);
        sbb->next_static_bb.insert(std::make_pair(next_bb_begin,next_bb_end));
      } 
    }
  }


}

void PathProf::procConfigFile(const char* filename) {
  std::string line,tag,val;
  std::ifstream ifs(filename);

  if(!ifs.good()) {
    std::cerr << filename << " doesn't look good\n";
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
    std::cerr << filename << " doesn't look good\n";
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
        getStat("l2.ReadExReq_accesses",tag,val,l2Writes); 
        getStat("l2.ReadReq_misses",tag,val,l2ReadMisses); 
        getStat("l2.ReadExReq_misses",tag,val,l2WriteMisses); 
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
    std::cerr << filename << " doesn't look good\n";
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



