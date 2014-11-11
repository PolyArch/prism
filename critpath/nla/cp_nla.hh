#ifndef CP_NLA
#define CP_NLA

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

#include "loopinfo.hh"
#include "nla_inst.hh"
#include "cp_utils.hh"

class NLARegionStats {
  public:
    uint64_t times_started=0;
    uint64_t total_cycles=0;    
    uint64_t total_insts=0;    
    uint64_t total_cinsts=0;    
    uint64_t network_activity[16]={0}; //histogram
    uint64_t cfu_used[16]={0};
    //CumWeights cum_weights;
};

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

  //This map tracks the BBs which will be "readied" by the particular instruction
  std::unordered_map<BB*, std::shared_ptr<NLAInst>> ctrl_inst_map;
  std::unordered_map<BB*, std::shared_ptr<Inst_t>> host_ctrl_inst_map;

  uint64_t _nla_int_ops=0, _nla_fp_ops=0, _nla_mult_ops=0;
  uint64_t _nla_regfile_fwrites=0, _nla_regfile_writes=0; //regfile is window acc
  uint64_t _nla_regfile_freads=0,  _nla_regfile_reads=0;

  uint64_t _nla_network_writes=0; //regfile is window acc
  uint64_t _nla_network_reads=0;
  uint64_t _nla_operand_fwrites=0, _nla_operand_writes=0; //regfile is window acc
  uint64_t _nla_operand_freads=0,  _nla_operand_reads=0;

  //Acc stats
  uint64_t _nla_int_ops_acc=0, _nla_fp_ops_acc=0, _nla_mult_ops_acc=0;
  uint64_t _nla_regfile_fwrites_acc=0, _nla_regfile_writes_acc=0;
  uint64_t _nla_regfile_freads_acc=0,  _nla_regfile_reads_acc=0;

  uint64_t _nla_network_writes_acc=0; 
  uint64_t _nla_network_reads_acc=0;
  uint64_t _nla_operand_fwrites_acc=0, _nla_operand_writes_acc=0; 
  uint64_t  _nla_operand_freads_acc=0,  _nla_operand_reads_acc=0;

  uint64_t _timesStarted=0;
  std::map<int,int> accelLogHisto;

  std::map<int,LoopInfo*> _id2li;
  std::map<LoopInfo*,NLARegionStats> _regionStats;

  unsigned _nla_max_cfu=6,_nla_max_mem=2, _nla_max_ops=80;
  unsigned _nla_config_time=1,_nla_iops=2;
  unsigned _nla_dataflow_cfu=0,_nla_dataflow_pure=0;

  bool _nla_dataflow_ctrl=true;
  bool _no_gams=false;

  bool _serialize_sgs=false;        //Subgraph execution is serialized
  bool _issue_inorder=false;        //Subgraphs are issued in dependence order
  bool _cfus_delay_writes=false;    //Reg and Forwarding happens at end of region
  bool _cfus_delay_reads=false;     //CFUs don't execute untill all inputs ready
  bool _inorder_address_calc=false; //mem issued in program order
  bool _mem_dep_predictor=false;    //should I predict memory?
  bool _software_mem_alias=true;    //software calculates mem deps
  bool _no_exec_speculation=true;   //no execution speculation
  bool _exclusive_cfus=false;       //impose exclusive constraints on CFU use
  bool _pipelined_cfus=true;        //impose pipeline constraints on CFU use
  bool _inorder_per_sg=true;        //no execution speculation

  bool _nla_ser_loops=false;         //impose pipeline constraints on CFU use
  int  _nla_loop_iter_dist=4;       //number of allowable loops in parallel
  int  _wb_networks=2;              //writeback networks total

  bool _predict_power_gating=true;  //predict time and shut off core?
  bool _optim_power_gating=false;   //don't charge for power gating
  unsigned _power_gating_factor=100; //how much longer than overhead factor?


  std::map<LoopInfo*, uint64_t> running_avg_cycles;

  void handle_argument(const char *name, const char *arg) {
    ArgumentHandler::parse("nla-serialize-sgs",        name,arg,_serialize_sgs       );
    ArgumentHandler::parse("nla-issue-inorder",        name,arg,_issue_inorder       );
    ArgumentHandler::parse("nla-cfus-delay-writes",    name,arg,_cfus_delay_writes   );
    ArgumentHandler::parse("nla-cfus-delay-reads",     name,arg,_cfus_delay_reads    );
    ArgumentHandler::parse("nla-inorder-address-calc", name,arg,_inorder_address_calc);
    ArgumentHandler::parse("nla-mem-dep-predictor",    name,arg,_mem_dep_predictor   );
    ArgumentHandler::parse("nla-software-mem-alias",   name,arg,_mem_dep_predictor   );
    ArgumentHandler::parse("nla-no-exec-speculation",  name,arg,_no_exec_speculation );
    ArgumentHandler::parse("nla-exclusive-cfus",       name,arg,_exclusive_cfus      );
    ArgumentHandler::parse("nla-pipelined-cfus",       name,arg,_pipelined_cfus      );
    ArgumentHandler::parse("nla-wb-networks",          name,arg,_wb_networks         );
    ArgumentHandler::parse("nla-dataflow-ctrl",        name,arg,_nla_dataflow_ctrl   );
    ArgumentHandler::parse("nla-inorder-per-sg",       name,arg,_inorder_per_sg      );

    ArgumentHandler::parse("nla-ser-loops",            name,arg,_nla_ser_loops       );
    ArgumentHandler::parse("nla-loop-iter-dist",       name,arg,_nla_loop_iter_dist  );

    ArgumentHandler::parse("no-gams",name,_no_gams);
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
        << " times " << _timesStarted;

    out << " hist ";
    for(auto const& p : accelLogHisto) {
      out << p.first << ":" << p.second << " ";
    }

//        << " idle-cycles " << idleCycles
//        << " non-pipeline-cycles" << nonPipelineCycles
    out   << ")";
  }


  std::map<Op*,std::shared_ptr<NLAInst>> nlaInstMap;
  unsigned nla_state;
  bool prev_transition=false;
  LoopInfo* li=NULL;  // the current loop info
  LoopInfo* _prev_li=NULL;  // the current loop info
  uint64_t _curNLAStartCycle=0, _curNLAStartInst=0;
  uint64_t _totalNLACycles=0, _totalNLAInsts=0;

//  T* nlaEndEv;
  std::shared_ptr<T> nlaEndEv;

  //Start Instructions to NLA region
  std::shared_ptr<Inst_t> nla_config_inst;
  std::shared_ptr<Inst_t> nla_start_inst;
  
  //possible config insts
  std::shared_ptr<Inst_t> func_transition_inst;
  std::shared_ptr<Inst_t> nla_done_inst;


  std::shared_ptr<NLAInst> prevNLAInst;
  std::vector<std::shared_ptr<NLAInst>> curNLAInsts;
  std::shared_ptr<BaseInst_t> prevMemNode;
  std::shared_ptr<DynSubgraph> prevMemSubgraph;

  //Use these variables to tell if I should check if we entered a new loop
  BB* prev_bb=NULL;
  int prev_bb_pos=-1;


  virtual void monitorFUUsage(uint64_t id, uint64_t numCycles, int usage) override {
    //Oh god this i/sets hacky
    if(id==(uint64_t)&_wb_networks) {
      _regionStats[li].network_activity[usage]+=numCycles;
    } else {
      if(_cfu_set) {
        for(auto i=_cfu_set->cfus_begin(),e=_cfu_set->cfus_end();i!=e;++i) {
          CFU* cfu = *i;
          if(id==(uint64_t)cfu) {
            _regionStats[li].cfu_used[cfu->ind()]+=numCycles;
            break;
          }
        }
      }
    }
  }


  CFU_set* _cfu_set=NULL;
  void setupCFUs(CFU_set* cfu_set) {
    if(cfu_set == _cfu_set) {
      return;
    }
    _cfu_set=cfu_set;

    //Add monitors too
    for(auto i=cfu_set->cfus_begin(),e=cfu_set->cfus_end();i!=e;++i) {
      CFU* cfu = *i;
      fu_monitors.insert((uint64_t)cfu); //NEATO
    }
    fu_monitors.insert((uint64_t)&_wb_networks); //Total insanity
  }

  bool should_power_gate_core() {
    return _predict_power_gating && 
           (running_avg_cycles[li] > _power_gating_factor * _cpu_wakeup_cycles);
  } 
 

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    //std::cout << op->func()->nice_name() << " " << op->cpc().first << " " << op->cpc().second << " " << op->bb()->rpoNum() << "\n";

    BB* bb = op->bb();
    FunctionInfo* f = bb->func();        

    unsigned old_nla_state=nla_state; 

    // --------------------------------------------------------------------------
    // --------------------------- Decide whether to NLA? -----------------------

    switch(nla_state) {
      case CPU:

        //Started NLA Loop
        if(prev_bb!=op->bb() || op->bb_pos() != prev_bb_pos+1) {
          li = op->func()->getLoop(op->bb());

          std::shared_ptr<Inst_t> prevInst = getCPDG()->peekPipe_sh(-1);

          if(li &&  li->hasSubgraphs(true) && prevInst) {
            setupCFUs(li->sgSchedNLA().cfu_set());
            nla_state=NLA;

            _id2li[li->id()]=li; //just do this for now, maybe there is a better way

            _timesStarted+=1;
            _regionStats[li].times_started+=1;

            _curNLAStartCycle=prevInst->cycleOfStage(prevInst->eventComplete());
            nla_start_inst=prevInst;
            assert(nla_start_inst);

            //need to pick between most recent possible config:
            nla_config_inst = NULL;

            if(_prev_li==NULL || li!=_prev_li) {
              //control dep instruction
              if(host_ctrl_inst_map.count(bb)) {
                nla_config_inst = host_ctrl_inst_map[bb];
              }
  
              //func xfer
              if(func_transition_inst  && (!nla_config_inst || 
                              func_transition_inst->_index > nla_config_inst->_index)) {
                nla_config_inst = func_transition_inst;
              }
  
              //nlaEndEv
              if(nla_done_inst && (!nla_config_inst || 
                                    nla_done_inst->_index > nla_config_inst->_index)) {
                nla_config_inst = nla_done_inst;
              }
              assert(nla_config_inst); //make sure one of these things is on
            }


            //Manage Controling Instructions
            Op* prev_op = prevInst->_op;
            if(prev_op->isCondCtrl() || prev_op->isIndirectCtrl()) {
      
              BB* prev_bb = prev_op->bb();
              if(f->pdomR_has(prev_bb)) {
                for(auto ii = f->pdomR_begin(prev_bb), ee = f->pdomR_end(prev_bb); ii != ee; ++ii) {
                  BB* cond_bb = ii->first;
                  BB* trigger_bb = ii->second;
                  if(cond_bb == bb) { //if we went in this direction
                    host_ctrl_inst_map[trigger_bb]=prevInst;
                  }
                }
              }
            }
            
          }
        }
        break;
      case NLA:

        if(op->bb_pos()==0) {
          BB* bb = op->bb();
          
          //TODO: is this the right condition?
          bool fall_through=true;
          for(auto i=bb->pred_begin(), e=bb->pred_end();i!=e;++i) {
            if((*i)->succ_size()!=1) {
              fall_through=false;
              break;
            }
          }
          if(bb->pred_size()==0 || fall_through==false) {
            schedule_cfus();  

            T* clean_event = getCPDG()->getHorizon(); //clean at last possible moment
            if(clean_event) {
              uint64_t clean_cycle=clean_event->cycle();
              cleanLSQEntries(clean_cycle);
              cleanUp(clean_cycle);
            }
          }

          if(!li->inLoop(bb)) {
            //need to connect bb up
            nla_state=CPU;
            nlaEndEv=prevNLAInst->endCFU();
            assert(curNLAInsts.size()==0);
            doneNLA(nlaEndEv->cycle());
            cleanUp(nlaEndEv->cycle()-1);
          }
        }
        break;
      default:
        assert(0); //not sure what to do
        break;
    }

    // --------------------------------------------------------------------------
    // --------------------------- Insert the Instruction -----------------------
    switch(nla_state) {
      case CPU: {
        //base cpu model
        InstPtr sh_inst = createInst(img,index,op);
        getCPDG()->addInst(sh_inst,index);

        if(nlaEndEv) { //First instruction
          uint64_t transition_cycles=8/_nla_iops; //change this to transfer live ins.
          if(_cpu_power_gated) {
            transition_cycles+=_cpu_wakeup_cycles;
          } 

          getCPDG()->insert_edge(*nlaEndEv,
                         *sh_inst, Inst_t::Fetch, transition_cycles, E_NCPU);

          nlaEndEv = NULL;
          nla_done_inst = sh_inst;
        } else { //after this instruction, lets turn power gating back off, regardless
                 //of what ever else happened
          _cpu_power_gated=false;
        }

        FunctionInfo* f = op->func();
        if(!func_transition_inst || f!=func_transition_inst->_op->func()) {
          func_transition_inst = sh_inst;
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
          //if(op->plainMove()) {
          createDummy(img,index,op);
          //}
          break;
        }

        if(old_nla_state != nla_state) {
          prev_transition=true;
        }
        if(prev_transition) {
          if(_optim_power_gating || should_power_gate_core()) {
            _cpu_power_gated=true; //wooooo!
          }
          prev_transition=false;
        }


        //make nla instruction
        //NLAInst* inst = new NLAInst(img,index,op);
        //std::shared_ptr<NLAInst> nla_inst = std::shared_ptr<NLAInst>(inst);
        auto nla_inst = std::make_shared<NLAInst>(img,index,op); 

        //setup iter number
        //LoopInfo* cur_li = Prof::get().curFrame()->curLoop();
        int iter = Prof::get().curFrame()->getLoopIterNum();
        //cout << "                   iter " << iter << "\n";
        nla_inst->iter=iter;

        //find loop lateches
        
        if(op->bb()->lastOp() == op) {
          LoopInfo* li = op->func()->innermostLoopFor(op->bb());
          if(li->isLatch(op->bb())) {
            
          }
        }

        //figure out which BBs this triggers.
        if(prevNLAInst) {
          Op* prev_op = prevNLAInst->_op;
          if(prev_op->isCondCtrl() || prev_op->isIndirectCtrl()) {
            BB* bb = op->bb();
            FunctionInfo* f = bb->func();
  
            BB* prev_bb = prev_op->bb();
            if(f->pdomR_has(prev_bb)) {
              for(auto ii = f->pdomR_begin(prev_bb), ee = f->pdomR_end(prev_bb); ii != ee; ++ii) {
                BB* cond_bb = ii->first;
                BB* trigger_bb = ii->second;
                if(cond_bb == bb) { //if we went in this direction
                  ctrl_inst_map[trigger_bb]=prevNLAInst;
                }
              }
            }
          }
        }

        getCPDG()->addInst(nla_inst,index);
        curNLAInsts.push_back(nla_inst);               
        addNLADeps(nla_inst,op,li);

        if(nla_inst->_isload || nla_inst->_isstore) {
          prevMemNode=nla_inst;
          prevMemSubgraph=nla_inst->dynSubgraph;
        }

        int lat=epLat(img._cc-img._ec,nla_inst.get(),img._isload,
                      img._isstore,img._cache_prod,img._true_cache_prod,true);
        nla_inst->updateLat(lat);
        int st_lat=stLat(img._xc-img._wc,img._cache_prod,
                         img._true_cache_prod,true/*is accelerated*/);
        nla_inst->updateStLat(st_lat);

        prevNLAInst=nla_inst;

        // delay to enable access prev instance of op
        keepTrackOfInstOpMap(nla_inst,op); 

        break;
      } default:
        assert(0); //never reaches here
        break;
    }

    prev_bb=op->bb();
    prev_bb_pos=op->bb_pos();
  }

  void doneNLA(uint64_t curCycle) {
    uint64_t cycle_diff = curCycle-_curNLAStartCycle;
    _regionStats[li].total_cycles+=cycle_diff;
    _totalNLACycles+=cycle_diff; 
    accelLogHisto[mylog2(cycle_diff)]++;
    running_avg_cycles[li]=running_avg_cycles[li]/2+cycle_diff/2;
    _prev_li=li;
  }

  virtual uint64_t finish() {
    uint64_t curCycle = numCycles();
    if(nla_state==NLA){
      doneNLA(curCycle);
    }
    return curCycle;
  }

    /*
  virtual uint64_t numCycles() {
    uint64_t curCycle = CP_DG_Builder::numCycles();

     if(nla_state==NLA) {
       _totalNLACycles+= curCycle-_curNLAStartCycle;
       _curNLAStartCycle=curCycle;
     }

    return curCycle;
  }*/



private:

  std::map<Subgraph*,std::shared_ptr<DynSubgraph>> _sgMap;
  std::map<Subgraph*,std::shared_ptr<DynSubgraph>> _prevSGMap;
  std::map<LoopInfo*,std::shared_ptr<NLAInst>> _latchOp;
  std::map<LoopInfo*,std::vector<std::shared_ptr<DynSubgraph>>> _lastOp;
  std::set<std::shared_ptr<DynSubgraph>> _curSubgraphs;
  std::vector<std::shared_ptr<DynSubgraph>> _vecSubgraphs;

  //standard depth first search implementation
  //1 -> seen, will finish
  //2 -> done, done
  void visit_subgraph(std::shared_ptr<DynSubgraph> d) {
    if(d->mark==1) {
      assert(0 && "set of subgraphs has cycle!\n");
    }
    if(d->mark==2) {
      return; 
    }
    d->mark = 1;
    //cout << d->static_sg->id() << " started\n";
    for(const auto &sgw : d->use_subgraphs) {
      const auto sg = sgw.lock();
      if(!sg->mark) {
        visit_subgraph(sg);
      }
    }
    //cout << d->static_sg->id() << "   stopped\n";
    d->mark=2; //now I'm done with it
    _vecSubgraphs.push_back(d);
  }

  void top_sort_subgraphs() {
    //cout << "sorting\n";
    for(const auto &sg1 : _curSubgraphs) {
      visit_subgraph(sg1);
    }

    bool assert_0=false;
    for(const auto &sg1 : _curSubgraphs) {
      if(sg1->mark!=2) {
        cout << sg1->static_sg->id() << " not found\n";
        assert_0=true;
      }
    }
    assert(assert_0==false);

    std::reverse(_vecSubgraphs.begin(),_vecSubgraphs.end());
    assert(_vecSubgraphs.size() == _curSubgraphs.size());
  }

  std::shared_ptr<NLAInst> ctrl_inst;

  std::shared_ptr<NLAInst> prev_ctrl_inst;
  std::shared_ptr<T> ctrl_event;
  std::shared_ptr<T> prev_begin_cfu;
  std::shared_ptr<T> prev_end_cfu;
  std::shared_ptr<DynSubgraph> prev_sg;

  // ----------------------------------Schedule CFUs------------------------------
  void schedule_cfus() {
    top_sort_subgraphs();

    T* horizon_event = getCPDG()->getHorizon(); 
    uint64_t horizon_cycle = 0;
    if(horizon_event) {
      horizon_cycle=horizon_event->cycle();
    }

    for(const auto &sg : _vecSubgraphs) { //-----------------FOR EACH SG------------

      assert(nla_start_inst); //have to have some start point
      if(nla_start_inst) {
        getCPDG()->insert_edge(*nla_start_inst, Inst_t::Commit,
                               *sg->startCFU, 8/_nla_iops,
                               //li->sgSchedNLA().numSubgraphs(),
                               E_NCPU); //TODO fix xfer
      }

      if(nla_config_inst) {
        getCPDG()->insert_edge(*nla_config_inst, Inst_t::Complete,
                               *sg->startCFU, /*8/_nla_iops*/ 
                               li->sgSchedNLA().numSubgraphs(),
                               E_NCFG); //TODO fix xfer
      }


      if(_serialize_sgs) {
        if(prev_end_cfu) { // completely serial subgraphs
          getCPDG()->insert_edge(*prev_end_cfu,
                                 *sg->startCFU, 0, E_NSER);
        }
      }

      if(_issue_inorder) { // issue CFUs in order
        if(prev_begin_cfu) {
           getCPDG()->insert_edge(*prev_begin_cfu,
                                  *sg->startCFU, 0, E_NSER);
//            cout << "NSER  " << prev_sg->static_sg->id() << "->" 
//                 << sg->static_sg->id() << "\n";
        }
      }

      if(_inorder_per_sg) {
        if(_prevSGMap.count(sg->static_sg)) {
          getCPDG()->insert_edge(*_prevSGMap[sg->static_sg]->startCFU,
                                 *sg->startCFU,  1, E_NSER);
        }
      }

      if(_no_exec_speculation) { // serialize instructions after control
        if(_nla_dataflow_ctrl) {
          std::shared_ptr<NLAInst> nla_inst = sg->insts.begin()->lock();
          BB* bb = nla_inst->_op->bb();

          if(ctrl_inst_map.count(bb)) {
            getCPDG()->insert_edge(*ctrl_inst_map[bb], NLAInst::Complete,
                                                 *sg->startCFU,  0,E_NCTL);
          }
        } else {
          if(ctrl_event) {
             getCPDG()->insert_edge(*ctrl_event, *sg->startCFU, 0, E_NCTL);
          }
        }

      } else { //Speculative
        if(ctrl_event && ctrl_inst && ctrl_inst->_ctrl_miss) {
          getCPDG()->insert_edge(*ctrl_event, *sg->startCFU, 10, E_NCTL); 
        }
      }

      if(_nla_ser_loops && prev_ctrl_inst) {
        //check if we need to wait for a loop to be done to issue this sg
        //std::shared_ptr<NLAInst> nla_inst = sg->insts.begin()->lock();
        Op* ctrl_op = prev_ctrl_inst->_op;
        LoopInfo* old_li = ctrl_op->func()->innermostLoopFor(ctrl_op->bb());
 
        std::shared_ptr<NLAInst> nla_inst = sg->insts.begin()->lock();
        LoopInfo* new_li = nla_inst->_op->func()->innermostLoopFor(nla_inst->_op->bb());

        if(old_li!=new_li) {                     //TODO: change to fwd
          getCPDG()->insert_edge(*prev_ctrl_inst,NLAInst::Complete,
                                 *sg->startCFU, 0, E_NSLP);  
         
          /*for(auto i = _sgMap.begin(), e = _sgMap.end(); i!=e; ++i) {
            Subgraph* old_sg = i->first;
            BB* sg_bb = (*old_sg->op_begin())->bb();
            if(old_li->inLoop(sg_bb)) {
              getCPDG()->insert_edge(*i->second->startCFU,
                                     *sg->startCFU, 0, E_SEBD);  
            }
          }*/

        }
      }

      std::shared_ptr<NLAInst> begin_nla_inst = sg->insts.begin()->lock();
      int iter = begin_nla_inst->iter;
      int mod_iter = iter % _nla_loop_iter_dist;
      LoopInfo* cur_li = 
        begin_nla_inst->_op->func()->innermostLoopFor(begin_nla_inst->_op->bb());

      if(iter>=_nla_loop_iter_dist) {
        std::shared_ptr<DynSubgraph> holdup_sg = 
          _lastOp[cur_li][ (iter-_nla_loop_iter_dist+1)%(_nla_loop_iter_dist) ];
        
        getCPDG()->insert_edge(*holdup_sg->endCFU,
                               *sg->startCFU, 0, E_NITR);   
      }

      //Check LSQ Size
      for(auto &inst : sg->insts) {
        std::shared_ptr<NLAInst> nla_inst = inst.lock();
        if(nla_inst->_isstore) {
          checkLSQSize(*sg->startCFU,nla_inst->_isload,nla_inst->_isstore);
          break;
        }
      }

      //Check Horizon
      if(horizon_event && sg->startCFU->cycle() < horizon_cycle) { 
        getCPDG()->insert_edge(*horizon_event,
                               *sg->startCFU, 0, E_HORZ);   
      }

//      std::cout << "begin cfu" << sg->static_sg->cfu()->ind() 
//        <<  " ind" << sg->ind
//        <<  " at cycle: " << sg->startCFU->cycle()
//        <<  "\n";

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

      /*std::shared_ptr<NLAInst> nla_inst = sg->insts.begin()->lock();
      BB* bb = nla_inst->_op->bb();

      cout << "begin cfu: " << sg->startCFU->cycle()
           <<"   bb" << bb->rpoNum() << " inst" << nla_inst->_op->id() << "\n";
           */


      for(auto &inst : sg->insts) {
        std::shared_ptr<NLAInst> nla_inst = inst.lock();
        (*nla_inst)[NLAInst::Execute].reCalculate();

        if(_pipelined_cfus) {
          NLAInst* dep_inst = static_cast<NLAInst*>(
               addResource((uint64_t)sg->static_sg->getCFUNode(nla_inst->_op),
               nla_inst->cycleOfStage(NLAInst::Execute),
               getFUIssueLatency(nla_inst->_op->opclass(),nla_inst->_op), 1, nla_inst));
  
          if (dep_inst) {
            getCPDG()->insert_edge(*nla_inst, NLAInst::Execute,
                                   *dep_inst, NLAInst::Execute, 
                                   getFUIssueLatency(dep_inst->_op->opclass(),
                                     dep_inst->_op),E_FU);
          }
        }

        //Execute -- 
        //TODO: Should we add another event for loads/stores who?
        //
        //Hmm: I think this was supposed to be memory port check.  I am updating to
        //reflext this.  (condition non-sensically used to be &&)
        if(nla_inst->_isload || nla_inst->_isstore) {
          int fuIndex = fuPoolIdx(nla_inst->_opclass,nla_inst->_op);
          int maxUnits = getNumFUAvailable(nla_inst->_opclass,nla_inst->_op); //opclass
          BaseInst_t* min_node = /*static_cast<BaseInst_t*>(*/
               addResource(fuIndex, nla_inst->cycleOfStage(NLAInst::Execute), 
                           getFUIssueLatency(nla_inst->_opclass,nla_inst->_op),
                                              maxUnits, nla_inst);
      
          if (min_node) {
            getCPDG()->insert_edge(*min_node, min_node->beginExecute(),
                              *nla_inst, NLAInst::Execute, 
                              getFUIssueLatency(nla_inst->_opclass,nla_inst->_op),E_MP);
          }
        }

        if(nla_inst->_isload) {
          //get the MSHR resource here!
          checkNumMSHRs(nla_inst); 
        } else {
          insertLSQ(nla_inst);
        }
        (*nla_inst)[NLAInst::Complete].reCalculate();

        //cout << "EP " << (*nla_inst)[NLAInst::Execute].cycle() << " "
        //              << (*nla_inst)[NLAInst::Complete].cycle() << "\n";


        if(_wb_networks != 0) {
          bool needs_forwarding=false;
          for(auto i=nla_inst->_op->adj_d_begin(),e=nla_inst->_op->adj_d_end();i!=e;++i){
            Op* dop = *i;
            if(!li->sgSchedNLA().opScheduled(dop)) {
              continue;
            }
            if(sg->static_sg->getCFUNode(dop)==NULL) {
              needs_forwarding=true; 
              break;
            }
          }

          (*nla_inst)[NLAInst::Forward].reCalculate();

          if(needs_forwarding) {
            NLAInst* dep_inst = static_cast<NLAInst*>(
                   addResource((uint64_t)&_wb_networks /*hacky hack hack*/,
                   nla_inst->cycleOfStage(NLAInst::Forward),
                   1/*latency*/, _wb_networks/*maxunits*/, nla_inst));
  
            if (dep_inst) {
              getCPDG()->insert_edge(*nla_inst, NLAInst::Forward,
                                     *dep_inst, NLAInst::Forward, 
                                     1,E_NNET);
            }
          }
        }

        (*nla_inst)[NLAInst::Writeback].reCalculate();
        if(nla_inst->_isstore) {
          checkNumMSHRs(nla_inst); 
        }
        
        //cout << "FW" << (*nla_inst)[NLAInst::Forward].cycle() << " "
        //             << (*nla_inst)[NLAInst::Writeback].cycle() << "\n";


        _totalNLAInsts+=1;
        _regionStats[li].total_insts+=1;

        if(_serialize_sgs || _issue_inorder) {
          countAccelSGRegEnergy(nla_inst->_op,
                                sg->static_sg,li->sgSchedNLA()._opset,
                                _nla_fp_ops,_nla_mult_ops,_nla_int_ops,
                                _nla_regfile_reads,_nla_regfile_freads,
                                _nla_regfile_writes,_nla_regfile_fwrites);
        } else {
          uint64_t temp_reads=0,temp_writes=0,temp_freads=0,temp_fwrites=0;

          countAccelSGRegEnergy(nla_inst->_op,
                                sg->static_sg,li->sgSchedNLA()._opset,
                                _nla_fp_ops,_nla_mult_ops,_nla_int_ops,
                                temp_reads,temp_writes,temp_freads,temp_fwrites);
          _nla_network_reads+=temp_reads + temp_freads;
          _nla_network_writes+=temp_writes + temp_fwrites;
          _nla_operand_reads+=temp_reads + temp_freads; //TODO: Makes sense?
          _nla_operand_writes+=temp_writes+temp_fwrites;
        }

//        std::cout << nla_inst->_index 
//                 << ": "  << nla_inst->cycleOfStage(NLAInst::Execute)
//                 << ","   << nla_inst->cycleOfStage(NLAInst::Complete)
//                 << " (op:" << nla_inst->_op->id() << ")"
//                 << "\n";
      }
      prev_sg=sg;
      prev_begin_cfu=sg->startCFU;
      prev_end_cfu=sg->endCFU;

      sg->endCFU->reCalculate();
      _regionStats[li].total_cinsts+=1;


      //LoopInfo* li = Prof::get().curFrame()->curLoop();
      //int iter = Prof::get().curFrame()->getLoopIterNum();

      //see if last op
      std::vector<std::shared_ptr<DynSubgraph>>& last_sgs = _lastOp[cur_li];
      if(last_sgs.size() < (unsigned)_nla_loop_iter_dist) {
        last_sgs.resize(_nla_loop_iter_dist);
      }
      if(!last_sgs[mod_iter] || 
          last_sgs[mod_iter]->endCFU->cycle() < sg->endCFU->cycle()) {
        last_sgs[mod_iter]=sg;
        //cout << iter << " " << mod_iter << " " << sg->endCFU->cycle() << "\n";
      }

      assert(sg.get());
      _prevSGMap[sg->static_sg] = sg;

//      std::cout << "end cfu: "
//        <<  sg->endCFU->cycle()
//        <<  "\n";

      //now lets clean up the dep/use subgraphs, otherwise we would leak
      sg->dep_subgraphs.clear();
      sg->use_subgraphs.clear();

      //cout << "end cfu: "   << prev_end_cfu->cycle() << "\n";

      /*if(prev_end_cfu->cycle() - prev_begin_cfu->cycle() > 100) {
        std::shared_ptr<NLAInst> nla_inst = sg->insts.begin()->lock();
        BB* bb = nla_inst->_op->bb();
        cout <<"!!!!!!!!!!!!!!!!   bb" 
          << bb->rpoNum() << " inst" << nla_inst->_op->id() << "\n";
      }*/
      
    }

    prev_ctrl_inst=ctrl_inst;
    ctrl_event=prev_end_cfu; //TODO: make this more robust
 
    curNLAInsts.clear();
    _curSubgraphs.clear();
    _vecSubgraphs.clear();
  //  std::cout << "clear\n";
  }

  virtual void checkNumMSHRs(std::shared_ptr<NLAInst>& n, uint64_t minT=0) {
    int ep_lat=n->ex_lat();
   
    if(n->_isload || n->_isstore) {
      calcCacheAccess(n.get(), n->_hit_level, n->_miss_level,
                      n->_cache_prod, n->_true_cache_prod,
                      l1_hits, l1_misses, l2_hits, l2_misses,
                      l1_wr_hits, l1_wr_misses, l2_wr_hits, l2_wr_misses);
    }

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

  bool error_with_dummy_inst=false;
  int dynSGindex=0;
  void addNLADeps(std::shared_ptr<NLAInst>& n, Op* op, LoopInfo* li) {
    Subgraph* sg = li->sgSchedNLA().sgForOp(op);
    assert(sg);

    if(op->isCtrl()) {
      ctrl_inst=n;
    }

    std::shared_ptr<DynSubgraph> dynSubgraph = _sgMap[sg];

    if(!dynSubgraph || dynSubgraph->ops_in_subgraph.count(op) == 1) {
      n->dynSubgraph = std::make_shared<DynSubgraph>(sg,dynSGindex++); //initialize dynsubgrpah
      _sgMap[sg] = n->dynSubgraph;
      n->dynSubgraph->setCumWeights(curWeights()); //&_regionStats[li].cum_weights);

/*      if(!dynSubgraph) {
        cout << "----new ";
      } else {
        cout << "----filled ";
      }
      cout << "make shared " << n->dynSubgraph->static_sg->id() 
           << "(op: " << op->id() << ")\n";
*/
      _curSubgraphs.insert(n->dynSubgraph);
    } else {
      n->dynSubgraph = dynSubgraph;
/*      cout << "----same " << n->dynSubgraph->static_sg->id()
           << "(op: " << op->id() << ")\n";
*/
    }

    n->dynSubgraph->ops_in_subgraph.insert(op);
    n->dynSubgraph->insts.push_back(n);

    //Hook up begining and end
    getCPDG()->insert_edge(*n->startCFU(), *n, NLAInst::Execute, 0, E_CFUB);

    n->ex_edge = getCPDG()->insert_edge(*n, NLAInst::Execute,
                            *n, NLAInst::Complete,/*op->avg_lat()*/1, E_EP);

    getCPDG()->insert_edge(*n, NLAInst::Complete,*n->endCFU(),   0, E_CFUE);

    getCPDG()->insert_edge(*n, NLAInst::Complete,
                           *n, NLAInst::Forward, 0, E_NFWD);

    n->st_edge = getCPDG()->insert_edge(*n, NLAInst::Forward,
                                        *n, NLAInst::Writeback, 0, E_WB);


    //serialize on dependent CFUs
    for (int i = 0; i < MAX_SRC_REGS; ++i) {
      unsigned prod = n->_prod[i];
      if (prod <= 0 || prod >= n->_index) {
        continue;
      }
      if(!getCPDG()->hasIdx(n->index()-prod)) {
        continue;
      }
      dg_inst_base<T,E>* orig_depInst=&(getCPDG()->queryNodes(n->index()-prod));

      bool out_of_bounds=false, error=false;
      dg_inst_base<T,E>* depInst = fixDummyInstruction(orig_depInst,out_of_bounds,error); //FTFY! : )
      if(error && error_with_dummy_inst==false) {
        error_with_dummy_inst=true;
        std::cerr << "ERROR: Dummy Inst of op had multiple prods:" << op->id() << "\n";
      }
      if(out_of_bounds) {
        continue;
      }

      int data_ready_event=-1;
      if(!depInst->isPipelineInst()) {
        NLAInst* prior_nla_inst = dynamic_cast<NLAInst*>(depInst);

        //DIFFERENT SUBGRAPHS
        if(prior_nla_inst->dynSubgraph != n->dynSubgraph) {
          if(_cfus_delay_writes) {
            getCPDG()->insert_edge(*prior_nla_inst->endCFU(),
                                   *n->startCFU(), 0, E_NDWR);

           /* cout << "NDWR: " << prior_nla_inst->dynSubgraph->static_sg->id() << "->" 
                   << n->dynSubgraph->static_sg->id() << "\n";*/
          } 
          DynSubgraph::addDep(prior_nla_inst->dynSubgraph,n->dynSubgraph);
          if(_cfus_delay_reads) {
            data_ready_event = NLAInst::Forward;
            getCPDG()->insert_edge(*depInst, data_ready_event,
                                   *n->startCFU(), 0, E_CFUR);
          }
          //assert(!SGSched::depFromTo(prior_nla_inst->dynSubgraph->static_sg,
          //                           n->dynSubgraph->static_sg));

    
        }
        //Any Data Dep
        data_ready_event = depInst->eventComplete();
        getCPDG()->insert_edge(*depInst,data_ready_event,
                             *n, NLAInst::Execute, 0, E_RDep);
      } else { //dataflow deps from proc
        data_ready_event = depInst->eventComplete();
        getCPDG()->insert_edge(*depInst,data_ready_event,
                             *n, NLAInst::Execute, 0, E_RDep);
      }

    }

    if(_software_mem_alias) {
      //iterate through potential memory deps
      for(auto mdi = op->m_begin(), mde = op->m_end(); mdi!=mde; ++mdi) {
        Op* md_op = *mdi;
        std::shared_ptr<BaseInst_t> sh_inst = getInstForOp(md_op);
        if(sh_inst && !sh_inst->isPipelineInst()) {
          NLAInst* mem_dep_inst = dynamic_cast<NLAInst*>(sh_inst.get());
          getCPDG()->insert_edge(*mem_dep_inst,NLAInst::Execute,
                                 *n,NLAInst::Execute, 1, E_NMTK);
          if(mem_dep_inst->dynSubgraph!=n->dynSubgraph) {
            DynSubgraph::addDep(mem_dep_inst->dynSubgraph,n->dynSubgraph);
          }
        }
      }
    }

#if 0
    //TODO: FIX INORDER ADDRESS CALC
    if(_inorder_address_calc && prevMemNode) {
      if((n->_isload || n->_isstore) && 
          !SGSched::depFromTo(n->dynSubgraph->static_sg,
                             prevMemSubgraph->static_sg)) {
        getCPDG()->insert_edge(*prevMemNode,prevMemNode->eventReady(),
                               *n, NLAInst::Execute, 1, E_NMTK);
        if(prevMemSubgraph && prevMemSubgraph != n->dynSubgraph) {
          DynSubgraph::addDep(prevMemSubgraph,n->dynSubgraph,true/*ignore if cycle*/);
        }
      }
    }
#endif

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

  virtual void printAccelHeader(std::ostream& out, bool hole) {
    out << std::setw(10) << (hole?"":"Avg_Time") << " ";
    out << std::setw(10) << (hole?"":"N_Invokes") << " ";
    out << std::setw(12) << (hole?"":"Insts") << " ";
    out << std::setw(11) << (hole?"":"CInsts") << " ";
    out << std::setw(12) << (hole?"":"NetworkHist") << " ";
    out << std::setw(24) << (hole?"":"Dyn_CInst_Usage") << " ";
    out << std::setw(32) << (hole?"":"Stat_CInst_Usage") << " ";
  }
 

  virtual void printAccelRegionStats(int id, std::ostream& out) {
    LoopInfo* print_li = _id2li[id];
    NLARegionStats& r = _regionStats[print_li];
    if(!print_li) {
      printAccelHeader(out,true);//print blank
      return;
    }
    
    out << std::setw(10) << ((double)r.total_cycles) / r.times_started << " ";
    out << std::setw(10) << r.times_started << " ";
    out << std::setw(12) << r.total_insts << " ";
    out << std::setw(11) << r.total_cinsts << " ";

    std::stringstream ss;

    //Network Hist
    int maxAct=0;
    for(int i=0; i < 16; ++i) {
      if(r.network_activity[i]) {
        maxAct=i;
      }
    }

    for(int i=1; i <= maxAct; ++i) { //start at 1 cause 9's bogus
      ss << std::fixed << std::setprecision(1) << ((double)r.network_activity[i])/(double)r.total_cycles;
      if(i!=maxAct) {
        ss << ",";
      }
    }

    out << std::setw(12) << ss.str() << " ";
    ss.str("");

    //Dyn Cinst Usage
    int maxCFU=0;
    uint64_t total=0;
    for(int i=0; i < 16; ++i) {
      if(r.cfu_used[i]) {
        total+=r.cfu_used[i];
        maxCFU=i;
      }
    }

    for(int i=0; i < maxCFU; ++i) {
      ss << std::fixed << std::setprecision(0) << (100.0*r.cfu_used[i])/total;
      if(i!=maxCFU) {
        ss << ",";
      }
    }

    out << std::setw(24) << ss.str() << " ";
    ss.str("");

    //Static Cinst Usage
    if(_cfu_set && print_li->hasSubgraphs(true/*NLA*/)) {
      int static_sg_usage[16]={0};

      for(auto i=print_li->sgSchedNLA().sg_begin(),
               e=print_li->sgSchedNLA().sg_end();i!=e;++i) {
        Subgraph* sg = *i;
        CFU* cfu = sg->cfu();
        static_sg_usage[cfu->ind()]+=1;
      }

      for(int i=1; i <= _cfu_set->numCFUs(); ++i) { //: ( why did i start at 1?
        if(i!=0) {
          ss << ",";
        }
        ss << static_sg_usage[i];
      }
    }

    out << std::setw(32) << ss.str() << " ";
    ss.str("");
  }


  NLAProcessor* mc_accel = NULL;
  virtual void setupMcPAT(const char* filename, int nm) 
  {
    CriticalPath::setupMcPAT(filename,nm); //do base class

    printAccelMcPATxml(filename,nm);
    ParseXML* mcpat_xml= new ParseXML(); 
    mcpat_xml->parse((char*)filename);
    mc_accel = new NLAProcessor(mcpat_xml); 

    /*system_core* core = &mc_accel->XML->sys.core[0];

    core->ALU_per_core=Prof::get().int_alu_count;
    core->MUL_per_core=Prof::get().mul_div_count;
    core->FPU_per_core=Prof::get().fp_alu_count;

    core->archi_Regs_IRF_size=8;
    core->archi_Regs_FRF_size=8;
    core->phy_Regs_IRF_size=64;
    core->phy_Regs_FRF_size=64;

    core->num_nla_units=8;
    core->imu_width=4;
    core->imu_entries=32;

    core->instruction_window_size=32;*/

    pumpCoreEnergyEvents(mc_accel,0); //reset all energy stats to 0
  } 

  virtual void pumpAccelMcPAT(uint64_t totalCycles) {
    pumpAccelEnergyEvents(totalCycles);
    mc_accel->computeEnergy();
  }

  virtual void pumpAccelEnergyEvents(uint64_t totalCycles) {
    mc_accel->XML->sys.total_cycles=totalCycles;

    //Func Units
    system_core* core = &mc_accel->XML->sys.core[0];
    core->ialu_accesses    = (uint64_t)(_nla_int_ops );
    core->fpu_accesses     = (uint64_t)(_nla_fp_ops  );
    core->mul_accesses     = (uint64_t)(_nla_mult_ops);

    core->cdb_alu_accesses = (uint64_t)(_nla_int_ops );
    core->cdb_fpu_accesses = (uint64_t)(_nla_fp_ops  );
    core->cdb_mul_accesses = (uint64_t)(_nla_mult_ops);

    //Reg File
    core->int_regfile_reads    = (uint64_t)(_nla_regfile_reads  ); 
    core->int_regfile_writes   = (uint64_t)(_nla_regfile_writes ); 
    core->float_regfile_reads  = (uint64_t)(_nla_regfile_freads ); 
    core->float_regfile_writes = (uint64_t)(_nla_regfile_fwrites); 

    if(_no_exec_speculation) {
      core->imu_reads=_nla_operand_reads+_nla_operand_freads;
      core->imu_writes=_nla_operand_writes+_nla_operand_fwrites;
      core->nla_network_accesses=_nla_network_writes;
    } else {
      core->inst_window_reads=_nla_operand_reads+_nla_operand_freads;
      core->inst_window_writes=_nla_operand_writes+_nla_operand_fwrites;
      core->inst_window_wakeup_accesses=_nla_operand_writes+_nla_operand_fwrites; 
    }

    core->nla_network_accesses=_nla_network_writes;

    accAccelEnergyEvents();
  }

  virtual double accel_leakage() override {

    //TODO: This is crappy right now -- make better later?
    if(_cfu_set && li && li->hasSubgraphs(true/*NLA*/) ) {

      //NLARegionStats& r = _regionStats[li];
      SGSched& sgsched = li->sgSchedNLA();
      bool lc = mc_accel->XML->sys.longer_channel_device;

      int imus_on=0;
      int nALUs_on=0;
      int nMULs_on=0;

      //if(r.cfu_used[i] > 0) {
      for(int i = 0; i < _cfu_set->numCFUs(); ++i) {
        if(sgsched.utilOf(i) > 0) {
          imus_on++;

          CFU* cfu = _cfu_set->getCFU(i);
          for(auto ii=cfu->nodes_begin(),ee=cfu->nodes_end();ii!=ee;++ii) {
            CFU_node* cfu_node = *ii;
            CFU_node::CFU_type cfu_type = cfu_node->type();
            switch(cfu_type) {
              case CFU_node::ALU: nALUs_on++; break;
              case CFU_node::MEM: break;
              case CFU_node::MPY: nMULs_on++; break;
              case CFU_node::SHF: break;
              case CFU_node::MAX: assert(0); break;
            }
          }
        }
      }
      if(nALUs_on) {
        nALUs_on=std::min(nALUs_on*1/2,1);
      }

      float ialu = RegionStats::get_leakage(mc_accel->cores[0]->exu->exeu,lc)* nALUs_on;
      float mul  = RegionStats::get_leakage(mc_accel->cores[0]->exu->mul,lc) * nMULs_on;
      float imu  = RegionStats::get_leakage(mc_accel->nlas[0]->imu_window,lc)* imus_on;
      float net  = RegionStats::get_leakage(mc_accel->nlas[0]->bypass,lc);

      //cout << "alu: " << nALUs_on << " " << ialu << "\n";
      //cout << "mul: " << nMULs_on << " " << mul << "\n";
      //cout << "imu: " << imus_on  << " " << imu << "\n";
      //cout << "net: " << 1        << " " << net << "\n";

      return ialu + mul + imu + net;
      //  float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
      //  float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 
      //  float network = mc_accel->nlas[0]->bypass->rt_power.readOp.dynamic; 
    } else {
      return 0;
    }
  }

  virtual double accel_region_en() override {
 
    float ialu  = mc_accel->cores[0]->exu->exeu->rt_power.readOp.dynamic; 
    float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
    float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 
    float reg  = mc_accel->cores[0]->exu->rfu->rt_power.readOp.dynamic; 

    float imu = mc_accel->nlas[0]->imu_window->rt_power.readOp.dynamic; 
    float network = mc_accel->nlas[0]->bypass->rt_power.readOp.dynamic; 

    return ialu + fpalu + calu + reg + imu + network;
  }

  virtual int is_accel_on() {
    if(nla_state==NLA) {
      return li->id();
    } else {
      return 0;
    }
  }

  virtual void printMcPAT_Accel() {
    mc_accel->computeAccPower(); //need to compute the accumulated power

    float ialu  = mc_accel->ialu_acc_power.rt_power.readOp.dynamic; 
    float fpalu = mc_accel->fpu_acc_power.rt_power.readOp.dynamic; 
    float calu  = mc_accel->mul_acc_power.rt_power.readOp.dynamic; 
    float reg   = mc_accel->rfu_acc_power.rt_power.readOp.dynamic; 

    float imu = mc_accel->imu_acc_power.rt_power.readOp.dynamic; 
    float network = mc_accel->nla_net_acc_power.rt_power.readOp.dynamic; 

    float total = ialu + fpalu + calu + reg + imu + network;

    std::cout << _name << " accel(" << _nm << "nm)... ";
    std::cout << total << " (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ", imu:" << imu << ", net:" << network << ")\n";

  }


  virtual void accAccelEnergyEvents() {
    _nla_int_ops_acc         +=  _nla_int_ops        ;
    _nla_fp_ops_acc          +=  _nla_fp_ops         ;
    _nla_mult_ops_acc        +=  _nla_mult_ops       ;
    _nla_regfile_reads_acc   +=  _nla_regfile_reads  ;
    _nla_regfile_writes_acc  +=  _nla_regfile_writes ;
    _nla_regfile_freads_acc  +=  _nla_regfile_freads ;
    _nla_regfile_fwrites_acc +=  _nla_regfile_fwrites;

    _nla_network_reads_acc   +=  _nla_network_reads  ;
    _nla_network_writes_acc  +=  _nla_network_writes ;

    _nla_operand_reads_acc   +=  _nla_operand_reads  ;
    _nla_operand_writes_acc  +=  _nla_operand_writes ;
    _nla_operand_freads_acc  +=  _nla_operand_freads ;
    _nla_operand_fwrites_acc +=  _nla_operand_fwrites;

    _nla_int_ops         = 0;
    _nla_fp_ops          = 0;
    _nla_mult_ops        = 0;
    _nla_regfile_reads   = 0; 
    _nla_regfile_writes  = 0; 
    _nla_regfile_freads  = 0; 
    _nla_regfile_fwrites = 0;

    _nla_network_reads   = 0; 
    _nla_network_writes  = 0; 

    _nla_operand_reads   = 0; 
    _nla_operand_writes  = 0; 
    _nla_operand_freads  = 0;   
    _nla_operand_fwrites = 0; 
  }

  // Handle enrgy events for McPAT XML DOC
  virtual void printAccelMcPATxml(std::string fname_base, int nm) override {
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
      sa(core_node,"machine_type",1);	//<!-- inorder/OoO; 1 inorder; 0 OOO-->

      sa(core_node,"total_cycles",numCycles());
      sa(core_node,"busy_cycles",0);
      sa(core_node,"idle_cycles",numCycles());

      sa(core_node,"ALU_per_core", 1); //Prof::get().int_alu_count);
      sa(core_node,"MUL_per_core", 1); //Prof::get().mul_div_count);
      sa(core_node,"FPU_per_core", 1); //Prof::get().fp_alu_count);
  
      sa(core_node,"ialu_accesses",_nla_int_ops_acc );
      sa(core_node,"fpu_accesses", _nla_fp_ops_acc  );
      sa(core_node,"mul_accesses", _nla_mult_ops_acc);

      sa(core_node,"archi_Regs_IRF_size",8);
      sa(core_node,"archi_Regs_FRF_size",8);
      sa(core_node,"phy_Regs_IRF_size",64);
      sa(core_node,"phy_Regs_FRF_size",64);

      sa(core_node,"num_nla_units",8);
      sa(core_node,"imu_width",4);
      sa(core_node,"imu_entries",32);

      //sa(core_node,"instruction_buffer_size",4);
      //sa(core_node,"decoded_stream_buffer_size",4);
      //sa(core_node,"instruction_window_scheme",0);
      sa(core_node,"instruction_window_size",32);
      //sa(core_node,"fp_instruction_window_size",8);

      if(_no_exec_speculation) {
        sa(core_node,"imu_reads",_nla_operand_reads_acc+_nla_operand_freads_acc);
        sa(core_node,"imu_writes",_nla_operand_writes_acc+_nla_operand_fwrites_acc);
      } else {
        sa(core_node,"inst_window_reads",_nla_operand_reads_acc+_nla_operand_freads_acc);
        sa(core_node,"inst_window_writes",_nla_operand_writes_acc+_nla_operand_fwrites_acc);
        sa(core_node,"inst_window_wakeup_accesses",_nla_operand_writes_acc+_nla_operand_fwrites_acc);      
      }

      sa(core_node,"int_regfile_reads",   _nla_regfile_reads_acc  );
      sa(core_node,"int_regfile_writes",  _nla_regfile_writes_acc );
      sa(core_node,"float_regfile_reads", _nla_regfile_freads_acc );
      sa(core_node,"float_regfile_writes",_nla_regfile_fwrites_acc);
 
    } else {
      std::cerr << "XML Malformed\n";
      return;
    }

    std::string fname=fname_base + std::string(".accel");
    accel_doc.save_file(fname.c_str());
  }

  virtual void calcAccelStaticPower(std::string fname_base,int nm) {
    
  }


  virtual void calcAccelEnergy(std::string fname_base,int nm) {
    return;

    /*std::string fname=fname_base + std::string(".accel");

    std::string outf = fname + std::string(".out");

    std::cout << _name << " old accel(" << nm << "nm)... ";
    std::cout.flush();

    execMcPAT(fname,outf);
    float ialu  = stof(grepF(outf,"Integer ALUs",7,4));
    float fpalu = stof(grepF(outf,"Floating Point Units",7,4));
    float calu  = stof(grepF(outf,"Complex ALUs",7,4));
    float reg   = stof(grepF(outf,"Register Files",7,4));
    float total = ialu + fpalu + calu + reg;
    std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ")\n";*/
  }

};



#endif //CP_NLA
