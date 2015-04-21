#include "loopinfo.hh"
#include "functioninfo.hh"

#include "cfu.hh"
#include "vectorization_legality.hh"

#include "../critpath/edge_table.hh"

using namespace std;

uint32_t Subgraph::_idCounter=1;
void Subgraph::checkVec() {
  //topological sort the subgraph
  if(_opVec.size() != _ops.size()) {
    assert(_opVec.size()==0);
    std::set<Op*> ops = _ops;
    while(ops.size() != 0) {
      for(auto i = ops.begin(),e=ops.end();i!=e;++i) {
        Op* op = *i;
        bool readyOp=true;
        for(auto dsi=op->d_begin(),dse=op->d_end();dsi!=dse;++dsi) {
          Op* dep_op = *dsi;
          if(LoopInfo::staticForwardDep(dep_op,op) && ops.count(dep_op)) {
            readyOp=false;
            break;
          }
        }
        if(readyOp) {
          ops.erase(op);
          _opVec.push_back(op);
          break;
        }
      }
    }
  } 
  assert(_ops.size() == _opVec.size());
}

uint32_t LoopInfo::_idcounter=1;

#define MAX_GAMS_SIZE 100
#define DONT_EVEN_TRY_GAMS_SIZE 1024

void LoopInfo::build_rpo() {
  BB* bb=_head;
  std::set<BB*> seen;
  build_rpo(bb,seen);  std::reverse(_rpo.begin(),_rpo.end());
}
void LoopInfo::build_rpo(BB* bb, std::set<BB*>& seen) {
  for(auto ip=bb->succ_begin(),ep=bb->succ_end(); ip!=ep;++ip) {
    BB* succ_bb = *ip;
    seen.insert(bb);
    if(_loopBody.count(succ_bb)==0) {
      continue;
    }
    if(seen.count(succ_bb)==0) {
      build_rpo(succ_bb,seen);
    }
  }
  _rpo.push_back(bb);  
}

void LoopInfo::initializePathInfo(BB* bb, map<BB*,int>& numPaths) {
  BB::BBvec::iterator ip,ep;
  numPaths[bb]=0;
  for(ip=bb->succ_begin(),ep=bb->succ_end(); ip!=ep;++ip) {
     BB* succ_bb = *ip;
     if(_loopBody.count(succ_bb)==0) {
       continue;
     }
     if(succ_bb!=_head && numPaths.count(succ_bb)==0) {
       initializePathInfo(succ_bb,numPaths);
     }
  }
  // all children have been processed

  int totalPaths=0,i;
  for(i=0,ip=bb->succ_begin(),ep=bb->succ_end();ip!=ep;++ip,++i) {
     BB* succ_bb = *ip;
     if(_loopBody.count(succ_bb)==0) {
       continue;
     }
     //bb->setWeight(i,totalPaths);
     
     _edgeWeight[make_pair(bb,succ_bb)]=totalPaths;
     totalPaths+=numPaths[succ_bb];
  }
  if(totalPaths==0) {
    totalPaths=1;
  }
  numPaths[bb]=totalPaths;
  if(totalPaths > _maxPaths) {
    _maxPaths=totalPaths;
  }
  //_rpo.push_back(bb);  
}

void LoopInfo::initializePathInfo() {
  map<BB*,int> numPaths;
  initializePathInfo(_head,numPaths); 
}

//generic version that doesn't track path
void LoopInfo::incIter(bool profile) {
    _curIter++;
    if(profile) {
      _totalIterCount++;
    }
}

//Called when a loop iteration completes
void LoopInfo::iterComplete(int pathIndex, BBvec& path) {
  if(_immInnerLoops.size() == 0) {
    if(!(pathIndex >=0 && pathIndex <= _maxPaths)) {
      std::cout << "ERROR: PATH IS BIGGER THAN MAX\n";
      std::cout << pathIndex << ">" <<  _maxPaths << "\n";
      std::cout << _funcInfo->nice_name();
      printLoopBody(std::cout);
    }

    _iterCount[pathIndex]++;
    if(_pathMap.count(pathIndex)==0) {
      _pathMap[pathIndex]=path;
    }
  }
  std::map<uint64_t, std::set<Op*> > _effAddr2Op;

  for (auto I = path.begin(), E = path.end(); I != E; ++I) {
    for (auto OI = (*I)->op_begin(), OE = (*I)->op_end(); OI != OE; ++OI) {
      if (!(*OI)->isLoad() && !(*OI)->isStore())
        continue;
      _effAddr2Op[(*OI)->getCurEffAddr()].insert(*OI);
    }
  }
  for (auto I = path.begin(), E = path.end(); I != E; ++I) {
    for (auto OI = (*I)->op_begin(), OE = (*I)->op_end(); OI != OE; ++OI) {
      if (!(*OI)->isLoad() && !(*OI)->isStore())
        continue;
      (*OI)->iterComplete(_effAddr2Op);
    }
  }
}

/*
 * Remember, this is only valid for inner loops with a hot path...
 */
bool LoopInfo::dependenceInPath(Op* dop, Op* op) {
  if(_instsInPath.empty()) {
    BBvec& theHotPath = getHotPath();
    for(auto i=theHotPath.begin(),e=theHotPath.end();i!=e;++i) {
      BB* bb= *i;
      for(auto oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
        Op* op = *oi;
        _instsInPath.insert(op);
      }
    }
  }
  return dependenceInPath(_instsInPath,dop,op);
}

bool LoopInfo::dependenceInPath(std::set<Op*>& relevantOps,Op* dop, Op* op) {
  if(relevantOps.count(dop)==0||relevantOps.count(op)==0) {
    return false;
  } else if(dop->bb() == op->bb()) { //only count forward ops
    if(dop->bb_pos() >= op->bb_pos()) {
      return false;
    }
  } else { //dop->bb() != op->bb() -- count forward bbs
    if(dop->bb()->rpoNum() >= op->bb()->rpoNum()) {
      return false;
    }
  }
  return true;
}

//just clear this stuff when we start
void LoopInfo::beginLoop(bool profile) {
  _curIter=0;
  if(profile) {
    _loopEntries++;
    _prevOpAddr.clear();
    _prevOpIter.clear();
  }
}

void LoopInfo::endLoop(bool profile) {
  _curIter=0;
  if(profile) {
    _loopCount++;
    _prevOpAddr.clear();
    _prevOpIter.clear();
  }
}

int LoopInfo::weightOf(BB* bb1, BB* bb2) {
  EdgeInt::iterator i = _edgeWeight.find(std::make_pair(bb1,bb2));
  if(i!=_edgeWeight.end()) {
    return i->second;
  } else {
    /*
    std::cout << _funcInfo->firstBB()->head().first << "\n";
    std::cout << bb1->rpoNum() << "->";
    std::cout << bb2->rpoNum() << ": ";
    EdgeInt::iterator i,e;
    for(i=_edgeWeight.begin(),e=_edgeWeight.end();i!=e;++i) {
      std::cout << i->first.first->rpoNum() << "->";
      std::cout << i->first.second->rpoNum() << " ";
    }
    std::cout << "\n";
*/

    //std::cout << _edgeWeight.size() << "\n";
    //assert(0 && "bad edge weight guess");
    return -1;
  }
}

int LoopInfo::depthNest() {
  if(_depth!=-1) {
    return _depth;
  }
  
  _immOuterLoop = NULL;

  LoopSet::iterator i,e;
  for(i=_outerLoops.begin(),e=_outerLoops.end();i!=e;++i) {
    LoopInfo* nextLoop = *i;
    if(_immOuterLoop == NULL || 
       nextLoop->_loopBody.size() < _immOuterLoop->_loopBody.size()) {
      _immOuterLoop = nextLoop;
    }
  }

  assert(_immOuterLoop != this);

  if(_immOuterLoop!=NULL) {
    _depth = _immOuterLoop->depthNest() + 1;
    _immOuterLoop->_immInnerLoops.insert(this);
  } else{
    _depth=1;
  }

  return _depth;    
}

//static Functions
void LoopInfo::checkNesting(LoopInfo* li1, LoopInfo* li2) {
  assert(li1!=li2);
  if (std::includes(li1->_loopBody.begin(), li1->_loopBody.end(),
                    li2->_loopBody.begin(), li2->_loopBody.end())) {
    li1->_innerLoops.insert(li2);
    li2->_outerLoops.insert(li1);
  }
}



bool LoopInfo::BBReachableHelper(BB* bb1, BB* bb2, set<BB*>& seen) {
//  cout << "trying bb: " << bb1->rpoNum() << " to " << bb2->rpoNum() << "\n";
  for(auto I = bb1->succ_begin(), E = bb1->succ_end(); I!=E; ++I) {
    BB* bb_next = *I;
    if(bb_next==bb2) { //base case
      //cout << "found\n";
      return true;
    } 
//    cout << "  new possibility: bb:" << bb_next->rpoNum() << "\n";

    if(inLoop(bb_next) && bb_next != loop_head() && seen.count(bb_next)==0
       && bb_next->freq()!=0) {
//      cout << "  Accepted \n";
      seen.insert(bb_next);
      bool success=BBReachableHelper(bb_next,bb2,seen);
      if(success) {
        return true;
      }
//      cout << "nope\n";
    } 
  }
  return false;
}

bool LoopInfo::BBReachable(BB* bb1, BB* bb2) {
  set<BB*> seen;
  if(bb1==bb2 || bb2==loop_head()) {
    return false;
  }
  return BBReachableHelper(bb1,bb2,seen);
}

bool LoopInfo::reachable_in_iter(Op* op1, Op* op2) {
  BB* bb1 = op1->bb();
  BB* bb2 = op2->bb();
  if(bb1==bb2) {
    return op1->bb_pos() < op2->bb_pos();
  } 
  return BBReachable(bb1,bb2); 
}

/*
 * Implement this later
void path(Op* op1, Op* op2) {

}
*/

//implicit use of _dist, must be called after calcRecurrences, I know, more
//lazy programming
int LoopInfo::longest_dist(std::set<Op*>& op_set) {   
  int longest=0;
  for(Op* op1 : op_set) {
    for(Op* op2 : op_set) {
      if(op1!=op2 && _dist[op1->t][op2->t]>0) {
        int length = -_dist[op1->t][op2->t];
        if(length > longest) {
          longest=length;
        }
      }
    } 
  }
  return longest;
}

//returns longest path which rec_chains take (assume rec_chains sorted)
//through the op_set in out_chain<first_op,last_op>
int longest_path_through_ops(std::vector<rec_chain>& rec_chains,
                             std::set<Op*>& op_set,
                             std::pair<Op*,Op*>& out_chain) {
  for(rec_chain& chain :rec_chains) {
    Op* start_chain=NULL,* stop_chain=NULL;
    int total_length=0;

    for(Op* op : chain.ops) {
      if(start_chain==NULL) {
        if(op_set.count(op)) {
          start_chain=op;
          stop_chain=op; //updated later
          total_length+=op->avg_lat();
        }
      } else { //we started the chain
        if(op_set.count(op)==0) {
          out_chain=make_pair(start_chain,stop_chain);
          return total_length-stop_chain->avg_lat();
        }
        total_length+=op->avg_lat();
        stop_chain=op; //this keeps getting overridden
      }
    }
  }
  return 0;
}


void update_dist_next(std::vector<std::vector<int>>& dist,
                      std::vector<std::vector<Op*>>& next,
                      Op* op1, Op* op2, int new_lat) {
  if(new_lat < dist[op1->t][op2->t]) {
    dist[op1->t][op2->t]=new_lat;
    next[op1->t][op2->t]=op2;
  }
}

void LoopInfo::add_rec_edge(std::vector<std::vector<int>>& dist,
                            std::vector<std::vector<Op*>>& next,
                  Op* op1, Op* op2, Op* tie_op1, Op* tie_op2) {
  if(reachable_in_iter(op1,op2)) {
    int new_lat = -op1->avg_lat();
    update_dist_next(dist,next,op1,op2,new_lat);

    if(op2 == tie_op1) {
      update_dist_next(dist,next,op1,tie_op2,new_lat);
    }
    if(op2 == tie_op2) {
      update_dist_next(dist,next,op1,tie_op1,new_lat);
    }

    //cout << op1->id() << " -> " << op2->id() << " l:" << new_lat << "\n";
  }
}

rec_chain recover_path(std::vector<std::vector<Op*>>& next, Op* op1, Op* op2) {
  rec_chain chain;
  if(next[op1->t][op2->t]!=NULL) {
    std::set<Op*> ops_seen;
    ops_seen.insert(op1);

    Op* cop = op1;
    //cout << cop->id();
    chain.add_op(cop);

    while(cop!=op2) {
      cop = next[cop->t][op2->t];      
      
      if(ops_seen.count(cop)||cop==NULL) {
        //cout << "ERROR:  Recover Path Failed!\n";
        return rec_chain();
      } 

      ops_seen.insert(cop);

      //cout << "," << cop->id();
      chain.add_op(cop);
    }
  }
  return chain;
}

void update_length(std::vector<std::vector<int>>& dist,
                   std::vector<std::vector<Op*>>& next,
                   std::map<Op*,int>& longest_recs,
                   std::vector<rec_chain>& rec_chains,
                   Op* op1, Op* op2, int& longest) {

  if(dist[op1->t][op2->t]<0) {
    //cout << op1->id() << "->" << op2->id() 
    //     << " has length: " << dist[op1->t][op2->t] << " : ";

    rec_chain chain = recover_path(next,op1,op2);
    if(chain.length>0) {
      rec_chains.push_back(chain);
    } 
    //cout << "\n";

    int length = -dist[op1->t][op2->t];
    if(length>0 && length > longest_recs[op1]) {
      longest_recs[op1]=length;
    }
    if(length>longest) {
      longest=length;
    }
  }
}

bool LoopInfo::kinda_inner() {
  if(isInnerLoop()) {
    return true;
  }
  if(_immInnerLoops.size()==1) {
    LoopInfo* ili = *_immInnerLoops.begin();
    if(ili->_loopCount==0) {
      BB* head_bb = ili->loop_head();
      if(head_bb && head_bb->freq()==0) {
        return true;
      }
    }
  }
  return false;
}

int LoopInfo::calcRecurrences(std::vector<Op*>& long_loop_recs,
                              std::vector<rec_chain>& rec_chains,
                              Op* tie_op1, Op* tie_op2) {
  //Only longest recurrences for pure inner loops
  if(!kinda_inner() || containsCallReturn()) {
    return 0;
  }

  //Floyd Warshall (lazy programming with maps, I know, I know -- time constarints...)
  //Since we are using the shortest path algorithm for longest path, we need to 
  //negate weights... unfortunately we can't handle arbitrary graphs for longest paths,
  //(cause it's NP complete and I don't want to implement a crazy algorithm), so we are
  //going to create a DAG and do that instead.


  std::vector<Op*> op_vec;
  std::set<Op*> op_set;

  int num_items = 0;

  //setup dist matrix
  for(auto I=body_begin(),E=body_end();I!=E;++I) {
    BB* bb = *I;

    for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
      Op* op = *II;
      if(op->shouldIgnoreInAccel()) {
        continue;
      }

      op_vec.push_back(op);
      op_set.insert(op);//more lazy programming : ))

      op->t=num_items++;
    }
  }

  auto& dist = _dist;
  auto& next = _next; 

  dist.clear();
  next.clear();

  dist.resize(num_items);
  next.resize(num_items);
  for(int i = 0; i < num_items; ++i) {
    dist[i].resize(num_items);
    next[i].resize(num_items);

    for(int j = 0; j < num_items; ++j) {
      dist[i][j]=0;
      next[i][j]=NULL;
    }
  }

  //setup dist matrix
  for(auto I=body_begin(),E=body_end();I!=E;++I) {
    BB* bb = *I;

    for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
      Op* op = *II;
      if(!op_set.count(op)) {
        continue;
      }

      //Print Data Dependences
      for(auto ui=op->adj_u_begin(),ue=op->adj_u_end();ui!=ue;++ui) {
        Op* uop = *ui;
        if(op_set.count(uop)) {
          add_rec_edge(dist,next, op, uop, tie_op1, tie_op2);
        }
      }

      //Print Ctrl Free Memory Dependences (we won't consider others, w/e)
      for(auto di=op->m_cf_begin(),de=op->m_cf_end();di!=de;++di) {
        Op* d_mop = *di;
        if(!op_set.count(d_mop) || (d_mop->isLoad() && op->isLoad())) {
          continue;
        }
        add_rec_edge(dist,next, d_mop, op, tie_op1, tie_op2);
      }
    }

    //Since we are targetting NLA, we have to add control deps too, as edges
    Op* bb_end_inst = bb->lastOp();
    if(bb_end_inst && op_set.count(bb_end_inst) && func()->pdomR_has(bb)) {
      for(auto ii = func()->pdomR_begin(bb), ee = func()->pdomR_end(bb); ii!=ee; ++ii){
        //BB* cond_bb = ii->first;
        BB* trigger_bb = ii->second;

        if(!inLoop(trigger_bb)) {
          continue;
        }

//        cout << bb->rpoNum() << " to " << trigger_bb->rpoNum() << ": ";
        if(BBReachable(bb,trigger_bb)) {
//          cout << "Yep\n";
          for(auto bi=trigger_bb->op_begin(),be=trigger_bb->op_end();bi!=be;++bi) {
            Op* t_op = *bi;
            if(op_set.count(t_op)) {
              add_rec_edge(dist,next, bb_end_inst, t_op, tie_op1, tie_op2);
            }
          }
        } else {
//          cout << "Nope\n";
        }

      }
    }

  }

  //Floyd Warshall
  for(unsigned k = 0; k<op_vec.size(); ++k) {
    for(unsigned i = 0; i<op_vec.size(); ++i) {
      for(unsigned j = 0; j<op_vec.size(); ++j) {

        if(i==k || k==j || i==j ||
           dist[i][k]==0 ||  dist[k][j]==0) {
          continue;
        }
        if(dist[i][k] + dist[k][j] < dist[i][j]) {

          dist[i][j] = dist[i][k] + dist[k][j];
          next[i][j] = next[i][k];
        }
      }
    }
  }

  std::map<Op*,int> longest_recs;
  int longest=0;

  //print interesting facts
  //cout << "data dep facts:" << nice_name_full() << "\n";
  for(auto I=body_begin(),E=body_end();I!=E;++I) {
    BB* bb = *I;

    for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
      Op* op = *II;
      if(!op_set.count(op)) {
         continue;
      }
      for(auto di=op->adj_d_begin(),de=op->adj_d_end();di!=de;++di) {
        Op* dop = *di;
        if(!op_set.count(dop)) {
           continue;
        }
        if(!reachable_in_iter(dop,op)) {
          update_length(dist,next,longest_recs,rec_chains,op,dop,longest);
          if(tie_op1==op) {
            update_length(dist,next,longest_recs,rec_chains,tie_op2,dop,longest);         
          }
          if(tie_op2==op) {
            update_length(dist,next,longest_recs,rec_chains,tie_op1,dop,longest);         
          }
        }
      }
    }
    if(isLatch(bb)) {
      Op* t_op = bb->lastOp();
      if(t_op && op_set.count(t_op)) {
        //cout << "latch facts:\n";
        BB* head = loop_head();
        for(auto II=head->op_begin(),EE=head->op_end();II!=EE;++II) {
          Op* h_op = *II;
          if(!op_set.count(h_op)) {
            continue;
          }
          update_length(dist,next,longest_recs,rec_chains,h_op,t_op,longest);
          if(tie_op1==h_op) {
            update_length(dist,next,longest_recs,rec_chains,tie_op2,t_op,longest);
          }
          if(tie_op2==h_op) {
            update_length(dist,next,longest_recs,rec_chains,tie_op1,t_op,longest); 
          }      
        }
      }
    }
  }

  std::sort(rec_chains.begin(),rec_chains.end());

  long_loop_recs.clear();
  for(auto iter : longest_recs) {
    Op* op = iter.first;
    int length = iter.second;
    if(length > (longest*2)/3 || length > longest-3) {
      long_loop_recs.push_back(op);
    }
  }

  return longest;
}






//Keep track of address Paterns
void LoopInfo::opAddr(Op* op, uint64_t addr, uint8_t acc_size) { 
   static std::set<Op*> null_op;
   if(addr==0 && !null_op.count(op)) {
     null_op.insert(op);
     std::cerr << "!!!!!!!!!!!!!!!!!!!!!! Warning: Null Memory Access at:" << op->id() << "!!!!!!!!!!!!!!!!!!!!!!\n";
     std::string disasm =  ExecProfile::getDisasm(op->cpc().first, op->cpc().second);
     std::cerr << disasm;
   }

   if(_opStriding.count(op)!=0 && _opStriding[op]==false) {
     //if we're not striding, faghettabout it
     return;
   }
  
   //first time i get this, setup my data and leave
   if(_prevOpAddr.count(op)==0) {
     _prevOpAddr[op]=addr;
     _prevOpIter[op]=_curIter; 
     return;
   }

   //Calculate diff between 
   uint64_t prevAddr = _prevOpAddr[op]; 
   int64_t addrDiff = addr - prevAddr;
 
   int prevIter = _prevOpIter[op];
   int iterDiff = _curIter - prevIter;

   if(iterDiff==0) {
     //this should only happen in a nested irreducible loop
     //this makes me sad -- we should fix this probably at some point
     return;
     //assert(iterDiff!=0); //then we must have forgot to increment curIter
   }

   _prevOpAddr[op]=addr; //keep track of old values
   _prevOpIter[op]=_curIter; 

   //Now calculate stride and rem
   int stride = addrDiff / iterDiff;
   int rem = addrDiff % iterDiff;

   if(rem!=0) { //this is real bad, so lets elimitate it right away
     _opStriding[op]=false;
     return;
   } 

   if(_opStride.count(op)==0 ) { //first stride, then set to true and set stride
     _opStriding[op]=true;
     _opStride[op]=stride;
     return;
   }

   if(_opStride[op]!=stride) { //set to false if we saw bad stride;
     _opStriding[op]=false;
   }
}


bool LoopInfo::useOutsideSet(std::set<Op*>& relevantOps, Op* op) {
   for(auto usi=op->adj_d_begin(),use=op->adj_d_end();usi!=use;++usi) {
    Op* use_op = *usi;
    if(relevantOps.count(use_op)>0) {
      return true;
    }
  }
  return false; 
}
bool LoopInfo::depOutsideSet(std::set<Op*>& relevantOps, Op* op) {
  for(auto dsi=op->adj_d_begin(),dse=op->adj_d_end();dsi!=dse;++dsi) {
    Op* dep_op = *dsi;
    if(relevantOps.count(dep_op)>0) {
      return true;
    }
  }
  return false;
}



bool LoopInfo::dataDepBT(std::set<Op*>& relevantOps,Op* cur_op, Op* search_op,
                                     std::set<Op*>& seenOps) {
  if(seenOps.count(cur_op) || relevantOps.count(cur_op) ==0) {
    return false;
  }
  seenOps.insert(cur_op);
  for(auto ui=cur_op->adj_u_begin(),ue=cur_op->adj_u_end();ui!=ue;++ui) {
     Op* look_op = *ui;
     if(!dependenceInPath(relevantOps,cur_op,look_op)) {
       continue; //keep searching if this isn't a good dependence
     }

     if(look_op==search_op) {
       return true; //found directly
     }
     if(dataDepBT(relevantOps,look_op,search_op,seenOps)) {
       return true; //found inside some other use
     }
  }
  return false; //couldn't find anywhere
}

bool LoopInfo::dataDepBT(std::set<Op*>& relevantOps,Op* dop, Op* op) {
  std::set<Op*> seenOps;
  return dataDepBT(relevantOps,dop,op,seenOps);
}


void LoopInfo::compatUse(Op* uop,
                     std::set<Op*>& ops, std::set<std::pair<Op*,Op*>>& closeSet, 
                     Op* orig_op, Op* cur_op, CFU_node* cur_fu,
                     std::set<Op*>& doneOps, std::set<CFU_node*>& doneFUs) {
  if(doneOps.count(uop)!=0 || !dependenceInPath(ops,cur_op,uop)) {
    return;
  }
  for(auto ii=cur_fu->outs_begin(), ee=cur_fu->outs_end(); ii!=ee; ++ii) {
    CFU_node* ufu = *ii;
    if(doneFUs.count(ufu) != 0) {
      return;
    }
    checkCompatible(ops,closeSet,orig_op,uop,ufu,doneOps,doneFUs);
  }
}

void LoopInfo::compatDep(Op* dop,
                     std::set<Op*>& ops, std::set<std::pair<Op*,Op*>>& closeSet, 
                     Op* orig_op, Op* cur_op, CFU_node* cur_fu,
                     std::set<Op*>& doneOps, std::set<CFU_node*>& doneFUs) {
  if(doneOps.count(dop)!=0 || !dependenceInPath(ops,dop,cur_op)) {
    return;
  }
  for(auto ii=cur_fu->ins_begin(), ee=cur_fu->ins_end(); ii!=ee; ++ii) {
    CFU_node* dfu = *ii;
    if(doneFUs.count(dfu) != 0) {
      return;
    }
    checkCompatible(ops,closeSet,orig_op,dop,dfu,doneOps,doneFUs);
  }
}


void LoopInfo::checkCompatible(std::set<Op*>& ops,
                     std::set<std::pair<Op*,Op*>>& closeSet, 
                     Op* orig_op, Op* cur_op, CFU_node* cur_fu,
                     std::set<Op*> doneOps, std::set<CFU_node*> doneFUs) {

  if(!CFU_node::kind_match(cur_op,cur_fu)) {
    return; //didn't match, bail out
  }
  if(orig_op!=cur_op) {
    closeSet.insert(std::make_pair(orig_op,cur_op));
//    cout << orig_op->id() << ") ";
//    cout << cur_op->id() << "." << cur_fu->ind() << ": ";
//    for(auto const& elem : doneOps) {
//      cout << " " << elem->id();
//    }
//    cout << ":";
//    for(auto const& elem : doneFUs) {
//      cout << " " << elem->ind();
//    }
//    cout << "\n";
  }
  doneOps.insert(cur_op);
  doneFUs.insert(cur_fu);

  for(auto ui=cur_op->adj_u_begin(),ue=cur_op->adj_u_end();ui!=ue;++ui) {
    compatUse(*ui,ops,closeSet,orig_op,cur_op,cur_fu,doneOps,doneFUs);
  }

  for(auto ui=cur_op->m_use_begin(),ue=cur_op->m_use_end();ui!=ue;++ui) {
    if(!dataDepBT(ops,cur_op,*ui)) {
      compatUse(*ui,ops,closeSet,orig_op,cur_op,cur_fu,doneOps,doneFUs);
    }
  }

  for(auto di=cur_op->adj_d_begin(),de=cur_op->adj_d_end();di!=de;++di) {
    compatDep(*di,ops,closeSet,orig_op,cur_op,cur_fu,doneOps,doneFUs);
  }
  for(auto di=cur_op->m_cf_begin(),de=cur_op->m_cf_end();di!=de;++di) {
    if((*di)->isLoad() && cur_op->isLoad()) {
      continue; //TODO: LoadLoad
    }
    if(!dataDepBT(ops,*di,cur_op)) {
      compatDep(*di,ops,closeSet,orig_op,cur_op,cur_fu,doneOps,doneFUs);
    }
  }

}



void LoopInfo::calcPossibleMappings(std::set<Op*>& ops, CFU_set* cfu_set, 
                          std::set<std::pair<Op*,Op*>>& closeSet) {
  std::set<Op*> emptyOps;
  std::set<CFU_node*> emptyNodes;

  for(auto i=ops.begin(),e=ops.end();i!=e;++i) {
    Op* op = *i;

    for(auto cfui = cfu_set->cfus_begin(), 
             cfue = cfu_set->cfus_end(); cfui != cfue; ++cfui) {
      CFU* cfu = *cfui;
      for(auto ni = cfu->nodes_begin(), ne = cfu->nodes_end(); ni != ne; ++ni) {
        CFU_node* cfu_node = *ni;

        checkCompatible(ops,closeSet,op,op,cfu_node,emptyOps,emptyNodes);

      }
    }
  }
}

void LoopInfo::printLoopDeps(std::ostream& out) {
    if(isLoopFullyParallelizable()) {
      out << "Parallelizable!\\n";
    }
    VectorizationLegality vl;  //this is really crappy

    if(vl.canVectorize(this,true,0.5)) {
      out << "Vectorizable at 0.5!\\n";
    } else {
      out << "Not Vectorizable at 0.5!\\n";
    }
    out << "SuperBlockEfficiency: " << superBlockEfficiency() << "\\n";

    if(vl.hasVectorizableMemAccess(this,true)) {
      out << "Vectorizable Mem!\\n";
    } else {
      out << "Not Vectorizable Mem!\\n";
    }
    if(vl.old_loop_dep(this)) {
      out << "Vectorizable Accordng To Venkat!\\n";
    } else {
      out << "Not Vectorizable Accordng To Venkat!\\n";
    }

    for(int i = 0; i < 3; ++i) {
      auto ldsi = ld_wr_begin(), ldse = ld_wr_end();
      if(i==0) {
        out << "wr dep: ";
      } else if(i==1) {
        out << "ww dep: ";
        ldsi = ld_ww_begin();
        ldse = ld_ww_end();
      } else if(i==2) {
        out << "rw dep: ";
        ldsi = ld_rw_begin();
        ldse = ld_rw_end();
      }       
      auto orig_ldsi = ldsi;
      for(;ldsi!=ldse;++ldsi) {
        LoopInfo::LoopDep dep = *ldsi;
        if(ldsi != orig_ldsi) {
          out << ", ";
        }
        
        for(auto di=dep.begin(),de=dep.end();di!=de;++di) {
          if(di != dep.begin()) {
            out << " ";
          }
          out << *di;
        }
      }
      out << "\\n";
    }

}

void LoopInfo::printSGPartText(std::ostream& out,
                           std::string resultfile, 
                           std::string fixes,
                           CFU_set* cfu_set) {

out << "file stdout / \"" << resultfile << "\" /;\n"
    << "stdout.pc=8;stdout.pw=4096;put stdout;\n";
out.flush();


  cfu_set->print_to_stream(out);  
out.flush();

const char * text_pre = R"HERE(

binary variable Mvn(v,n), Bvv(v,v);
positive variable Tv(v);
positive variable U(v,v,fu);
positive variable W(v);
W.up(v)=1;
positive variable LAT, READS, WRITES, CP;
variable GOAL;
alias(v,v1,v2,v3,v4,vi);
alias(n,n1,n2);
alias(fu,fu1,fu2);
alias(s,s1,s2);
positive variable PC(v1,v2);

* if v1 is close to v2
set close_dep(v1,v2);
set iter_close(v1,v2);

A(v1,v2)$(D(v1,v2))=YES;

close_dep(v1,v2)$(ORD(v1) eq ORD(v2)) = YES;
scalar depth;
for (depth = 0 to 4,
  loop((v1,v2,v3)$( (A(v2,v3) and close_dep(v1,v2)) or
                    (A(v3,v2) and close_dep(v2,v1))),
    iter_close(v1,v3) = YES;
  );
  close_dep(v1,v2)$iter_close(v1,v2)=YES;
);

*display close_dep;

Bvv.fx(v1,v2)$(not possDep(v1,v2))=1;

)HERE";

out.flush();



out << text_pre;
out.flush();

out << fixes;
out.flush();

const char * text = R"HERE(

* generate compatibility matrix
set c(v,n);
loop(k,
    c(v,n)$(kv(v,k) and kn(n,k))=YES;
);
*display c;
c(v,'nreg')=YES;


Equations 
  map1(v),
  map2(v),
*recently removed route1, its redundant with boundary
*  route1(v,v,n), 
  regforce(v,v),
*  init_used(v,n),
*  prop_used1(v,v,n,v),
*  prop_used2(v,v,n,v),
*  restrict_use(v,n),

  transitive_sameness1(v,v,v),
  transitive_sameness2(v,v,v),
  transitive_sameness3(v,v,v),
  restrict_fu(v,v,n),

  boundary(v,v,n),
  timing1(v,v),
*  timing2(v,v),
  timing3a(v,v,v),
  timing3b(v,v,v),
  path(v,v),
  c_cp,
  c_lat(v),
  c_writes,
  c_reads,
  c_goal
  ;


*All nodes mapped to some FU. (include kind in this later)
map1(v)..                    sum(fu$     c(v,fu),  Mvn(v,fu)) =e= 1;
map2(v)..                    sum(fu$(not c(v,fu)), Mvn(v,fu)) =e= 0;

*route1(v1,v2,fu2)$(A(v1,v2) and c(v2,fu2)).. 
*      Mvn(v2,fu2) =l= sum(n1$(c(v1,n1) and Hnn(n1,fu2)), Mvn(v1,n1));

regforce(v1,v2)$A(v1,v2).. Bvv(v1,v2) =l= W(v1);

*init_used(v1,fu1)$(c(v1,fu1)).. U(v1,v1,fu1) =g= Mvn(v1,fu1); 
*prop_used1(v1,v2,fu1,v3)
*  $(A(v1,v2) and possDep(v2,v3)).. 
*  U(v2,v3,fu1) =g= U(v1,v3,fu1) - Bvv(v1,v2);
*prop_used2(v1,v2,fu1,v3)
*  $(A(v1,v2) and possDep(v1,v3))..
*  U(v1,v3,fu1) =g= U(v2,v3,fu1) - Bvv(v1,v2);
*restrict_use(v1,fu)$(c(v1,fu)).. 
*  sum(v2$(c(v2,fu) and possDep(v1,v2)),U(v1,v2,fu)) =l= 1;


* This equation allows incomming edges to any node. TODO: Split this into
* two equations to only proper inputs to allow incomming edges.
boundary(v1,v2,fu2)$(A(v1,v2) and c(v2,fu2))..
         Bvv(v1,v2) =g= -sum(fu1$(c(v1,fu1) and Hnn(fu1,fu2)),Mvn(v1,fu1)) + Mvn(v2,fu2);

transitive_sameness1(v1,v2,v3)$(ORD(v1) lt ORD(v2) and ORD(v2) lt ORD(v3) and
                              close_dep(v1,v2) and close_dep(v2,v3) and close_dep(v1,v3))..
                              (1-Bvv(v1,v2)) + (1-Bvv(v2,v3)) -1 =l= (1-Bvv(v1,v3));

transitive_sameness2(v1,v2,v3)$(ORD(v1) lt ORD(v2) and ORD(v2) lt ORD(v3) and
                              close_dep(v1,v2) and close_dep(v2,v3) and close_dep(v1,v3))..
                              (1-Bvv(v1,v3)) + (1-Bvv(v2,v3)) -1 =l= (1-Bvv(v1,v2));

transitive_sameness3(v1,v2,v3)$(ORD(v1) lt ORD(v2) and ORD(v2) lt ORD(v3) and
                              close_dep(v1,v2) and close_dep(v2,v3) and close_dep(v1,v3))..
                              (1-Bvv(v1,v2)) + (1-Bvv(v1,v3)) -1 =l= (1-Bvv(v2,v3));


* Enforce that !Bvv(v1,v2) -> M(v,fu1), M(v,fu2) fu1!=fu2
restrict_fu(v1,v2,fu)$(c(v1,fu) and c(v2,fu) and (ORD(v1) lt ORD(v2)) and possDep(v1,v2))..
                              Mvn(v1,fu) + Mvn(v2,fu) =l= Bvv(v1,v2) +1;

timing1(v1,v2)$(A(v1,v2))..
                              Tv(v2) =g= Bvv(v1,v2) + L(v1) + Tv(v1);
*                              Tv(v2) =g= Bvv(v1,v2) + Tv(v1);

*timing2(v1,v2)$(A(v1,v2))..
*                              Tv(v2) =l= CARD(v)*Bvv(v1,v2) + Tv(v1);

timing3a(v1,v2,vi)$(possDep(v1,v2) and A(vi,v2) and (ORD(v1) lt ORD(v2)) and (ORD(v1) ne ORD(vi)))..
                    Tv(v1) =g= (Bvv(vi,v2)-Bvv(v1,v2)-1)*sum(v,L(v)+1) + L(vi) + Tv(vi)+1;

timing3b(v1,v2,vi)$(possDep(v1,v2) and A(vi,v1) and (ORD(v1) lt ORD(v2)) and (ORD(v2) ne ORD(vi)))..
                    Tv(v2) =g= (Bvv(vi,v1)-Bvv(v1,v2)-1)*sum(v,L(v)+1) + L(vi) + Tv(vi)+1;

path(v1,v2)$(P(v1,v2) ne 0).. PC(v1,v2) =e= Tv(v2) - Tv(v1) - P(v1,v2);

c_cp.. CP =e= sum((v1,v2)$(P(v1,v2) ne 0),PC(v1,v2));


c_writes.. WRITES =e= sum(v, W(v));
c_reads..  READS  =e= sum((v1,v2)$(A(v1,v2)), Bvv(v1,v2));

c_lat(v)$(sum(v2$A(v,v2),1) eq 0).. LAT    =g= Tv(v) - O_LAT;
c_goal..   GOAL   =e= 5*CP + LAT_F*LAT + 2*WRITES + READS;


option optca = 0.9999;
option optcr = 0.03;
option reslim = 250;
option threads = 16;

Model sg/all/;
sg.limrow=1;
sg.limcol=1;
solve sg using mip minimizing GOAL;

*display Tv.l;
*display Mvn.l;
*display Bvv.l;
*display LAT.l;

scalar t;

*loop(v,
*put v.tl Tv.l(v)/
*);

*loop((v,n)$Mvn.l(v,n),
*put v.tl n.tl/
*);

* current supbgraph starters at this time step
set par(v);
* done subgraphs
set done(v);
* the current subgraph which we are printing
set curr(v);

loop(v,par(v)=NO);
loop(v,done(v)=NO);

for (t = smin(v,Tv.l(v)) to smax(v,Tv.l(v)),
*  put "TIME" t" "/

  loop(v,
    if(Tv.l(v) >= t-0.05 and Tv.l(v) <= t+0.05,
       par(v)=YES;
    );
  );
  loop(v,
    if(par(v) and not done(v),
      loop(v1,curr(v1)=NO);

      loop((s,n)$(Mvn.l(v,n) and cfu(s,n)),
        put s.tl;
      );

      done(v)=YES;
      curr(v)=YES;

      loop(vi$(ORD(vi) < ORD(v) and (1-Bvv.l(vi,v))),
        curr(vi)=YES;
        done(vi)=YES;
      )
      loop(vi$(ORD(v) < ORD(vi) and (1-Bvv.l(v,vi))),
        curr(vi)=YES;
        done(vi)=YES;
      )



*      loop(v3$par(v3),
*        loop((v1,v2,fu1,fu2)$(A(v1,v2)    and Hnn(fu1,fu2)  and
*                              Mvn.l(v1,fu1) and Mvn.l(v2,fu2) and
*                              (curr(v2) or curr(v1))
*                              and Tv.l(v1) >= t-0.05 and Tv.l(v1) <= t+0.05
*                              and Tv.l(v2) >= t-0.05 and Tv.l(v2) <= t+0.05
*                              ),
*          curr(v1)=YES;
*          curr(v2)=YES;
*          par(v1)=NO;
*          par(v2)=NO;
*        );
*      );
      loop((v1,fu1)$(curr(v1) and Mvn.l(v1,fu1)),
        put v1.tl fu1.tl;
      );
      put /;
    );
  );

*  put /;
);


)HERE";

    out << text;
out.flush();

}

void LoopInfo::printGamsPartitionText(std::ostream& out,int count,
                              std::string resultfile,
                              std::string fixes,
                              int nMemDepOps, int max_beret_size, int max_mem_ops) {
  int maxSize = max_beret_size;
  int minSize = 2;
 
  int maxSEBs =floor((((count+maxSize-1)/maxSize)*1.10+nMemDepOps));
  //if(maxSEBs < cout+maxSize
  
  if(max_mem_ops==1) {
    minSize=1;
  }

  out << "scalar K/" << maxSize << "/;\n"
      << "scalar minK/" << minSize << "/;\n"
      << "scalar maxM/" << max_mem_ops << "/;\n"

      << "Set l/l1*l" << maxSEBs <<"/;\n";

const char * text = R"HERE(

set first_l(l) first element of l
    last_l(l)  last element of l;

    first_l(l) = ord(l) = 1;
    last_l(l) = ord(l) = card(l);

alias(u,v);
alias(l,l1,l2,l3);

variable interf_edges;
*integer variable x(v,l),y(v,l);
integer variable x(v),y(v,l);

variable num_mapped(l);
binary variable on(l);

Equations one_part(v),calc_mapped(l),part_size(l),mem_size(l),min_size(l),on_ok(l),order(l,l),order_on(l,l),interf_calc(u,v,l),obj;
one_part(v)..    sum(l,y(v,l)) =e= 1;
calc_mapped(l).. num_mapped(l) =e= sum(v,y(v,l));
part_size(l)..   num_mapped(l) =l= on(l) * K;
min_size(l)..    num_mapped(l) =g= on(l) * minK;
mem_size(l)..   sum(v$(M(v)),y(v,l)) =l= on(l) * maxM;

*on.fx(l)=1;

on_ok(l)..   num_mapped(l) =g= on(l);
order_on(l1,l2)$(ORD(l1)+1=ORD(l2)).. on(l1) =g= on(l2);
order(l1,l2)$(ORD(l1)+1=ORD(l2)).. num_mapped(l1) =g= num_mapped(l2);

*interf_force(v,l).. x(v,l) =l= y(v,l);
interf_calc(u,v,l)$(A(u,v)).. y(u,l) =l= y(v,l) + x(u);

variable bucket(u);
Equations calcBucket(u), straighten2(u,v), memdep(u,v);
calcBucket(u).. bucket(u) =e= sum(l,ORD(l)*y(u,l));
straighten2(u,v)$(A(u,v)).. bucket(u) =l= bucket(v);

*prevent memdependencies
memdep(u,v)$(D(u,v)).. bucket(u) + 1 =l= bucket(v);

Equations interf_calc2(u,v);
interf_calc2(u,v)$(A(u,v)).. bucket(u) + x(u)*CARD(l) =g= bucket(v);

*Subgraph Flow
binary variable f(l,l);
f.fx(l,l)=0;
Equations flow1(v,v,l,l),flow2(l,l,l);
flow1(u,v,l1,l2)$(A(u,v) and ORD(l1)<>ORD(l2)).. y(u,l1) + y(v,l2) - 1 =l= f(l1,l2);
flow2(l1,l2,l3)$(ORD(l1)<>ORD(l2) and ORD(l2)<>ORD(l3) ).. f(l1,l2) + f(l2,l3) -1 =l= f(l1,l3);

*Subgraph Timestamp
positive variable ts(l);
Equations sg_order(u,v,l,l);
sg_order(u,v,l1,l2)$(A(u,v) and ORD(l1)<>ORD(l2)).. 1000*(y(u,l1) + y(v,l2) - 2) + ts(l1) + 1 =l= ts(l2);

positive variable tsv(v);
Equations v2v_order(u,v),v2sg_order(v,l),sg2v_order(v,l,l),sg2sg_order(l,l);
v2v_order(u,v)$(A(u,v)).. tsv(u) + 1 =l= tsv(v);
v2sg_order(v,l).. -100 * (1- y(v,l)) + tsv(v) +1 =l= ts(l);
sg2v_order(v,l1,l2)$(ORD(l1)+1=ORD(l2))..
              -100 * (1- y(v,l2)) + ts(l1) +1 =l= tsv(v);
sg2sg_order(l1,l2)$(ORD(l1)+1=ORD(l2)).. ts(l1) +1 =l= ts(l2);

*Equations straighten(u,v,l,l);
*straighten(u,v,l1,l2)$(A(u,v) and ORD(l1) > ORD(l2)).. y(u,l1) + y(v,l2) =l= 1;

*Convexity
binary variable ancestorT(v,l);
binary variable descendantT(v,l);

Equations an1(v,v,l),an2(v,v,l),an3(v,l);
Equations de1(v,v,l),de2(v,v,l),de3(v,l);
Equations convexity(v,l);

an1(u,v,l)$(A(u,v)).. ancestorT(v,l) =g= y(u,l);
an2(u,v,l)$(A(u,v)).. ancestorT(v,l) =g= ancestorT(u,l);
an3(v,l).. ancestorT(v,l) =l= sum(u$(A(u,v)),y(u,l)+ancestorT(u,l));

de1(u,v,l)$(A(u,v)).. descendantT(u,l) =g= y(v,l);
de2(u,v,l)$(A(u,v)).. descendantT(u,l) =g= descendantT(v,l);
de3(u,l).. descendantT(u,l) =l= sum(v$(A(u,v)),y(v,l)+descendantT(v,l));

convexity(v,l).. ancestorT(v,l) + descendantT(v,l) - y(v,l) =l= 1;

*obj.. interf_edges =e= sum(v, x(v))+sum(l,on(l))*1/100;
*obj.. interf_edges =e= sum(v, x(v));
obj.. interf_edges =e= sum(v,x(v)) + sum(l,on(l))*1 + sum(l$(last_l(l)),ts(l))*1/100;
*obj.. interf_edges =e= sum(v,x(v)) + sum(l$(last_l(l)),ts(l));
*obj.. interf_edges =e= sum(l$(last_l(l)),ts(l));

option limrow = 1;
option limcol = 1;
option optca = 1.9999;
option optcr = 0.15;
option reslim = 100;
option threads = 16;
Model partition/one_part,calc_mapped,part_size,min_size,order_on,
*an1,an2,de1,de2,
*flow1,flow2,
*sg_order,
*sg2sg_order,
*v2v_order,v2sg_order,sg2v_order,
*convexity
*straighten,
interf_calc,
memdep,
mem_size,
interf_calc2,
calcBucket,
straighten2,
obj/;

)HERE";

    //FIXME/TODO/OMG:  These two lines seem to fix something about the here docs on
    //my implementation of gcc?  I really don't know what's going on, but without
    //them, the text printed is a series of @ symbols.  WEIRD.
    stringstream text_ss;
    text_ss << text;

    out << text;
    out << fixes;
    out << "solve partition using mip minimizing interf_edges;\n";

    out << "file stdout / \"" << resultfile << "\" /;\n"
    << "stdout.pc=8;stdout.pw=4096;put stdout;\n"
    << "loop(l,loop(v,if(y.l(v,l) <> 0,put v.tl ;););put /;);\n"
    << "display x.l\n"
    << "display y.l\n";
}

bool LoopInfo::printGamsPartitionProgram(std::string filename, CFU_set* cfu_set,
    bool gams_details,bool no_gams, int max_beret_size, int max_mem_ops, bool NLA) {

    _sgSchedBeret.reset();  
    BBvec& bbVec = getHotPath();
    return printGamsPartitionProgram(filename,
      bbVec, _sgSchedBeret, 
      cfu_set, gams_details, no_gams,NLA);
}


bool LoopInfo::scheduleNLA(CFU_set* cfu_set,   
                 bool gams_details, bool no_gams,
                 bool& attempted, uint64_t max_insts) { 
   _sgSchedNLA.reset();
   _sgSchedNLA.setCFUSet(cfu_set);
  return scheduleNLA(cfu_set, _sgSchedNLA, gams_details, no_gams,
                     attempted, max_insts);
}

void LoopInfo::inlinedBBs(std::set<FunctionInfo*>& funcsSeen,std::vector<BB*>& totalVec){
  for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
    FunctionInfo* fi = i->first.second;
    fi->inlinedBBs(funcsSeen,totalVec);
  }
  for(auto i=_immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
    LoopInfo* li=*i;
    li->inlinedBBs(funcsSeen,totalVec);
  }  
}


/* This algo Searches chunks up the outer loop into peices, and and schedules
 * each one at a time.  This will create a bunch of BBs
 */
bool LoopInfo::scheduleNLA(CFU_set* cfu_set, SGSched& sgSched,
                 bool gams_details, bool no_gams, 
                 bool& attempted, uint64_t max_insts) { 
  //Find successive basic blocks, throw those at the gams scheduler
  BBvec curVec,totalVec;
  int piece=0;

  uint64_t total_dyn = totalDynamicInlinedInsts();
  uint64_t total_static = inlinedStaticInsts();

  if(total_dyn < 512 || total_static * 10 > total_dyn) {
    sgSched.reset();
    return false; //skip if any are true
  }

  for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii, ++piece) {
    BB* bb = *ii;  
    totalVec.push_back(bb);
  }

  std::set<FunctionInfo*> funcsSeen;
  //funcsSeen.insert(func()); hopefully shouldn't see myself

  if(containsCallReturn()) {
    if(inlinedStaticInsts() < 1536) { //little more than we want, fudge for removed ops
//      uint64_t my_dyn = numInsts();
//      if(((double)my_dyn)/((double)total_dyn) < 0.1 ){
//        sgSched.reset();
//        return false; //skip if adding outer loop doesn't matter much
//      }
  
      std::cout << "b";
      inlinedBBs(funcsSeen,totalVec);
      
    } else {
      sgSched.reset();
      return false;
    }
  }

  sgSched.setFuncs(funcsSeen);
  attempted=true;
  //for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii, ++piece) 
  for(auto ii=totalVec.begin(),ee=totalVec.end();ii!=ee;++ii, ++piece) {
    BB* bb = *ii;  

    curVec.push_back(bb);
   
    if(bb->succ_size()!=1 || (*bb->succ_begin())->pred_size()!=1 || ii==_rpo.end()) {
      //time to schedule!
      stringstream ss;
      ss << "schedNLA." << id() << "." << piece; 
      bool ret = printGamsPartitionProgram(ss.str(),
                     curVec,sgSched,
                     cfu_set, gams_details, no_gams,100,100, true/*NLA*/); 
      if(!ret) {
        sgSched.reset();
        return false; //fail if any piece fails
      }

      curVec.clear();
    }
  }
  return true;
}


bool LoopInfo::printGamsPartitionProgram(std::string filename,
     BBvec& bbVec,
     SGSched& sgSched,
     CFU_set* cfu_set, bool gams_details,bool no_gams,
     int max_beret_size, int max_mem_ops, bool NLA) {

  static int id=0;

  unsigned orig_size = sgSched.opSet().size();
  unsigned orig_sgs = sgSched.numSubgraphs();

  if(orig_size==0) {
   id=0;
  }

  if(cfu_set && !no_gams) {
    //check max size of bb()
    for(auto const& bb : bbVec) {
      if(bb->len() > DONT_EVEN_TRY_GAMS_SIZE) {
        std::cerr << "bb too big for cfu scheduling!\n";
        return false;
      }
    }
  }

  //Recursive resched if too big -- kind of dumb, but it works for now
  if(/*cfu_set && *//*!no_gams &&*/ bbVec.size()>1) { //cfu scheduling has no heuristic -- split up into many pgms
    int size=0;
    for(auto const& bb : bbVec) {
      size+=bb->len();
    }
  
    if(size > MAX_GAMS_SIZE) {
      //refactor to do many calls
      int cursize=0;
      BBvec newBBVec;
      bool worked=true;
  
      for(auto const& bb : bbVec) {
        if(bb->len() + cursize > MAX_GAMS_SIZE) {
          //std::cout << "aux scheduling, len:" << cursize << "\n";
          worked &= printGamsPartitionProgram(filename,newBBVec,sgSched,cfu_set, 
                             gams_details, no_gams, max_beret_size, max_mem_ops, NLA);

          if(!worked) {
            std::cerr << "failed 1!\n";
          }

          cursize=0;
          newBBVec.clear();
        }
  
        cursize+=bb->len();
        newBBVec.push_back(bb);

      }
      //last one
      //std::cout << "aux scheduling, len:" << cursize << "\n";
      worked &= printGamsPartitionProgram(filename,newBBVec,sgSched,cfu_set, 
                                          gams_details, no_gams, max_beret_size, max_mem_ops, NLA);

          if(!worked) {
            std::cerr << "failed 2!\n";
          }

      return worked; //recursively done!  (cheap hack, but w/e)
    }
  }

  BB* first_bb = *bbVec.begin();
  LoopInfo* inner_li = first_bb->func()->innermostLoopFor(first_bb);
  /*std::cout << "\n SCHEDULE -- " << first_bb->func()->nice_name();
  if(inner_li) {
    std::cout << "(inner li:" << inner_li->id() << ")";
  }

  for(auto const& bb : bbVec) {
    cout << " " << bb->rpoNum();
  }
  cout << "\n";*/


  ++id;
  stringstream ids;
  ids << filename << "." << id << ".gams";
  filename = ids.str();

  std::map<uint32_t, Op*> intOpMap;
  std::stringstream ss;

  ss << "$ONEMPTY;\n";

  if(cfu_set) {
    CFU_node::print_kinds(ss);
  }


  ss << "set v/";

  std::set<Op*> opSet;

  int countElements=0;
  //find all instructions in path;
  for(auto i=bbVec.begin(),e=bbVec.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
//      if((op->shouldIgnoreInAccel() || op->plainMove()) && !useOutsideLoop(op)) {
//        continue;
//      }
      //std::cout << op->func()->nice_name();
      if((op->shouldIgnoreInAccel() || op->plainMove())) {
        continue;
      }

      assert(op);
      opSet.insert(op);
      intOpMap[op->id()]=op;
     
      if(countElements++ != 0) { 
        ss << ",";
      }
      if(countElements%1024==1023) {
        ss << "\r\n";
      }
      ss << op->id();
    }
  }
  ss << "/;\n"; 

  if(countElements==0) {
    return true; //nothing to schedule
  }

/*  if(setContainsCallReturn(opSet)) 
    //if loop contains call, don't perform subgraph matching...
    return false;
  */

  std::ofstream out((string("gams/") + filename).c_str()); 
  out << ss.str();

  out << "set A(v,v)/";
  std::stringstream fixes,fixes2,streamD,streamM,streamK,streamL;
  countElements=0;
  unsigned countOps=0;
  int countElementsD=0,countElementsM=0,countElementsK=0;
  int nMemDepOps=0;
  //print graph
  for(auto i=bbVec.begin(),e=bbVec.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
      if(!opSet.count(op)) {
        continue;
      }
      countOps++;
      for(auto ui=op->adj_u_begin(),ue=op->adj_u_end();ui!=ue;++ui) {
        Op* uop = *ui;
        //Don't consider any irrelevant edges.
        if(!dependenceInPath(opSet,op,uop)) {
          fixes << "x.fx('" << op->id() << "')=1;\n"; // must write reg
          fixes2 << "Mvn.fx('" << op->id() << "','nreg')=1;"; // must write reg
          fixes2 << "W.fx('" << op->id() << "')=1;\n"; //must write reg
          continue;
        }
        if(countElements++ != 0) {
          out <<",";
        }
        if(countElements%1024==1023) {
          out << "\r\n";
        }
        out << op->id() << "." << uop->id();
      }
      if(op->isMem()) {
        if(countElementsM++ != 0) {
          streamM <<",";
        }
        streamM<<op->id();
        countElementsM++;
      }

      if(countElementsK++ != 0) { //can share with L
        streamK <<",";
        streamL <<",";
      }

      streamK << op->id() << "." << CFU_node::kindName(CFU_node::kindOf(op->opclass()));
      streamL << op->id() << " " << op->avg_lat();

      //put data dependence between ctrlFree memory dependences
      for(auto di=op->m_cf_begin(),de=op->m_cf_end();di!=de;++di) {
        Op* mop = *di;
        if(mop->isLoad() && op->isLoad()) {
          continue;
        }

        if(dependenceInPath(opSet,mop,op) && !dataDepBT(opSet,mop,op)) {
          if(countElementsD++ != 0) {
            streamD <<",";
          }
          streamD << mop->id() << "." << op->id();
          nMemDepOps++;
        }
      }

      if(op->numUses()==0 && op->numMemUses()==0) {
        fixes << "x.fx('" << op->id() << "')=0;\n"; //inst does not write reg
        fixes2 << "Mvn.fx('" << op->id() << "','nreg')=0;"; //inst does not write reg
        fixes2 << "W.fx('" << op->id() << "')=0;\n"; //inst does not write reg
      }
      if(op->isCtrl()) {
        fixes2 << "Bvv.fx('" << op->id() << "',v)$possDep('" << op->id() << "',v)=1;\n";
      }
      
    }
  }
  out << "/;\n"; 

  out << "set D(v,v)/";
  out << streamD.str();
  out << "/;\n"; 
  out << "set M(v)/";
  out << streamM.str();
  out << "/;\n"; 

  out << "parameter L(v)/";
  out << streamL.str();
  out << "/;\n"; 

  if(cfu_set) {
    out << "set kv(v,k)/";
    out << streamK.str();
    out << "/;\n"; 
  }


  
  if(cfu_set) {
    std::set<std::pair<Op*,Op*>> closeSet;
    calcPossibleMappings(opSet, cfu_set,closeSet);

    out << "set possDep(v,v)/";
    int countElements=0;
    for(auto i = closeSet.begin(), e = closeSet.end(); i!=e; ++i) {
      Op* op1 = i->first;
      Op* op2 = i->second;
      if(i!=closeSet.begin()) {
        out << ",";
      }

      if(countElements++%512==511) {
        out << "\n";
      }

      out << op1->id() << "." << op2->id();
    }
    out << "/;\n"; 



    if(!NLA) {
      out << "scalar LAT_F/1/;\n";
      out << "scalar O_LAT/0/;\n";
    }

    if(NLA) {
      std::vector<Op*> long_loop_recs;
      std::vector<rec_chain> rec_chains;
      int orig_rec_len = 0;
      if(inner_li) {
        inner_li->calcRecurrences(long_loop_recs,rec_chains);
      }
 
      //first print any important recurrences
      out << "parameter P(v,v)/";
      std::pair<Op*,Op*> rec_pair;
      if(inner_li) {
        int rec_len = longest_path_through_ops(rec_chains,opSet,rec_pair);
        if(rec_len>0 && rec_pair.first && rec_pair.second) {
          out << rec_pair.first->id() << "." << rec_pair.second->id() << " " << rec_len; 
        }
      }
      out << "/;\n";

      out << "scalar O_LAT/"; //original latency
      if(inner_li) {
        int length = longest_dist(opSet); //reads _dist implicitly
        out << length << "/;\n";
        out << "scalar LAT_F/4/;\n";
      } else {
        out << "0/;\n"; //fix this -- should be crit-path latency
        out << "scalar LAT_F/1/;\n";
      }

      for(Op* op : long_loop_recs) { //fix long recurrences to 0
        bool dep_on_other_rec=false; 
        for(Op* check_op : long_loop_recs) { //first make sure isn't dep on other recs
          if(op==check_op) {
            continue;
          }
          if(dataDepBT(opSet,check_op,op)) {
            dep_on_other_rec=true;
            break;
          }
        }
        if(dep_on_other_rec==false && opSet.count(op)) {
          fixes2 << "Bvv.fx(v,'" <<op->id() << "')$possDep(v,'" << op->id() << "')=1;\n";
          //std::cout << "Bvv.fx(v,'" <<op->id() << "')$possDep(v,'" << op->id() << "')=1;\n";
        }
      }

      //generate bad Bvv set -- O(n^3) but who cares?
      for(const std::pair<Op*,Op*>& p : closeSet) {
        Op* op1 = p.first;
        Op* op2 = p.second;
        if(op1==op2) continue;
        if(op1->id()>op2->id()) continue; //just do one pair
      
        bool op1_inc=false,op2_inc=false;
        bool op1_out=false,op2_out=false;
        bool fix_off =false;

        if(!dataDepBT(opSet,op1,op2) && !dataDepBT(opSet,op2,op1)) { //operations can go in parallel
          op1_inc=depOutsideSet(opSet,op1);
          op2_inc=depOutsideSet(opSet,op2);
          op1_out=useOutsideSet(opSet,op1);
          op2_out=useOutsideSet(opSet,op2);          
          if( (op1_inc && op2_out) || (op1_out && op2_inc) ) {
            fix_off=true;
          }

          if(!fix_off) {
            if(inner_li && _dist.size() <100) {

              //cout << "\n" << op1->id() << "~~" << op2->id() << "\n";            
              int new_len=inner_li->calcRecurrences(long_loop_recs,rec_chains,op1,op2);
              if (new_len > orig_rec_len + 2) {
                fix_off=true; 
              }
              //cout  << " new:" << new_len << " old:" << orig_rec_len << "\n";
            }
          }
        }

        if(fix_off) {
          fixes2 << "Bvv.fx('" << op1->id() << "','" <<op2->id() << "')=1;\n";
          fixes2 << "Bvv.fx('" << op2->id() << "','" <<op1->id() << "')=1;\n";
        }
      }
    }

  }

  std::string resultfile=filename + ".out";

  if(nMemDepOps < countElementsM/max_mem_ops) {
    nMemDepOps = countElementsM/max_mem_ops;
  }
 
 
  //TODO: don't redo gams, just make it check current files
  if(!cfu_set) { //size-based
    printGamsPartitionText(out,opSet.size(),resultfile,
                           fixes.str(),nMemDepOps,max_beret_size,max_mem_ops);
  } else {       //non-size-based
    printSGPartText(out,resultfile,fixes2.str(),cfu_set);
  }


  out.close(); 
  Subgraph* subgraph=NULL;
  //Delete 225? Directories

  unsigned ops_in_a_subgraph = 0;

  CFU* curCFU = NULL;

  bool gams_attempted=false;

  //if it's size based, the size can only go so high
  if(!cfu_set && countOps > MAX_GAMS_SIZE) {
    //std::cerr << "(ed)";
  } else if (!no_gams) {
    gams_attempted=true;

    // Delete previous
    string full_resultfile = std::string("gams/") + resultfile;
    string rm_cmd  = std::string("rm -f ") + full_resultfile;
    system(rm_cmd.c_str());

    //run gams
    string gams_cmd=std::string("gams ") + filename + std::string(" mip=gurobi wdir=gams")
      + (!gams_details?string(" lo=2"):string(""));
    system(gams_cmd.c_str());

    //Read in file
    std::ifstream ifs((string("gams/")+resultfile).c_str());

    //First just make sure we can read it
    set<Op*> found_ops;
    while(ifs.good()) {
      using namespace boost;
      std::string line;
      std::getline(ifs,line);
  
      char_separator<char> sep(" ");
      tokenizer<char_separator<char>> tokens(line, sep);

      for (const auto& t : tokens) {
        string t_post = t.substr(1,string::npos);
        if(t[0] == 's' || t[0] == 'n') {
          continue;
        }

        uint32_t i = std::stoi(t);
        assert(intOpMap.count(i));
        Op* op = intOpMap[i];
        found_ops.insert(op);
      }
    }

    ifs.clear();
    ifs.seekg(0,ifs.beg); // seek back to begining to parse file for real
    if(found_ops.size() != countOps) {
      ops_in_a_subgraph=found_ops.size();
    } else {

      while(ifs.good()) {
        using namespace boost;
    
        std::string line;
        std::getline(ifs,line);
    
        char_separator<char> sep(" ");
        tokenizer<char_separator<char>> tokens(line, sep);
  
        Op* prev_op = NULL; //previous op
        for (const auto& t : tokens) {
          if(subgraph==NULL) {
            subgraph=new Subgraph();
            sgSched.insertSG(subgraph);
          }
   
          string t_post = t.substr(1,string::npos);
  
          if(t[0] == 's') {
            uint32_t i = std::stoi(t_post);
            curCFU=cfu_set->getCFU(i);
            subgraph->setCFU(curCFU);
            continue;
          }
  
          if(t[0] == 'n') {
            uint32_t i = std::stoi(t_post);
            assert(curCFU);
            assert(prev_op);
            CFU_node* cfu_node=curCFU->getCFUNode(i);
            sgSched.setMapping(prev_op,cfu_node,subgraph);
            //cout << func()->nice_name() << " sets " << prev_op->id() << "\n";
            continue;
          }
  
          uint32_t i = std::stoi(t);
          assert(intOpMap.count(i));
          Op* op = intOpMap[i];
          assert(op);
          //op->setSubgraph(subgraph);
          subgraph->insertOp(op);
          ops_in_a_subgraph++;
          prev_op=op;
        }
        subgraph=NULL; //reset subgraph for next group
      }
    }
  }

  if(ops_in_a_subgraph!=countOps) {
    //GAMS Couldn't Do It
    // do it manually
    //std::cerr << "ops_in_a_subgraph:" << ops_in_a_subgraph << "\n";
    //std::cerr << "countElements:" << countOps << "\n";
    //std::cerr << "GAMS COULD NOT SCHEDULE FOR BERET --- So I'm going to do it manually\n";
    if(gams_attempted) {
      std::cerr << "GAMS-FAILED on file: \"" << filename 
                << "\", falling back to heuristic.  ";
      std::cerr << "(ops sched: " << ops_in_a_subgraph << " / " << countOps << ")\n";

      //maybe it failed because of too many files? ... lets delete temp files so that
      //nex time it works.
      system("rm -rf 225?/ &> /dev/null");
    }

    if(cfu_set) {
      std::cerr << "No Heuristic Method For CFU Scheduling";
      return false;
    }

    int curOp=0;
    for(auto i=bbVec.begin(),e=bbVec.end();i!=e;++i) {
      BB* bb= *i;
      BB::OpVec::iterator oi,oe;
      for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
        Op* op = *oi;
        /*if(!sgSched.opScheduled(op)) {
          continue;
        }*/
        if(!opSet.count(op)) {
          continue;
        }

        curOp++;

        //try to find a good spot
        //first find earliest possible subgraph which is legal:
        //it must come after all subgraphs with dependencies
        
        //int latestSGInd=0; replace with iterator below
        SGSched::SubgraphVec::iterator latest_sg = sgSched.sg_begin();
        //unsigned sg_ind =0;
        bool someDep=false;
        int usesInPath=0;

        for(auto ui=op->u_begin(),ue=op->u_end();ui!=ue;++ui) {
          Op* use_op = *ui;
          usesInPath+=dependenceInPath(opSet,op,use_op);
        }


        for(auto sgi=sgSched.sg_begin(), sge=sgSched.sg_end();sgi!=sge;++sgi){
          Subgraph* sg = *sgi;
          for(auto dsi=op->adj_d_begin(),dse=op->adj_d_end();dsi!=dse;++dsi) {
            Op* dep_op = *dsi;
            if(/*dependenceInPath(instsInPath,dep_op,op) &&*/ sg->hasOp(dep_op)) {
              //latestSGInd=sg_ind;
              latest_sg=sgi;
              someDep = true;
            }
          }
          for(auto di=op->m_cf_begin(),de=op->m_cf_end();di!=de;++di) {
            //TODO: LoadLoad
            Op* mem_op = *di;
            if(sg->hasOp(mem_op)) {
              //latestSGInd=sg_ind+1;
              latest_sg=sgi;
            }
          }
          //sg_ind++;
        }

        //find earliest position to put it in
        if(someDep || /*usesInPath*/ true || curOp + max_beret_size > countOps) {
          Subgraph* subgraphFound=NULL;
          //for(sg_ind=latestSGInd; sg_ind < subgraphVec.size(); ++sg_ind) 

          for(auto i=latest_sg, e=sgSched.sg_end();i!=e;++i) {
		    Subgraph* sg = *i;
            if(sg->size() < (unsigned)max_beret_size) {
              int mopsPerSubgraph=0;
              for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
                Op* sg_op = *oi;
                if(sg_op->isMem()) {
                  mopsPerSubgraph++;
                }
              }
              //cout << mopsPerSubgraph << " " << max_mem_ops << "\n";
              if(!op->isMem() || mopsPerSubgraph < max_mem_ops) {
                subgraphFound=sg;
                break;
              }
            }
          }
          if(subgraphFound) {
            subgraphFound->insertOp(op);
            ops_in_a_subgraph++;
            //op->setSubgraph(subgraphFound);
            continue; //go to next op
          }
        }

        //couldn't find any spots
        subgraph=new Subgraph();
        sgSched.insertSG(subgraph);
        subgraph->insertOp(op);
        ops_in_a_subgraph++;
      }
    }

  }

  for(auto op : opSet) {
    sgSched.insertOp(op);
  } 
  assert(sgSched.numSubgraphs() >= (int)orig_sgs);
  assert(sgSched.opSet().size() == countOps + orig_size);
  
  return true;
}

//No one uses this, don't remember what its for
bool LoopInfo::canFlowOP(vector<Op*>& worklist, Op* dest_op, bool from) {
  set<Op*> seen;
  seen.insert(worklist.begin(),worklist.end());

  while(!worklist.size()==0) {
    Op* op = worklist.back();
    worklist.pop_back();
    seen.insert(op);
    if(!from) { //flow to
      for(auto i=op->u_begin(),e=op->u_end();i!=e;++i) {
        Op* use_op = *i;
        if(!dependenceInPath(op,use_op)) {
          continue;
        }
        if(use_op == dest_op) {
          return true;
        }
        if(seen.count(use_op) == 0) {
          worklist.push_back(use_op);
        }
      }
    } else {
      for(auto i=op->d_begin(),e=op->d_end();i!=e;++i) {
        Op* dep_op = *i;
        if(!dependenceInPath(dep_op,op)) {
          continue;
        }
        if(dep_op == dest_op) {
          return true;
        }
        if(seen.count(dest_op) == 0 ) {
          worklist.push_back(dest_op);
        }
      }
    }
  }
  return false;
}

//the are already serialized
void LoopInfo::serializeSubgraphs() {
  assert(0 && "not implemented -- need to put in sgsched");
}

/*
void LoopInfo::printSubgraphDot(std::ostream& out, 
                                std::unordered_map<Op*,std::map<Op*,std::map<unsigned,double>>>* critEdges) {
  
}
*/

//helper function to print node
void print_dot_node(std::ostream& out, 
    std::unordered_map<Op*,std::map<Op*,std::map<unsigned,double>>>* critEdges, 
    Op* op, int weight_min, bool insg=true) {
  out << "\"" << op->id() << "\" "
      << "[";

  std::string extra("");

  if(critEdges && (*critEdges)[op][op].size()!=0) {
    stringstream ss;
    ss << "\\n";
    for(const auto& i : (*critEdges)[op][op]) {
      unsigned type = i.first;
      double weight = i.second;
      if(/*type == E_RDep || */weight < weight_min) {
        continue;
      }
      ss << edge_name[type] << ":" << weight << "\\n";
    }

    extra=ss.str();
  }

  out << op->dotty_name_and_tooltip(extra);

  if(insg) {
    out << ",style=filled, color=white";
  }
  out << "]\n";
}

void print_dot_sg(std::ostream& out, 
  std::unordered_map<Op*,std::map<Op*,std::map<unsigned,double>>>* critEdges,
  unordered_map<Op*,map<Op*,string>>& e_msg, unordered_map<Op*,map<Op*,bool>>& e_constr,
  unordered_map<Op*,map<Op*,bool>>& e_backwards,
  SGSched& sgSched, Subgraph* sg, int weight_min) {

  out << "subgraph"
      << "\"cluster_" << sg->id() << "\"{" 
  
      << "label=\"sg " << sg->id();
      
  if(sg->cfu()) {
   out << " (cfu" << sg->cfu()->ind() << ")";
  }
  out << "\"\n";

  out << "style=\"filled,rounded\"\n";
  out << "color=cornsilk\n";

  for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
    Op* op = *oi;
    print_dot_node(out,critEdges,op,weight_min);
  }

  out << "}\n";
 
  //Iterate through Ops
  for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
    Op* op = *oi;
    
    Op::Deps::iterator ui,ue; //uses
    for(ui=op->adj_u_begin(),ue=op->adj_u_end();ui!=ue;++ui) {
      Op* uop = *ui;
      if(op->func()==uop->func()) { //only check deps inside the function
        //string label="";

        e_msg[op][uop]+="";

        /*
        if(critEdges && (*critEdges)[op][uop][E_RDep] >= weight_min) {
          stringstream ss;
          ss << "label=" << (*critEdges)[op][uop][E_RDep] << ",";
          label=ss.str();
         }
         */

        if(LoopInfo::dependenceInPath(sgSched.opSet(),op,uop)) {  //for forward deps
           /*out << "\"" << op->id() << "\"" << " -> "
               << "\"" << uop->id() << "\"[" << label;
           out << "weight=0.5]\n";*/
           e_constr[op][uop]=true; //makes it black
        } else {
           /*out << "\"" << op->id() << "\"" << " -> "
               << "\"" << uop->id() << "\"[" << label;

           out << "weight=0.5,color=red,fontcolor=red,constraint=false]\n";*/
           e_backwards[op][uop]=true;
        }
      }
    }
  }
}

void LoopInfo::printSubgraphDot(std::ostream& out, 
          SGSched& sgSched,
          std::unordered_map<Op*,std::map<Op*,std::map<unsigned,double>>>* critEdges, 
          bool NLA, bool only_sgs,  LoopInfo* li) {

  out << "digraph GB{\n";
  out << "compound=true\n";

  int weight_min=10; //TODO: make this better

  std::set<Subgraph*> done_sgs;

  //need to coalesce edges for good routing
  unordered_map<Op*,map<Op*,string>> e_msg;
  unordered_map<Op*,map<Op*,bool>> e_constr;
  unordered_map<Op*,map<Op*,bool>> e_backwards;

  // Make all the clusters and groups
  for(auto I=li->body_begin(),E=li->body_end();I!=E;++I) {
    BB* bb = *I;
    out << "subgraph"
        << "\"cluster_bb" << bb->rpoNum() << "\"{" 
   
        << "label=\"BB " << bb->rpoNum() << "\"\n";

    out << "style=\"filled,rounded\"\n";
    out << "color=lightgrey\n";

    for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
      Op* op = *II;
      Subgraph * sg = sgSched.sgForOp(op);
      if(sg && !done_sgs.count(sg)) {
        done_sgs.insert(sg);
        print_dot_sg(out,critEdges,e_msg,e_constr,e_backwards,sgSched,sg,weight_min);
      }
    }
    out << "}\n";

    //Print non-sg operands
    if(!only_sgs) {
      for(auto II=bb->op_begin(),EE=bb->op_end();II!=EE;++II) {
        Op* op = *II;
        if(sgSched.sgForOp(op)==NULL) {
          
          print_dot_node(out,critEdges,op,weight_min,false); //print the node
  
          Op::Deps::iterator ui,ue; //uses
          for(ui=op->adj_u_begin(),ue=op->adj_u_end();ui!=ue;++ui) {
            Op* uop = *ui;
            string label="";  
            e_msg[op][uop]+="";
            e_constr[op][uop]=true;

          /* if(critEdges && (*critEdges)[op][uop][E_RDep] >= weight_min) {
              stringstream ss;
              ss << "label=" << (*critEdges)[op][uop][E_RDep] << ",";
              label=ss.str();
             }*/
  
            //if(dependenceInPath(sgSched.opSet(),op,uop)) {  //for forward deps
          /*     out << "\"" << op->id() << "\"" << " -> "
                   << "\"" << uop->id() << "\"[" << label;
               out << "weight=0.5]\n";*/
            /*} else {
               out << "\"" << op->id() << "\"" << " -> "
                   << "\"" << uop->id() << "\"[" << label;
  
               out << "weight=0.5,color=red,fontcolor=red,constraint=false]\n";
            }*/
          }
          /*for(const auto& i : (*critEdges)[op]) {
            Op* dest_op = i.first;
            if(op==dest_op) {
              continue;
            }
            
            for(const auto& j : i.second) {
              unsigned type   = j.first;
              double   weight = j.second;
              if(weight >= weight_min) {
                if(e_msg[op][uop].length()>0) {
                  e_msg+="\\n";
                }
                e_msg += edge_name[type] << ":" << weight;

//                out << "\"" <<     op->id() << "\"" << " -> "
  //                 << "\"" << dest_op->id() << "\"[";

                //out << "label=\"" << edge_name[type] << ":" << weight << "\",";
                //out << "weight=0.5,color=purple,fontcolor=purple,constraint=false]\n";
              }
            }         
          }*/
        }

      }
    }
  }
 
  for(auto sgi=sgSched.sg_set_begin(), sge=sgSched.sg_set_end(); sgi!=sge;++sgi) {
    Subgraph* sg = *(sgi);
    if(!done_sgs.count(sg)) {
      print_dot_sg(out,critEdges,e_msg,e_constr,e_backwards,sgSched,sg,weight_min);
    }
  }

  if(critEdges) {
    for(auto sgi=sgSched.sg_set_begin(), sge=sgSched.sg_set_end(); sgi!=sge;++sgi) {
      Subgraph* sg = *(sgi);
      for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
        Op* op = *oi;
        
        for(const auto& i : (*critEdges)[op]) {
          Op* dest_op = i.first;
          if(op==dest_op) {
            continue;
          }
          
          for(const auto& j : i.second) {
            unsigned type   = j.first;
            double   weight = j.second;

            /*if(type==E_RDep) {
              continue;
            }*/

            if(weight >= weight_min) {
                if(e_msg[op][dest_op].length()>0) {
                  e_msg[op][dest_op]+="\\n";
                }
                stringstream ss;
                ss <<  edge_name[type] << ":" << weight;
                e_msg[op][dest_op] +=ss.str();

              /*out << "\"" <<     op->id() << "\"" << " -> "
                 << "\"" << dest_op->id() << "\"[";
              out << "label=\"" << edge_name[type] << ":" << weight << "\",";
              out << "weight=0.5,color=purple,fontcolor=purple,constraint=false]\n";*/
            }
          }         
        }
      }
    }
  }
  //Print BB Connections
  for(auto I=li->body_begin(),E=li->body_end();I!=E;++I) {
    BB* bb = *I;
    for(auto si=bb->succ_begin(),  se=bb->succ_end(); si!=se; ++si) {
      BB* succ_bb = *si;
      if(bb->len() > 0 && succ_bb->len() > 0) {
        out << "\"" << bb->lastOp()->id() << "\"->" 
            << "\"" << succ_bb->firstOp()->id() << "\" [";

        out << "ltail=\"cluster_bb" << bb->rpoNum() << "\" ";
        out << "lhead=\"cluster_bb" << succ_bb->rpoNum() << "\" ";

        out << "arrowhead=open arrowsize=1.5 weight=1 penwidth=4 color=blue ];\n";       
      } 
    }
  }


  for(const auto kv1 : e_msg) {
    Op* op = kv1.first;
    for(const auto kv2 : e_msg[op]) {
      Op* uop = kv2.first;
      string msg = kv2.second; 
      out << "\"" <<  op->id() << "\"" << " -> "
          << "\"" << uop->id() << "\"[";

      out << "label=\"" << msg << "\",";
      if(e_constr[op][uop]) {
        out << "weight=0.5,color=black,fontcolor=black,constraint=true]\n";
      } else if (e_backwards[op][uop]) {
        out << "weight=0.5,color=red,fontcolor=red,constraint=false]\n";
      } else {
        out << "weight=0.5,color=purple,fontcolor=purple,constraint=false]\n";
      }
    }
  }
  

  out << "}\n";
}

bool LoopInfo::calledOnlyFrom(std::set<FunctionInfo*>& fiSet,
                              std::set<LoopInfo*>& liSet, 
                              bool first) {
  //if we made it to a recursive function, don't keep going,
  //or we might go forever. just return that it can't be eliminated
  if(cantFullyInline()) {
    return false;
  }

  //if this loop is contained, then we're done
  if(!first && liSet.count(this)) {
    return true;
  }

  //check the next outer loop
  if(_immOuterLoop) {
    return _immOuterLoop->calledOnlyFrom(fiSet,liSet,false);
  } else { //try the function
    return _funcInfo->calledOnlyFrom(fiSet,liSet,false);
  }
}


uint64_t LoopInfo::totalDynamicInlinedInsts() {
  if(cantFullyInline()) {
    return 0; //zero means recursive
  }
  uint64_t total = numInsts();
  for(auto i = _immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
    LoopInfo* li = *i;
    total += li->totalDynamicInlinedInsts();
  }

  for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
    FunctionInfo* fi = i->first.second;
    float ratio = i->second / fi->calls();  //divide dynamic insts up by calls
    total+=fi->totalDynamicInlinedInsts() * ratio;
  }

  return total;
}

int LoopInfo::innerLoopStaticInsts() {  
  int static_insts=0;
  if(!isInnerLoop()) {
    for(auto i=_immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
      LoopInfo* li=*i;
      static_insts+=li->innerLoopStaticInsts();
    }
  } else { //INNER LOOPS ONLY
    bool is_pure=true;

    int temp_static_insts=0;
    for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
      FunctionInfo* fi = i->first.second;
      temp_static_insts+=fi->inlinedStaticInsts();
      if(fi->numLoops() > 0) {
        is_pure=false;
      }
    }

    if(is_pure) {
      static_insts = temp_static_insts; //all functions i call
      static_insts += staticInsts(); //all insts in me
    } else {
      for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
        FunctionInfo* fi = i->first.second;
        static_insts += fi->innerLoopStaticInsts();
      }
    }
  }
  assert(static_insts>0);
  return static_insts;
}


int LoopInfo::inlinedOnlyStaticInsts() {
  int static_insts=0;
  for(auto i=_calledToMap.begin(),e=_calledToMap.end();i!=e;++i) {
    FunctionInfo* fi = i->first.second;
    static_insts+=fi->inlinedStaticInsts();
  }
  for(auto i=_immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
    LoopInfo* li=*i;
    static_insts+=li->inlinedOnlyStaticInsts();
  }
  return static_insts;
}

std::string LoopInfo::nice_name() {
  stringstream ss;
  ss <<  func()->nice_name() << "_" << id();
  return ss.str();
}

void LoopInfo::nice_name_tree(stringstream& ss) {
  if(_immOuterLoop) {
    _immOuterLoop->nice_name_tree(ss);
  }
  ss <<  "::" << id();
}
std::string LoopInfo::nice_name_full() {
  stringstream ss;
  nice_name_tree(ss);
  return func()->nice_name() + ss.str();
}

bool LoopInfo::is_revolverable() {
  if(!isInnerLoop()) {
    return false;
  }
  if(cantFullyInline()) {
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


bool LoopInfo::callsRecursiveFunc() {
  // pair<pair<Op, FunctionInfo*>,int>
  for(auto i=_calledToMap.begin(), e=_calledToMap.end();i!=e; ++i) {
    FunctionInfo* fi = i->first.second;
    if(fi->callsRecursiveFunc()) {
      return true;
    }
  }
  for(auto i=_immInnerLoops.begin(),e=_immInnerLoops.end();i!=e;++i) {
    LoopInfo* li=*i;
    if(li->callsRecursiveFunc()) {
      return true;
    }
  }

  return false;
}

// Here -1 means "not set yet"
// and -2 means "no not path index"
int LoopInfo::getHotPathIndex() {
  if(_hotPathIndex!=-1) {
    return _hotPathIndex;
  }

  int hotness=0;
  for(auto it=_iterCount.begin(),et=_iterCount.end();it!=et;++it) {
    if(it->second > hotness) {
      hotness=it->second;
      _hotPathIndex= it->first;
    }
  }
  //assert(_hotPathIndex!=-1);
  //assert(hotness!=0);
  if(_hotPathIndex==-1) {
    return -2;
  } else {
    return _hotPathIndex;
  }
} 

LoopInfo::BBvec& LoopInfo::getHotPath() {
  return _pathMap[getHotPathIndex()];
}


