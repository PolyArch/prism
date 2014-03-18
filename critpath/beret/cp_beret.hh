#ifndef CP_BERET
#define CP_BERET

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "loopinfo.hh"
#include "beret_inst.hh"

// CP_BERET
class cp_beret : public ArgumentHandler,
      public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef dg_inst<T, E> Inst_t;

 /* 
  BeretOpInfo *bOpArr;
  BeretOpInfo* bOpI(Op* li) {
    return bOpArr[op->id()];
  }

  void analyzeInnerLoop(LoopInfo* li) {
    LoopInfo::BBvec& hotPath = li->getHotPath();
    set<Op*> opsInTrace;
   
    LoopInfo::BBvec::iterator bi,be;
    for(bi=hotPath.begin(),be=hotPath.end();bi!=be;++bi) {
      BB* bb = *I;
      
      BB::OpVec::iterator io,eo;
      for(io=bb.opv_begin(),eo=bb.opv_end();io!=eo;++io) {
        Op* op = *io;
        opsInTrace.insert(op); 
      }
    }     
  }


  void analyzeFunction(FunctionInfo* fi) {
    FuncInfo::LoopList::iterator I,E;
    for(I=prof.fbegin(),E=prof.fend();I!=E;++I) {
      LoopInfo* li = I->second;
      if(li->parentLoop()==NULL) {
        analyzeInnerLoop(li);
      }
    }
  }

  void analyzeProfile() {
    PathProf& prof=Prof::get()
    
    //initialize all analysis info
    bOpArr = new BeretOpInfo[prof.maxOps];

    PathProf::FuncMap::iterator I,E;
    for(I=prof.fbegin(),E=prof.fend();I!=E;++I) {
      FunctionInfo* fi = I->second;
      analyzeFunction(fi);
    }
  }
*/

enum BERET_STATE {
  CPU,
  BERET
};

public:
  cp_beret() : CP_DG_Builder<T,E>() {
    beret_state=CPU;
  }

  virtual ~cp_beret(){
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;

  //std::map<LoopInfo*,LoopInfo::SubgraphVec> li2sgmap;
  //std::map<LoopInfo*,LoopInfo::SubgraphSet> li2ssmap;
  std::map<LoopInfo*,SGSched> li2sgmap;
  LoopInfo* _prevLoop=NULL;

  uint64_t _beret_int_ops=0, _beret_fp_ops=0, _beret_mult_ops=0;
  uint64_t _beret_regfile_fwrites=0, _beret_regfile_writes=0;
  uint64_t _beret_regfile_freads=0,  _beret_regfile_reads=0;

  unsigned _beret_max_seb=6,_beret_max_mem=2,_beret_max_ops=80;
  unsigned _beret_config_time=1,_beret_iops=2;
  unsigned _beret_dataflow_seb=0,_beret_dataflow_pure=0;
  unsigned _no_gams=false;
  void handle_argument(const char *name, const char *optarg) {
    if (strcmp(name, "beret-max-seb") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_seb = temp;
      }  else {
        std::cerr << "ERROR: \"" << name << "\" arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-max-mem") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_mem = temp;
      } else {
        std::cerr << "ERROR: \"" << name << "\" arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-max-ops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_max_ops = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-config-time") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_config_time = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-iops") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_iops = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-dataflow-seb") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_dataflow_seb = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "beret-dataflow-pure") == 0) {
      unsigned temp = atoi(optarg);
      if (temp != 0) {
        _beret_dataflow_pure = temp;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
      }
    }
    if (strcmp(name, "no-gams") == 0) {
      _no_gams=true;
    }


  }

  virtual void setDefaultsFromProf() {
    CP_DG_Builder::setDefaultsFromProf();
    //set up the subgraphs
    std::multimap<uint64_t,LoopInfo*> loops;
    PathProf::FuncMap::iterator i,e;
    for(i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
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
  
      //generate this only for 
      //1. Inner Loops
      //2. >50% Loop-Back
      //3. Executed >= 10 Times
  
      int hpi = loopInfo->getHotPathIndex();
      std::cerr << "func: " << loopInfo->func()->nice_name() 
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
         && loopInfo->getLoopBackRatio(hpi) >= 0.7
         && loopInfo->getTotalIters() >= 2
         && loopInfo->instsOnPath(hpi) <= (int)_beret_max_ops
         ) {
        std::stringstream part_gams_str;
        part_gams_str << _run_name << "partition." << loopInfo->id() << ".gams";
  
        bool gams_details=false;
        std::cerr << _beret_max_seb << " " << _beret_max_mem << "\n";
        worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
            loopInfo->getHotPath(),
            li2sgmap[loopInfo],
            Prof::get().beret_cfus(),
            gams_details,_no_gams,_beret_max_seb,_beret_max_mem);
        if(worked) {
          std::cerr << " -- Beretized\n";
        } else {
          std::cerr << " -- NOT Beretized (Probably had Func Calls)\n";
        }
      } else {
        std::cerr << " -- NOT Beretized -- did not satisfy criteria\n";
      }
    }
    
  }

virtual void printEdgeDep(std::ostream& outs, BaseInst_t& inst, int ind,
                    unsigned default_type1, unsigned default_type2 = E_NONE)
  {
    if (!getTraceOutputs())
      return;
    E* laEdge = inst[ind].lastArrivingEdge();
    unsigned lae;
    if (laEdge) {
      lae = laEdge->type();
      if ((lae != default_type1 && lae!=default_type2) || laEdge->len()>1) {
        outs << edge_name[lae];
        /*int idiff=-laEdge->src()->_index + inst._index;
        if(idiff != 0) {
          outs << idiff;
        }*/
        if(laEdge->len()!=0) {
          outs << laEdge->len();
        }
      }
    }
    outs << ",";
  }


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (!getTraceOutputs())
      return;
    if(!getCPDG()->hasIdx(index))
      return;

    outs() << index + Prof::get().skipInsts << ": ";

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);

    for (unsigned i = 0; i < inst.numStages(); ++i) {
      outs() << inst.cycleOfStage(i) << " ";
      printEdgeDep(outs(),inst,i,E_FF);

    }
    CriticalPath::traceOut(index,img,op);
    outs() << "\n";
  }
  

  virtual void accelSpecificStats(std::ostream& out, std::string& name) {
//out << "Beret Cycles = " << _totalBeretCycles << "\n";
    out << " (beret-only " << _totalBeretCycles 
        << " beret-insts " << _totalBeretInsts
//        << " idle-cycles " << idleCycles
//        << " non-pipeline-cycles" << nonPipelineCycles
       << ")";
  }

  /*
   *  Beret Iteration State
   */
  unsigned beret_state;
  std::map<Op*,std::shared_ptr<BeretInst>> binstMap;
  LoopInfo* li;
  Op* curLoopHead;
  std::vector<std::tuple<CP_NodeDiskImage,uint64_t,Op*>> replay_queue;
  unsigned _whichBB;
  uint64_t _curBeretStartCycle=0, _curBeretStartInst=0;
  uint64_t _totalBeretCycles=0, _totalBeretInsts=0;

  std::shared_ptr<T> prevStartSEB = NULL;
  //Add a loop iteration based on the CFG and hot path
  std::shared_ptr<T> addLoopIteration(T* startIterEv, unsigned xfer_cycles) {
    binstMap.clear();
    replay_queue.clear();
    _whichBB=0;
    //create instruction for each SEB

    assert(li2sgmap[li].valid());
    std::shared_ptr<T> prevEndSEB   = NULL;
    for(auto i =li2sgmap[li].sg_begin(),e =li2sgmap[li].sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      std::shared_ptr<T> startSEB(new T());
      std::shared_ptr<T> endSEB(new T());
      endSEB->_index=343;

      //if load or store queue is full, then delay
      checkLSQSize(*startIterEv,true,true);

      if(_beret_dataflow_pure) {
        if(!prevEndSEB && xfer_cycles!=0 && startIterEv) {
          getCPDG()->insert_edge(*startIterEv,*startSEB, xfer_cycles, E_BXFR);
        }
        if(prevStartSEB) {
          getCPDG()->insert_edge(*prevStartSEB,*startSEB, 1, E_SEBS);
        }
      } else if (_beret_dataflow_seb) {
        if(!prevEndSEB && xfer_cycles!=0 && startIterEv) {
          getCPDG()->insert_edge(*startIterEv,*startSEB, xfer_cycles, E_BXFR);
        }
        if(prevStartSEB) {
          getCPDG()->insert_edge(*prevStartSEB,*startSEB, 1, E_SEBS);
        }
      } else {
        if(!prevEndSEB) {
          if(startIterEv) {
            getCPDG()->insert_edge(*startIterEv,
                      *startSEB, xfer_cycles, E_BXFR);
          } 
        } else {
           getCPDG()->insert_edge(*prevEndSEB,
               *startSEB,0,E_SEBS); 
        }
      }

      T* horizon_event = getCPDG()->getHorizon();
      uint64_t horizon_cycle = 0;
      if(horizon_event) {
        horizon_cycle=horizon_event->cycle();
      }

      if(horizon_event && startSEB->cycle() < horizon_cycle) { 
        getCPDG()->insert_edge(*horizon_event,
                               *startSEB, 0, E_HORZ);   
      }



      for(auto opi = sg->opv_begin(),ope=sg->opv_end();opi!=ope;++opi) {
        Op* op = *opi;
        BeretInst* b_inst = new BeretInst(op);

        b_inst->startSEB=startSEB;
        b_inst->endSEB=endSEB;

        binstMap.emplace(std::piecewise_construct,
                         std::forward_as_tuple(op),
                         std::forward_as_tuple(b_inst));
        assert(binstMap[op]);

        getCPDG()->insert_edge(*startSEB, *b_inst, BeretInst::Execute,0,E_SEBB);
        getCPDG()->insert_edge(*b_inst, BeretInst::Complete,*endSEB, 0, E_SEBL);

        b_inst->ex_edge = getCPDG()->insert_edge(*b_inst, BeretInst::Execute,
                   *b_inst, BeretInst::Complete,/*op->avg_lat()*/1, E_SEBL);

        b_inst->st_edge = getCPDG()->insert_edge(*b_inst, BeretInst::Complete,
                               *b_inst, BeretInst::Writeback, 0, E_SEBW);

      }
      prevEndSEB=endSEB;
      prevStartSEB=startSEB;
    }

    //Add Deps
    //(this is in a seperate loop, because we need to make sure that all the binsts
    //in each subgraph are created before continuing
    for(auto i =li2sgmap[li].sg_begin(),e =li2sgmap[li].sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->opv_begin(),ope=sg->opv_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        assert(b_inst);

        for(auto di = op->adj_d_begin(),de =op->adj_d_end();di!=de;++di) {
          Op* dop = *di;
           
          if(binstMap.count(dop) && li->forwardDep(dop,op)) {
            std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
            getCPDG()->insert_edge(*dep_BeretInst, BeretInst::Complete,
                       *b_inst, BeretInst::Execute, 0,E_SEBD);
            /*std::cout << dop->id() << "->" << op->id()
            << " " << binstMap.count(dop) << " " << li->forwardDep(dop,op);
            std::cout << "\n";*/
          }
        }
        b_inst->reCalculate();
        /*std::cout << b_inst->cycleOfStage(0) << " "
                  << b_inst->cycleOfStage(1) << " "
                  << b_inst->cycleOfStage(2) << "\n";*/
      }
    }
    return prevEndSEB;
  }
  
  void reCalcBeretLoop(bool commit_iter) {
    //recalc loop, and get last cycle
    uint64_t last_cycle=0;
    for(auto i =li2sgmap[li].sg_begin(),e =li2sgmap[li].sg_end();i!=e;++i) {
      Subgraph* sg = *i;
      for(auto opi = sg->opv_begin(),ope=sg->opv_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        assert(b_inst);
        b_inst->reCalculate();
        if(b_inst->_isload) {
          //get the MSHR resource here!
          checkNumMSHRs(b_inst); 
        }
        b_inst->reCalculate();  //TODO: check this! : 

        countAccelSGRegEnergy(op,sg,li2sgmap[li]._opset,
                              _beret_fp_ops,_beret_mult_ops,_beret_int_ops,
                              _beret_regfile_reads,_beret_regfile_freads,
                              _beret_regfile_writes,_beret_regfile_fwrites);
       
        //regfile_fwrites+=inst._numFPDestRegs;
        //regfile_writes+=inst._numIntDestRegs;


        uint64_t cycle=b_inst->cycleOfStage(BeretInst::Complete);
        if(last_cycle < cycle) {
          last_cycle=cycle;
        }
      }
    }

    //write stores from store buffer
    int iseb=0;
    for(auto i=li2sgmap[li].sg_begin(), e=li2sgmap[li].sg_end(); i!=e;++i,++iseb) {
      Subgraph* sg = *i;
      for(auto opi = sg->opv_begin(),ope=sg->opv_end();opi!=ope;++opi) {
        Op* op = *opi;
        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        assert(b_inst);
        if(commit_iter && b_inst->_isstore) {
          //make sure we can fit all of our stores
          // stores must come after all computation is complete
          checkNumMSHRs(b_inst, last_cycle); 

          //update the LSQ with this info
          insertLSQ(b_inst); 
        }
        //done with this instruction
        //getCPDG()->done(sh_inst);

/*
        if (getTraceOutputs()) {
          std::cout << "SEB" << iseb << ", op" << op->id() << ":";
          std::cout << b_inst->startSEB->cycle() << " ";
          for (unsigned i = 0; i < b_inst->numStages(); ++i) {
            std::cout << b_inst->cycleOfStage(i) << " ";
          }
          std::cout << b_inst->endSEB->cycle();


          std::cout << ",";
          dg_inst_base<T,E>& inst = *b_inst;
          for (unsigned i = 0; i < inst.numStages(); ++i) {
            printEdgeDep(std::cout,inst,i,E_FF);
            std::cout << "-" << inst[i].numPredEdges();
            std::cout << ",";
          }
 
          std::cout << "\n";
          //std::cout << b_inst->ex_lat() << "\n";
        }
*/
      }
    }

  //  std::cout << "\n";
  }

  std::shared_ptr<T> beretEndEv;

  //Debugging
  uint64_t last_iter_switch_cycle=0,last_replay_cycle=0;

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    //std::cout << op->func()->nice_name() << " " << op->cpc().first << " " << op->cpc().second << " " << op->bb()->rpoNum() << "\n";

    switch(beret_state) {
      case CPU:
        //Started BERET Loop
        if(op->bb_pos()==0) {
          li = op->func()->getLoop(op->bb());
         
          if(li && li2sgmap.count(li)!=0 && li2sgmap[li].valid()) {
            //std::cout << " .. and it's beret-able!\n";
            curLoopHead=op;
            beret_state=BERET;
            unsigned config_time = _beret_config_time*li2sgmap[li].numSubgraphs();
            if(li==_prevLoop) {
              config_time=1;
            }
          
            Inst_t* prevInst = getCPDG()->peekPipe(-1); 
 
            T* iterStartEv=NULL;
            if(prevInst) {
              _curBeretStartCycle=prevInst->cycleOfStage(prevInst->eventComplete());
              iterStartEv=&(*prevInst)[Inst_t::Commit];
            }
            _curBeretStartInst=index;

            beretEndEv = addLoopIteration(iterStartEv,8/_beret_iops + config_time);
            _prevLoop=li;
          }
        }
        break;
      case BERET:
        if(op==curLoopHead) { //came back into beret
          reCalcBeretLoop(true); //get correct beret timing
          std::shared_ptr<T> event = addLoopIteration(beretEndEv.get(),0);
          cleanLSQEntries(beretEndEv->cycle()); // TODO: i think this doesn't work with the relaxations turned on?

          //TODO, why do I need this to be so high?
          //uint64_t clean_cycle=beretEndEv->cycle()-std::min((uint64_t)100000,beretEndEv->cycle());
          //cleanUp(clean_cycle);
          T* clean_event = getCPDG()->getHorizon(); //clean at last possible moment
          uint64_t clean_cycle=0;
          if(clean_event) {
            clean_cycle=clean_event->cycle();
            cleanUp(clean_cycle);
          }

          last_iter_switch_cycle=clean_cycle; //for debugging

          beretEndEv=event;
          assert(beretEndEv);
        } else if(op->bb_pos()==0) {
          ++_whichBB;
          LoopInfo::BBvec& bbvec = li->getHotPath();
          if(bbvec.size()==_whichBB || bbvec[_whichBB] !=op->bb()) {
            beret_state = CPU;  //WRONG PATH - SWITCH TO CPU
          
            //std::shared_ptr<BeretInst> binst;
            T* finalBeretEvent;
            bool replay=false;
            if(bbvec.size()==_whichBB) {
              //binst = binstMap[*(--(li2sgmap[li][li2sgmap[li].size()-1]->op_end()))];
              reCalcBeretLoop(false); //get correct beret wrong-path timing
              finalBeretEvent=beretEndEv.get();
            } else {
               assert( bbvec[_whichBB] != op->bb() );
               std::shared_ptr<BeretInst> binst = binstMap[li->getHotPath()[_whichBB-1]->firstNonIgnoredOp()];
               reCalcBeretLoop(false); //get correct beret wrong-path timing
               finalBeretEvent=&((*binst)[BeretInst::Complete]);
               replay=true;
            }

            //get timing for beret loop
            uint64_t endBeretCyc=finalBeretEvent->cycle(); //binst->cycleOfStage(binst->eventComplete());
            _totalBeretCycles+=endBeretCyc-_curBeretStartCycle;
            //not really, but close enough
            _totalBeretInsts+=index-_curBeretStartInst;

            if(replay) {
              //Replay BERET on CPU
              for(unsigned i=0; i < replay_queue.size();++i) {
                CP_NodeDiskImage& img = std::get<0>(replay_queue[i]);
                uint64_t& index = std::get<1>(replay_queue[i]);
                Op* op = std::get<2>(replay_queue[i]);

                InstPtr sh_inst = createInst(img,index,op);
                if(i==0) {
                  getCPDG()->insert_edge(*finalBeretEvent,
                                         *sh_inst, Inst_t::Fetch,8/_beret_iops,E_BXFR); 
                }
                getCPDG()->addInst(sh_inst,index);
                addDeps(sh_inst,op); //regular add deps
                pushPipe(sh_inst);
                inserted(sh_inst);
                last_replay_cycle=sh_inst->cycleOfStage(Inst_t::Fetch);
              }
              beretEndEv = NULL;
            }
            //Done Replaying up to the instruction before the bad instruction!
            //The CPU execution is now caught up.
            replay_queue.clear();
          } else {
            //still on the loop
            assert(Prof::get().curFrame()->curLoop()==li);


          }
        }
        break;
      default:
        assert(0); //not sure what to do
        break;
    }

    switch(beret_state) {
      case CPU: {
        //base cpu model
        InstPtr sh_inst = createInst(img,index,op);
        getCPDG()->addInst(sh_inst,index);
        if(beretEndEv) {
          getCPDG()->insert_edge(*beretEndEv,
                         *sh_inst, Inst_t::Fetch, 8/_beret_iops, E_BXFR);

          beretEndEv = NULL;
        }
        addDeps(sh_inst,op);
        pushPipe(sh_inst);
        inserted(sh_inst);
        break;
      } case BERET: {
        //make beret instruction
        /*BeretInst* b_inst = new BeretInst(img,index);
        std::shared_ptr<BeretInst> sh_inst = std::shared_ptr<BeretInst>(inst);
        getCPDG()->addInst(sh_inst,index);
        addBeretDeps(*inst);*/

        if(!li2sgmap[li].opScheduled(op)) {
          break;
        }

        std::shared_ptr<BeretInst> b_inst = binstMap[op];
        assert(b_inst);
        std::shared_ptr<BeretInst> sh_inst(b_inst);
        getCPDG()->addInst(sh_inst,index);

        //this sets the latency for a beret instruction
        int lat=epLat(img._cc-img._ec,img._opclass,img._isload,
               img._isstore,img._cache_prod,img._true_cache_prod,true);
        b_inst->updateLat(lat);
        int st_lat=stLat(img._xc-img._wc,img._cache_prod,
                         img._true_cache_prod,true/*is accelerated*/);
        b_inst->updateStLat(st_lat);
        b_inst->updateImg(img);
        b_inst->_index=index;

        //std::cout << "adding index: " << b_inst->_index << "\n";

        if(_beret_dataflow_pure) {
          add_dataflow_pure_dep(*b_inst);
        } else if (_beret_dataflow_seb) {
          add_dataflow_seb_dep(*b_inst); 
        }
        replay_queue.push_back(std::make_tuple(img,index,op));
        break;
      } default:
        assert(0); //not sure what to do
        break;
    }
  }

private:

  virtual void checkNumMSHRs(std::shared_ptr<BeretInst>& n, uint64_t minT=0) {
    int ep_lat=n->ex_lat();
    
 //epLat(n->_ex_lat,n->_opclass,n->_isload,n->_isstore,
               //   n->_cache_prod,n->_true_cache_prod);
    int st_lat=stLat(n->_st_lat,n->_cache_prod,n->_true_cache_prod,true);

    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 

    int rechecks=0;
    uint64_t extraLat=0;

    uint64_t access_time=reqDelayT + n->cycleOfStage(BeretInst::Execute);

    if (minT > access_time) {
      access_time=minT;
    } 

    if(n->_isload) {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, BeretInst::Execute, mshrT+respDelayT, E_MSHR);
      }
    } else { //store
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, BeretInst::Writeback, mshrT+respDelayT, E_MSHR);
      }
    }
  }


  //Ensure that the data dependencies are enforced --
  //this is only necessary under _dataflow_seb and _dataflow_pure models
  virtual void add_dataflow_seb_dep(BeretInst &inst) {
    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst._index) {
        continue;
      }
      if(!getCPDG()->hasIdx(inst.index()-prod)) {
        continue;
      }

      dg_inst_base<T,E>& depInst=getCPDG()->queryNodes(inst.index()-prod);

      if(!depInst.isPipelineInst()) {
        BeretInst* dep_binst = dynamic_cast<BeretInst*>(&depInst);
        assert(dep_binst);
        if(dep_binst->startSEB != inst.startSEB) {
          if(!inst.startSEB->has_pred(dep_binst->endSEB.get(),E_SEBS)) {
            getCPDG()->insert_edge(*dep_binst->endSEB,*inst.startSEB, 0, E_SEBS);
          }
        } else {
          //getCPDG()->insert_edge(depInst, depInst.eventComplete(),
          //                      inst, Inst_t::Ready, 0,E_RDep);
        }
      }
    }
    
    //memory dependence
    if (inst._mem_prod > 0 && inst._mem_prod < inst._index) {
      BaseInst_t& depInst = static_cast<BaseInst_t&>( 
                          getCPDG()->queryNodes(inst.index()-inst._mem_prod));

      if(!depInst.isPipelineInst()) {
        BeretInst* dep_binst = dynamic_cast<BeretInst*>(&depInst);
        assert(dep_binst);
        if(dep_binst->startSEB != inst.startSEB
          &&(!(dep_binst->_isload && inst._isload))) {  
          if(!inst.startSEB->has_pred(dep_binst->endSEB.get(),E_SEBS)) {
            getCPDG()->insert_edge(*dep_binst->endSEB,*inst.startSEB, 0, E_SEBS);
          }
        } else {
          //getCPDG()->insert_edge(depInst, depInst.eventComplete(),
          //                      inst, Inst_t::Ready, 0,E_RDep);
        }
      }
/*    if (prev_node._isstore && inst._isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, true);
      } else if (prev_node._isstore && inst._isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, true);
      } else if (prev_node._isload && inst._isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, true);
      } */
    }
     
  }


  //Ensure that the data dependencies are enforced --
  //this is only necessary under _dataflow_seb and _dataflow_pure models
  virtual void add_dataflow_pure_dep(BeretInst &inst) {
    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst._index) {
        continue;
      }
      if(!getCPDG()->hasIdx(inst.index()-prod)) {
        continue;
      }

      dg_inst_base<T,E>& depInst=getCPDG()->queryNodes(inst.index()-prod);
      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             inst, BeretInst::Execute, 0,E_RDep);
    }
    
    //Memory dependence enforced by BB ordering -- if this is going to be
    //relaxed, then go ahead and implement mem dependence

    //memory dependence
    if (inst._mem_prod > 0 && inst._mem_prod < inst._index) {
      BaseInst_t& prev_node = static_cast<BaseInst_t&>( 
                          getCPDG()->queryNodes(inst.index()-inst._mem_prod));

      if (prev_node._isstore && inst._isload) {
        //data dependence
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, E_MDep);
      } else if (prev_node._isstore && inst._isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, E_MDep);
      } else if (prev_node._isload && inst._isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, BeretInst::Execute, 0, E_MDep);
      }
    }
  }

/*
  virtual void setCompleteCycle_beret(BeretInst& inst) {
    getCPDG()->insert_edge(inst, BeretInst::Execute,
                           inst, BeretInst::Complete, inst._ex_lat);
  }
*/

  // Handle enrgy events for McPAT XML DOC
  virtual void printAccelMcPATxml(std::string fname_base, int nm) {
    #include "mcpat-defaults.hh"
    pugi::xml_document accel_doc;
    std::istringstream ss(xml_str);
    pugi::xml_parse_result result = accel_doc.load(ss);
  
    //pugi::xml_parse_result result = doc.load_file("tony-ooo.xml");
  
    if(result) {
      pugi::xml_node system_node = accel_doc.child("component").find_child_by_attribute("name","system");
      //set the total_cycles so that we get power correctly
      sa(system_node,"total_cycles",numCycles());
      sa(system_node,"busy_cycles",0);
      sa(system_node,"idle_cycles",numCycles());

      sa(system_node,"core_tech_node",nm);
      sa(system_node,"device_type",0);


      pugi::xml_node core_node =
                system_node.find_child_by_attribute("name","core0");
      sa(core_node,"total_cycles",numCycles());
      sa(core_node,"busy_cycles",0);
      sa(core_node,"idle_cycles",numCycles());

      sa(core_node,"ALU_per_core", Prof::get().int_alu_count);
      sa(core_node,"MUL_per_core", Prof::get().mul_div_count);
      sa(core_node,"FPU_per_core", Prof::get().fp_alu_count);
  
      sa(core_node,"ialu_accesses",_beret_int_ops);
      sa(core_node,"fpu_accesses",_beret_fp_ops);
      sa(core_node,"mul_accesses",_beret_mult_ops);

      sa(core_node,"archi_Regs_IRF_size",8);
      sa(core_node,"archi_Regs_FRF_size",8);
      sa(core_node,"phy_Regs_IRF_size",64);
      sa(core_node,"phy_Regs_FRF_size",64);

      //sa(core_node,"instruction_buffer_size",4);
      //sa(core_node,"decoded_stream_buffer_size",4);
      //sa(core_node,"instruction_window_scheme",0);
      //sa(core_node,"instruction_window_size",8);
      //sa(core_node,"fp_instruction_window_size",8);

      sa(core_node,"int_regfile_reads",_beret_regfile_reads);
      sa(core_node,"int_regfile_writes",_beret_regfile_writes);
      sa(core_node,"float_regfile_reads",_beret_regfile_freads);
      sa(core_node,"float_regfile_writes",_beret_regfile_fwrites);
 
    } else {
      std::cerr << "XML Malformed\n";
      return;
    }

    std::string fname=fname_base + std::string(".accel");
    accel_doc.save_file(fname.c_str());
  }

  virtual void calcAccelEnergy(std::string fname_base,int nm) {
    std::string fname=fname_base + std::string(".accel");

    std::string outf = fname + std::string(".out");

    std::cout << _name << " accel(" << nm << "nm)... ";
    std::cout.flush();

    execMcPAT(fname,outf);
    float ialu  = stof(grepF(outf,"Integer ALUs",7,4));
    float fpalu = stof(grepF(outf,"Floating Point Units",7,4));
    float calu  = stof(grepF(outf,"Complex ALUs",7,4));
    float reg   = stof(grepF(outf,"Register Files",7,4));
    float total = ialu + fpalu + calu + reg;
    std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ")\n";
  }



};



#endif //CP_BERET
