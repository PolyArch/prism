#include "loopinfo.hh"
#include "functioninfo.hh"

using namespace std;

uint32_t Subgraph::_idCounter=0;
uint32_t LoopInfo::_idcounter=0;

#define MAX_GAMS_SIZE 200

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
  _rpo.push_back(bb);  
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
}

bool LoopInfo::dependenceInPath(Op* dop, Op* op) {
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




void LoopInfo::printGamsPartitionText(std::ostream& out,int count,
                              std::string resultfile,std::string fixes,
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

alias(V,u,v);
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
bool LoopInfo::printGamsPartitionProgram(std::string filename,
    bool gams_details,bool no_gams, int max_beret_size, int max_mem_ops) {
    
    _subgraphSet.clear();
    _subgraphVec.clear();
    return LoopInfo::printGamsPartitionProgram(filename,
      _subgraphSet, _subgraphVec,
       gams_details, no_gams);
}


bool LoopInfo::printGamsPartitionProgram(std::string filename,
     SubgraphSet& subgraphSet, SubgraphVec& subgraphVec,
     bool gams_details,bool no_gams,
     int max_beret_size, int max_mem_ops) {

  BBvec& hotPath = getHotPath();
  std::set<Op*> instsInPath;
  std::map<uint32_t, Op*> intOpMap;

  std::stringstream ss;

  ss << "$ONEMPTY; \n set V/";

  int countElements=0;
  //find all instructions in path;
  for(auto i=hotPath.begin(),e=hotPath.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
      instsInPath.insert(op);
      intOpMap[op->id()]=op;
     
      if(countElements++ != 0) { 
        ss << ",";
      }
      ss << op->id();
    }
  }
  ss << "/;\n"; 

  _instsInPath=instsInPath;
  if(setContainsCallReturn(instsInPath)) {
    //if loop contains call, don't perform subgraph matching...
    return false;
  }

  std::ofstream out((string("gams/") + filename).c_str()); 
  out << ss.str();

  out << "set A(v,v)/";
  std::stringstream fixes,streamD,streamM;
  countElements=0;
  int countOps=0;
  int countElementsD=0,countElementsM=0;
  int nMemDepOps=0;
  //print graph
  for(auto i=hotPath.begin(),e=hotPath.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
      countOps++;
      for(auto ui=op->u_begin(),ue=op->u_end();ui!=ue;++ui) {
        Op* uop = *ui;
        //Don't consider any irrelevant edges.
        if(!dependenceInPath(instsInPath,op,uop)) {
          fixes << "x.fx('" << op->id() << "')=1;\n"; // must write reg
          continue;
        }
        if(countElements++ != 0) {
          out <<",";
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

      for(auto di=op->m_begin(),de=op->m_end();di!=de;++di) {
        Op* mop = *di;
        if(dependenceInPath(instsInPath,mop,op)) {
          if(countElementsD++ != 0) {
            streamD <<",";
          }
          streamD << mop->id() << "." << op->id();
          nMemDepOps++;
        }
      }

      if(op->numUses()==0) {
        fixes << "x.fx('" << op->id() << "')=0;\n"; //inst does not write reg
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

  std::string resultfile=filename + ".out";

  if(nMemDepOps < countElementsM/max_mem_ops) {
    nMemDepOps = countElementsM/max_mem_ops;
  }

  printGamsPartitionText(out,instsInPath.size(),resultfile,
                         fixes.str(),nMemDepOps,max_beret_size,max_mem_ops);

  out.close(); 

  Subgraph* subgraph=NULL;

  //Delete 225? Directories

  int ops_in_a_subgraph = 0;

  system("rm -rf 225?/");

  if(!no_gams && countOps<=MAX_GAMS_SIZE) {
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
      for (const auto& t : tokens) {
        if(subgraph==NULL) {
          subgraph=new Subgraph();
          subgraphSet.insert(subgraph);
          subgraphVec.push_back(subgraph);
        }
  
        uint32_t i = std::stoi(t);
        Op* op = intOpMap[i];
        //op->setSubgraph(subgraph);
        subgraph->insertOp(op);
        ops_in_a_subgraph++;
      }
      subgraph=NULL; //reset subgraph for next group
    }
  } 
  if(countOps > MAX_GAMS_SIZE) {
    std::cerr << "(size too big)";
  }

  if(ops_in_a_subgraph!=countOps) {
    //GAMS Couldn't Do It
    // do it manually
    //std::cerr << "ops_in_a_subgraph:" << ops_in_a_subgraph << "\n";
    //std::cerr << "countElements:" << countOps << "\n";
    //std::cerr << "GAMS COULD NOT SCHEDULE FOR BERET --- So I'm going to do it manually\n";
    std::cerr << "GAMS-FAILED";
    

    int curOp=0;
    for(auto i=hotPath.begin(),e=hotPath.end();i!=e;++i) {
      BB* bb= *i;
      BB::OpVec::iterator oi,oe;
      for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
        Op* op = *oi;
        curOp++;

        //try to find a good spot
        //first find earliest possible subgraph which is legal:
        //it must come after all subgraphs with dependencies
        int latestSGInd=0;
        unsigned sg_ind =0;
        bool someDep=false;
        int usesInPath=0;

        for(auto ui=op->u_begin(),ue=op->u_end();ui!=ue;++ui) {
          Op* use_op = *ui;
          usesInPath+=dependenceInPath(instsInPath,op,use_op);
        }


        for(auto sgi=subgraphVec.begin(),sge=subgraphVec.end();sgi!=sge;++sgi){
          Subgraph* sg = *sgi;
          for(auto dsi=op->d_begin(),dse=op->d_end();dsi!=dse;++dsi) {
            Op* dep_op = *dsi;
            if(/*dependenceInPath(instsInPath,dep_op,op) &&*/ sg->hasOp(dep_op)) {
              latestSGInd=sg_ind;
              someDep = true;
            }
          }
          for(auto di=op->m_begin(),de=op->m_end();di!=de;++di) {
            Op* mem_op = *di;
            if(sg->hasOp(mem_op)) {
              latestSGInd=sg_ind+1;
            }
          }
          sg_ind++;
        }

        //find earliest position to put it in
        if(someDep || /*usesInPath*/ true || curOp + max_beret_size > countOps) {
          Subgraph* subgraphFound=NULL;
          for(sg_ind=latestSGInd; sg_ind < subgraphVec.size(); ++sg_ind) {
            if(subgraphVec[sg_ind]->size() < (unsigned)max_beret_size) {
              Subgraph* sg = subgraphVec[sg_ind];
              int mopsPerSubgraph=0;
              for(auto oi=sg->op_begin(),oe=sg->op_end();oi!=oe;++oi) {
                Op* sg_op = *oi;
                if(sg_op->isMem()) {
                  mopsPerSubgraph++;
                }
              }
              //cout << mopsPerSubgraph << " " << max_mem_ops << "\n";
              if(!op->isMem() || mopsPerSubgraph < max_mem_ops) {
                subgraphFound=subgraphVec[sg_ind];
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
        subgraphSet.insert(subgraph);
        subgraphVec.push_back(subgraph);
        //op->setSubgraph(subgraph);
        subgraph->insertOp(op);
      }
    }

  }
  
  //assert(_subgraphVec.size() >= 0);
  //serializeSubgraphs();

  return true;
}


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
        if(seen.count(use_op) == 0 ) {
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
/*  SubgraphSet checker;
  BBvec& hotPath = getHotPath();
  BBvec::iterator i,e;

  for(i=hotPath.begin(),e=hotPath.end();i!=e;++i) {
    BB* bb= *i;
    BB::OpVec::iterator oi,oe;
    for(oi=bb->op_begin(),oe=bb->op_end();oi!=oe;++oi)  {
      Op* op = *oi;
      if(checker.count(op->subgraph())==0) {
        checker.insert(op->subgraph());
        _subgraphVec.push_back(op->
      }
    }
  }
*/
}

  static const char* opname(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return "none";
    case 1: //IntALU
      return "ialu";

    case 2: //IntMult
      return "imult";
    case 3: //IntDiv
      return "idiv";

    case 4: //FloatAdd
      return "fadd";
    case 5: //FloatCmp
      return "fcmp";
    case 6: //FloatCvt
      return "fcvt";
    case 7: //FloatMult
      return "fmul";
    case 8: //FloatDiv
      return "fdiv";
    case 9: //FloatSqrt
      return "fsq";
    default:
      return "-";
    }
    return "?";
  }


void LoopInfo::printSubgraphDot(std::ostream& out) {
  out << "digraph GB{\n";
  out << "compound=true\n";

  SubgraphSet::iterator i,e;
  for(i=_subgraphSet.begin(), e=_subgraphSet.end(); i!=e;++i) {
    Subgraph* sg = *(i);
    out << "subgraph"
        << "\"cluster_" << sg->id() << "\"{" 
   
        << "label=\"sg " << sg->id() << "\"\n";

    out << "style=\"filled,rounded\"\n";
    out << "color=lightgrey\n";

    Subgraph::OpSet::iterator oi,oe;
    int i;
    for(oi=sg->op_begin(),oe=sg->op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;
      
      out << "\"" << op->id() << "\" "
          << "[label=\"";

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
      } else {
        out << "~" << opname(op->opclass());
      }


      out << i << "\" style=filled, color=white]\n";
    }

    out << "}\n";
 
 //Iterate through Ops

    for(oi=sg->op_begin(),oe=sg->op_end(),i=0;oi!=oe;++oi,++i) {
      Op* op = *oi;
      
      /*
      Op::Deps::iterator di,de; //deps
      for(di=op->d_begin(),de=op->d_end();di!=de;++di) {
        Op* dop = *di;
        if(dop->func()==op->func()) { //only check deps inside the function
          if(dependenceInPath(pathOps,dop,op)) {  //for forward deps
             out << "\"" << dop->id() << "\"" << " -> "
                 << "\"" << op->id() << "\"[";

             out << "weight=0.5]\n";
          } else {
             out << "\"" << dop->id() << "\"" << " -> "
                 << "\"" << op->id() << "\"[";

             out << "weight=0.5,color=blue]\n";
          }
        }
      }*/
      Op::Deps::iterator ui,ue; //uses
      for(ui=op->u_begin(),ue=op->u_end();ui!=ue;++ui) {
        Op* uop = *ui;
        if(op->func()==uop->func()) { //only check deps inside the function
          if(dependenceInPath(_instsInPath,op,uop)) {  //for forward deps
             out << "\"" << op->id() << "\"" << " -> "
                 << "\"" << uop->id() << "\"[";

             out << "weight=0.5]\n";
          } else {
             out << "\"" << op->id() << "\"" << " -> "
                 << "\"" << uop->id() << "\"[";

             out << "weight=0.5,color=red]\n";
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


