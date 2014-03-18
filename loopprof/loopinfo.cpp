#include "loopinfo.hh"
#include "functioninfo.hh"

#include "cfu.hh"

using namespace std;

uint32_t Subgraph::_idCounter=0;
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







uint32_t LoopInfo::_idcounter=0;

#define MAX_GAMS_SIZE 150

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
    _curIter++;
    _totalIterCount++;

    if(_pathMap.count(pathIndex)==0) {
      _pathMap[pathIndex]=path;
    }
  }
  std::map<uint64_t, std::set<Op*> > _effAddr2Op;

  for (auto I = path.begin(), E = path.end(); I != E; ++I) {
    for (auto OI = (*I)->op_begin(), OE = (*I)->op_end();
         OI != OE; ++OI) {
      if (!(*OI)->isLoad() && !(*OI)->isStore())
        continue;
      _effAddr2Op[(*OI)->getCurEffAddr()].insert(*OI);
    }
  }
  for (auto I = path.begin(), E = path.end(); I != E; ++I) {
    for (auto OI = (*I)->op_begin(), OE = (*I)->op_end();
         OI != OE; ++OI) {
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
void LoopInfo::beginLoop() {
  _curIter=0;
  _prevOpAddr.clear();
  _prevOpIter.clear();
}

void LoopInfo::endLoop() {
  _loopCount++;
  _curIter=0;
  _prevOpAddr.clear();
  _prevOpIter.clear();
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

//Keep track of address Paterns
void LoopInfo::opAddr(Op* op, uint64_t addr, uint8_t acc_size) { 
   static int nullAccess=0;
   if(addr==0 && nullAccess<100) {
     nullAccess++;
     std::cerr << "!!!!!!!!!!!!!!!!!!!!!! Warning: Null Memory Access at:" << op->id() << "!!!!!!!!!!!!!!!!!!!!!!\n";
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



void LoopInfo::checkCompatible(std::set<Op*>& ops,
                     std::set<std::pair<Op*,Op*>>& closeSet, 
                     Op* orig_op, 
                     Op* cur_op,
                     CFU_node* cur_fu,
                     std::set<Op*> doneOps,
                     std::set<CFU_node*> doneFUs) {
  

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
    Op* uop = *ui;   //forwards
    if(doneOps.count(uop)!=0 || !dependenceInPath(ops,cur_op,uop)) {
      continue;
    }
    for(auto ii=cur_fu->outs_begin(), ee=cur_fu->outs_end(); ii!=ee; ++ii) {
      CFU_node* ufu = *ii;
      if(doneFUs.count(ufu) != 0) {
        continue;
      }
      checkCompatible(ops,closeSet,orig_op,uop,ufu,doneOps,doneFUs);
    }
  }

  for(auto di=cur_op->adj_d_begin(),de=cur_op->adj_d_end();di!=de;++di) {
    Op* dop = *di;   //backwards
    if(doneOps.count(dop)!=0 || !dependenceInPath(ops,dop,cur_op)) {
      continue;
    }
    for(auto ii=cur_fu->ins_begin(), ee=cur_fu->ins_end(); ii!=ee; ++ii) {
      CFU_node* dfu = *ii;
      if(doneFUs.count(dfu) != 0) {
        continue;
      }
      checkCompatible(ops,closeSet,orig_op,dop,dfu,doneOps,doneFUs);
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

void LoopInfo::printSGPartText(std::ostream& out,
                           std::string resultfile, 
                           std::string fixes,
                           CFU_set* cfu_set) {

out << "file stdout / \"" << resultfile << "\" /;\n"
    << "stdout.pc=8;stdout.pw=4096;put stdout;\n";


  cfu_set->print_to_stream(out);  

const char * text_pre = R"HERE(

binary variable Mvn(v,n), Bvv(v,v);
positive variable Tv(v);
positive variable U(v,v,fu);
variable LAT, READS, WRITES, GOAL;
alias(v,v1,v2,v3,v4);
alias(n,n1,n2);
alias(fu,fu1,fu2);
alias(s,s1,s2);

* if v1 is close to v2
set close_dep(v1,v2);
set iter_close(v1,v2);

close_dep(v1,v2)$(ORD(v1) eq ORD(v2)) = YES;
scalar depth;
for (depth = 0 to 4,
  loop((v1,v2,v3)$( (A(v2,v3) and close_dep(v1,v2)) or
                    (A(v3,v2) and close_dep(v2,v1))),
    iter_close(v1,v3) = YES;
  );
  close_dep(v1,v2)$iter_close(v1,v2)=YES;
);

display close_dep;

Bvv.fx(v1,v2)$(not possDep(v1,v2))=1;


)HERE";




out << text_pre;
out << fixes;

const char * text = R"HERE(

* generate compatibility matrix
set c(v,n);
loop(k,
    c(v,n)$(kv(v,k) and kn(n,k))=YES;
)
display c;
c(v,'nreg')=YES;


Equations 
  map1(v),
  map2(v),
  route1(v,v,n),
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
  timing2(v,v),
  c_lat(v),
  c_writes,
  c_reads,
  c_goal
  ;


*All nodes mapped to some FU. (include kind in this later)
map1(v)..                    sum(fu$     c(v,fu),  Mvn(v,fu)) =e= 1;
map2(v)..                    sum(fu$(not c(v,fu)), Mvn(v,fu)) =e= 0;

route1(v1,v2,fu2)$(A(v1,v2) and c(v2,fu2)).. 
      Mvn(v2,fu2) =l= sum(n1$(c(v1,n1) and Hnn(n1,fu2)), Mvn(v1,n1));

regforce(v1,v2)$A(v1,v2).. Bvv(v1,v2) =l= Mvn(v1,'nreg');

*init_used(v1,fu1)$(c(v1,fu1)).. U(v1,v1,fu1) =g= Mvn(v1,fu1); 
*prop_used1(v1,v2,fu1,v3)
*  $(A(v1,v2) and possDep(v2,v3)).. 
*  U(v2,v3,fu1) =g= U(v1,v3,fu1) - Bvv(v1,v2);
*prop_used2(v1,v2,fu1,v3)
*  $(A(v1,v2) and possDep(v1,v3))..
*  U(v1,v3,fu1) =g= U(v2,v3,fu1) - Bvv(v1,v2);
*restrict_use(v1,fu)$(c(v1,fu)).. 
*  sum(v2$(c(v2,fu) and possDep(v1,v2)),U(v1,v2,fu)) =l= 1;


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
                              Tv(v2) =g= Bvv(v1,v2) + Tv(v1);

timing2(v1,v2)$(A(v1,v2))..
                              Tv(v2) =l= CARD(v)*Bvv(v1,v2) + Tv(v1);

c_writes.. WRITES =e= sum(v, Mvn(v,'nreg'));
c_reads..  READS  =e= sum((v1,v2)$(A(v1,v2)), Bvv(v1,v2));

c_lat(v)$(sum(v2$A(v,v2),1) eq 0).. LAT    =g= Tv(v);
c_goal..   GOAL   =e= LAT + WRITES + READS;


option optca = 1.9999;
option optcr = 0.1;
option reslim = 200;
option threads = 16;

Model sg/all/;
sg.limrow=10000
solve sg using mip minimizing GOAL;

display Tv.l;
display Mvn.l;
display Bvv.l;
display LAT.l;

scalar t;

*loop(v,
*put v.tl Tv.l(v)/
*);

*loop((v,n)$Mvn.l(v,n),
*put v.tl n.tl/
*);


* all verticies at this time step
set par(v); 
* the current subgraph which we are printing
set curr(v);


for (t = 0 to LAT.l,
*  put "TIME" t" "/

  loop(v,par(v)=NO);
  loop(v,
    if(Tv.l(v) >= t-0.05 and Tv.l(v) <= t+0.05,
       par(v)=YES;
    );
  );
  loop(v,
    if(par(v),
      loop(v1,curr(v1)=NO);
 
      loop((s,n)$(Mvn.l(v,n) and cfu(s,n)),
        put s.tl;
      );
     
      
      par(v)=NO;
      curr(v)=YES;
      loop(v3$par(v3),
        loop((v1,v2,fu1,fu2)$(A(v1,v2)    and Hnn(fu1,fu2)  and 
                              Mvn.l(v1,fu1) and Mvn.l(v2,fu2) and
                              (curr(v2) or curr(v1)) 
                              and Tv.l(v1) >= t-0.05 and Tv.l(v1) <= t+0.05
                              and Tv.l(v2) >= t-0.05 and Tv.l(v2) <= t+0.05
                              ),
          curr(v1)=YES;
          curr(v2)=YES;
          par(v1)=NO;
          par(v2)=NO;
        );
      );
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

*option limrow = 100;
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
    bool gams_details,bool no_gams, int max_beret_size, int max_mem_ops) {

    _sgSchedBeret.reset();  
    BBvec& bbVec = getHotPath();
    return printGamsPartitionProgram(filename,
      bbVec, _sgSchedBeret, 
      cfu_set, gams_details, no_gams);
}


bool LoopInfo::scheduleNLA(CFU_set* cfu_set,   
                 bool gams_details, bool no_gams) { 
   _sgSchedNLA.reset();
   _sgSchedNLA.setCFUSet(cfu_set);
  return scheduleNLA(cfu_set, _sgSchedNLA, gams_details, no_gams);
}


/* This algo Searches chunks up the outer loop into peices, and and schedules
 * each one at a time.  This will create a bunch of BBs
 */
bool LoopInfo::scheduleNLA(CFU_set* cfu_set,   
                 SGSched& sgSched,
                 bool gams_details,
                 bool no_gams) { 
  //Find successive basic blocks, throw those at the gams scheduler
  BBvec curVec;
  int piece=0;

  for(auto ii=_rpo.begin(),ee=_rpo.end();ii!=ee;++ii, ++piece) {
    BB* bb = *ii;  

    curVec.push_back(bb);
   
    if(bb->succ_size()!=1 || (*bb->succ_begin())->pred_size()!=1 || ii==_rpo.end()) {
      //time to schedule!
      stringstream ss;
      ss << "schedNLA." << id() << "." << piece; 
      bool ret = printGamsPartitionProgram(ss.str(),
                     curVec,sgSched,
                     cfu_set, gams_details, no_gams,100,100); 
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
     int max_beret_size, int max_mem_ops) {


  if(cfu_set && !no_gams) {
    //check max size of bb()
    for(auto const& bb : bbVec) {
      if(bb->len() > 500) {
        std::cerr << "bb too big for cfu scheduling!\n";
        return false;
      }
    }
  }

  if(cfu_set && !no_gams && bbVec.size()>1) { //cfu scheduling has no heuristic -- split up into many pgms
    int size=0;
    for(auto const& bb : bbVec) {
      size+=bb->len();
    }
  
    int num_times = size / MAX_GAMS_SIZE + 1;
  
    if(num_times!=1) {
      //refactor to do many calls
      int cursize=0;
      BBvec newBBVec;
      bool worked=true;
  
      for(auto const& bb : bbVec) {
        if(bb->len() + cursize > MAX_GAMS_SIZE) {
          std::cout << "aux scheduling, len:" << cursize << "\n";
          worked &= printGamsPartitionProgram(filename,newBBVec,sgSched,cfu_set, 
                             gams_details, no_gams, max_beret_size, max_mem_ops);
          cursize=0;
          newBBVec.clear();
        }
  
        cursize+=bb->len();
        newBBVec.push_back(bb);
      }
      //last one
      std::cout << "aux scheduling, len:" << cursize << "\n";
      worked &= printGamsPartitionProgram(filename,newBBVec,sgSched,cfu_set, 
                                          gams_details, no_gams, max_beret_size, max_mem_ops);

      return worked; //recursively done!  (cheap hack, but w/e)
    }
  }




  std::map<uint32_t, Op*> intOpMap;
  std::stringstream ss;

  ss << "$ONEMPTY;\n";
  CFU_node::print_kinds(ss);
  ss << "set v/";

  std::set<Op*> opSet;

  int countElements=0;
  //find all instructions in path;
  for(auto i=bbVec.begin(),e=bbVec.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
      if(op->shouldIgnoreInAccel() && !useOutsideLoop(op)) {
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


  if(setContainsCallReturn(opSet)) {
    //if loop contains call, don't perform subgraph matching...
    return false;
  }

  std::ofstream out((string("gams/") + filename).c_str()); 
  out << ss.str();

  out << "set A(v,v)/";
  std::stringstream fixes,fixes2,streamD,streamM,streamK;
  countElements=0;
  int countOps=0;
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
          fixes2 << "Mvn.fx('" << op->id() << "','nreg')=1;\n"; // must write reg
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

      if(countElementsK++ != 0) {
        streamK <<",";
      }
      streamK << op->id() << "." << CFU_node::kindName(CFU_node::kindOf(op->opclass()));
      

      for(auto di=op->m_begin(),de=op->m_end();di!=de;++di) {
        Op* mop = *di;
        if(dependenceInPath(opSet,mop,op)) {
          if(countElementsD++ != 0) {
            streamD <<",";
          }
          streamD << mop->id() << "." << op->id();
          nMemDepOps++;
        }
      }

      if(op->numUses()==0) {
        fixes << "x.fx('" << op->id() << "')=0;\n"; //inst does not write reg
        fixes2 << "Mvn.fx('" << op->id() << "','nreg')=0;\n"; //inst does not write reg
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
  out << "set kv(v,k)/";
  out << streamK.str();
  out << "/;\n"; 


  
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

      if(countElements++%1024==1023) {
        out << "\r\n";
      }

      out << op1->id() << "." << op2->id();
    }
    out << "/;\n"; 
  }

  std::string resultfile=filename + ".out";

  if(nMemDepOps < countElementsM/max_mem_ops) {
    nMemDepOps = countElementsM/max_mem_ops;
  }


  if(!cfu_set) {
    printGamsPartitionText(out,opSet.size(),resultfile,
                           fixes.str(),nMemDepOps,max_beret_size,max_mem_ops);
  } else {
    printSGPartText(out,resultfile,fixes2.str(),cfu_set);
  }


  out.close(); 
  Subgraph* subgraph=NULL;
  //Delete 225? Directories

  int ops_in_a_subgraph = 0;

  system("rm -rf 225?/");

  CFU* curCFU = NULL;

  bool gams_attempted=false;

  //if it's size based, the size can only go so high
  if(!cfu_set && countOps > MAX_GAMS_SIZE) {
    std::cerr << "(size too big for size based)";
  } else if (!no_gams) {
    gams_attempted=true;
    //run gams
    system((std::string("gams ") + filename + std::string(" mip=gurobi wdir=gams")
      + (gams_details?string(" lo=2"):string(""))).c_str());
  
    std::ifstream ifs((string("gams/")+resultfile).c_str());

    while(ifs.good()) {
      using namespace boost;
  
      std::string line;
      std::getline(ifs,line);
  
      char_separator<char> sep(" ");
      tokenizer<char_separator<char>> tokens(line, sep);

      Op* prev_op; //previous op
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

  if(ops_in_a_subgraph!=countOps) {
    //GAMS Couldn't Do It
    // do it manually
    //std::cerr << "ops_in_a_subgraph:" << ops_in_a_subgraph << "\n";
    //std::cerr << "countElements:" << countOps << "\n";
    //std::cerr << "GAMS COULD NOT SCHEDULE FOR BERET --- So I'm going to do it manually\n";
    if(gams_attempted) {
      std::cerr << "GAMS-FAILED falling back to heuristic\n";
      std::cerr << "ops scheduled: " << ops_in_a_subgraph << " / " << countOps << "\n";
    }

    if(cfu_set) {
      assert(0 && "No Heuristic Method For CFU Scheduling");
    }

    int curOp=0;
    for(auto i=bbVec.begin(),e=bbVec.end();i!=e;++i) {
      BB* bb= *i;
      BB::OpVec::iterator oi,oe;
      for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
        Op* op = *oi;
        if(!sgSched.opScheduled(op)) {
          continue;
        }
        curOp++;

        //try to find a good spot
        //first find earliest possible subgraph which is legal:
        //it must come after all subgraphs with dependencies
        
        //int latestSGInd=0; replace with iterator below
        SGSched::SubgraphVec::iterator latest_sg;
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
          for(auto di=op->m_begin(),de=op->m_end();di!=de;++di) {
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
          //for(sg_ind=latestSGInd; sg_ind < subgraphVec.size(); ++sg_ind) {
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
            //op->setSubgraph(subgraphFound);
            continue; //go to next op
          }
        }

        //couldn't find any spots
        subgraph=new Subgraph();
        sgSched.insertSG(subgraph);
        subgraph->insertOp(op);
      }
    }

  }

  for(auto op : opSet) {
    sgSched.insertOp(op);
  }  
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

void LoopInfo::printSubgraphDot(std::ostream& out, 
                                SGSched& sgSched,
                                bool NLA) {
  out << "digraph GB{\n";
  out << "compound=true\n";

  for(auto sgi=sgSched.sg_set_begin(), sge=sgSched.sg_set_end(); sgi!=sge;++sgi) {
    Subgraph* sg = *(sgi);
    out << "subgraph"
        << "\"cluster_" << sg->id() << "\"{" 
   
        << "label=\"sg " << sg->id();
        
    if(sg->cfu()) {
     out << " (cfu" << sg->cfu()->ind() << ")";
    }
    out << "\"\n";

    out << "style=\"filled,rounded\"\n";
    out << "color=lightgrey\n";

    for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
      Op* op = *oi;
      
      out << "\"" << op->id() << "\" "
          << "[";

      out << op->dotty_name_and_tooltip();

      out << ",style=filled, color=white]\n";

    }

    out << "}\n";
 
 //Iterate through Ops
    for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
      Op* op = *oi;
      
      Op::Deps::iterator ui,ue; //uses
      for(ui=op->adj_u_begin(),ue=op->adj_u_end();ui!=ue;++ui) {
        Op* uop = *ui;
        if(op->func()==uop->func()) { //only check deps inside the function
          if(dependenceInPath(sgSched.opSet(),op,uop)) {  //for forward deps
             out << "\"" << op->id() << "\"" << " -> "
                 << "\"" << uop->id() << "\"[";

             out << "weight=0.5]\n";
          } else {
             out << "\"" << op->id() << "\"" << " -> "
                 << "\"" << uop->id() << "\"[";

             out << "weight=0.5,color=red,constraint=false]\n";
          }
        }
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


