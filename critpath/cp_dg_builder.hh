#ifndef CP_DG_BUILDER_HH
#define CP_DG_BUILDER_HH

#include "cp_dep_graph.hh"
#include "critpath.hh"
#include "exec_profile.hh"

#include <memory>
#include "op.hh"

#include "edge_table.hh"
#include "prof.hh"
#include "pugixml/pugixml.hpp"
#include <unordered_set>
#include "cp_utils.hh"

#define MAX_RES_DURATION 64
#define QUAD_ISSUE_CORE_CLOCK 3000
//This assumes the original code ran at 2000Mhz, and that memory lat was 30ns
#define MEMORY_LATENCY 60

template<typename T, typename E>
class CP_DG_Builder : 
  public CritEdgeListener<dg_edge_impl_t<dg_event>>,
  public CriticalPath {

public:
  typedef dg_inst_base<T, E> BaseInst_t;
  typedef dg_inst<T, E> Inst_t;

  typedef std::shared_ptr<BaseInst_t> BaseInstPtr;
  typedef std::shared_ptr<Inst_t> InstPtr;  

  CP_DG_Builder() : CriticalPath() {
     MSHRUseMap[0];
     MSHRUseMap[(uint64_t)-10000];
     MSHRResp[0];
     MSHRResp[(uint64_t)-10000];
     LQ.resize(LQ_SIZE);
     SQ.resize(SQ_SIZE);
     activityMap.insert(1);
     rob_head_at_dispatch=0;
     lq_head_at_dispatch=0;
     sq_head_at_dispatch=0;

     rob_growth_rate=mylog2(0);
     rob_growth_rate=0;
     avg_rob_head=20;
  }

  std::unordered_map<FunctionInfo*,critFuncEdges> _crit_edge_map;
  virtual void listen(E* edge, float weight) {
    T* src  = edge->src();
    T* dest = edge->dest();
    if(src->_op && dest->_op) {
      _crit_edge_map[src->_op->func()].add_edge(src->_op,dest->_op,edge->type(),weight);
    }
  }

  virtual void loopsgDots(std::ostream&out, std::string &name) {
    string dir="analysis/";
    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo& fi = *i->second;
      std::stringstream filename;
      std::string file_base = fi.nice_filename(); 

      for(auto II=fi.li_begin(), EE=fi.li_end(); II!=EE; ++II) {
        LoopInfo* li = II->second;

        if(li->hasSubgraphs(true)) {
	  filename.str("");
	  filename << dir << "/" << _run_name << file_base << "_" << _name << ".crit_loopsg" << li->id() << ".dot"; 
          std::ofstream dotOutFile(filename.str().c_str()); 

          li->printSubgraphDot(dotOutFile,li->sgSchedNLA(),
                               &(_crit_edge_map[&fi].critEdges),true,removes_uops(),li);
	}


      }
    }



  }

  virtual ~CP_DG_Builder() {}

  virtual void setWidth(int i,bool scale_freq, bool match_simulator,
                        bool revolver,int mem_ports, int num_L1_MSHRs) {
    _enable_revolver=revolver;

    FETCH_WIDTH = i;
    D_WIDTH = i;
    ISSUE_WIDTH = i;
    WRITEBACK_WIDTH = i;
    COMMIT_WIDTH = i;
    SQUASH_WIDTH = i;
    _scale_freq=scale_freq;
    if(_isInOrder) {
      FETCH_TO_DISPATCH_STAGES=3;
      COMMIT_TO_COMPLETE_STAGES=0;


      N_ALUS=ISSUE_WIDTH; //only need 1-issue, 2-issue
      N_FPU=1;
      RW_PORTS=1;
      N_MUL=1;

      PIPE_DEPTH=10;//roughly bobcat, but depth is too low
      PHYS_REGS=32;

      //overide incoming defaults
      IQ_WIDTH=8;
      ROB_SIZE=32;
      LQ_SIZE = 16;
      SQ_SIZE = 20;

      PEAK_ISSUE_WIDTH=ISSUE_WIDTH+1;  
      if(scale_freq) {
        CORE_CLOCK=1000;
      }

    } else {
      N_ALUS=std::min(std::max(1u,ISSUE_WIDTH*3/4),6u);
      if(ISSUE_WIDTH==2) {
        N_ALUS=2;
      }
      N_FPU=std::min(std::max(1u,ISSUE_WIDTH/2),4u);
      RW_PORTS=std::min(std::max(1u,ISSUE_WIDTH/2),3u);
      N_MUL=std::min(std::max(1u,ISSUE_WIDTH/2),2u);

      if(ISSUE_WIDTH==1) {
        std::cout << "Single issue ooo?";
      } else if(ISSUE_WIDTH==2) { //roughly jaguar
        CORE_CLOCK=2500;
        FETCH_TO_DISPATCH_STAGES=4;
        COMMIT_TO_COMPLETE_STAGES=1;
        PHYS_REGS=136;
        PIPE_DEPTH=13;
        IQ_WIDTH=32;
        LQ_SIZE = 16;
        SQ_SIZE = 20;
        ROB_SIZE=64;
      } else if(ISSUE_WIDTH==4) {
        CORE_CLOCK=QUAD_ISSUE_CORE_CLOCK;
        FETCH_TO_DISPATCH_STAGES=4;
        COMMIT_TO_COMPLETE_STAGES=2;
        PIPE_DEPTH=17;
        PHYS_REGS=160;
        IQ_WIDTH=48;
        LQ_SIZE = 64;
        SQ_SIZE = 36;
        ROB_SIZE=168;
      } else if(ISSUE_WIDTH==6) { //roughly haswell
        CORE_CLOCK=3000;
        FETCH_TO_DISPATCH_STAGES=5;
        COMMIT_TO_COMPLETE_STAGES=2;
        PIPE_DEPTH=18;
        PHYS_REGS=168;
        IQ_WIDTH=52;
        LQ_SIZE = 42;
        SQ_SIZE = 72;
        ROB_SIZE=192;
      } else if(ISSUE_WIDTH==8) {
        CORE_CLOCK=3500;
        FETCH_TO_DISPATCH_STAGES=6;
        COMMIT_TO_COMPLETE_STAGES=3;
        PIPE_DEPTH=19;
        PHYS_REGS=168;
        IQ_WIDTH=60;
        LQ_SIZE = 42;
        SQ_SIZE = 72;
        ROB_SIZE=192;
      }

      if(!scale_freq) {
        CORE_CLOCK=2000;
        if(ISSUE_WIDTH==2) { //roughly jaguar
          FETCH_TO_DISPATCH_STAGES=4;
          COMMIT_TO_COMPLETE_STAGES=1;
          PHYS_REGS=136;
          PIPE_DEPTH=13;
          IQ_WIDTH=32;
          LQ_SIZE = 16;
          SQ_SIZE = 20;
          ROB_SIZE=64;
        } else if(ISSUE_WIDTH==4) {
          FETCH_TO_DISPATCH_STAGES=4;
          COMMIT_TO_COMPLETE_STAGES=2;
          PIPE_DEPTH=17;
          PHYS_REGS=160;
          IQ_WIDTH=40;
          LQ_SIZE = 64;
          SQ_SIZE = 36;
          ROB_SIZE=168;
        } else if(ISSUE_WIDTH==6) { //roughly haswell
          FETCH_TO_DISPATCH_STAGES=5;
          COMMIT_TO_COMPLETE_STAGES=2;
          PIPE_DEPTH=18;
          PHYS_REGS=180;
          IQ_WIDTH=48;
          LQ_SIZE = 42;
          SQ_SIZE = 72;
          ROB_SIZE=192;
        } else if(ISSUE_WIDTH==8) {
          FETCH_TO_DISPATCH_STAGES=6;
          COMMIT_TO_COMPLETE_STAGES=3;
          PIPE_DEPTH=19;
          PHYS_REGS=220;
          IQ_WIDTH=64;
          LQ_SIZE = 42;
          SQ_SIZE = 72;
          ROB_SIZE=228;
        } else if(ISSUE_WIDTH==16) {
          FETCH_TO_DISPATCH_STAGES=7;
          COMMIT_TO_COMPLETE_STAGES=4;
          PIPE_DEPTH=20;
          PHYS_REGS=240;
          IQ_WIDTH= 80;
          LQ_SIZE = 64;
          SQ_SIZE = 96;
          ROB_SIZE=280;
        } else if(ISSUE_WIDTH<16) {
          cout << "Nonstandard issue width, please check params!\n";
        } else if(ISSUE_WIDTH>16) { 
          cout << "ISSUE > 16?  Ok, check params!\n";
          FETCH_TO_DISPATCH_STAGES=4;
          COMMIT_TO_COMPLETE_STAGES=2;
          PIPE_DEPTH=17;
          PHYS_REGS=168;
          IQ_WIDTH=250;
          LQ_SIZE =150;
          SQ_SIZE =150;
          ROB_SIZE=500;
        } 
        
        if(match_simulator) {
          CORE_CLOCK=2000;
          FETCH_TO_DISPATCH_STAGES=4;
          COMMIT_TO_COMPLETE_STAGES=2;
          PIPE_DEPTH=19;
          PHYS_REGS=168;
          IQ_WIDTH=48;
          LQ_SIZE = 32;
          SQ_SIZE = 32;
          ROB_SIZE=192;
        }

      }

      LQ.resize(LQ_SIZE);
      SQ.resize(SQ_SIZE);

      IBUF_SIZE=std::max(FETCH_TO_DISPATCH_STAGES*ISSUE_WIDTH,16u);
      PEAK_ISSUE_WIDTH=ISSUE_WIDTH+2;
    }

    if(mem_ports!=-1) {
      RW_PORTS=mem_ports;
    }
    cout << "Override RW_PORTS: " << RW_PORTS << "\n";


    if(num_L1_MSHRs!=-1) {
      L1_MSHRS=num_L1_MSHRs;
      cout << "Override L1_MSHRS: " << L1_MSHRS << "\n";
    }
    //IBUF_SIZE=FETCH_TO_DISPATCH_STAGES * ISSUE_WIDTH;
  }

  bool _supress_errors=false;
  //Functions to surpress and enable errors
  virtual void supress_errors(bool val) {
    _supress_errors=val;
  }

  virtual void set_func_trans_inst(std::shared_ptr<Inst_t> t_inst){
    assert(t_inst);
    func_transition_inst=t_inst;
  }


  virtual void track_revolver(uint64_t index, Op* op) {
    FunctionInfo* fi = op->func();
    BB* bb = op->bb();
    _prev_cycle_revolver_active=_revolver_active; //record this for energy accounting

    //two conditions for breaking out:
    //1. inside li->func but not inside bbs
    //2. return from li->func
    //3. same BB twice

    bool breakout=false, side_exit=false;
    if(_cur_revolver_li) {
      if(_cur_revolver_li->func() == fi) { //in same function
        if(op->isReturn()) {
          breakout=true;
          side_exit=true;
        } else if(op->bb_pos()==0) {
          if(!_cur_revolver_li->inLoop(bb)) {
            breakout=true;
            if(_prev_op && !_cur_revolver_li->isLatch(_prev_op->bb())) {
              side_exit=true;
//              cout << " not loop latch\n";
            }
          }
        }
      }
      if(!breakout && op->bb_pos()==0) { //check for either loop or hot
        BB* bb = op->bb();
//        cout << "bb " << bb->rpoNum() << "\n";

        if(_revolver_bbs[_cur_revolver_li].count(bb)) {
          if(!_revolver_active) {  //this means i found a loop in the func
            if(bb!=_cur_revolver_li->loop_head()) {
              breakout=true;
              side_exit=true;
//              cout << " double add\n";
            }
          }
        } else { //did not find bb
          if(_revolver_active) {  // took wrong path inside loop
            if(bb!=_cur_revolver_li->loop_head()) {
              breakout=true;
              side_exit=true;
//              cout << " coud not find bb\n";
            }
          }
        }
      }

      if(breakout) {
        if(side_exit) {
          _revolver_bbs[_cur_revolver_li].clear();
          _revolver_profit[_cur_revolver_li]=0;
//          cout << " side_exit\n";
        } else {
          _revolver_profit[_cur_revolver_li]=std::min(
              _revolver_profit[_cur_revolver_li]+2,16);
        }
//        if(_revolver_active) {
//          cout << "revolver de-activated: " << bb->rpoNum();
//          cout << "\n";
//        }
        _revolver_active=false;
        _cur_revolver_li=NULL;
      }
    }


    //enter?
    if(!_revolver_active) {
      if(op->bb_pos()==0) {
        LoopInfo* li = fi->getLoop(bb);
  
        //Detected a loop head
        if(li && li->isInnerLoop() && !li->cantFullyInline()) {

           //adjust profit
           if(li==_cur_revolver_li) {
             if(_revolver_prev_trace == _revolver_bbs[li]) {
                _revolver_profit[li]=std::min(_revolver_profit[li]+1,16);           
             } else {
                _revolver_profit[li]=std::max(_revolver_profit[li]-2,0);           
             }
             _revolver_prev_trace=_revolver_bbs[li];

           } else {
             _revolver_prev_trace.clear();
             _revolver_profit[_cur_revolver_li]=0;
             _revolver_bbs[_cur_revolver_li].clear();
           }


           //see what the current profit is:
           if(_revolver_profit[li] >= 8) {
             if(_revolver_bbs.size() > 0) {
               _revolver_active=true;
//               cout << "revolver activated: " << bb->rpoNum() << " " << "-- size:" <<_revolver_bbs.size() << "\n   ";
//               for(BB* rbb : _revolver_bbs[li]) {
//                 cout << rbb->rpoNum() << " ";
//               }
//               cout << "\n";

             }
           } else {
             _revolver_bbs.clear(); //clear if not setting true
           }
  
//           cout << "li" << li->loop_head()->rpoNum() 
//                << "profit: " << _revolver_profit[li] << "\n";

           _cur_revolver_li=li;
        }

        if(_cur_revolver_li && !_revolver_active) {
          _revolver_bbs[_cur_revolver_li].insert(bb); //insert BB
        }

      }
    } 
  }

  //Returns the estimated benefit of the model versus the baseline core
  virtual float estimated_benefit(LoopInfo* li) {
    return 1.0f;
  }

  virtual void setGraph(dep_graph_impl_t<Inst_t,T,E>*) {}


  Op* _prev_op=NULL;
  //Public function which inserts functions from the trace
  virtual void insert_inst(const CP_NodeDiskImage &img,
                   uint64_t index, Op* op) {
    //The Important Stuff
    InstPtr sh_inst = createInst(img,index,op,false);
    getCPDG()->addInst(sh_inst,index);
    addDeps(sh_inst,op);
    pushPipe(sh_inst);
    inserted(sh_inst);
  }

  virtual void pushPipe(std::shared_ptr<Inst_t>& sh_inst) {
    if(_elide_mem && sh_inst->isAccelerated) {
      if(sh_inst->_op && (sh_inst->_op->shouldIgnoreInAccel() ||
                          sh_inst->_op->isLoad())) {
        return;
      }
    }

    insertLSQ(sh_inst);

    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(depInst) {
      uint64_t commitCycle = depInst->cycleOfStage(Inst_t::Commit);
      uint64_t fetchCycle = sh_inst->cycleOfStage(Inst_t::Fetch);
      int64_t diff = fetchCycle - commitCycle;
      if(diff > 0) {
        nonPipelineCycles+=diff;
      }
    }
    getCPDG()->pushPipe(sh_inst);
    rob_head_at_dispatch++;
  }

  virtual dep_graph_t<Inst_t,T,E> * getCPDG() = 0;

  virtual uint64_t numCycles() {
    return getCPDG()->getMaxCycles();
  }
 

  virtual uint64_t finish() {
    getCPDG()->finish(maxIndex);
    uint64_t final_cycle = getCPDG()->getMaxCycles();
    activityMap.insert(final_cycle);
    activityMap.insert(final_cycle+1);
    std::cout << _name << " has finished!\n";
    std::cout << _name << " cleanup until cycle: " << final_cycle+3000 << "\n";
    cleanUp(final_cycle+3000);
    return final_cycle;
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


  virtual void printEdgeDep(Inst_t& inst, int ind,
                    unsigned default_type1, unsigned default_type2 = E_NONE)
  {
    if (!getTraceOutputs())
      return;
    E* laEdge = inst[ind].lastArrivingEdge();
    unsigned lae;
    if (laEdge) {
      lae = laEdge->type();
      if ((lae != default_type1 && lae!=default_type2) || laEdge->len()>1) {
        outs() << edge_name[lae];
        outs() << laEdge->len();
      }
    }
    outs() << ",";
  }

  virtual void traceOut(uint64_t index,
                        const CP_NodeDiskImage &img, Op* op) {
    if (!getTraceOutputs())
      return;

    Inst_t& inst =
      static_cast<Inst_t&>(getCPDG()->queryNodes(index));

    outs() << index + Prof::get().skipInsts;
    outs() << "<op" << op->id() << ">";
    outs() << ": ";
    outs() << inst.cycleOfStage(0) << " ";
    outs() << inst.cycleOfStage(1) << " ";
    outs() << inst.cycleOfStage(2) << " ";
    outs() << inst.cycleOfStage(3) << " ";
    outs() << inst.cycleOfStage(4) << " ";
    outs() << inst.cycleOfStage(5) << " ";

    if (img._isstore) {
      outs() << inst.cycleOfStage(6) << " ";
    }

    //outs() << (_isInOrder ? "io":"ooo");

    printEdgeDep(inst,0,E_FF);
    printEdgeDep(inst,1,E_FD);
    printEdgeDep(inst,2,E_DR);
    printEdgeDep(inst,3,E_RE);
    printEdgeDep(inst,4,E_EP);
    printEdgeDep(inst,5,E_PC,E_CC);

    if (img._isstore) {
      printEdgeDep(inst,6,E_WB);
    }

    outs() << " rs:" << rob_head_at_dispatch
           << "-" << (int)(rob_growth_rate*100);
    outs() << " iq:" << InstQueue.size();

    outs() << " lq:" << lqSize();
    outs() << " sq:" << sqSize();

    outs() << "\n";
    outs().flush();
  }


protected:

  int lqSize() {
     return (lq_head_at_dispatch<=LQind) ? (LQind-lq_head_at_dispatch):
      (LQind-lq_head_at_dispatch+32);
  }

  int sqSize() {
    return (sq_head_at_dispatch<=SQind) ? (SQind-sq_head_at_dispatch):
      (SQind-sq_head_at_dispatch+32);
  }


  //Code to map instructions to ops, and back
  std::unordered_map<Op *, BaseInstPtr> _op2InstPtr;
  //std::map<Inst_t *, Op*> _inst2Op;

  virtual InstPtr createInst(const CP_NodeDiskImage &img, 
                             uint64_t index, Op *op, bool track=true) {
    InstPtr ret = InstPtr(new Inst_t(img, index, op));
    if (track && op) {
      keepTrackOfInstOpMap(ret, op);
    }
    return ret;
  }

  void createDummy(const CP_NodeDiskImage &img,uint64_t index, Op *op, 
      int dtype = dg_inst_dummy<T,E>::DUMMY_MOVE) {
    std::shared_ptr<dg_inst_dummy<T,E>> dummy_inst
                = std::make_shared<dg_inst_dummy<T,E>>(img,index,op,dtype); 
    getCPDG()->addInst(dummy_inst,index);

    unsigned dummy_prod=0;
    bool err = dummy_inst->getProd(dummy_prod);
    if (!err && dummy_prod !=0 && dummy_prod < dummy_inst->index() &&
           getCPDG()->hasIdx(dummy_inst->index()-dummy_prod)) {

      dg_inst_base<T,E>* retInst = 
         &(getCPDG()->queryNodes(dummy_inst->index()-dummy_prod));

      getCPDG()->insert_edge(*retInst, retInst->eventComplete(),
                             *dummy_inst, dummy_inst->beginExecute(), 0, E_RDep);
    }

    keepTrackOfInstOpMap(dummy_inst,op);
  }

  std::unordered_set<Op*> dummy_cycles;


  dg_inst_base<T,E>* fixDummyInstruction(dg_inst_base<T,E>* depInst, 
                                         bool& out_of_bounds, bool& error) {
    std::set<Op*> seen_dummies;
    return fixDummyInstruction(depInst,out_of_bounds,error,seen_dummies); 
  }


  //puts the correct instruction here
  //returns true if there is an error
  dg_inst_base<T,E>* fixDummyInstruction(dg_inst_base<T,E>* depInst, 
                                         bool& out_of_bounds, bool& error,
                                         std::set<Op*>& seen_dummies) {
    seen_dummies.insert(depInst->_op);

    if(depInst->isDummy()) {
      auto dummy_inst = dynamic_cast<dg_inst_dummy<T,E>*>(depInst);
      unsigned dummy_prod=0;

      dg_inst_base<T,E>* retInst=NULL;

      if(dummy_inst->_dtype == dg_inst_dummy<T,E>::DUMMY_STACK_SLOT) { 
        //dummy through memory
        if(dummy_inst->_isload) {
          //error |= dummy_inst->getMemProd(dummy_prod);
          assert(depInst->_op && depInst->_op->isStack());
          Op* st_op = depInst->_op->storeForStackLoad();
          assert(st_op->isStack());
          retInst = getInstForOp(st_op).get();
          assert(retInst);
        } else {
          error |= dummy_inst->getDataProdOfStore(dummy_prod);
          if (dummy_prod ==0 || dummy_prod >= dummy_inst->index() || 
              !getCPDG()->hasIdx(dummy_inst->index()-dummy_prod)) {
            out_of_bounds|=true;
            return NULL;
          }
          retInst = &(getCPDG()->queryNodes(dummy_inst->index()-dummy_prod));
        }
      } else if (dummy_inst->_dtype == dg_inst_dummy<T,E>::DUMMY_MOVE) { 
        //dummy through data, use dynamic trace
        error |= dummy_inst->getProd(dummy_prod);
        if (dummy_prod ==0 || dummy_prod >= dummy_inst->index() || 
            !getCPDG()->hasIdx(dummy_inst->index()-dummy_prod)) {
          out_of_bounds|=true;
          return NULL;
        }
        retInst = &(getCPDG()->queryNodes(dummy_inst->index()-dummy_prod));
      } /*else if (dummy_inst->_dtype == dg_inst_dummy<T,E>::DUMMY_MOVE) { 
        out_of_bounds|=true;
        return NULL;
      }*/


/*
      if(retInst && seen_dummies.count(retInst->_op)) {
        //found a cycle
        if(!dummy_cycles.count(retInst->_op)) {
          std::cerr << "Found dummy cycle: ";
          for(Op* op : seen_dummies) {
            std::cerr << op->dotty_name() << " ";
          }
          std::cerr << "\n";
        }
        for(Op* op : seen_dummies) {
          dummy_cycles.insert(op);
        }
        out_of_bounds|=true;
        return NULL;
      }
*/
      if(retInst) {
        if(retInst->index() > dummy_inst->index()) {
          out_of_bounds|=true;
          return NULL;
        }
      }

      return fixDummyInstruction(retInst,out_of_bounds,error,seen_dummies);
    }
    return depInst;
  }


  virtual void keepTrackOfInstOpMap(BaseInstPtr ret, Op *op) {
    assert(ret);
    _op2InstPtr[op] = ret;
    ret->setCumWeights(curWeights());

    //_inst2Op[ret.get()] = op;
  }

  virtual Op* getOpForInst(Inst_t &n, bool allowNull = false) {
    if (n._op!=NULL) {
      return n._op; 
    }

    if (!allowNull) {
      if (n.hasDisasm()) {
        return 0; // Fake instructions ....
      }
      assert(0 &&  "inst2Op map does not have inst??");
    }
    return 0;
  }

  virtual BaseInstPtr getInstForOp(Op *op) {
    auto Op2I = _op2InstPtr.find(op);
    if (Op2I != _op2InstPtr.end())
      return Op2I->second;
    return 0;
  }

  //function to add dependences onto uarch event nodes -- for the pipeline
  virtual void addDeps(std::shared_ptr<Inst_t>& inst, Op* op = NULL) {
    if(_elide_mem && inst->isAccelerated) { //normal path
      if (inst->_isload || (op && (op->isLoad() || op->shouldIgnoreInAccel()))) {
        checkDataDep(*inst);
        checkRE(*inst);
        checkEP(*inst);
        return;
      }
    }

    setFetchCycle(*inst);
    setDispatchCycle(*inst);
    setReadyCycle(*inst);
    setExecuteCycle(inst); //uses shared_ptr
    setCompleteCycle(*inst,op);
    setCommittedCycle(*inst);
    setWritebackCycle(inst); //uses shared_ptr
  }

  //Variables to hold dynamic state not directly expressible in graph theory
  typedef std::multimap<uint64_t,Inst_t*> ResVec;
  ResVec InstQueue;

  typedef std::vector<std::shared_ptr<BaseInst_t>> ResQueue;  
  ResQueue SQ, LQ; 
  unsigned SQind;
  unsigned LQind;
 
  typedef std::map<uint64_t,std::vector<Inst_t*>> SingleCycleRes;
  SingleCycleRes wbBusRes;
  SingleCycleRes issueRes;

  typedef std::map<uint64_t,int> FuUsageMap; //maps cycle to usage until next cycle
  typedef std::map<uint64_t,FuUsageMap> FuUsage;
 
  std::map<uint64_t,std::set<uint64_t>> MSHRUseMap;
  std::map<uint64_t,std::shared_ptr<BaseInst_t>> MSHRResp;

  typedef typename std::map<uint64_t,std::shared_ptr<BaseInst_t>> NodeRespMap;
  typedef typename std::map<uint64_t,NodeRespMap> NodeResp;

  std::map<uint64_t,uint64_t> minAvail; //res->min avail time
  FuUsage fuUsage;
  NodeResp nodeResp; 

  typedef std::set<uint64_t> ActivityMap;
  ActivityMap activityMap;
  
  typedef std::unordered_map<Op*,std::set<Op*>> MemDepSet;
  MemDepSet memDepSet;

  int rob_head_at_dispatch;
  int prev_rob_head_at_dispatch;
  int prev_squash_penalty;
  float avg_rob_head;
  float rob_growth_rate;

  unsigned lq_head_at_dispatch;
  unsigned sq_head_at_dispatch;

  int pipe_index;
  uint16_t lq_size_at_dispatch[PSIZE];
  uint16_t sq_size_at_dispatch[PSIZE];

  uint64_t maxIndex;
  uint64_t _curCycle;
  uint64_t _lastCleaned=0;
  uint64_t _latestCleaned=0;

  std::unordered_set<uint64_t> fu_monitors;
 
  std::shared_ptr<Inst_t> func_transition_inst;

  virtual void print_edge_weights(std::ostream& out, AnalysisType analType) override {
    getCPDG()->cumWeights()->print_edge_weights(out,analType);
    /* double total_weight=0; //should roughly equal number of cycles in program
    for(int i = 0; i < E_NUM; ++i) {
      double weight = getCPDG()->weightOfEdge(i,analType);
      total_weight+=weight;
    }

    for(int i = 0; i < E_NUM; ++i) {
      double weight = getCPDG()->weightOfEdge(i,analType);

      if(weight!=0.0) {
        cout << edge_name[i] << ":" << weight/total_weight << " ";
      }
    }
    cout << " (total: " << total_weight << ")";*/
  }

  virtual void monitorFUUsage(uint64_t id, uint64_t numCycles, int usage) {
    
  }

  virtual void cleanFU(uint64_t curCycle) {
    //for funcUnitUsage
    for(auto &pair : fuUsage) {
      auto& fuUseMap = pair.second;
      if(fuUseMap.begin()->first < curCycle) {
        //debug MSHRUse Deletion
        //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
        auto upperUse = --fuUseMap.upper_bound(curCycle);
        auto firstUse = fuUseMap.begin();

        if(upperUse->first > firstUse->first) {
          if(fu_monitors.count(pair.first)) {
            uint64_t prev_cycle;
            int prev_usage;
            for(auto i=firstUse; i!=upperUse; ++i) {
              if(i!=firstUse) {
                monitorFUUsage(pair.first,i->first-prev_cycle,prev_usage);
              }
              prev_cycle=i->first;
              prev_usage=i->second;
            }
          }

          fuUseMap.erase(firstUse,upperUse); 
        }
      }
    }

    for(typename NodeResp::iterator i=nodeResp.begin(),e=nodeResp.end();i!=e;++i) {
      NodeRespMap& respMap = i->second;
      if(respMap.begin()->first < curCycle) {
        //debug MSHRUse Deletion
        //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
        auto upperUse = --respMap.upper_bound(curCycle);
        auto firstUse = respMap.begin();
        if(upperUse->first > firstUse->first) {
          respMap.erase(firstUse,upperUse); 
        }
      }

    }
  }

  virtual void cleanMSHR(uint64_t curCycle) {
    //DEBUG MSHR Usage
/*    for(auto i=MSHRUseMap.begin(),e=MSHRUseMap.end();i!=e;) {
      uint64_t cycle = i->first;

      if(cycle > 10000000) {
        break;
      }
      std::cerr << i->first << " " << i->second.size() << ", ";
      ++i;
    }
    std::cerr << "(" << MSHRUseMap.size() << ")\n";*/

    if(MSHRUseMap.begin()->first < curCycle) {
      //debug MSHRUse Deletion
      //std::cout <<"MSHRUse  << MSHRUseMap.begin()->first <<" < "<< curCycle << "\n";
      auto upperMSHRUse = --MSHRUseMap.upper_bound(curCycle);
      auto firstMSHRUse = MSHRUseMap.begin();
      if(upperMSHRUse->first > firstMSHRUse->first) {
/*        auto i = firstMSHRUse;
        auto e = upperMSHRUse;
        for(;i!=e;++i) {
         uint64_t cycle = i->first;
         std::cerr << cycle << " " << i->second.size() << "\n";
        }*/
        MSHRUseMap.erase(firstMSHRUse,upperMSHRUse); 
      }
    }

    auto upperMSHRResp = MSHRResp.upper_bound(curCycle-200);
    auto firstMSHRResp = MSHRResp.begin();
    if(upperMSHRResp->first > firstMSHRResp->first) {
      MSHRResp.erase(firstMSHRResp,upperMSHRResp); 
    }
  }

  virtual void cleanUp(uint64_t curCycle) {
    _lastCleaned=curCycle;
    _latestCleaned=std::max(curCycle,_latestCleaned);

    typename SingleCycleRes::iterator upperbound_sc;
    upperbound_sc = wbBusRes.upper_bound(curCycle);
    wbBusRes.erase(wbBusRes.begin(),upperbound_sc); 
    upperbound_sc = issueRes.upper_bound(curCycle);
    issueRes.erase(issueRes.begin(),upperbound_sc); 

    //summing up idle cycles
    uint64_t prevCycle=0;
    for(auto I=activityMap.begin(),EE=activityMap.end();I!=EE;) {
      uint64_t cycle=*I;
      if(prevCycle!=0 && cycle-prevCycle>18) {
        idleCycles+=(cycle-prevCycle-18);
      }
      if (cycle + 100  < curCycle && activityMap.size() > 2) {
        I = activityMap.erase(I);
        prevCycle=cycle;
      } else {
        break;
      }
    }

    cleanMSHR(curCycle);
    cleanFU(curCycle);
  }

  //Update Queues: IQ, LQ, SQ
  virtual void insertLSQ(std::shared_ptr<BaseInst_t> inst) {
    if (inst->_isload) {
      LQ[LQind]=inst;
      LQind=(LQind+1)%LQ_SIZE;
    }
    if (inst->_isstore) {
      SQ[SQind]=inst;
      SQind=(SQind+1)%SQ_SIZE;
    }
  }


  virtual void inserted(std::shared_ptr<Inst_t>& inst) {
    if(inst->_op) {
      keepTrackOfInstOpMap(inst,inst->_op);
      if(_enable_revolver&&inst->_index!=0) { 
        track_revolver(inst->_index,inst->_op);
      }
      _prev_op=inst->_op;
    }

    maxIndex = inst->index();
    _curCycle = inst->cycleOfStage(Inst_t::Fetch);

    //add to activity
    for(int i = 0; i < Inst_t::NumStages -1 + inst->_isstore; ++i) {
      uint64_t cyc = inst->cycleOfStage(i);
      activityMap.insert(cyc);
      
      if(i == Inst_t::Fetch) {
        activityMap.insert(cyc+3);
      }
      if(i == Inst_t::Complete) {
        activityMap.insert(cyc+2);
      }


    }
    cleanUp(_curCycle);
  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addMSHRResource(uint64_t min_cycle, uint32_t duration, 
                     std::shared_ptr<BaseInst_t> cpnode,
                     uint64_t eff_addr,
                     int re_check_frequency,
                     int& rechecks,
                     uint64_t& extraLat,
                     bool not_coalescable=false) { 

    unsigned maxUnits = L1_MSHRS;
    rechecks=0; 

    uint64_t cur_cycle=min_cycle;
    std::map<uint64_t,std::set<uint64_t>>::iterator
                       cur_cycle_iter,next_cycle_iter,last_cycle_iter;
  
    assert(MSHRUseMap.begin()->first < min_cycle);
 
    cur_cycle_iter = --MSHRUseMap.upper_bound(min_cycle);

    uint64_t filter_addr=(((uint64_t)-1)^(Prof::get().cache_line_size-1));
    uint64_t addr = eff_addr & filter_addr;

    //keep going until we find a spot .. this is gauranteed
    while(true) {
      assert(cur_cycle_iter->second.size() <= maxUnits);
      assert(cur_cycle_iter->first < ((uint64_t)-20000));

      if(cur_cycle_iter->second.size() < maxUnits && //max means cache-blocked
         cur_cycle_iter->second.count(addr)) {
        //return NULL; //just add myself to some other MSHR, and return
        //we found a match... iterate until addr is gone
        while(true) {
          cur_cycle_iter--;
          if(cur_cycle_iter->second.count(addr)==0) {
            cur_cycle_iter++;
            auto  respIter = MSHRResp.find(cur_cycle_iter->first);
            if(respIter!=MSHRResp.end()) {
              return respIter->second.get();
            } else {
              return NULL; 
            }
          }
        }
      }

      //If no spots, keep looking later (pessimistic)
      if(cur_cycle_iter->second.size() == maxUnits) {
        if(re_check_frequency<=1) {
          ++cur_cycle_iter;
          assert(cur_cycle_iter->first < ((uint64_t)-20000));
          cur_cycle=cur_cycle_iter->first;
        } else {
          cur_cycle+=re_check_frequency;
          cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
          assert(cur_cycle_iter->first < ((uint64_t)-20000));
          rechecks++;
        }
        continue;
      }

      //we have found a spot with less than maxUnits, now we need to check
      //if there are enough cycles inbetween
      assert(cur_cycle_iter->second.size() < maxUnits);

      next_cycle_iter=cur_cycle_iter;
      bool foundSpot=false;
      
      while(true) {
        ++next_cycle_iter;
        if(next_cycle_iter->first >= cur_cycle+duration) {
          foundSpot=true;
          last_cycle_iter=next_cycle_iter;
          break;
        } else if(next_cycle_iter->second.size() == maxUnits) {
          //we couldn't find a spot, so increment cur_iter and restart
          if(re_check_frequency<=1) {
            ++next_cycle_iter;
            cur_cycle_iter=next_cycle_iter;
            cur_cycle=cur_cycle_iter->first;
            assert(cur_cycle_iter->first < ((uint64_t)-20000));
          } else {
            cur_cycle+=re_check_frequency;
            cur_cycle_iter = --MSHRUseMap.upper_bound(cur_cycle);
            rechecks++;
            assert(cur_cycle_iter->first < ((uint64_t)-20000));
          }
          break;
        }
      }

      if(foundSpot) {
        if(cur_cycle_iter->first!=cur_cycle) {
          MSHRUseMap[cur_cycle]=cur_cycle_iter->second; 
          cur_cycle_iter = MSHRUseMap.find(cur_cycle); 
        }
        cur_cycle_iter->second.insert(addr);

 
        //iterate through the others
        ++cur_cycle_iter; //i already added here
        while(cur_cycle_iter->first < cur_cycle + duration) {
          assert(cur_cycle_iter->second.size() < maxUnits);
          cur_cycle_iter->second.insert(addr);
          ++cur_cycle_iter;
        }

        //cur_cycle_iter now points to the iter aftter w
        //final cycle iter
        if(cur_cycle_iter->first==cur_cycle+duration) {
          //no need to add a new marker
        } else {
          //need to return to original usage here
          --cur_cycle_iter;
          MSHRUseMap[cur_cycle+duration] = cur_cycle_iter->second;
          MSHRUseMap[cur_cycle+duration].erase(addr);
        }
       
        auto respIter = MSHRResp.find(cur_cycle);
        MSHRResp[cur_cycle+duration]=cpnode;

        if(rechecks>0) {
          respIter = --MSHRResp.upper_bound(cur_cycle);
          extraLat = cur_cycle-respIter->first;
        }

        if(respIter == MSHRResp.end() || min_cycle == cur_cycle) {
          return NULL;
        } else {
          return respIter->second.get();
        }
        
      }
    }
    assert(0);
    return NULL;
  }

  void checkResourceEmpty(FuUsageMap& fuUse) {
    if(fuUse.size() == 0) {
      for(auto &fuUse : fuUsage) {
        fuUse.second[0]=0; //begining usage is 0
        fuUse.second[(uint64_t)-10000]=0; //end usage is 0 too (1000 to prevent overflow)
      }
    }
  }

  //Adds a resource to the resource utilization map.
  BaseInst_t* addResource(uint64_t resource_id, uint64_t min_cycle_in, 
                     uint32_t duration, int maxUnits, 
                     std::shared_ptr<BaseInst_t> cpnode) {
    FuUsageMap& fuUseMap = fuUsage[resource_id];
    uint64_t minAvailRes = minAvail[resource_id]; //todo: not implemented yet
    NodeRespMap& nodeRespMap = nodeResp[resource_id];

    checkResourceEmpty(fuUseMap);
   
    uint64_t min_cycle = std::max(min_cycle_in,minAvailRes); //TODO: is this every useful?
    assert(fuUseMap.begin()->first <= min_cycle);


    uint64_t cur_cycle=min_cycle;
    FuUsageMap::iterator cur_cycle_iter,next_cycle_iter,last_cycle_iter;
   
    cur_cycle_iter = --fuUseMap.upper_bound(min_cycle);

    //keep going until we find a spot .. this is gauranteed
    while(true) {
      assert(cur_cycle_iter->second <= maxUnits);
      if(cur_cycle_iter->second == maxUnits) {
        ++cur_cycle_iter;
        cur_cycle=cur_cycle_iter->first;
        continue;
      }
      //we have found a spot with less than maxUnits, now we need to check
      //if there are enough cycles inbetween
      assert(cur_cycle_iter->second < maxUnits);

      next_cycle_iter=cur_cycle_iter;
      bool foundSpot=false;
      
      while(true) {
        ++next_cycle_iter;
        if(next_cycle_iter->first >= cur_cycle+duration) {
          foundSpot=true;
          last_cycle_iter=next_cycle_iter;
          break;
        } else if(next_cycle_iter->second == maxUnits) {
          //we couldn't find a spot, so increment cur_iter and restart
          ++next_cycle_iter;
          cur_cycle_iter=next_cycle_iter;
          cur_cycle=cur_cycle_iter->first;
          break;
        }
      }

      if(foundSpot) {
        if(cur_cycle_iter->first==cur_cycle) {
          //if we started on time point we already have
          cur_cycle_iter->second++;
        } else {
          //create new entry for this cycle
          fuUseMap[cur_cycle]=cur_cycle_iter->second+1; 
          cur_cycle_iter = fuUseMap.find(cur_cycle); 
        }        

        
        FuUsageMap::iterator delete_cycle_iter;
        bool delete_me=false;

        //now do something cool!
        if(cur_cycle_iter->second == maxUnits) {
          //first, we can delete nodeResp, cause noone will use this for sure!
          nodeRespMap.erase(cur_cycle);

          if(cur_cycle_iter != fuUseMap.begin()) {
            auto prev_cycle_iter = std::prev(cur_cycle_iter);
            if(prev_cycle_iter->second == maxUnits) {
               //prev_cycle_iter!=fuUseMap.begin()) {
               //auto two_prev_cycle_iter = std::prev(prev_cycle_iter);
               delete_cycle_iter=cur_cycle_iter;
               delete_me=true;
            }
          }
        }

        //iterate through the others
        ++cur_cycle_iter;
        while(cur_cycle_iter->first < cur_cycle + duration) {
          assert(cur_cycle_iter->second < maxUnits);
          cur_cycle_iter->second++;
          ++cur_cycle_iter;
        }

        //final cycle iter
        if(cur_cycle_iter->first==cur_cycle+duration) {
          //no need to add a new marker
        } else {
          //need to return to original usage here
          --cur_cycle_iter;
          fuUseMap[cur_cycle+duration] = cur_cycle_iter->second-1;
          assert(fuUseMap[cur_cycle+duration]>=0);
        }
        
        if(delete_me) {
          fuUseMap.erase(delete_cycle_iter);
        }

        auto respIter = nodeRespMap.find(cur_cycle);
        nodeRespMap[cur_cycle+duration]=cpnode;

        if(respIter == nodeRespMap.end() || min_cycle == cur_cycle) {
          return NULL;
        } else {
          return respIter->second.get();
        }
        
      }
    }
    assert(0);
    return NULL;
  }

  virtual void setFetchCycle(Inst_t& inst) {
    if(!_revolver_active) {
      checkFBW(inst);
      checkPipeStalls(inst);
    }
    checkControlMispeculate(inst);
    checkFF(inst);
    checkSerializing(inst);

    if(!_revolver_active) {
      //-----FETCH ENERGY-------
      //Need to get ICACHE accesses somehow
      if(inst._icache_lat>0) {
        icache_read_accesses++;
      }
      if(inst._icache_lat>20) {
        icache_read_misses++;
      }
    }

    if(_isInOrder) {
      //checkBranchPredict(inst,op); //just control dependence
    } else {
      //checkIQStalls(inst);
      checkLSQStalls(inst);
    }
  }


  //This eliminates LSQ entries which can be freed before "cycle"
  virtual void cleanLSQEntries(uint64_t cycle) {
    //HANDLE LSQ HEADS
    //Loads leave at complete
    do {
      auto depInst = LQ[lq_head_at_dispatch];
      if (depInst.get() == 0) {
        break;
      }
      //TODO should this be complete or commit?
      if (depInst->cycleOfStage(depInst->memComplete()) < cycle ) {
        LQ[lq_head_at_dispatch] = 0;
        lq_head_at_dispatch += 1;
        lq_head_at_dispatch %= LQ_SIZE;
      } else {
        break;
      }
    } while (lq_head_at_dispatch != LQind);

    do {
      auto depInst = SQ[sq_head_at_dispatch];
      if(depInst.get() == 0) {
        break;
      }
      if (depInst->cycleOfStage(depInst->memComplete()) < cycle ) {
        SQ[sq_head_at_dispatch] = 0;
        sq_head_at_dispatch += 1;
        sq_head_at_dispatch %= SQ_SIZE;
      } else {
        break;
      }
    } while (sq_head_at_dispatch != SQind);
    lq_size_at_dispatch[getCPDG()->getPipeLoc()%PSIZE] = lqSize();
    sq_size_at_dispatch[getCPDG()->getPipeLoc()%PSIZE] = sqSize();
  }


  virtual void setDispatchCycle(Inst_t &inst) {
    checkFD(inst);

//    if(!_revolver_active) {
      checkDD(inst);
      checkDBW(inst);
//    }

    checkROBSize(inst); //Finite Rob Size  (for INORDER -- finite # in flight)

    if(_isInOrder) {
      //nothing
      // TODO: This is here because with out it, fetch and dispatch get
      // decoupled from the rest of the nodes, causing memory explosion
      // b/c clean dosen't get called
      // ideally, we should 
    } else {
      checkIQStalls(inst);
      checkLSQSize(inst[Inst_t::Dispatch],inst._isload,inst._isstore);
      // ------ ENERGY EVENTS -------
      if(inst._floating) {
        iw_fwrites++;
      } else {
        iw_writes++;
      }
    }

    if(!_revolver_active) {
      //rename_fwrites+=inst._numFPDestRegs;
      //rename_writes+=inst._numIntDestRegs;
      if(inst._floating) {
        rename_freads+=inst._numSrcRegs;
        rename_fwrites+=inst._numFPDestRegs+inst._numIntDestRegs;
      } else {
        rename_reads+=inst._numSrcRegs;
        rename_writes+=inst._numFPDestRegs+inst._numIntDestRegs;
      }
    }

    //HANDLE ROB STUFF
    uint64_t dispatch_cycle = inst.cycleOfStage(Inst_t::Dispatch);
    do {
      Inst_t* depInst = getCPDG()->peekPipe(-rob_head_at_dispatch);
      if(!depInst) {
        break;
      }
      if(depInst->cycleOfStage(Inst_t::Commit) < dispatch_cycle ) {
        rob_head_at_dispatch--;
      } else {
        break;
      }
    } while(rob_head_at_dispatch>=1);

    avg_rob_head = 0.01 * rob_head_at_dispatch + 
                   0.99 * avg_rob_head;

    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(depInst==0 || !depInst->_ctrl_miss) { 
      rob_growth_rate = 0.92f * rob_growth_rate
                      + 0.08f * (rob_head_at_dispatch - prev_rob_head_at_dispatch); 
    } else {
      rob_growth_rate=0;
    }

    prev_rob_head_at_dispatch=rob_head_at_dispatch;

    cleanLSQEntries(dispatch_cycle);
  }

  virtual void setReadyCycle(Inst_t &inst) {
    checkDR(inst);
    checkNonSpeculative(inst);
    checkDataDep(inst);
    if(_isInOrder) {
      //checkExecutePipeStalls(inst);
    } else {
      //no other barriers
    }

    /*
    // num src regs is wrong... eventually fix this in gem5
    if(inst._floating) {
      regfile_freads+=inst._numSrcRegs;
    } else {
      regfile_reads+=inst._numSrcRegs;
    }*/

    regfile_fwrites+=inst._numFPDestRegs;
    regfile_writes+=inst._numIntDestRegs;

    regfile_freads+=inst._numFPSrcRegs;
    regfile_reads+=inst._numIntSrcRegs;

  }

  virtual void setExecuteCycle(std::shared_ptr<Inst_t>& inst) {
    checkRE(*inst);

    if(_isInOrder) {
      checkEE(*inst); //issue in order
      checkInorderIssueWidth(*inst); //in order issue width
      checkPipelength(*inst);
    } else {
      checkIssueWidth(*inst);
    }

    checkFuncUnits(inst);

    if(inst->_isload) {
      //loads acquire MSHR at execute
      checkNumMSHRs(inst);
    }

    if(!_isInOrder) {
      //Non memory instructions can leave the IQ now!
      //Push onto the IQ by cycle that we leave IQ (execute cycle)
      if(!inst->isMem()) {
        InstQueue.insert(
          std::make_pair(inst->cycleOfStage(Inst_t::Execute),inst.get()));
      }
    }

    if(inst->_floating) {
      iw_freads+=2;
    } else {
      iw_reads+=2;
    }
  }

  virtual void setCompleteCycle(Inst_t &inst, Op* op) {
    checkEP(inst);
    if (!_isInOrder) {
      if(inst._isload) {
        checkPP(inst);
      }
      checkWriteBackWidth(inst);

      //Memory instructions can leave the IQ now!
      //Push onto the IQ by cycle that we leave IQ (complete cycle)
      if(inst.isMem()) {
        InstQueue.insert(
               std::make_pair(inst.cycleOfStage(Inst_t::Complete),&inst));
      }
    } 

  }
  virtual void setCommittedCycle(Inst_t &inst) {
    checkPC(inst);
    if(!_isInOrder) {
      checkSquashPenalty(inst);
    } else {
      checkEC(inst);
    }
    checkCC(inst);
    checkCBW(inst);
 
    getCPDG()->commitNode(inst.index());

    if(!_isInOrder) {
      //Energy
      rob_writes+=2;
      rob_reads++;
    }
    setEnergyStatsPerInst(inst);
  }

  virtual void setEnergyStatsPerInst(Inst_t& inst)
  {
    ++committed_insts;
    committed_int_insts += !inst._floating;
    committed_fp_insts += inst._floating;
    committed_branch_insts += inst._isctrl;
    committed_load_insts += inst._isload;
    committed_store_insts += inst._isstore;
    func_calls += inst._iscall;
  }

  virtual void setWritebackCycle(std::shared_ptr<Inst_t>& inst) {
    if(inst->_isstore) {
      int st_lat=stLat(inst->_st_lat,inst->_cache_prod,
                       inst->_true_cache_prod,inst->isAccelerated);
      getCPDG()->insert_edge(*inst, Inst_t::Commit,
                             *inst, Inst_t::Writeback, 2+st_lat, E_WB);
      checkNumMSHRs(inst);
      checkPP(*inst);
    }
  }

  virtual bool predictTaken(Inst_t& dep_n, Inst_t &n) {
    bool predict_taken = false;
    if(dep_n._pc == n._pc) { //this probably doesn't happen much!
      predict_taken = dep_n._upc + 1 != n._upc;
    } else {
      predict_taken |= n._pc < dep_n._pc;
      predict_taken |= n._pc > dep_n._pc+3;//x86 jumps are 2-3 bytes, I think
    }

    return predict_taken;
  }

  //==========FETCH ==============
  //Inorder fetch
  virtual Inst_t &checkFF(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    int lat=n._icache_lat;

    if(depInst->_isctrl) {
      //check if previous inst was a predict taken branch
      bool predict_taken=predictTaken(*depInst,n);
      if(predict_taken && lat==0/* TODO: check this*/) {
        lat+=1;
      }
    }

    if(_revolver_active) {
      lat=0;
    }

    //static int total_icache=0, total_ff=0;
    //total_icache+=n._icache_lat;
    //total_ff+=lat;

    //if(lat!=0) {
    //  std::cout << "ILAT: " << n._icache_lat << " " << lat << "(" << total_icache << ", " << total_ff << ")\n";
    //}


    getCPDG()->insert_edge(*depInst, Inst_t::Fetch,
                           n, Inst_t::Fetch,lat,E_FF);
    return n;
  }

  virtual Inst_t &checkSerializing(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    } 

    if(n._serialBefore || depInst->_serialAfter) {
      getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                             n, Inst_t::Fetch, 1,E_SER);
    }
    return n;
  }


  //Finite fetch bandwidth
  virtual Inst_t &checkFBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-FETCH_WIDTH);
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Fetch,
                           n, Inst_t::Fetch, 1, E_FBW); 
    return n;
  }

  virtual Inst_t &checkPipeStalls(Inst_t &n) {
    // 0   -1     -2        -3          -4 
    // F -> De -> Rename -> Dispatch -> Rob
    // F -> De -> Rename -> Dispatch -> Rob
    // F -> De -> Renane -> Dispatch -> Rob

    // ??? Should I add n._icache_lat?
    Inst_t* depInst = getCPDG()->peekPipe(
            -(FETCH_TO_DISPATCH_STAGES)*FETCH_WIDTH);
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                     n, Inst_t::Fetch, 0,E_FPip);

    return n;
  }

  //Control Dep
  virtual Inst_t &checkControlMispeculate(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    if (depInst->_ctrl_miss) {
         bool predict_taken=!predictTaken(*depInst,n);
     
        int adjusted_squash_cycles=2;
        int insts_to_squash=SQUASH_WIDTH;
        if(!_isInOrder){
          adjusted_squash_cycles=prev_squash_penalty-2*predict_taken;
          insts_to_squash=(prev_squash_penalty-2)*SQUASH_WIDTH;
        }
        squashed_insts+=insts_to_squash;


        getCPDG()->insert_edge(*depInst, Inst_t::Complete,
                               n, Inst_t::Fetch,
                               //n._icache_lat + prev_squash_penalty 
                               std::max((int)n._icache_lat, adjusted_squash_cycles)
                               + 1,E_CM); 

        //we need to make sure that the processor stays active during squash
        uint64_t i=depInst->cycleOfStage(Inst_t::Complete);
        for(;i<n.cycleOfStage(Inst_t::Fetch);++i) {
          activityMap.insert(i);
        }

        mispeculatedInstructions++;
    }
    return n;
  }

  //inst queue full stalls
  virtual Inst_t &checkIQStalls(Inst_t &n) {

    //First, erase all irrelevant nodes
    typename ResVec::iterator upperbound;
    upperbound = InstQueue.upper_bound(n.cycleOfStage(Inst_t::Dispatch));
    InstQueue.erase(InstQueue.begin(),upperbound);

    //Next, check if the IQ is full
    if (InstQueue.size() < (unsigned)IQ_WIDTH) {
      return n;
    }

    Inst_t* min_node=0;
    for (auto I=InstQueue.begin(), EE=InstQueue.end(); I!=EE; ++I) {
      Inst_t* i =I->second;  //I->second.get();
      if(i->iqOpen==false) {
        i->iqOpen=true;
        min_node=i;
        break;
      } else {
        //assert(0);
      }
    }
    
    //min_node=InstQueue.upper_bound(0)->second.get();
    assert(min_node);
    getCPDG()->insert_edge(*min_node, Inst_t::Execute,
                           n, Inst_t::Dispatch, n._icache_lat + 1, E_IQ);
    return n;
  }



  int numInstsNotInRob=0;
  //This should be called after FBW
  virtual Inst_t &checkLSQStalls(Inst_t &n) {
     Inst_t* prevInst = getCPDG()->peekPipe(-1); 
     if(!prevInst) {
       return n;
     }

     uint64_t cycleOfFetch = n.cycleOfStage(Inst_t::Fetch);
     if(prevInst->cycleOfStage(Inst_t::Fetch) == cycleOfFetch) {
       return n; //only check this on first fetch of new cycle
     }
   
     int pipeLoc = getCPDG()->getPipeLoc();

     //Caculate number of insts not in rob at time of fetch
     int i = 1;
     for(; i < PSIZE; ++i) {
       Inst_t* depInst = getCPDG()->peekPipe(-i); 
       if(!depInst) {
         return n;
       }
       if(depInst->cycleOfStage(Inst_t::Dispatch) <= cycleOfFetch) {
         break;
       } 
     }
 
    numInstsNotInRob=i;
    int blocking_dispatch=i;

    if(numInstsNotInRob + lq_size_at_dispatch[((pipeLoc-blocking_dispatch))%PSIZE]
         > (int)LQ_SIZE) {
      Inst_t* depInst = getCPDG()->peekPipe(-blocking_dispatch); 
      getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                              n, Inst_t::Fetch, 2, E_LQTF); //2-3 cycles emperical
    }

    return n;
    
  }


  //========== DISPATCH ==============

  //Dispatch follows fetch
  virtual Inst_t &checkFD(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Fetch,
                     n, Inst_t::Dispatch, FETCH_TO_DISPATCH_STAGES, E_FD);
    return n;
  }

  //Dispatch bandwdith
  //This edge is possibly never required?
  virtual Inst_t &checkDBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-D_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                          n, Inst_t::Dispatch, 1,E_DBW);
    return n;
  }

  //Dispatch Inorder
  virtual Inst_t &checkDD(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Dispatch,
                          n, Inst_t::Dispatch, 0,E_DD);
    return n;
  }

  //Finite ROB (commit to dispatch)
  //(for inorder, finite number in flight)
  virtual Inst_t &checkROBSize(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-ROB_SIZE); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Dispatch, 1,E_ROB);
    return n;
  }

  //Finite LQ & SQ
  //This function delays the event until there is an open spot in the LSQ
  virtual void checkLSQSize(T& event,bool isload, bool isstore) {
    if (isload) {
      if(LQ[LQind]) {
        getCPDG()->insert_edge(*LQ[LQind], LQ[LQind]->memComplete(),
                               event, 1+2,E_LSQ); //TODO: complete or commit
      }
    }
    if (isstore) {
      if(SQ[SQind]) {
        getCPDG()->insert_edge(*SQ[SQind], SQ[SQind]->memComplete(),
                               event, 1,E_LSQ);
      }
    }
  }


  //========== READY ==============
  //Ready Follows Dispatch
  virtual Inst_t &checkDR(Inst_t &n)
  {
    getCPDG()->insert_edge(n, Inst_t::Dispatch,
                     n, Inst_t::Ready, 0,E_DR);
    return n;
  }

   //NonSpeculative Insts
  virtual Inst_t &checkNonSpeculative(Inst_t &n) {
    if (n._nonSpec) {
      int ind=(SQind-1+SQ_SIZE)%SQ_SIZE;
      if(SQ[ind]) {
        getCPDG()->insert_edge(*SQ[ind], SQ[ind]->memComplete(),
                               n, Inst_t::Ready, 1,E_NSpc);
      }
      Inst_t* depInst = getCPDG()->peekPipe(-1); 
      if(depInst) {
        getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                                 n, Inst_t::Ready, 2,E_NSpc);
      }

    }
    return n;
  }
 

  //Data dependence
  virtual Inst_t &checkDataDep(Inst_t &n) {
    //register dependence
    checkRegisterDependence(n);

    //memory dependence
    if(n._isload || n._isstore) {
      checkMemoryDependence(n);
      insert_mem_predicted_edges(n);
    }
    return n;
  }

  // Register Data Dependence
  std::unordered_set<Op*> ops_with_missing_deps;

  virtual Inst_t &checkRegisterDependence(Inst_t &n) {
    const int NumProducer = MAX_SRC_REGS; // FIXME: X86 specific
    for (int i = 0; i < NumProducer; ++i) {
      unsigned prod = n._prod[i];
      if (prod <= 0 || prod >= n.index()) {
        continue;
      }

      if(!getCPDG()->hasIdx(n.index()-prod)) {
        if(!_supress_errors) {
          if(ops_with_missing_deps.count(n._op)==0) {
            ops_with_missing_deps.insert(n._op);
            std::cerr << "WARNING: OP:" << n._op->id() 
                      << " BB:" << n._op->bb()->rpoNum()
                      << " ind:" << n.index()
                      << ", func:" << n._op->func()->nice_name()
                      << " is missing an op (-" << prod << ")\n";
          }
        }
        continue;
      }
      BaseInst_t& depInst = getCPDG()->queryNodes(n.index()-prod);

      getCPDG()->insert_edge(depInst, depInst.eventComplete(),
                             n, Inst_t::Ready, 0, E_RDep);
    }
    return n;
  }


  // Memory Dependence
  virtual void checkMemoryDependence(Inst_t &n) {
    static bool memory_dependence_error=false;
    if ( (n._mem_prod > 0 && n._mem_prod < n.index() )  ) {

      if(getCPDG()->hasIdx(n.index()-n._mem_prod)) {
        BaseInst_t& dep_inst=getCPDG()->queryNodes(n.index()-n._mem_prod);
        addTrueMemDep(dep_inst,n);
      } else if (memory_dependence_error == false) {
        memory_dependence_error=true;
        std::cerr << "MEMORY DEPENDENCE ERROR\n";
      }
    }
  }


  //Instructions dep_n and n are truely dependent
  virtual void addTrueMemDep(BaseInst_t& dep_n, BaseInst_t& n) {

//    std::cout << "ooo-try-pred" << dep_n._op->id() << "-m->" << n._op->id() << "\n";
        
    //This should only get seen as a mem dep, if the dependent instruction is 
    //actually not complete "yet"
    if(dep_n.cycleOfStage(dep_n.eventComplete()) > 
           n.cycleOfStage(n.eventReady())) {

      if(n._op!=NULL && dep_n._op!=NULL) {
        //add this to the memory predictor list
//      std::cout << "ooo-will-pred" << dep_n._op->id() << "-m->" << n._op->id() << "\n";

        memDepSet[n._op].insert(dep_n._op);
      } else {
        //We better insert the mem-dep now, because it didn't get added to memDepSet
        //This is sort of an error, should probably fix any cases like this.
        insert_mem_dep_edge(dep_n,n);
      }
    }
  }

  int numMemChecks=0;
  virtual void checkClearMemDep() {
    if(++numMemChecks==250000) {
      memDepSet.clear();
    }
  }

  //Inserts edges predicted by memory dependence predictor
  virtual void insert_mem_predicted_edges(BaseInst_t& n) {
    checkClearMemDep();
    //insert edges 
    for(auto i=memDepSet[n._op].begin(), e=memDepSet[n._op].end(); i!=e;++i) {
      Op* dep_op = *i;
      BaseInstPtr sh_inst = getInstForOp(dep_op);

      if(sh_inst) {
        insert_mem_dep_edge(*sh_inst,n,E_PDep);
      }
    } 
  }


  virtual void insert_mem_dep_edge(BaseInst_t &prev_node, BaseInst_t &n, int edge_type=E_MDep)
  {
    assert(&prev_node != &n);
    if (prev_node._isstore && n._isload) {
      // RAW true dependence
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventReady(), 0, edge_type);
    } else if (prev_node._isstore && n._isstore) {
      // WAW dependence (output-dep)
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventComplete(), 0, edge_type);
    } else if (prev_node._isload && n._isstore) {
      // WAR dependence (load-store)
      getCPDG()->insert_edge(prev_node, prev_node.eventComplete(),
                             n, n.eventReady(), 0, edge_type);
    }
  }



  enum FU_Type{
    FU_Other=0,
    FU_Unknown=1,
    FU_IntALU=2,
    FU_IntMulDiv=3,
    FU_FloatALU=4,
    FU_FloatMulDiv=5,
    FU_MemPort=6,
    FU_Sigmoid=100
  };

  //==========EXECUTION ==============
  //Execution follows Ready

  //get index for fuPool
  virtual int fuPoolIdx(int opclass1, Op* op) {
    if(op && op->is_sigmoid()) {
      return 100;
    }

    if (opclass1 > 9 && opclass1 < 30) {
      return FU_Other;
    }

    switch(opclass1) {
    default: 
      return FU_Unknown;
    case 1: //IntALU
      return FU_IntALU;
    case 2: //IntMult
    case 3: //IntDiv
      return FU_IntMulDiv;
    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return FU_FloatALU;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return FU_FloatMulDiv;
    case 30: //MemRead
    case 31: //MemWrite
      return FU_MemPort;
    }
    assert(0);
  }

  virtual unsigned getNumFUAvailable(Inst_t &n) {
    return getNumFUAvailable(n._opclass,n._op);
  }

  virtual unsigned getNumFUAvailable(uint64_t opclass, Op* op) {
    if(op && op->is_sigmoid()) {
      return 1;
    }

    if(opclass > 50) {
      return 1;
    }

    switch(opclass) {
    case 0: //No_OpClass
      return ROB_SIZE+IQ_WIDTH;
    case 1: //IntALU
      return N_ALUS;

    case 2: //IntMult
    case 3: //IntDiv
      return N_MUL;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return N_FPU;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return N_MUL_FPU;
    case 30: //MemRead
    case 31: //MemWrite
      return RW_PORTS;

    default:
      return 4; //hopefully this never happens
    }
    return 4; //and this!
  }

  virtual unsigned getFUIssueLatency(int opclass, Op* op) {
    if(op && op->is_sigmoid()) {
      return 8;
    }

    switch(opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return Prof::get().int_alu_issueLat;

    case 2: //IntMult
      return Prof::get().mul_issueLat;
    case 3: //IntDiv
      return Prof::get().div_issueLat;

    case 4: //FloatAdd
      return Prof::get().fadd_issueLat;
    case 5: //FloatCmp
      return Prof::get().fcmp_issueLat;
    case 6: //FloatCvt
      return Prof::get().fcvt_issueLat;
    case 7: //FloatMult
      return Prof::get().fmul_issueLat;
    case 8: //FloatDiv
      return Prof::get().fdiv_issueLat;
    case 9: //FloatSqrt
      return Prof::get().fsqrt_issueLat;
    default:
      return 1;
    }
    return 1;
  }

  static unsigned getFUOpLatency(int opclass,Op* op) {
    if(op && op->is_sigmoid()) {
      return 8;
    }

    switch(opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return Prof::get().int_alu_opLat;

    case 2: //IntMult
      return Prof::get().mul_opLat;
    case 3: //IntDiv
      return Prof::get().div_opLat;

    case 4: //FloatAdd
      return Prof::get().fadd_opLat;
    case 5: //FloatCmp
      return Prof::get().fcmp_opLat;
    case 6: //FloatCvt
      return Prof::get().fcvt_opLat;
    case 7: //FloatMult
      return Prof::get().fmul_opLat;
    case 8: //FloatDiv
      return Prof::get().fdiv_opLat;
    case 9: //FloatSqrt
      return Prof::get().fsqrt_opLat;
    default:
      return 1;
    }
    return 1;
  }

  void countOpclassEnergy(int opclass) {
    switch(opclass) {

    case 0: //No_OpClass
      return;

    case 1: //IntALU
      int_ops++;
      return;
    case 2: //IntMult
      mult_ops++;
      return;
    case 3: //IntDiv
      mult_ops++;
      return;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      fp_ops++;
      return;

    default:
      return;
    }
    assert(0);
    return;
  }

  //KNOWN_HOLE: SSE Issue Latency Missing
  //shouldn't matter though, as we always use scalar inputs
  virtual unsigned getFUIssueLatency(Inst_t &n) {
    return getFUIssueLatency(n._opclass,n._op);
  }

  //Check Functional Units to see if they are full
  virtual void checkFuncUnits(std::shared_ptr<BaseInst_t> inst) {
    countOpclassEnergy(inst->_opclass);

    // no func units if load, store, or if it has no opclass
    //if (n._isload || n._isstore || n._opclass==0)
    if (inst->_opclass==0) {
      return;
    }
    
    int fuIndex = fuPoolIdx(inst->_opclass, inst->_op);
    int maxUnits = getNumFUAvailable(inst->_opclass,inst->_op); //opclass
    Inst_t* min_node = static_cast<Inst_t*>(
         addResource(fuIndex, inst->cycleOfStage(inst->beginExecute()), 
                                   getFUIssueLatency(inst->_opclass,inst->_op), 
                                   maxUnits, inst));

    if (min_node) {
      int edge_type = E_FU;
      if(fuIndex==FU_MemPort) {
        edge_type = E_MP;
      }

      getCPDG()->insert_edge(*min_node, min_node->beginExecute(),
                        *inst, inst->beginExecute(), 
                        getFUIssueLatency(min_node->_opclass,min_node->_op),edge_type);
    }
  }

  virtual bool l1dTiming(bool isload, bool isstore, int ex_lat, int st_lat,
                 int& mlat, int& reqDelayT, int& respDelayT, int& mshrT){
    assert(isload || isstore);
    if(isload) {
      mlat = ex_lat;
    } else {
      mlat = st_lat;
    }
    if(mlat <= Prof::get().dcache_hit_latency + 
               Prof::get().dcache_response_latency+3) {
      //We don't need an MSHR for non-missing loads/stores
      return false;
    }

    reqDelayT =Prof::get().dcache_hit_latency; // # cyc before MSHR
    respDelayT=Prof::get().dcache_response_latency; // # cyc MSHR release
    mshrT = mlat - reqDelayT - respDelayT;  // actual MSHR time
    assert(mshrT>0);
    return true;
  }


  virtual void checkMSHRStallLoadCore(std::shared_ptr<Inst_t> n,
      int reqDelayT, int respDelayT, int mshrT) {
#if 0
      int insts_to_squash=instsToSquash();
      int squash_cycles = squashCycles(insts_to_squash)/2;
      int recheck_cycles = squash_cycles;
#else
      int insts_to_squash=std::max(1u,FETCH_TO_DISPATCH_STAGES*FETCH_WIDTH-3);//13
      int recheck_cycles=FETCH_TO_DISPATCH_STAGES*2+1; //9;

#endif

      int rechecks=0;
      uint64_t extraLat=0;

      assert(recheck_cycles>=1); 
      BaseInst_t* min_node =
         addMSHRResource(reqDelayT + n->cycleOfStage(n->beginExecute()), 
              mshrT, n, n->_eff_addr, recheck_cycles, rechecks, extraLat);
      if(rechecks!=0) {
        //create a copy of the instruction to serve as a dummy
        Inst_t* dummy_inst = new Inst_t();
        dummy_inst->copyEvents(n.get());
        dummy_inst->setCumWeights(curWeights());

        //Inst_t* dummy_inst = new Inst_t(*n);
        std::shared_ptr<Inst_t> sh_dummy_inst(dummy_inst);
        
        n->saveInst(sh_dummy_inst);

        //pushPipe(sh_dummy_inst);
        //assert(min_node); 
        if(!min_node) {
        }

        n->reset_inst();
        setFetchCycle(*n);
        /*getCPDG()->insert_edge(*min_node, min_node->memComplete(),
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);*/

        getCPDG()->insert_edge(*dummy_inst, Inst_t::Execute,
          *n, Inst_t::Fetch, rechecks*recheck_cycles, E_MSHR);

        setDispatchCycle(*n);
        setReadyCycle(*n);
        checkRE(*n);

        //std::cout << insts_to_squash << "," << rechecks << "\n";
        //squashed_insts+=rechecks*(insts_to_squash*0.8);
        squashed_insts+=insts_to_squash+(rechecks-1)*insts_to_squash/1.5;

        //we need to make sure that the processor stays active during squash
        uint64_t i=sh_dummy_inst->cycleOfStage(Inst_t::Execute);
        for(;i<n->cycleOfStage(Inst_t::Execute);++i) {
          activityMap.insert(i);
        }

      }
  }

  virtual void checkMSHRStallLoad(std::shared_ptr<BaseInst_t>& n,
      int reqDelayT, int respDelayT, int mshrT) {

      int rechecks=0;
      uint64_t extraLat=0;

      BaseInst_t* min_node =
           addMSHRResource(reqDelayT + n->cycleOfStage(n->beginExecute()), 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
          getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                         *n, n->beginExecute(), mshrT+respDelayT, E_MSHR);
     }
  }

  virtual void checkMSHRStallStore(std::shared_ptr<BaseInst_t>& n,
      int reqDelayT, int respDelayT, int mshrT) {

      int rechecks=0;
      uint64_t extraLat=0;

       BaseInst_t* min_node =
           addMSHRResource(reqDelayT + n->cycleOfStage(n->eventCommit()), 
                           mshrT, n, n->_eff_addr, 1, rechecks, extraLat);
      if(min_node) {
        getCPDG()->insert_edge(*min_node, min_node->memComplete(),
                 *n, n->memComplete(), mshrT+respDelayT, E_MSHR);
      }
  }

  //check MSHRs to see if they are full
  virtual void checkNumMSHRs(std::shared_ptr<BaseInst_t> n) {

    //First do energy accounting
    if(n->_isload || n->_isstore) {
      calcCacheAccess(n.get(), n->_hit_level, n->_miss_level,
                      n->_cache_prod, n->_true_cache_prod,
                      l1_hits, l1_misses, l2_hits, l2_misses,
                      l1_wr_hits, l1_wr_misses, l2_wr_hits, l2_wr_misses);
    }

    int ep_lat=epLat(n->_ex_lat,n.get(),n->_isload,n->_isstore,
                  n->_cache_prod,n->_true_cache_prod,n->isAccelerated);
    int st_lat=stLat(n->_st_lat,n->_cache_prod,n->_true_cache_prod,
                     n->isAccelerated);

    int mlat, reqDelayT, respDelayT, mshrT; //these get filled in below
    if(!l1dTiming(n->_isload,n->_isstore,ep_lat,st_lat,
                  mlat,reqDelayT,respDelayT,mshrT)) {
      return;
    } 

    if(n->_isload) {
      if(n->isPipelineInst()) {
        checkMSHRStallLoadCore(std::static_pointer_cast<Inst_t>(n),
            reqDelayT,respDelayT,mshrT);
      } else {
        checkMSHRStallLoad(n,reqDelayT,respDelayT,mshrT);
      }
    } else { //if store
      checkMSHRStallStore(n,reqDelayT,respDelayT,mshrT);
    }

    return;
  }

  //Ready to Execute Stage -- no delay
  virtual Inst_t &checkRE(Inst_t &n) {
    int len=0;
    if(n._nonSpec) {
      len+=1;
    }
    getCPDG()->insert_edge(n, Inst_t::Ready,
                           n, Inst_t::Execute, len,E_RE);
    return n;
  }

  //Inorder Execute
  virtual Inst_t &checkEE(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    assert(depInst);
    getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                           n, Inst_t::Execute, 0,E_EE);
    return n;
  }

  //Execute Bandwidth (issue width)
  virtual Inst_t &checkInorderIssueWidth(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-ISSUE_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                          n, Inst_t::Execute, 1,E_IBW);
    return n;
  }

  //for inorder only, 
  virtual Inst_t &checkPipelength(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1);
    if(!depInst) {
      return n;
    }
    //getCPDG()->insert_edge(*depInst, Inst_t::Commit,
    //                       n, Inst_t::Execute, -INORDER_EX_DEPTH, E_EPip);
   
    int cycles_between=(int64_t)depInst->cycleOfStage(Inst_t::Commit) -
                       (int64_t)depInst->cycleOfStage(Inst_t::Execute);

    if(cycles_between > (int)INORDER_EX_DEPTH) {
      getCPDG()->insert_edge(*depInst, Inst_t::Execute,
                                    n, Inst_t::Execute, 
                             cycles_between-INORDER_EX_DEPTH, E_EPip);
    }
    return n;
  }


  //Insert Dynamic Edge for constraining issue width, 
  //each inst reserves slots in the issueRes map
  virtual Inst_t &checkIssueWidth(Inst_t &n) {    
    //find an issue slot
    int index = n.cycleOfStage(Inst_t::Execute); //ready to execute cycle
    int orig_index = index;
    while(issueRes[index].size()==ISSUE_WIDTH) {
      index++;
    }

    //slot found in index
    issueRes[index].push_back(&n);

    //don't add edge if not necessary
    if(index == orig_index) {
      return n;
    }

    //otherwise, add a dynamic edge to execute from last inst of previous cycle
    Inst_t* dep_n = issueRes[index-1].back();
    getCPDG()->insert_edge(*dep_n, Inst_t::Execute,
                           n, Inst_t::Execute, 1,E_IBW);
    return n;
  }

  int stLat(int st_lat, bool cache_prod, 
            bool true_cache_prod, bool isAccelerator) {
    int lat = st_lat;
 /*
    if(cache_prod && true_cache_prod) {
      lat = Prof::get().dcache_hit_latency + 
            Prof::get().dcache_response_latency;
    }
 */

    if( (applyMaxLatToAccel && isAccelerator ) ||
        !applyMaxLatToAccel) {
      if(lat > _max_mem_lat) {
        lat = _max_mem_lat;
      }
    }
    return lat;
  }

  /* TODO: Streaming accesses in slower processors may experience less cache
   * access.  Why?  Because its complicated. 
   * We can probably use cache producer information here to help.
   */
  void calcCacheAccess(BaseInst_t* inst, int hit_level, int miss_level,
                       bool cache_prod, bool true_cache_prod,
                       uint64_t& l1_hits,    uint64_t& l1_misses,    
                       uint64_t& l2_hits,    uint64_t& l2_misses,
                       uint64_t& l1_wr_hits, uint64_t& l1_wr_misses, 
                       uint64_t& l2_wr_hits, uint64_t& l2_wr_misses) {
     
     if(inst->_isload) {
       if(miss_level>=1) {
         l1_misses+=1;
       }
       if(miss_level>=2) {
         l2_misses+=1;
       }
       if(hit_level==1) {
         l1_hits+=1;
       }
       if(hit_level==2) {
         l2_hits+=1;
       }
     } else if (inst->_isstore) {
       if(miss_level>=1) {
         l1_wr_misses+=1;
       }
       if(miss_level>=2) {
         l2_wr_misses+=1;
       }
       if(hit_level==1) {
         l1_wr_hits+=1;
       }
       if(hit_level==2) {
         l2_wr_hits+=1;
       }
     } else {
       assert(0);
     }
  }

  //logic to determine ep latency based on information in the trace
  int epLat(int ex_lat, BaseInst_t* inst, bool isload, bool isstore, 
            bool cache_prod, bool true_cache_prod, bool isAccelerator) {
    //memory instructions bear their memory latency here.  If we have a cache
    //producer, that means we should be in the cache, so drop the latency
    int lat;
    if(isload) {
      if(cache_prod && true_cache_prod) {
        lat = std::min(ex_lat,
           Prof::get().dcache_hit_latency + Prof::get().dcache_response_latency);

      } else {
        lat = ex_lat;
      }
    } else {
      //don't want to use this dynamic latency
      //lat = n._ex_lat;
      //lat = getFUOpLatency(opclass);
      lat = getFUOpLatency(inst->_opclass,inst->_op);
    }

    if(_scale_freq) {
      if(isload || isstore) { //if it's a mem
        if(lat > MEMORY_LATENCY) {
          lat = (lat - MEMORY_LATENCY) + (MEMORY_LATENCY * CORE_CLOCK) / QUAD_ISSUE_CORE_CLOCK;
        }
      }
    }

    if(_elide_mem && isAccelerator && isload) {
      if(lat > 1) {
        lat = 1;
      }
    }

    if( (applyMaxLatToAccel && isAccelerator ) ||
        !applyMaxLatToAccel) {
      //check if latencies are bigger than max
      if(isload || isstore) { //if it's a mem
        if(lat > _max_mem_lat) {
          lat = _max_mem_lat;
        }
      } else { //if it's an ex
        if(lat > _max_ex_lat) {
          lat = _max_ex_lat;
        }
      }
    }
    return lat;
  }

  //==========COMPLTE ==============
  //Complete After Execute
  virtual Inst_t &checkEP(Inst_t &n) {
    int lat = epLat(n._ex_lat, &n, n._isload,
                    n._isstore,n._cache_prod,n._true_cache_prod,
                    n.isAccelerated);

    lat = n.adjustExecuteLatency(lat);
    getCPDG()->insert_edge(n, Inst_t::Execute,
                           n, Inst_t::Complete, lat, E_EP);
    return n;
  }

  //issueRes map
  virtual Inst_t &checkWriteBackWidth(Inst_t &n) {    
    //find an issue slot
    uint64_t index = n.cycleOfStage(Inst_t::Complete); //ready to execute cycle
    while(wbBusRes[index].size()==WRITEBACK_WIDTH) {
      index++;
    }

    //slot found in index
    wbBusRes[index].push_back(&n);

    //don't add edge if not necessary
    if(index == n.cycleOfStage(Inst_t::Complete)) {
      return n;
    }

    //otherwise, add a dynamic edge to execute from last inst of previous cycle
    Inst_t* dep_n = wbBusRes[index-1].back();
    getCPDG()->insert_edge(*dep_n, Inst_t::Complete,
                           n, Inst_t::Complete, 1, E_WBBW);
    return n;
  }

  std::unordered_map<Op*,int> dummies_dep;
  std::unordered_map<Op*,int> errors_dep;

  //Cache Line Producer
  virtual void checkPP(BaseInst_t &n, unsigned edge_type=E_PP, BaseInst_t* cdep=NULL) {
    uint64_t cache_prod = n._cache_prod;

    BaseInst_t* depInst=NULL;

    if(cdep) {
      depInst=cdep;
    } else if (cache_prod > 0 && cache_prod < n.index()) {
      depInst=cache_dep_at(n,n.index()-cache_prod);
    }

    if(depInst) {        
      if(depInst->isDummy()) {
        if(!dummies_dep.count(depInst->_op)) {
          std::cerr << "Dummy Dep: "  <<  n._op->dotty_name()
                    << " -> "   << depInst->_op->dotty_name() << "\n";
        }
        dummies_dep[depInst->_op]++;
        return;
      } 

      //this check is here because simd does not keep track of its thigns
      if(depInst->_isload) {
        add_cache_dep(*depInst, n, 1, edge_type);
      }

      /*
      if(depInst._isstore) {
        if(depInst._eff_addr != n._eff_addr) {
          add_cache_dep(depInst, n,0);        
        }
      }*/
    }

    return;
  }

  BaseInst_t* cache_dep_at(BaseInst_t& n, uint64_t index) {
    if(!getCPDG()->hasIdx(index)) {
      if(!errors_dep.count(n._op)) {
        std::cerr << "ERROR: Cache Prod MISSING!: ";
        if(n._op) {
          std::cerr << n._op->dotty_name();
        } else {
          std::cerr <<"no op->id()";
        }
        std::cerr << "\n";
      }
      errors_dep[n._op]++;
      return NULL;
    }

    return &getCPDG()->queryNodes(index);
  }

  virtual void add_cache_dep(BaseInst_t& depInst, BaseInst_t &n, int wait_lat,
      unsigned edge_type) {
    getCPDG()->insert_edge(depInst, depInst.memComplete(),
                           n, n.memComplete(), wait_lat, edge_type);
  }

  //For inorder, commit comes after execute
  virtual Inst_t &checkEC(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Execute,
                           n, Inst_t::Commit, INORDER_EX_DEPTH,E_EPip);
    return n;
  }


  //Commit follows complete
  virtual Inst_t &checkPC(Inst_t &n) {
    getCPDG()->insert_edge(n, Inst_t::Complete,
                           n, Inst_t::Commit, COMMIT_TO_COMPLETE_STAGES,E_PC);  
    //simulator says: minimum two cycles between commit and complete
    return n;
  }

  // heuristic for calculating insts to squash
  int instsToSquash() {
    int insts_to_squash;
    
    insts_to_squash=rob_head_at_dispatch;
    if(rob_growth_rate-0.05>0) {
      insts_to_squash+=12*(rob_growth_rate-0.05);
    }

    return insts_to_squash*0.6f + avg_rob_head*0.4f;
  }
  
  int squashCycles(int insts_to_squash) {
    if(!_isInOrder) {
      return 1 +  insts_to_squash/SQUASH_WIDTH 
               + (insts_to_squash%SQUASH_WIDTH!=0);  
    } else {
      return 0;
    }
  }

  virtual Inst_t &checkSquashPenalty(Inst_t &n) {
    //BR_MISS_PENALTY no longer used
    //Instead we calculate based on the rob size, how long it's going to
    //take to squash.  This is a guess, to some degree, b/c we don't
    //know what happened after the control instruction was executed,
    //but it might be close enough.
    if (n._ctrl_miss) {

        int insts_to_squash=instsToSquash();
        int squash_cycles = squashCycles(insts_to_squash); 
        

        //int squash_cycles = (rob_head_at_dispatch + 1- 12*rob_growth_rate)/SQUASH_WIDTH;

        if(squash_cycles<5) {
          squash_cycles=5;
        } /*else if (squash_cycles > (ROB_SIZE-10)/SQUASH_WIDTH) {
          squash_cycles = (ROB_SIZE-10)/SQUASH_WIDTH;
        }*/

        uint64_t rob_insert_cycle = n.cycleOfStage(Inst_t::Dispatch)-1;
        uint64_t complete_cycle = n.cycleOfStage(Inst_t::Complete);
        uint64_t max_cycles = ((complete_cycle-rob_insert_cycle)*ISSUE_WIDTH*.9)/SQUASH_WIDTH + 2;

        if(squash_cycles > (int)max_cycles) {
          squash_cycles=(int)max_cycles;
        }


        //add two extra cycles, because commit probably has to play catch up
        //because it can't commit while squashing
        getCPDG()->insert_edge(n, Inst_t::Complete,
                               n, Inst_t::Commit,squash_cycles+1,E_SQUA);
        
        prev_squash_penalty=squash_cycles;
    }
    return n;
  }

  //in order commit
  virtual Inst_t &checkCC(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-1); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                           n, Inst_t::Commit, 0,E_CC);
    return n;
  }

  //Finite commit width
  virtual Inst_t &checkCBW(Inst_t &n) {
    Inst_t* depInst = getCPDG()->peekPipe(-COMMIT_WIDTH); 
    if(!depInst) {
      return n;
    }
    getCPDG()->insert_edge(*depInst, Inst_t::Commit,
                          n, Inst_t::Commit, 1,E_CBW);
    return n;
  }

  virtual void printAccEnergyEvents() {
    std::cout << "committed_insts_acc        " <<  committed_insts_acc        << "\n";
    std::cout << "committed_int_insts_acc    " <<  committed_int_insts_acc    << "\n";
    std::cout << "committed_fp_insts_acc     " <<  committed_fp_insts_acc     << "\n";
    std::cout << "committed_fp_insts_acc     " <<  committed_fp_insts_acc     << "\n";
    std::cout << "committed_branch_insts_acc " <<  committed_branch_insts_acc << "\n";
    std::cout << "committed_branch_insts_acc " <<  committed_branch_insts_acc << "\n";
    std::cout << "committed_load_insts_acc   " <<  committed_load_insts_acc   << "\n";
    std::cout << "committed_store_insts_acc  " <<  committed_store_insts_acc  << "\n";
    std::cout << "squashed_insts_acc         " <<  squashed_insts_acc         << "\n";
    std::cout << "idlecycles_acc             " <<  idleCycles_acc             << "\n";
    std::cout << "rob_reads_acc              " <<  rob_reads_acc              << "\n";
    std::cout << "rob_writes_acc             " <<  rob_writes_acc             << "\n";
    std::cout << "rename_reads_acc           " <<  rename_reads_acc           << "\n";
    std::cout << "rename_writes_acc          " <<  rename_writes_acc          << "\n";
    std::cout << "rename_freads_acc          " <<  rename_freads_acc          << "\n";
    std::cout << "rename_fwrites_acc         " <<  rename_fwrites_acc         << "\n";
    std::cout << "func_calls_acc             " <<  func_calls_acc             << "\n";
    std::cout << "int_ops_acc                " <<  int_ops_acc                << "\n";
    std::cout << "fp_ops_acc                 " <<  fp_ops_acc                 << "\n";
    std::cout << "mult_ops_acc               " <<  mult_ops_acc               << "\n";
    std::cout << "iw_reads_acc               " <<  iw_reads_acc               << "\n";
    std::cout << "iw_writes_acc              " <<  iw_writes_acc              << "\n";
    std::cout << "iw_freads_acc              " <<  iw_freads_acc              << "\n";
    std::cout << "iw_fwrites_acc             " <<  iw_fwrites_acc             << "\n";
    std::cout << "regfile_reads_acc          " <<  regfile_reads_acc          << "\n";
    std::cout << "regfile_writes_acc         " <<  regfile_writes_acc         << "\n";
    std::cout << "regfile_freads_acc         " <<  regfile_freads_acc         << "\n";
    std::cout << "regfile_fwrites_acc        " <<  regfile_fwrites_acc        << "\n";
    std::cout << "icache_read_accesses_acc   " <<  icache_read_accesses_acc   << "\n";
    std::cout << "icache_read_misses_acc     " <<  icache_read_misses_acc     << "\n";
    std::cout << "l1_hits_acc                " <<  l1_hits_acc                << "\n";
    std::cout << "l1_misses_acc              " <<  l1_misses_acc              << "\n";
    std::cout << "l1_wr_hits_acc             " <<  l1_wr_hits_acc             << "\n";
    std::cout << "l1_wr_misses_acc           " <<  l1_wr_misses_acc           << "\n";
    std::cout << "l2_hits_acc                " <<  l2_hits_acc                << "\n";
    std::cout << "l2_misses_acc              " <<  l2_misses_acc              << "\n";
    std::cout << "l2_wr_hits_acc             " <<  l2_wr_hits_acc             << "\n";
    std::cout << "l2_wr_misses_acc           " <<  l2_wr_misses_acc           << "\n";
  }

  virtual void accCoreEnergyEvents() {
    committed_insts_acc        += committed_insts;              
    committed_int_insts_acc    += committed_int_insts;
    committed_fp_insts_acc     += committed_fp_insts;
    committed_fp_insts_acc     += committed_fp_insts;
    committed_branch_insts_acc += committed_branch_insts;
    committed_branch_insts_acc += committed_branch_insts;
    committed_load_insts_acc   += committed_load_insts;
    committed_store_insts_acc  += committed_store_insts;
    squashed_insts_acc         += squashed_insts;
    idleCycles_acc             += idleCycles;
    rob_reads_acc              += rob_reads;
    rob_writes_acc             += rob_writes;
    rename_reads_acc           += rename_reads;
    rename_writes_acc          += rename_writes;
    rename_freads_acc          += rename_freads;
    rename_fwrites_acc         += rename_fwrites;
    func_calls_acc             += func_calls;
    int_ops_acc                += int_ops;
    fp_ops_acc                 += fp_ops;
    mult_ops_acc               += mult_ops;
    iw_reads_acc               += iw_reads;
    iw_writes_acc              += iw_writes;
    iw_freads_acc              += iw_freads;
    iw_fwrites_acc             += iw_fwrites;
    regfile_reads_acc          += regfile_reads;
    regfile_writes_acc         += regfile_writes;
    regfile_freads_acc         += regfile_freads;
    regfile_fwrites_acc        += regfile_fwrites;
    icache_read_accesses_acc   += icache_read_accesses;
    icache_read_misses_acc     += icache_read_misses;
    l1_hits_acc                += l1_hits;
    l1_misses_acc              += l1_misses;
    l1_wr_hits_acc             += l1_wr_hits;
    l1_wr_misses_acc           += l1_wr_misses;
    l2_hits_acc                += l2_hits;
    l2_misses_acc              += l2_misses;
    l2_wr_hits_acc             += l2_wr_hits;
    l2_wr_misses_acc           += l2_wr_misses;

    committed_insts = 0;
    committed_int_insts = 0;
    committed_fp_insts = 0;
    committed_fp_insts =0;
    committed_branch_insts = 0;
    committed_branch_insts = 0;
    committed_load_insts = 0;
    committed_store_insts = 0;
    squashed_insts=0;
    idleCycles = 0;
    rob_reads = 0;
    rob_writes = 0;
    rename_reads = 0;
    rename_writes = 0;
    rename_freads = 0;
    rename_fwrites = 0;
    func_calls = 0;
    int_ops = 0;
    fp_ops = 0;
    mult_ops = 0;
    iw_reads = 0;
    iw_writes = 0;
    iw_freads = 0;
    iw_fwrites = 0;
    regfile_reads = 0;
    regfile_writes = 0;
    regfile_freads = 0;
    regfile_fwrites = 0;
    icache_read_accesses = 0;
    icache_read_misses = 0;
    l1_hits = 0;
    l1_misses = 0;
    l1_wr_hits = 0;
    l1_wr_misses = 0;
    l2_hits = 0;
    l2_misses = 0;
    l2_wr_hits = 0;
    l2_wr_misses = 0;
  }

  virtual void pumpCoreEnergyEvents(PrismProcessor* proc, uint64_t totalCycles) {
    system_core* core = &proc->XML->sys.core[0];

    uint64_t busyCycles;
    if(idleCycles < totalCycles) {
      busyCycles=totalCycles-idleCycles;
    } else {
      busyCycles=0;
    }

    //Modify relevent events to account for squashing
    double squashRatio=0;
    if(committed_insts!=0) {
      squashRatio =(double)squashed_insts/(double)committed_insts;
    }
    double highSpecFactor = 1.00+1.5*squashRatio;
    double specFactor = 1.00+squashRatio;
    double halfSpecFactor = 1.00+0.5*squashRatio;
    double fourthSpecFactor = 1.00+0.25*squashRatio;
    //double eigthSpecFactor = 1.00+0.125*squashRatio;
    double sixteenthSpecFactor = 1.00+0.0625*squashRatio;

    uint64_t calc_rob_reads=(rob_reads+busyCycles)*halfSpecFactor;

    core->ROB_reads = calc_rob_reads; //(calc_rob_reads+busyCycles)*halfSpecFactor;

    proc->XML->sys.total_cycles=totalCycles;

    core->total_cycles=totalCycles; 
    core->idle_cycles=idleCycles; 
    core->busy_cycles=busyCycles; 

    core->total_instructions    = (uint64_t)(committed_insts*specFactor); 
    core->int_instructions      = (uint64_t)(committed_int_insts*specFactor); 
    core->fp_instructions       = (uint64_t)(committed_fp_insts*specFactor); 
    core->branch_instructions   = (uint64_t)(committed_branch_insts*highSpecFactor); 
    core->branch_mispredictions = (uint64_t)(committed_branch_insts*sixteenthSpecFactor);

    core->load_instructions     = (uint64_t)(committed_load_insts*fourthSpecFactor);
    core->store_instructions    = (uint64_t)(committed_store_insts*fourthSpecFactor);

    core->committed_instructions     = committed_insts;
    core->committed_int_instructions = committed_int_insts;
    core->committed_fp_instructions  = committed_fp_insts;

    core->ROB_writes       = (uint64_t)(rob_writes*specFactor+squashed_insts);
    core->rename_reads     = (uint64_t)(rename_reads*specFactor);
    core->rename_writes    = (uint64_t)(rename_writes*specFactor+squashed_insts);
    core->fp_rename_reads  = (uint64_t)(rename_freads*specFactor);
    core->fp_rename_writes = (uint64_t)(rename_fwrites*specFactor);

    core->inst_window_reads  = (uint64_t)(iw_reads*specFactor+busyCycles);
    core->inst_window_writes = (uint64_t)(iw_writes*specFactor+squashed_insts);
    core->inst_window_wakeup_accesses = (uint64_t)(iw_writes*specFactor);

    core->fp_inst_window_reads  = (uint64_t)(iw_freads*specFactor);
    core->fp_inst_window_writes = (uint64_t)(iw_fwrites*specFactor);
    core->fp_inst_window_wakeup_accesses = (uint64_t)(iw_fwrites*specFactor);

    core->int_regfile_reads    = (uint64_t)(regfile_reads*halfSpecFactor);
    core->int_regfile_writes   = (uint64_t)(regfile_writes*specFactor);
    core->float_regfile_reads  = (uint64_t)(regfile_freads*halfSpecFactor);
    core->float_regfile_writes = (uint64_t)(regfile_fwrites*specFactor);

    core->function_calls   = (uint64_t)(func_calls*specFactor);

    core->ialu_accesses    = (uint64_t)(int_ops*specFactor);
    core->fpu_accesses     = (uint64_t)(fp_ops*specFactor);
    core->mul_accesses     = (uint64_t)(mult_ops*specFactor);

    core->cdb_alu_accesses = (uint64_t)(int_ops*specFactor);
    core->cdb_fpu_accesses = (uint64_t)(fp_ops*specFactor);
    core->cdb_mul_accesses = (uint64_t)(mult_ops*specFactor);

    //icache
    core->icache.read_accesses = icache_read_accesses;
    core->icache.read_misses    = icache_read_misses;

    //dcache
    core->dcache.read_accesses   = l1_hits + l1_misses;
    core->dcache.read_misses     = l1_misses;
    core->dcache.write_accesses  = l1_wr_hits + l1_wr_misses;
    core->dcache.write_misses    = l1_wr_misses;

    //TODO: TLB Energy Modeling
    core->itlb.total_accesses=icache_read_accesses;
    core->itlb.total_misses=0;
    core->itlb.total_hits=0;
    core->dtlb.total_accesses=l1_hits+l1_misses+l1_wr_hits+l1_wr_misses;
    core->dtlb.total_misses=0;
    core->dtlb.total_hits=0;
    
    double pipe_duty=0.01;
    if(totalCycles>0) {
      pipe_duty=std::max(std::min(((committed_insts*specFactor)/totalCycles)*3.0/2.0/ISSUE_WIDTH,2.0),0.01);
    }

    proc->cores[0]->coredynp.pipeline_duty_cycle=pipe_duty;

    //l2
    system_L2* l2 = &proc->XML->sys.L2[0];
    l2->read_accesses   = l2_hits + l2_misses;
    l2->read_misses     = l2_misses;
    l2->write_accesses  = l2_wr_hits + l2_wr_misses;
    l2->write_misses    = l2_wr_misses;

    /* 
    //Debug
    cout << "l1a:  " << core->dcache.read_accesses  << "\n";
    cout << "l1m:  " << core->dcache.read_misses    << "\n";  
    cout << "l1wa: " << core->dcache.write_accesses  << "\n"; 
    cout << "l1wm: " << core->dcache.write_misses    << "\n"; 
    cout << "l2a:  " << l2->read_accesses           << "\n"; 
    cout << "l2m:  " << l2->read_misses             << "\n"; 
    cout << "l2wa: " << l2->write_accesses           << "\n"; 
    cout << "l2wm: " << l2->write_misses             << "\n";  
    */

    accCoreEnergyEvents();
  }

  // Handle enrgy events for McPAT XML DOC
  virtual void setEnergyEvents(pugi::xml_document& doc, int nm) {
    //set the normal events based on the m5out/stats file
    CriticalPath::setEnergyEvents(doc,nm);

    uint64_t totalCycles=numCycles();
    uint64_t busyCycles=totalCycles-idleCycles_acc;

    pugi::xml_node system_node = doc.child("component").find_child_by_attribute("name","system");
    pugi::xml_node core_node =
              system_node.find_child_by_attribute("name","core0");

    sa(system_node,"total_cycles",totalCycles);
    sa(system_node,"idle_cycles", idleCycles_acc);
    sa(system_node,"busy_cycles",busyCycles);

    //std::cout << "squash: " << squashed_insts
    //          << "commit: " << committed_insts << "\n";

    //Modify relevent events to be what we predicted
    double squashRatio=0;
    if(committed_insts_acc!=0) {
      squashRatio =(double)squashed_insts_acc/(double)committed_insts_acc;
    }

    if(!_isInOrder) {
      std::cout << "Squash Ratio for \"" << _name << "\" is " << squashRatio << "\n";
    }
    double highSpecFactor = 1.00+1.5*squashRatio;
    double specFactor = 1.00+squashRatio;
    double halfSpecFactor = 1.00+0.5*squashRatio;
    double fourthSpecFactor = 1.00+0.25*squashRatio;
    //double eigthSpecFactor = 1.00+0.125*squashRatio;
    double sixteenthSpecFactor = 1.00+0.0625*squashRatio;

    //uint64_t intOps=committed_int_insts-committed_load_insts-committed_store_insts;
    //uint64_t intOps=committed_int_insts;

    sa(core_node,"total_instructions",(uint64_t)(committed_insts_acc*specFactor));
    sa(core_node,"int_instructions",(uint64_t)(committed_int_insts_acc*specFactor));
    sa(core_node,"fp_instructions",(uint64_t)(committed_fp_insts_acc*specFactor));
    sa(core_node,"branch_instructions",(uint64_t)(committed_branch_insts_acc*highSpecFactor));
    sa(core_node,"branch_mispredictions",(uint64_t)(mispeculatedInstructions_acc*sixteenthSpecFactor));
    sa(core_node,"load_instructions",(uint64_t)(committed_load_insts_acc*fourthSpecFactor));
    sa(core_node,"store_instructions",(uint64_t)(committed_store_insts_acc*fourthSpecFactor));

    sa(core_node,"committed_instructions", committed_insts_acc);
    sa(core_node,"committed_int_instructions", committed_int_insts_acc);
    sa(core_node,"committed_fp_instructions", committed_fp_insts_acc);

    sa(core_node,"total_cycles",totalCycles);
    sa(core_node,"idle_cycles", idleCycles_acc); 
    sa(core_node,"busy_cycles",busyCycles);

    sa(core_node,"ROB_reads",(uint64_t)((rob_reads_acc+busyCycles)*halfSpecFactor));
    sa(core_node,"ROB_writes",(uint64_t)(rob_writes_acc*specFactor)+squashed_insts);

    sa(core_node,"rename_reads",(uint64_t)(rename_reads_acc*specFactor));
    sa(core_node,"rename_writes",(uint64_t)(rename_writes_acc*specFactor)+squashed_insts);
    sa(core_node,"fp_rename_reads",(uint64_t)(rename_freads_acc*specFactor));
    sa(core_node,"fp_rename_writes",(uint64_t)(rename_fwrites_acc*specFactor));
                                                                    
    sa(core_node,"inst_window_reads",(uint64_t)(iw_reads_acc*specFactor)+busyCycles);
    sa(core_node,"inst_window_writes",(uint64_t)(iw_writes_acc*specFactor)+squashed_insts);
    sa(core_node,"inst_window_wakeup_accesses",(uint64_t)(iw_writes_acc*specFactor));

    sa(core_node,"fp_inst_window_reads",(uint64_t)(iw_freads_acc*specFactor));
    sa(core_node,"fp_inst_window_writes",(uint64_t)(iw_fwrites_acc*specFactor));
    sa(core_node,"fp_inst_window_wakeup_accesses",(uint64_t)(iw_fwrites_acc*specFactor));

    sa(core_node,"int_regfile_reads",(uint64_t)(regfile_reads_acc*halfSpecFactor));
    sa(core_node,"int_regfile_writes",(uint64_t)(regfile_writes_acc*specFactor));
    sa(core_node,"float_regfile_reads",(uint64_t)(regfile_freads_acc*halfSpecFactor));
    sa(core_node,"float_regfile_writes",(uint64_t)(regfile_fwrites_acc*specFactor));

    sa(core_node,"function_calls",(uint64_t)(func_calls_acc*specFactor));

    sa(core_node,"ialu_accesses",(uint64_t)(int_ops_acc*specFactor));
    sa(core_node,"fpu_accesses",(uint64_t)(fp_ops_acc*specFactor));
    sa(core_node,"mul_accesses",(uint64_t)(mult_ops_acc*specFactor));

    sa(core_node,"cdb_alu_accesses",(uint64_t)(int_ops_acc*specFactor));
    sa(core_node,"cdb_fpu_accesses",(uint64_t)(fp_ops_acc*specFactor));
    sa(core_node,"cdb_mul_accesses",(uint64_t)(mult_ops_acc*specFactor));

    // ---------- icache --------------
    pugi::xml_node icache_node =
              core_node.find_child_by_attribute("name","icache");
    sa(icache_node,"read_accesses",icache_read_accesses_acc);
    sa(icache_node,"read_misses",icache_read_misses_acc);
    //sa(icache_node,"conflicts",Prof::get().icacheReplacements);
  }

//    if(op->isFloating()) {
//      fp_ops++;
//    } else if (op->opclass()==2) {
//      int_ops++; //TODO:FIXME: DELETE
//      mult_ops++;
//    } else {
//      int_ops++; 
//    }

  virtual void countOpEnergy(Op*op,
      uint64_t& fp_ops, uint64_t& mult_ops, uint64_t& int_ops) {
    if(op->isCtrl()) {
      return;
    } 

    switch(op->opclass()) {
      case 0: //No_OpClass
        return;
      case 1: //IntALU
        int_ops++;
        return;
      case 2: //IntMult
        mult_ops++;
        return;
      case 3: //IntDiv
        mult_ops++;
        return;
  
      case 4: //FloatAdd
      case 5: //FloatCmp
      case 6: //FloatCvt
      case 7: //FloatMult
      case 8: //FloatDiv
      case 9: //FloatSqrt
        fp_ops++;
        return;
  
      default:
        return;
    }
    assert(0);
    //TODO: Div operations?
  }

  virtual void countAccelSGRegEnergy(Op* op, Subgraph* sg, std::set<Op*>& opset,
      uint64_t& fp_ops, uint64_t& mult_ops, uint64_t& int_ops,
      uint64_t& regfile_reads, uint64_t& regfile_freads,
      uint64_t& regfile_writes, uint64_t& regfile_fwrites) {

    countOpEnergy(op, fp_ops,mult_ops,int_ops);

    for(auto di = op->adj_d_begin(),de = op->adj_d_end();di!=de;++di) {
      Op* dop = *di;
      if(opset.count(dop) /*&& li->forwardDep(dop,op)*/) {
        //std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
        if(!sg->hasOp(dop)) {
          if(dop->isFloating()) {
            regfile_freads+=1;
           } else {
            regfile_reads+=1;
          }
        }
      }
    }
  
    for(auto ui = op->u_begin(),ue = op->u_end();ui!=ue;++ui) {
      Op* uop = *ui;
      if(opset.count(uop) /*&& li->forwardDep(op,uop)*/) {
        //std::shared_ptr<BeretInst> dep_BeretInst = binstMap[dop];
        if(!sg->hasOp(uop)) {
          if(uop->isFloating()) {
            regfile_fwrites+=1;
           } else {
            regfile_writes+=1;
          }
          break;
        }
      }
    }
  }


};


#endif

