#ifndef CP_SUPER
#define CP_SUPER

#include <algorithm>

#include "cp_dg_builder.hh"
#include "cp_registry.hh"
#include <memory>

extern int TraceOutputs;

class SuperInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      Execute = 0,
      Complete = 1,
      Writeback = 2,
      NumStages
  };
private:
  T events[NumStages];

public:
 //std::shared_ptr<T>   endBB;
 //std::shared_ptr<T> startBB;

  virtual ~SuperInst() { 
  }

  //Things from CP_Node image
  bool _isctrl=0;
  bool _ctrl_miss=0;
  uint16_t _icache_lat=0;
  uint16_t _prod[MAX_SRC_REGS]={0,0,0,0,0,0,0,0};
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint64_t _true_cache_prod=false;
  uint16_t _ex_lat=0;
  bool _serialBefore=0;
  bool _serialAfter=0;
  bool _nonSpec=0;
  uint16_t _st_lat=0;
  uint64_t _pc=0;
  uint16_t _upc=0;
  uint64_t _eff_addr;
  bool _floating=false;
  bool _iscall=false;
  bool bypassed=false;

  SuperInst(const CP_NodeDiskImage &img, uint64_t index, Op* op):
              dg_inst_base<T,E>(index){
    this->_opclass=img._opclass;
    this->_isload=img._isload;
    this->_isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    _icache_lat=img._icache_lat;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _true_cache_prod=img._true_cache_prod;
    _ex_lat=img._ep_lat;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._st_lat;
    _pc=img._pc;
    _upc=img._upc;
    _floating=img._floating;
    _iscall=img._iscall;
    _hit_level=img._hit_level;
    _miss_level=img._miss_level;
    _eff_addr=img._eff_addr;
    isAccelerated=true;
    this->_op=op;
  }

  SuperInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }
  virtual unsigned numStages() {
    return NumStages;
  }
  virtual uint64_t cycleOfStage(const unsigned i) {
    return events[i].cycle(); 
  }
  virtual unsigned eventCommit() {
    return Complete; 
  }
  virtual unsigned beginExecute() {
    return Execute; 
  }
  virtual unsigned eventReady() {
    return Execute; 
  }
  virtual unsigned eventComplete() {
    return Complete; 
  }
  virtual unsigned memComplete() {
    if(this->_isload) {
      return Complete; 
    } else if (this->_isstore) {
      return Writeback;
    } 
    assert(0);
    return -1;
  }

};


class cp_super : public ArgumentHandler,
  public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  
  typedef dg_inst<T, E> Inst_t;

public:
  cp_super() : CP_DG_Builder<T,E>() {
    getCPDG()->setCritListener(this);
  }

  virtual ~cp_super() {
  }

  virtual bool removes_uops() {return true;}
  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };
  dep_graph_impl_t<Inst_t,T,E> cpdg;
  //uint64_t min_cycle=0;
  uint64_t max_cycle=0;

  bool _no_speculation=false,_dataflow_no_spec=false;
  bool _inorder_per_instruction=false, _model_sq=false;
  void handle_argument(const char *name, const char *arg) {
    ArgumentHandler::parse("super-no-spec",name,arg,_no_speculation);
    ArgumentHandler::parse("super-dataflow-no-spec",name,arg,_dataflow_no_spec);
    ArgumentHandler::parse("inorder-per-instruction",name,arg,_inorder_per_instruction);
    ArgumentHandler::parse("model-sq",name,arg,_model_sq);
    // cout << "spec: " << _no_speculation << "\n";
  }


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img,Op* op) {
    if (getTraceOutputs()){
      dg_inst_base<T,E>& inst = getCPDG()->queryNodes(index);  
  
      for (unsigned i = 0; i < inst.numStages(); ++i) {
        outs() << inst.cycleOfStage(i) << " ";
        printEdgeDep(outs(),inst,i,E_FF);
      }

//      outs() << index + Prof::get().skipInsts << ": ";
//      outs() << inst.cycleOfStage(0) << " ";
//      outs() << inst.cycleOfStage(1) << " ";
//      outs() << inst.cycleOfStage(2) << " ";

      CriticalPath::traceOut(index,img,op);
      outs() << "\n";
    }
  }

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) {
    computeDFCTL(op,img);

    if(op &&( op->shouldIgnoreInAccel() || op->plainMove())) {
//      getCPDG()->insert_edge(*inst, SuperInst::Execute,
 //                          *inst, SuperInst::Complete, 0, E_EP);
      createDummy(img,index,op);
      return;      
    }

    SuperInst* inst = new SuperInst(img,index,op);
    std::shared_ptr<SuperInst> sh_inst(inst);
    getCPDG()->addInst(sh_inst,index);


    addDeps(sh_inst,img);
    countOpEnergy(op, _super_fp_ops, _super_mult_ops, _super_int_ops);

    fixMin(*inst);

    uint64_t last_cycle=inst->cycleOfStage(SuperInst::Complete);
    if(max_cycle < last_cycle) {
      max_cycle = last_cycle;
    }

    if(op->isCall() || op->isReturn()) {
       ctrl_inst=sh_inst;
       //ctrl_inst_map.clear();
    }
    if(_no_speculation) {
      if(op->isCondCtrl() || op->isIndirectCtrl() || op->isCall() || op->isReturn()) {
        ctrl_inst=sh_inst;
      }
    }

    keepTrackOfInstOpMap(sh_inst,op); 
    prev_inst=sh_inst;
  }

private:
  typedef std::vector<std::shared_ptr<SuperInst>> CCoresBB;
  std::shared_ptr<T> prev_bb_end, cur_bb_end;

  uint64_t _super_int_ops=0,    _super_fp_ops=0,    _super_mult_ops=0;
  uint64_t _super_int_ops_acc=0,_super_fp_ops_acc=0,_super_mult_ops_acc=0;
  bool  _optimistic_stack=true;      //optimistic stack

  std::shared_ptr<SuperInst> ctrl_inst;
  std::shared_ptr<SuperInst> prev_inst;

  std::unordered_map<BB*, std::shared_ptr<SuperInst>> ctrl_inst_map;

  //Compute Dataflow Control -- this must be performed before adding deps
  void computeDFCTL(Op* op, const CP_NodeDiskImage &img) {
    if(_dataflow_no_spec) {

      if(prev_inst) {
        Op* prev_op = prev_inst->_op;
        if(prev_op->isCondCtrl() || prev_op->isIndirectCtrl()) {
          BB* bb = op->bb();
          FunctionInfo* f = bb->func();
  
          BB* prev_bb = prev_op->bb();
          if(f->pdomR_has(prev_bb)) {
            for(auto ii = f->pdomR_begin(prev_bb), ee = f->pdomR_end(prev_bb); ii != ee; ++ii) {
              BB* cond_bb = ii->first;
              BB* trigger_bb = ii->second;
              if(cond_bb == bb) { //if we went in this direction
                ctrl_inst_map[trigger_bb]=prev_inst;
//                std::cout << inst->_index << " " << prev_op->id() 
//                          << "@BB" << prev_bb->rpoNum()
//                          << "-> BB"  << trigger_bb->rpoNum() << "\n";

              }
            }
          }
        }
      }
    }
  }

  virtual void addDeps(std::shared_ptr<SuperInst>& inst,const CP_NodeDiskImage &img) {   
    setExecuteCycle_s(inst);
    setCompleteCycle_s(inst);
    setWritebackCycle_s(inst); 
  }

#if 0
  //This node when current ccores BB is active
  virtual void setBBReadyCycle_cc(SuperInst& inst, const CP_NodeDiskImage &img) {
    CCoresBB::iterator I,E;
    /*for(I=prev_bb.begin(),E=prev_bb.end();I!=E;++I) {
        SuperInst* cc_inst= I->get(); 
        getCPDG()->insert_edge(*cc_inst, SuperInst::Complete,
                               inst, SuperInst::BBReady, 0);
    }*/
    if(prev_bb_end) {
      inst.startBB=prev_bb_end;
      getCPDG()->insert_edge(*prev_bb_end,
                               inst, SuperInst::BBReady, 0);
    }
  }
#endif 

  //this node when current BB is about to execute 
  //(no need for ready, b/c it has dedicated resources)
  virtual void setExecuteCycle_s(std::shared_ptr<SuperInst> &n) {

    if(_inorder_per_instruction) {
      BaseInstPtr prev_inst_for_op = getInstForOp(n->_op);
      if(prev_inst_for_op) {
        getCPDG()->insert_edge(*prev_inst_for_op, prev_inst_for_op->beginExecute(),
                               *n, SuperInst::Execute, 1, E_SER);
      }
    }

    if(_model_sq && n->_isstore) {
      if(!ignoreMem(*n)) {
        checkLSQSize((*n)[SuperInst::Execute],n->_isload,n->_isstore);
        insertLSQ(n);
      }
    }

    for (int i = 0; i < MAX_SRC_REGS; ++i) {
      unsigned prod = n->_prod[i];
      if (prod <= 0 || prod >= n->index()) {
        continue;
      }

      dg_inst_base<T,E>* orig_depInst=&(getCPDG()->queryNodes(n->index()-prod));
      if(orig_depInst->_op && orig_depInst->_op->shouldIgnoreInAccel()) {
        continue;
      }

      bool out_of_bounds=false, error=false;
      dg_inst_base<T,E>* depInst = fixDummyInstruction(orig_depInst,out_of_bounds,error); //FTFY! : )
//      if(error && error_with_dummy_inst==false) {
 //       error_with_dummy_inst=true;
  //      std::cerr << "ERROR: Dummy Inst of op had multiple prods:" << op->id() << "\n";
  //    }
      if(out_of_bounds) {
        continue;
      }
      if(depInst->_op && 
          (n->_op->bad_incr_dop(depInst->_op) ||
           depInst->_op->shouldIgnoreInAccel() )) {
        continue; 
      }


      //BaseInst_t& depInst = getCPDG()->queryNodes(n->index()-prod);
      unsigned dep_lat=0;

      Op*  op  = n->_op;
      Op* dop = depInst->_op;
      if(op && dop) {
        FunctionInfo* fi=op->func();
        if(_dataflow_no_spec) {
          if(fi != dop->func()) {
            dep_lat=1;
          } else if((op->bb() == dop->bb() &&
                     op->bb_pos() > dop->bb_pos()) || 
                    (op->numDepOpsAtIndex(i) <= 1 &&  
                    fi->dominates(op->bb(),dop->bb())) ) {
            dep_lat=0;
          } else {
            dep_lat=1;
          }
        } else if (_no_speculation) {
          if(fi != dop->func()) {
            dep_lat=1;
          } else if(op->bb() == dop->bb() &&
                    op->bb_pos() > dop->bb_pos()) { 
  
            dep_lat=0;
          } else {
            dep_lat=1;
          }
        }
      }

      if(depInst->_op->ctrlMove()) {
        dep_lat=0;
      }

      getCPDG()->insert_edge(*depInst, depInst->eventComplete(),
                             *n, SuperInst::Execute, dep_lat, E_RDep);
    }
    //memory dependence
    if (n->_mem_prod > 0 && n->_mem_prod < n->index()) {
      BaseInst_t& prev_node = getCPDG()->queryNodes(n->index()-n->_mem_prod);
      insert_mem_dep_edge(prev_node,*n);
    }

    if(_no_speculation && ctrl_inst) {
       getCPDG()->insert_edge(*ctrl_inst,SuperInst::Complete,
                                     *n,SuperInst::Execute, 0, E_NCTL);
    }

    if(_dataflow_no_spec) {
      BB* bb = n->_op->bb();

      if(ctrl_inst_map.count(bb)) {
        getCPDG()->insert_edge(*ctrl_inst_map[bb],SuperInst::Complete,
                                               *n,SuperInst::Execute, 0,E_NCTL);
//        BB* dep_bb = ctrl_inst_map[bb]->_op->bb();
/*        cout << "Func: " << bb->func()->id() << " BB Dep: " << dep_bb->rpoNum()
             << " " 
             << ctrl_inst_map[bb]->_op->id() << " -c-> " << n->_op->id() << " "
             << ctrl_inst_map[bb]->_index << "->" << n->_index << "\n";*/
      } else if (ctrl_inst) {
        getCPDG()->insert_edge(*ctrl_inst,SuperInst::Complete,
                                     *n,SuperInst::Execute, 0, E_NCTL);
/*        cout << "Call Dependence: " << ctrl_inst->_op->func()->id() << ")"
                                    << ctrl_inst->_op->func()->nice_name() << ") " 
                                    << bb->func()->id() 
                                    << "(" << bb->func()->nice_name() << ") ->" 
                                    << bb->rpoNum() << "\n";*/
       
      }
    }
    

    /*
    //set_min_cycle -- cheat to set the min cycle
    if(n->index()>1 && n->cycleOfStage(SuperInst::Execute) < min_cycle) {
      SuperInst& prev_node = static_cast<SuperInst&>( 
                            getCPDG()->queryNodes(n->index()-1));

      getCPDG()->insert_edge(prev_node, SuperInst::Execute,
                             *n, SuperInst::Execute,  
                             min_cycle - prev_node.cycleOfStage(SuperInst::Execute),  
                             E_CHT);
    }
*/

/*
    if(n->index>1 && ) {
      SuperInst& prev_node = static_cast<SuperInst&>( 
                            getCPDG()->queryNodes(n->index()-1));
      
    }*/


    T* horizon_event = getCPDG()->getHorizon();
    uint64_t horizon_cycle = 0;
    if(horizon_event) {
      horizon_cycle=horizon_event->cycle();
    }

    if(horizon_event && n->cycleOfStage(SuperInst::Execute) < horizon_cycle) { 
      getCPDG()->insert_edge(*horizon_event,
                             *n, SuperInst::Execute, 0, E_HORZ);   
    }

    if(horizon_event) {
      uint64_t clean_cycle = horizon_event->cycle();
      if(clean_cycle>1000) {
        cleanUp(clean_cycle-1000);
      }
    }

    //Only constrain memory port resources
    if(!ignoreMem(*n)) {
      if(n->_opclass==30 || n->_opclass==31) {
        checkFuncUnits(n);
      }
    }

    if(n->_isload) {
      if(!ignoreMem(*n)) {
        checkNumMSHRs(n);
      }
    }
  }


  /*
  virtual void checkNumMSHRs(std::shared_ptr<SuperInst>& n) {
    assert(n->_isload || n->_isstore);
    int ep_lat=epLat(n->_ex_lat,n.get(),n->_isload,n->_isstore,
                  n->_cache_prod,n->_true_cache_prod,true);
    int st_lat=stLat(n->_st_lat,n->_cache_prod,n->_true_cache_prod,true);


    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 
    int rechecks=0;
    uint64_t extraLat=0;

    assert(mshrT>0);
    BaseInst_t* min_node =
         addMSHRResource(reqDelayT + n->cycleOfStage(SuperInst::Execute), 
                         mshrT, n, n->_eff_addr, 1, rechecks, extraLat);

    if(min_node) {
        getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                       *n, SuperInst::Execute, mshrT+respDelayT, E_MSHR);
    }
  }
  */
  bool ignoreMem(SuperInst& n) {
    if(_optimistic_stack && (n._op->isConstLoad() || n._op->isStack())) {
      return true;
    }
    if(n.bypassed) {
      return true;   
    }
    return false;
  }

  virtual unsigned getNumFUAvailable(uint64_t opclass, Op* op) {
    if(opclass==30 || opclass==31) {
      return RW_PORTS;
    }
    return 50;
  }


  
  virtual void setCompleteCycle_s(std::shared_ptr<SuperInst>& inst) {
    int lat=epLat(inst->_ex_lat,inst.get(),inst->_isload,
                  inst->_isstore,inst->_cache_prod,inst->_true_cache_prod,true);

    if(ignoreMem(*inst) || inst->_op->ctrlMove()) {
      lat=0;
    }

    getCPDG()->insert_edge(*inst, SuperInst::Execute,
                           *inst, SuperInst::Complete, lat, E_EP);

    checkPP(*inst);
  }

  virtual void setWritebackCycle_s(std::shared_ptr<SuperInst>& inst) {
    if(inst->_isstore) {
      int st_lat=stLat(inst->_st_lat,inst->_cache_prod,inst->_true_cache_prod,true);

      if(ignoreMem(*inst)) {
        st_lat=0;
      }

      getCPDG()->insert_edge(*inst, SuperInst::Complete,
                             *inst, SuperInst::Writeback, st_lat, E_WB);

      if(!ignoreMem(*inst)) {
        checkNumMSHRs(inst);
        checkPP(*inst);
      }
    }
  }

  PrismProcessor* mc_accel = NULL;
  virtual void setupMcPAT(const char* filename, int nm) 
  {
    CriticalPath::setupMcPAT(filename,nm); //do base class

    printAccelMcPATxml(filename,nm);
    ParseXML* mcpat_xml= new ParseXML(); 
    mcpat_xml->parse((char*)filename);
    mc_accel = new PrismProcessor(mcpat_xml); 
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
    core->ialu_accesses    = (uint64_t)(_super_int_ops );
    core->fpu_accesses     = (uint64_t)(_super_fp_ops  );
    core->mul_accesses     = (uint64_t)(_super_mult_ops);

    core->cdb_alu_accesses = (uint64_t)(_super_int_ops );
    core->cdb_fpu_accesses = (uint64_t)(_super_fp_ops  );
    core->cdb_mul_accesses = (uint64_t)(_super_mult_ops);

    accAccelEnergyEvents();
  }

  virtual double accel_region_en() {
 
    float ialu  = mc_accel->cores[0]->exu->exeu->rt_power.readOp.dynamic; 
    float fpalu = mc_accel->cores[0]->exu->fp_u->rt_power.readOp.dynamic; 
    float calu  = mc_accel->cores[0]->exu->mul->rt_power.readOp.dynamic; 

    return ialu + fpalu + calu;
  }

  virtual int is_accel_on() {
    return -1;
  }

  virtual void printMcPAT_Accel() {
    mc_accel->computeAccPower(); //need to compute the accumulated power

    float ialu  = mc_accel->ialu_acc_power.rt_power.readOp.dynamic; 
    float fpalu = mc_accel->fpu_acc_power.rt_power.readOp.dynamic; 
    float calu  = mc_accel->mul_acc_power.rt_power.readOp.dynamic; 

    float total = ialu + fpalu + calu;

    std::cout << _name << " accel(" << _nm << "nm)... ";
    std::cout << total << " (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ")\n";
  }


  virtual void accAccelEnergyEvents() {
    _super_int_ops_acc         +=  _super_int_ops        ;
    _super_fp_ops_acc          +=  _super_fp_ops         ;
    _super_mult_ops_acc        +=  _super_mult_ops       ;

    _super_int_ops         = 0;
    _super_fp_ops          = 0;
    _super_mult_ops        = 0;
  }

  virtual void fixMin(SuperInst& si) {
    //if(si._index % 1024 == 0) {
    //  min_cycle = si.cycleOfStage(SuperInst::Execute);
    //}

    //cleanMSHR(min_cycle);
    //cleanFU(min_cycle);


#if 0
    //delete irrelevent 
    auto upperMSHRResp = MSHRResp.upper_bound(min_cycle);
    MSHRResp.erase(MSHRResp.begin(),upperMSHRResp); 

    //for funcUnitUsage
    for(FuUsage::iterator I=fuUsage.begin(),EE=fuUsage.end();I!=EE;++I) {
      FuUsageMap& fuUseMap = I->second;
      for(FuUsageMap::iterator i=++fuUseMap.begin(),e=fuUseMap.end();i!=e;) {
        uint64_t cycle = i->first;
        assert(cycle!=0);
        if (cycle + 50  < min_cycle) {
          i = fuUseMap.erase(i);
        } else {
          //++i;
          break;
        }
      }
    }

    for(auto I=nodeResp.begin(),EE=nodeResp.end();I!=EE;++I) {
      NodeRespMap& respMap = I->second;
      for(typename NodeRespMap::iterator i=respMap.begin(),e=respMap.end();i!=e;) {
        uint64_t cycle = i->first;
        if (cycle  < min_cycle) {
          i = respMap.erase(i);
        } else {
          //++i;
          break;
        }
      }
    }
#endif
  }

  uint64_t numCycles() {
    return max_cycle;
  }

};

#endif //CP_SUPER
