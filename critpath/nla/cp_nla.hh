#ifndef CP_NLA
#define CP_NLA

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "loopinfo.hh"
#include "nla_inst.hh"

// CP_NLA
class cp_nla : public ArgumentHandler,
      public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef dg_inst<T, E> Inst_t;

enum NLA_STATE {
  CPU,
  NLA
};

public:
  cp_nla() : CP_DG_Builder<T,E>() {
    nla_state=CPU;
  }

  virtual ~cp_nla() { 
  }

  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;

  

  //std::map<LoopInfo*,LoopInfo::SubgraphVec> li2sgmap;
  //std::map<LoopInfo*,LoopInfo::SubgraphSet> li2ssmap;
  
  LoopInfo* _prevLoop=NULL;

  uint64_t _nla_int_ops=0, _nla_fp_ops=0, _nla_mult_ops=0;
  uint64_t _nla_regfile_fwrites=0, _nla_regfile_writes=0;
  uint64_t _nla_regfile_freads=0,  _nla_regfile_reads=0;

  unsigned _nla_max_cfu=6,_nla_max_mem=2, _nla_max_ops=80;
  unsigned _nla_config_time=1,_nla_iops=2;
  unsigned _nla_dataflow_cfu=0,_nla_dataflow_pure=0;
  unsigned _no_gams=false;

  bool _serialize_sgs=false;        //Subgraph execution is serialized
  bool _issue_inorder=false;        //Subgraphs are issued in dependence order
  bool _cfus_delay_writes=false;     //Reg and Forwarding happens at end of region
  bool _cfus_delay_reads=false;      //CFUs don't execute untill all inputs ready
  bool _inorder_address_calc=false; //mem issued in program order
  bool _mem_dep_predictor=false;    //should I predict memory?
  bool _software_mem_alias=true;    //software calculates mem deps
  bool _no_exec_speculation=true;   //no execution speculation
  bool _exclusive_cfus=false;       //impose exclusive constraints on CFU use
  bool _pipelined_cfus=true;        //impose pipeline constraints on CFU use
  int  _wb_networks=1;              //impose pipeline constraints on CFU use

  void handle_argument(const char *name, const char *arg) {
    ArgumentHandler::parse("nla_serialize_sg",        name,arg,_serialize_sgs       );
    ArgumentHandler::parse("nla_issue_inorder",       name,arg,_issue_inorder       );
    ArgumentHandler::parse("nla_cfus_delay_writes",   name,arg,_cfus_delay_writes   );
    ArgumentHandler::parse("nla_cfus_delay_reads",   name,arg,_cfus_delay_reads     );
    ArgumentHandler::parse("nla_inorder_address_calc",name,arg,_inorder_address_calc);
    ArgumentHandler::parse("nla_mem_dep_predictor",   name,arg,_mem_dep_predictor   );
    ArgumentHandler::parse("nla_software_mem_alias",   name,arg,_mem_dep_predictor  );
    ArgumentHandler::parse("nla_no_exec_speculation", name,arg,_no_exec_speculation );
    ArgumentHandler::parse("nla_exclusive_cfus",      name,arg,_exclusive_cfus      );
    ArgumentHandler::parse("nla_pipelined_cfus",      name,arg,_pipelined_cfus      );
    ArgumentHandler::parse("nla_wb_networks",         name,arg,_wb_networks         );

    if (strcmp(name, "no-gams") == 0) {
      _no_gams=true;
    }
  }

#if 0
  virtual void setDefaultsFromProf() override {
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
         && loopInfo->instsOnPath(hpi) <= (int)_nla_max_ops
         ) {
        std::stringstream part_gams_str;
        part_gams_str << _run_name << "partition." << loopInfo->id() << ".gams";
  
        bool gams_details=false;
        std::cerr << _nla_max_cfu << " " << _nla_max_mem << "\n";
        worked = loopInfo->printGamsPartitionProgram(part_gams_str.str(),
            loopInfo->getHotPath(),
            li2ssmap[loopInfo],li2sgmap[loopInfo],
            Prof::get().nla_cfus(),
            gams_details,_no_gams,_nla_max_cfu,_nla_max_mem);
        if(worked) {
          std::cerr << " -- NLAized\n";
        } else {
          std::cerr << " -- NOT NLAized (Probably had Func Calls)\n";
        }
      } else {
        std::cerr << " -- NOT NLAized -- did not satisfy criteria\n";
      }
    }
    
  }
#endif

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

    outs() << index + Prof::get().skipInsts << ": ";

    if(!getCPDG()->hasIdx(index)) {
      outs() << "optimized out";
      return;
    }

    dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);

    for (unsigned i = 0; i < inst.numStages(); ++i) {
      outs() << inst.cycleOfStage(i) << " ";
      printEdgeDep(outs(),inst,i,E_FF);

    }
    CriticalPath::traceOut(index,img,op);
    outs() << "\n";
  }
  



  virtual void accelSpecificStats(std::ostream& out, std::string& name) {
//out << "NLA Cycles = " << _totalNLACycles << "\n";
    out << " (nla-only " << _totalNLACycles 
        << " nla-insts " << _totalNLAInsts
//        << " idle-cycles " << idleCycles
//        << " non-pipeline-cycles" << nonPipelineCycles
       << ")";
  }


  std::map<Op*,std::shared_ptr<NLAInst>> nlaInstMap;
  unsigned nla_state;
  LoopInfo* li;
  uint64_t _curNLAStartCycle=0, _curNLAStartInst=0;
  uint64_t _totalNLACycles=0, _totalNLAInsts=0;

  T* nlaEndEv;
  std::shared_ptr<NLAInst> prevNLAInst;
  std::vector<std::shared_ptr<NLAInst>> curNLAInsts;
  std::shared_ptr<BaseInst_t> prevMemNode;
  std::shared_ptr<DynSubgraph> prevMemSubgraph;


  T* nlaStartEv=NULL;
  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    //std::cout << op->func()->nice_name() << " " << op->cpc().first << " " << op->cpc().second << " " << op->bb()->rpoNum() << "\n";

    switch(nla_state) {
      case CPU:
        //Started NLA Loop
        if(op->bb_pos()==0) {
          li = op->func()->getLoop(op->bb());
         
          //if(li && li2sgmap.count(li)!=0 && li2sgmap[li].size()!=0) {
          if(li &&  li->hasSubgraphs(true)) {
            //std::cout << " .. and it's nla-able!\n";
            //curLoopHead=op;
            nla_state=NLA;
            /*unsigned config_time = li->staticInsts()/3;
            if(li==_prevLoop) {
              config_time=1;
            }*/
          
            Inst_t* prevInst = getCPDG()->peekPipe(-1); 
 
            if(prevInst) {
              _curNLAStartCycle=prevInst->cycleOfStage(prevInst->eventComplete());
              nlaStartEv=&(*prevInst)[Inst_t::Commit];
            }
            //_curNLAStartInst=index;

            //nlaEndEv = addLoopIteration(iterStartEv,8/_nla_iops + config_time);
            //_prevLoop=li;
          }
        }
        break;
      case NLA:
          //reCalcNLALoop(true); //get correct nla timing
          //std::shared_ptr<T> event = addLoopIteration(nlaEndEv.get(),0);
          //cleanLSQEntries(nlaEndEv->cycle());
        if(op->bb_pos()==0) {
          BB* bb = op->bb();
          //if(bb->pred_size() != 1 || (*bb->pred_begin())->succ_size()!=1 ) {
          if((*bb->pred_begin())->succ_size()!=1 ) {
            schedule_cfus();  
          }
         
          if(!li->inLoop(bb)) {
            //need to connect bb up
            nla_state=CPU;
            nlaEndEv=&((*prevNLAInst)[NLAInst::Writeback]);
            assert(curNLAInsts.size()==0);
          }
        } 
        break;
      default:
        assert(0); //not sure what to do
        break;
    }

    switch(nla_state) {
      case CPU: {
        //base cpu model
        InstPtr sh_inst = createInst(img,index,op);
        getCPDG()->addInst(sh_inst,index);
        if(nlaEndEv) {
          getCPDG()->insert_edge(*nlaEndEv,
                         *sh_inst, Inst_t::Fetch, 8/_nla_iops, E_BXFR);

          nlaEndEv = NULL;
        }

        if(sh_inst->_isload || sh_inst->_isstore) {
          prevMemNode=sh_inst;
          prevMemSubgraph.reset();
        }

        addDeps(sh_inst,op);
        pushPipe(sh_inst);
        inserted(sh_inst);
        break;
      } case NLA: {
        if(!li->sgSchedNLA().opScheduled(op)) {
          break;
        }

        //make nla instruction
        NLAInst* inst = new NLAInst(img,index,op);
        std::shared_ptr<NLAInst> nla_inst = std::shared_ptr<NLAInst>(inst);
        keepTrackOfInstOpMap(nla_inst,op);

        getCPDG()->addInst(nla_inst,index);
        curNLAInsts.push_back(nla_inst);               
        addNLADeps(nla_inst,op,li);

        if(inst->_isload || inst->_isstore) {
          prevMemNode=nla_inst;
          prevMemSubgraph=nla_inst->dynSubgraph;
        }

        int lat=epLat(img._cc-img._ec,img._opclass,img._isload,
                      img._isstore,img._cache_prod,img._true_cache_prod,true);
        nla_inst->updateLat(lat);
        int st_lat=stLat(img._xc-img._wc,img._cache_prod,
                         img._true_cache_prod,true/*is accelerated*/);
        nla_inst->updateStLat(st_lat);

        if(nlaStartEv) {
          getCPDG()->insert_edge(*nlaStartEv,
                         *nla_inst->startCFU(), 8/_nla_iops, E_BXFR);
        }
        prevNLAInst=nla_inst;


        break;
      } default:
        assert(0); //never reaches here
        break;
    }
  }

private:

  std::map<Subgraph*,std::shared_ptr<DynSubgraph>> _sgMap;
  std::set<std::shared_ptr<DynSubgraph>> _curSubgraphs;
  std::vector<std::shared_ptr<DynSubgraph>> _vecSubgraphs;

  void visit_subgraph(std::shared_ptr<DynSubgraph> d) {
    d->done=true;
    for(auto &sg : d->use_subgraphs) {
      if(!sg->done) {
        visit_subgraph(sg);
      }
    }
    _vecSubgraphs.push_back(d);
  }

  void top_sort_subgraphs() {
    for(auto &sg1 : _curSubgraphs) {
      //don't schedule any graph which as deps inside the sched range
      for(auto &sg2 : _curSubgraphs) {
        if(sg1->use_subgraphs.count(sg2)) {
          break;
        } 
      }
      visit_subgraph(sg1);
    }
    std::reverse(_vecSubgraphs.begin(),_vecSubgraphs.end());
  }

  std::shared_ptr<T> ctrl_event;
  std::shared_ptr<T> prev_begin_cfu;
  std::shared_ptr<T> prev_end_cfu;

  void schedule_cfus() {
    top_sort_subgraphs();

    if(ctrl_event) {
      std::cout << "Schedule CFUS (last ctrl cycle = " << ctrl_event->cycle() << ")\n";
    }

    for(auto &sg : _vecSubgraphs) {
      if(_serialize_sgs) {
        if(prev_end_cfu) { // completely serial subgraphs
          getCPDG()->insert_edge(*prev_end_cfu,
                                 *sg->startCFU, 0, E_SEBD);
        }
      }

      if(_issue_inorder) { // issue CFUs in order
        if(prev_begin_cfu) {
           getCPDG()->insert_edge(*prev_begin_cfu,
                                  *sg->startCFU, 0, E_SEBD);
        }
      }

      if(_no_exec_speculation) { // serialize instructions after control
        if(ctrl_event) {
           getCPDG()->insert_edge(*ctrl_event,
                                  *sg->startCFU, 0, E_SEBD);
           //std::cout << "ctrl " << ctrl_event->cycle() << "\n";
        }
      }

      std::cout << "begin cfu" << sg->static_sg->cfu()->ind() 
        <<  " ind" << sg->ind
        <<  " at cycle: " << sg->startCFU->cycle()
        <<  "\n";

      
      auto max_inst = sg->calcCritCycles(); //must come before startCFU->reCalc
      sg->startCFU->reCalculate(); 

      if(_exclusive_cfus) {
        NLAInst* dep_inst = static_cast<NLAInst*>(
             addResource((uint64_t)sg->static_sg->cfu(),
             sg->startCFU->cycle(), sg->critCycles, 1, max_inst));

        if (dep_inst) {
          getCPDG()->insert_edge(*dep_inst->dynSubgraph->endCFU,
                                 *sg->startCFU,
                                 dep_inst->dynSubgraph->critCycles,E_FU);
        }
      }


      for(auto &inst : sg->insts) {
        std::shared_ptr<NLAInst> nla_inst = inst.lock();
        (*nla_inst)[NLAInst::Execute].reCalculate();

        if(_pipelined_cfus) {
          NLAInst* dep_inst = static_cast<NLAInst*>(
               addResource((uint64_t)sg->static_sg->getCFUNode(nla_inst->_op),
               nla_inst->cycleOfStage(NLAInst::Execute),
               getFUIssueLatency(nla_inst->_op->opclass()), 1, nla_inst));
  
          if (dep_inst) {
            getCPDG()->insert_edge(*nla_inst, NLAInst::Execute,
                                   *dep_inst, NLAInst::Execute, 
                                   getFUIssueLatency(dep_inst->_op->opclass()),E_FU);
          }
        }

        //Execute -- 
        //TODO: Should we add another event for loads/stores who?
        if(nla_inst->_isload && nla_inst->_isstore) {
          int fuIndex = fuPoolIdx(nla_inst->_opclass);
          int maxUnits = getNumFUAvailable(nla_inst->_opclass); //opclass
          Inst_t* min_node = static_cast<Inst_t*>(
               addResource(fuIndex, nla_inst->cycleOfStage(Inst_t::Execute), 
                           getFUIssueLatency(nla_inst->_opclass), maxUnits, nla_inst));
      
          if (min_node) {
            getCPDG()->insert_edge(*min_node, NLAInst::Execute,
                              *nla_inst, NLAInst::Execute, 
                              getFUIssueLatency(nla_inst->_opclass),E_FU);
          }
        }

        if(nla_inst->_isload) {
          //get the MSHR resource here!
          checkNumMSHRs(nla_inst); 
        }
        (*nla_inst)[NLAInst::Complete].reCalculate();

        if(_wb_networks != 0) {
          bool needs_forwarding=false;
          for(auto i=nla_inst->_op->d_begin(), e=nla_inst->_op->d_end();i!=e;++i) {
            Op* dop = *i;
            if(!li->sgSchedNLA().opScheduled(dop)) {
              continue;
            }
            if(sg->static_sg->getCFUNode(dop)==NULL) {
              needs_forwarding=true; 
              break;
            }
          }

          if(needs_forwarding) {
            NLAInst* dep_inst = static_cast<NLAInst*>(
                   addResource((uint64_t)&_wb_networks /*hacky hack hack*/,
                   nla_inst->cycleOfStage(NLAInst::Complete),
                   1/*latency*/, _wb_networks/*maxunits*/, nla_inst));
  
            if (dep_inst) {
              getCPDG()->insert_edge(*nla_inst, NLAInst::Complete,
                                     *dep_inst, NLAInst::Complete, 
                                     1,E_FU);
            }
          }
        }

        if(nla_inst->_isstore) {
          checkNumMSHRs(nla_inst); 
        }
        (*nla_inst)[NLAInst::Forward].reCalculate();
        (*nla_inst)[NLAInst::Writeback].reCalculate();
        std::cout << nla_inst->_index 
                 << ": "  << nla_inst->cycleOfStage(NLAInst::Execute)
                 << ","   << nla_inst->cycleOfStage(NLAInst::Complete)
                 << " (op:" << nla_inst->_op->id() << ")"
                 << "\n";
      }
      prev_begin_cfu=sg->startCFU;
      prev_end_cfu=sg->endCFU;

      sg->endCFU->reCalculate();

      std::cout << "end cfu: "
        <<  sg->endCFU->cycle()
        <<  "\n";
    }

    ctrl_event=prev_end_cfu; //TODO: make this more robust
    curNLAInsts.clear();
    _curSubgraphs.clear();
    _vecSubgraphs.clear();
  }

  virtual void checkNumMSHRs(std::shared_ptr<NLAInst>& n, uint64_t minT=0) {
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

    uint64_t access_time=reqDelayT + n->cycleOfStage(NLAInst::Execute);

    if (minT > access_time) {
      access_time=minT;
    } 

    if(n->_isload) {
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, NLAInst::Execute, mshrT+respDelayT, E_MSHR);
      }
    } else { //store
      BaseInst_t* min_node =
           addMSHRResource(access_time, 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, NLAInst::Writeback, mshrT+respDelayT, E_MSHR);
      }
    }
  }

  int dynSGindex=0;
  void addNLADeps(std::shared_ptr<NLAInst>& n, Op* op, LoopInfo* li) {
    Subgraph* sg = li->sgSchedNLA().sgForOp(op);
    assert(sg);

    std::shared_ptr<DynSubgraph> dynSubgraph = _sgMap[sg];

    if(!dynSubgraph || dynSubgraph->ops_in_subgraph.count(op) == 1) {
      n->dynSubgraph = std::make_shared<DynSubgraph>(sg,dynSGindex++); //initialize dynsubgrpah
      _sgMap[sg] = n->dynSubgraph;
      _curSubgraphs.insert(n->dynSubgraph);
    } else {
      n->dynSubgraph = dynSubgraph;
    }

    n->dynSubgraph->ops_in_subgraph.insert(op);
    n->dynSubgraph->insts.push_back(n);

    //Hook up begining and end
    getCPDG()->insert_edge(*n->startCFU(), *n, NLAInst::Execute, 0, E_SEBB);

    n->ex_edge = getCPDG()->insert_edge(*n, NLAInst::Execute,
                            *n, NLAInst::Complete,/*op->avg_lat()*/1, E_SEBL);

    getCPDG()->insert_edge(*n, NLAInst::Complete,*n->endCFU(),   0, E_SEBL);

    n->st_edge = getCPDG()->insert_edge(*n, NLAInst::Complete,
                            *n, NLAInst::Forward, 0, E_SEBW);

    n->st_edge = getCPDG()->insert_edge(*n, NLAInst::Forward,
                            *n, NLAInst::Writeback, 0, E_SEBW);


    //serialize on dependent CFUs
    for (int i = 0; i < 7; ++i) {
      unsigned prod = n->_prod[i];
      if (prod <= 0 || prod >= n->_index) {
        continue;
      }

      if(!getCPDG()->hasIdx(n->index()-prod)) {
        continue;
      }
      dg_inst_base<T,E>& depInst=getCPDG()->queryNodes(n->index()-prod);
      

      if(!depInst.isPipelineInst()) {
        NLAInst* prior_nla_inst = dynamic_cast<NLAInst*>(&depInst);
        if(prior_nla_inst->dynSubgraph != n->dynSubgraph) {
          if(_cfus_delay_writes) {
            getCPDG()->insert_edge(*prior_nla_inst->endCFU(),*n->startCFU(), 0, E_SEBX);
          } 
         DynSubgraph::addDep(prior_nla_inst->dynSubgraph,n->dynSubgraph);
        }
      }
      if(_cfus_delay_reads) {
        getCPDG()->insert_edge(depInst,depInst.eventComplete(),*n->startCFU(), 0, E_SEBX);
      }
      //dataflow deps
      getCPDG()->insert_edge(depInst,depInst.eventComplete(),
                             *n,NLAInst::Execute, 0, E_SEBD);
    }

    if(_inorder_address_calc) {
      if(n->_isload || n->_isstore) {
        getCPDG()->insert_edge(*prevMemNode,prevMemNode->eventReady(),
                               *n,NLAInst::Execute, 1, E_SEBD);
        if(prevMemSubgraph) {
          DynSubgraph::addDep(prevMemSubgraph,n->dynSubgraph);
        }
      }
    }

    if(_software_mem_alias) {
      //iterate through potential memory deps
      for(auto mdi = op->m_begin(), mde = op->m_end(); mdi!=mde; ++mdi) {
        Op* md_op = *mdi;
        std::shared_ptr<BaseInst_t> sh_inst = getInstForOp(md_op);
        if(!sh_inst->isPipelineInst()) {
          NLAInst* mem_dep_inst = dynamic_cast<NLAInst*>(sh_inst.get());
          DynSubgraph::addDep(mem_dep_inst->dynSubgraph,n->dynSubgraph);
          getCPDG()->insert_edge(*mem_dep_inst,NLAInst::Execute,
                                 *n,NLAInst::Execute, 1, E_SEBD);
        }
      }
    }

    if(_mem_dep_predictor) {
      //memory dependence
      if(n->_isload || n->_isstore) {
        if ( (n->_mem_prod > 0 && n->_mem_prod < n->index() )  ) {
          BaseInst_t& dep_inst=getCPDG()->queryNodes(n->index()-n->_mem_prod);
          addTrueMemDep(dep_inst,*n);
        }
        insert_mem_predicted_edges(*n); 
      }
    } 

    //memory dependence
    if (n->_mem_prod > 0 && n->_mem_prod < n->_index) {
      BaseInst_t& prev_node = static_cast<BaseInst_t&>( 
                          getCPDG()->queryNodes(n->index()-n->_mem_prod));

      insert_mem_dep_edge(prev_node,*n);
    }
  }

#if 0
  //Ensure that the data dependencies are enforced --
  //this is only necessary under _dataflow_cfu and _dataflow_pure models
  virtual void add_dataflow_cfu_dep(NLAInst &inst) {
    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst._index) {
        continue;
      }
      dg_inst_base<T,E>& depInst=getCPDG()->queryNodes(inst.index()-prod);

      if(!depInst.isPipelineInst()) {
        NLAInst* dep_binst = dynamic_cast<NLAInst*>(&depInst);
        assert(dep_binst);
        if(dep_binst->startCFU != inst.startCFU) {
          if(!inst.startCFU->has_pred(dep_binst->endCFU.get(),E_CFUS)) {
            getCPDG()->insert_edge(*dep_binst->endCFU,*inst.startCFU, 0, E_CFUS);
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
        NLAInst* dep_binst = dynamic_cast<NLAInst*>(&depInst);
        assert(dep_binst);
        if(dep_binst->startCFU != inst.startCFU
          &&(!(dep_binst->_isload && inst._isload))) {  
          if(!inst.startCFU->has_pred(dep_binst->endCFU.get(),E_CFUS)) {
            getCPDG()->insert_edge(*dep_binst->endCFU,*inst.startCFU, 0, E_CFUS);
          }
        } 
      }
    }
     
  }


  //Ensure that the data dependencies are enforced --
  //this is only necessary under _dataflow_cfu and _dataflow_pure models
  virtual void add_dataflow_pure_dep(NLAInst &inst) {
    for (int i = 0; i < 7; ++i) {
      unsigned prod = inst._prod[i];
      if (prod <= 0 || prod >= inst._index) {
        continue;
      }
      dg_inst_base<T,E>& depInst=getCPDG()->queryNodes(inst.index()-prod);
      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             inst, NLAInst::Execute, 0,E_RDep);
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
                                  inst, NLAInst::Execute, 0, E_MDep);
      } else if (prev_node._isstore && inst._isstore) {
        //anti dependence (output-dep)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, NLAInst::Execute, 0, E_MDep);
      } else if (prev_node._isload && inst._isstore) {
        //anti dependence (load-store)
        getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                                  inst, NLAInst::Execute, 0, E_MDep);
      }
    }
  }
#endif

/*
  virtual void setCompleteCycle_nla(NLAInst& inst) {
    getCPDG()->insert_edge(inst, NLAInst::Execute,
                           inst, NLAInst::Complete, inst._ex_lat);
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
  
      sa(core_node,"ialu_accesses",_nla_int_ops);
      sa(core_node,"fpu_accesses",_nla_fp_ops);
      sa(core_node,"mul_accesses",_nla_mult_ops);

      sa(core_node,"archi_Regs_IRF_size",8);
      sa(core_node,"archi_Regs_FRF_size",8);
      sa(core_node,"phy_Regs_IRF_size",64);
      sa(core_node,"phy_Regs_FRF_size",64);

      //sa(core_node,"instruction_buffer_size",4);
      //sa(core_node,"decoded_stream_buffer_size",4);
      //sa(core_node,"instruction_window_scheme",0);
      //sa(core_node,"instruction_window_size",8);
      //sa(core_node,"fp_instruction_window_size",8);

      sa(core_node,"int_regfile_reads",_nla_regfile_reads);
      sa(core_node,"int_regfile_writes",_nla_regfile_writes);
      sa(core_node,"float_regfile_reads",_nla_regfile_freads);
      sa(core_node,"float_regfile_writes",_nla_regfile_fwrites);
 
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
    float ialu  = stof(grepF(outf,"Integer ALUs",7,5));
    float fpalu = stof(grepF(outf,"Floating Point Units",7,5));
    float calu  = stof(grepF(outf,"Complex ALUs",7,5));
    float reg   = stof(grepF(outf,"Register Files",7,5)) * 3;
    float total = ialu + fpalu + calu + reg;
    std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ")\n";
  }



};



#endif //CP_NLA
