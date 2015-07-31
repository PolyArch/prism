#ifndef CP_OPDG_BUILDER_HH
#define CP_OPDG_BUILDER_HH

#include "cp_dg_builder.hh"
#include "exec_profile.hh"

static unsigned floor_to_pow2(unsigned num)
{
  if (num & (num-1)) {
    num = num - 1;
    num |= (num >> 1);
    num |= (num >> 2);
    num |= (num >> 4);
    num |= (num >> 8);
    num |= (num >> 16);
    num += 1;
    num >>= 1;
  }
  return num;
}

// Common DySER & SIMD Problems:
// 1. These don't preserve their entire graph inside of the cpdg.  This means
// that the critical dependence graph calculations won't quite be correct
// that's okay, though, we'll still get the cycles right, just not criticallity


namespace DEBUG_CPDG {
  class default_cpdg_t : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>>
  {
public:
    bool _dump_inst_flag = false;

    default_cpdg_t() {
      _isInOrder = false;
    }
    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;

    dep_graph_t<Inst_t, T, E> *getCPDG()
      {
        return &cpdg;
      }

    dep_graph_impl_t<Inst_t, T, E> cpdg;

    virtual void pushPipe(InstPtr &inst) {
      CP_DG_Builder<T, E>::pushPipe(inst);
      if (_dump_inst_flag)
        dumpInst(inst);
    }
  };
}

template<typename T, typename E>
class CP_OPDG_Builder : public CP_DG_Builder<T, E> {
  public:
  typedef dg_inst<T, E> Inst_t;
  typedef dg_inst_base<T, E> BaseInst_t;

  typedef std::shared_ptr<Inst_t> InstPtr;
  typedef std::shared_ptr<BaseInst_t> BaseInstPtr;

protected:
  bool  usePipeDeps = false;
  bool  useOpDeps   = false;
  bool  useInstList = false;

  virtual bool usePipeDependence() const { return usePipeDeps; }
  virtual bool useOpDependence() const   { return useOpDeps; }
  virtual bool useInstListDependence() const { return useInstList;}

  virtual void addPipeDeps(InstPtr &n, Op *op) {
    usePipeDeps = true;
    this->addDeps(n, op);
    usePipeDeps = false;
  }

  virtual void addInstListDeps(InstPtr &n, Op *op) {
    useInstList = true;
    this->addDeps(n, op);
    useInstList = false;
  }

  virtual void addInstToProducerList(InstPtr n) {
    _producerInsts.push_back(n);
  }

public:
  virtual void printDisasm(Op *op) {
    std::cout << "<" << op << ">: ";
    this->printDisasmPC(op->cpc().first, (int)op->cpc().second);
  }

public:
  CP_OPDG_Builder() : CP_DG_Builder<T, E> () {}
  virtual ~CP_OPDG_Builder() {}

  void pushPipe(InstPtr &inst) {
    CP_DG_Builder<T, E>::pushPipe(inst);
    _lastInst = inst;
    if (getenv("DUMP_MAFIA_PIPE"))
      this->dumpInst(inst);
  }

  //THIS DOES NOTHING
  virtual void add_inst_checked(std::shared_ptr<Inst_t>& inst) {
    //TODO:FIXME -- this project is abandoned for now
    //It got replaced with a different strategy.  See ITERFIX

    //this->cpdgAddInstChecked(inst, inst->_index);  
  }

  void insert_inst(const CP_NodeDiskImage &img,
                   uint64_t index, Op* op) {
    InstPtr sh_inst = this->createInst(img, index, op, false);
    sh_inst->_type=100;
    this->cpdgAddInst(sh_inst, index);
    this->addDeps(sh_inst, op);
    this->pushPipe(sh_inst);
    this->inserted(sh_inst);
  }

  std::string   long_op_name(Op* op) {
    if(!op) {
      return "NULLOP";
    }
    std::stringstream ss;
    ss << op->id() << "_" << op->getUOPName();
    if(op->bb()) {
      ss<< "_BB" << op->bb()->rpoNum();
    }
    if(op->func()) {
      ss<< "_" << op->func()->nice_name();
    }
    return ss.str();
  }

//  virtual void addDeps(std::shared_ptr<Inst_t>& inst, Op* op = NULL) {
//    std::cout << "Add deps " << inst->index() << ",t:" << (int)inst->_type 
//                             << ": " << long_op_name(op) << " ptr: " << &* inst<< "\n";
//    CP_DG_Builder<T,E>::addDeps(inst,op);
//  }


  void cpdgAddInst(std::shared_ptr<dg_inst_base<T,E>> dg, uint64_t index) {
    //std::cout << "index ---- " << index << " " << "type:" << (int)dg->_type 
    //          << "  ptr:" << &*dg << " dg->_index: " << dg->_index;
    //if(dg->_op) {
    //  cout  << " (" << long_op_name(dg->_op) << ")";
    //}
    //std::cout << "\n";

    getCPDG()->addInst(dg,index);
  }


  std::vector<InstPtr> _producerInsts;

  virtual dep_graph_t<Inst_t, T, E> *getCPDG() = 0;

  virtual Inst_t &checkRegisterDependence(Inst_t &n) {
    if (!(useOpDependence() || usePipeDependence()
          || useInstListDependence()
          || n.hasOperandInsts()))
      return CP_DG_Builder<T, E>::checkRegisterDependence(n);

    if (usePipeDependence()) { //Tony:  HIGHLY QUESTIONABLE!
      const int NumProducer = MAX_SRC_REGS; // X86 dep
      for (int i = 0; i < NumProducer; ++i) {
        unsigned prod = n._prod[i];
        Inst_t *inst = getCPDG()->peekPipe(-prod);
        if (!inst)
          continue;
        getCPDG()->insert_edge(*inst, inst->eventComplete(),
                               n, Inst_t::Ready, 0, E_RDep);
      }
      return n;
    }

    if (useInstListDependence()) {
      for (auto I = _producerInsts.begin(), IE = _producerInsts.end();
           I != IE; ++I) {
        InstPtr inst = *I;
        if (inst.get() == 0)
          continue;
        getCPDG()->insert_edge(*inst.get(), inst->eventComplete(),
                               n, Inst_t::Ready, 0, E_RDep);
      }
      _producerInsts.clear();
      return n;
    }

    //instructions for operand are stored directly
    //these are used generally with the loop inst trace -- why?
    if (n.hasOperandInsts()) {
//      std::cout << "operand insts:" << n._op->id() << " " << n._op->getUOPName() << " " << n._index << ":";
      for (auto I = n.op_begin(), IE = n.op_end(); I != IE; ++I) {
        auto inst = *I;
        getCPDG()->insert_edge(*inst, inst->eventComplete(),
                               n, Inst_t::Ready, 0, E_RDep);
//        std::cout << " " << inst->_index << " -- cc:" << inst->cycleOfStage(Inst_t::Complete) <<"\n";


      }

/*      std::cout << "\n     would have had: ";
      const int NumProducer = MAX_SRC_REGS; // FIXME: X86 specific
      for (int i = 0; i < NumProducer; ++i) {
        unsigned prod = n._prod[i];
        if (prod <= 0 || prod >= n.index()) {
          continue;
        }
        BaseInst_t& depInst = getCPDG()->queryNodes(n.index()-prod);
        std::cout << " " << depInst.index() << " -- cc:" << depInst.cycleOfStage(Inst_t::Complete) <<"\n";
      }*/
      return n;
    }

    //This is generally used for LoopInfo based insts
    Op *op = this->getOpForInst(n);
    if (!op) {
      assert(n.hasDisasm() && n.getDisasm() == "dyser_config");
      return n;
    }

    for (auto I = op->d_begin(), IE = op->d_end(); I != IE; ++I) {
      Op *DepOp = *I;
      BaseInstPtr depInst = this->getInstForOp(DepOp);
      assert(&n!=&(*depInst));
      if (!depInst.get()) {
        continue;
      }
      getCPDG()->insert_edge(*depInst, depInst->eventComplete(),
                             n, Inst_t::Ready, 0, E_RDep);
    }

    return n;
  }

  // Override DataDependence Check
  virtual void checkMemoryDependence(Inst_t &n) {
    if (!useOpDependence() && !n.hasMemOperandInsts())
      return CP_DG_Builder<T, E>::checkMemoryDependence(n);

    //Tony: When does this happen?
    if (n.hasMemOperandInsts()) {
      for (auto I = n.mem_op_begin(), IE = n.mem_op_end(); I != IE; ++I) {
        std::shared_ptr<dg_inst_base<T, E> > inst = *I;
        Inst_t *depInst = dynamic_cast<Inst_t*>(inst.get());
        //this->insert_mem_dep_edge(*depInst, n);
        this->addTrueMemDep(*depInst,n); //this will keep track of dependent edges

      }
      return;
    }

    //Tony: Is this super conservative, b/c you are adding any potential mem dep?
    //Probably won't matter in practice, but still...
    Op *op = this->getOpForInst(n);

    if (!op) {
      assert(n.hasDisasm() && n.getDisasm() == "dyser_config");
      return;
    }


    for (auto I = op->m_begin(), IE = op->m_end(); I != IE; ++I) {
      Op *DepOp = *I;
      BaseInstPtr depInst = this->getInstForOp(DepOp);

      BaseInst_t *depInstPtr = depInst.get();
      if (!depInstPtr)
        continue;
      //this->insert_mem_dep_edge(*depInstPtr, n);
      this->addTrueMemDep(*depInstPtr,n);
    }
  }

protected:
  InstPtr _lastInst = 0;


  template <class _F, class _S, class _T>
  struct _InstInfo {
    _F first;
    _S second;
    _T third;
    _InstInfo(_F &f, _S &s, const _T &t):
      first(f), second(s), third(t)
    {}
  };

  std::list< _InstInfo<Op*, InstPtr, CP_NodeDiskImage> > _loop_InstTrace;
  std::unordered_map<Op*, uint16_t> _cacheLat;
  std::unordered_map<Op*, uint16_t> _icacheLat;
  //std::map<Op*, int> _cacheProd;
  std::unordered_map<Op*, bool> _trueCacheProd;
  std::unordered_map<Op*, bool> _ctrlMiss;
  //std::unordered_map<Op*, uint64_t> _latestLoopIdx; //Todo: make this more robust
  std::unordered_map<Op*, std::pair<uint8_t,uint8_t>> _cacheHitLevel;

  virtual void trackLoopInsts(LoopInfo *li, Op *op, InstPtr inst,
                              const CP_NodeDiskImage &img) {
    _loop_InstTrace.push_back(_InstInfo<Op*, InstPtr, CP_NodeDiskImage>(op,
                                                                        inst,
                                                                        img));
    //_latestLoopIdx[op]=inst->_index; 
    aggrLoopInstsStat(op, inst);
  }

  virtual void aggrLoopInstsStat(Op *op, InstPtr inst) {
    if (op->isLoad() || op->isStore()) {
      _cacheLat[op] = std::max( (op->isLoad() ? inst->_ex_lat : inst->_st_lat),
                                _cacheLat[op]);
      if (_trueCacheProd.count(op))
        _trueCacheProd[op] &= inst->_true_cache_prod;
      else
        _trueCacheProd[op] = inst->_true_cache_prod;

      _cacheHitLevel[op]=std::make_pair(
          std::max(_cacheHitLevel[op].first, inst->_hit_level),
          std::max(_cacheHitLevel[op].second,inst->_miss_level));
      // TODO: do something for _cache_prod
    }

    _icacheLat[op] = std::max(_icacheLat[op],inst->_icache_lat);

    if (op->isCtrl()) {
      // if one missed, simd misses.
      // Tony: why shouldn't there be no control misses for simd, b/c it
      // hyperblocks everything?
      if (_ctrlMiss.count(op))
        _ctrlMiss[op] |= inst->_ctrl_miss;
      else
        _ctrlMiss[op] = inst->_ctrl_miss;
    }

    // cache the op in the insts -- why? (tony)
    // TODO:FIXME:UNSURE:WARNING -- does this affect anything?
   
   /* 
    {
      const int NumProducer = MAX_SRC_REGS;
      for (int i = 0; i < NumProducer; ++i) {
        unsigned prod = inst->_prod[i];
        if (prod <= 0 || prod >= inst->index())
          continue;
        inst->addOperandInst(getCPDG()->queryInst(inst->index() - prod));
      }
      if (inst->_mem_prod > 0 && inst->_mem_prod < inst->index()) {
        inst->addMemOperandInst(getCPDG()->queryInst(inst->index()
                                                     - inst->_mem_prod));
      }
    }
    */

  }

  virtual void updateInstWithTraceInfo(Op *op, InstPtr inst,
                                       bool useInst) {

    uint16_t inst_ilat = useInst ? inst->_icache_lat : 0;
    inst->_icache_lat = std::max(inst_ilat, _icacheLat[op]);

    if (op->isLoad()) {
      uint16_t inst_lat = useInst ? inst->_ex_lat : 0;
      inst->_ex_lat = std::max(inst_lat, _cacheLat[op]);
    }
    if (op->isStore()) {
      uint16_t inst_lat = useInst ? inst->_st_lat : 0;
      inst->_st_lat = std::max(inst_lat, _cacheLat[op]);
    }

    if (op->isLoad() || op->isStore()) {
      if (useInst) {
        inst->_true_cache_prod &= _trueCacheProd[op];
      } else {
        inst->_true_cache_prod = _trueCacheProd[op];
      }
    }
    
    inst->_hit_level = _cacheHitLevel[op].first;
    inst->_miss_level = _cacheHitLevel[op].second;

    if (op->isCtrl()) {
      if (useInst)
        inst->_ctrl_miss |= _ctrlMiss[op];
      else
        inst->_ctrl_miss = _ctrlMiss[op];
    }


    //if(inst->_index==0) {
    //  inst->_index=_latestLoopIdx[op];
    //}

  }

  //ITERFIX: find the instruction associated with op,
  //then place it in the cpdg index
  virtual void fix_cpdg_inst_map(uint64_t index, Op* op) {
    BaseInstPtr inst = this->getInstForOp(op);
    if(inst) {
      //std::cout << index << " fix\n";
      //this->cpdgAddInstChecked(inst, index);
      if(inst->index()==0) {
        inst->_index=index;
      }
      this->cpdgAddInst(inst, index);
    } else {
      //I guess some instruction got skipped?
//      std::cerr << "no entry for: " << op->id() <<
//                  " " << index << " " << op->getUOPName();
//      std::cerr<<"\n";

       //  ExecProfile::getDisasm(op->cpc().first,op->cpc().second);
        
    }
  }

  virtual void cleanupLoopInstTracking() {
    for (auto I = _loop_InstTrace.begin(), IE = _loop_InstTrace.end();
         I != IE; ++I) {
      InstPtr inst = I->second;
      inst->clearOperandInsts();

      Op *op = I->first;
      fix_cpdg_inst_map(inst->_index, op);
    }
    _loop_InstTrace.clear();
    _icacheLat.clear();
    _cacheLat.clear();
    _trueCacheProd.clear();
    _ctrlMiss.clear();
    _cacheHitLevel.clear();
    //_latestLoopIdx.clear();
  }

  virtual void cleanupLoopInstTracking(LoopInfo *li, unsigned iter) {
    int numIter = -1;

    // clear only delete iter worth of instructions.
    while (!_loop_InstTrace.empty()) {
      auto Entry = _loop_InstTrace.front();
      Op *op = Entry.first;
      if (op->bb_pos() == 0 && li->loop_head() == op->bb())
        ++ numIter;
      if ((unsigned)numIter  == iter)
        return;
      InstPtr inst = Entry.second;

      fix_cpdg_inst_map(inst->_index, op);

      inst->clearOperandInsts();

      _loop_InstTrace.pop_front();
    }
    redoTrackLoopInsts(li, (unsigned)-1);
  }

  //This function clears *all* loop tracking information, and
  //re-does it based on a subset of the trace.  (up to iter)
  virtual void redoTrackLoopInsts(LoopInfo *li, unsigned iter) {

    _icacheLat.clear();
    _cacheLat.clear();
    _trueCacheProd.clear();
    _ctrlMiss.clear();
    _cacheHitLevel.clear();

    unsigned numIter = 0;
    // repopulate --- for iter worth of instructions
    for (auto I = _loop_InstTrace.begin(), IE = _loop_InstTrace.end();
         I != IE; ++I) {
      Op *op = I->first;
      InstPtr inst = I->second;
      if (op->bb_pos() == 0 && li->loop_head() == op->bb())
        ++ numIter;
      if ((unsigned)numIter > iter)
        return;
      this->aggrLoopInstsStat(op, inst);
    }
  }

  void insert_inst_trace_to_default_pipe()
  {
    if (getenv("DUMP_MAFIA_PIPE") == 0)
      return ;

      if (getenv("DUMP_MAFIA_DEFAULT_PIPE") != 0)
        _default_cp._dump_inst_flag = true;
      std::cout << " =========== Begin Default Pipe ========\n";

      for (auto I = _loop_InstTrace.begin(), IE = _loop_InstTrace.end();
           I != IE; ++I) {
        _default_cp.insert_inst(I->third, I->second->index(), I->first);
      }

      std::cout << " =========== End Default Pipe =========\n";
      _default_cp._dump_inst_flag = false;
    }

  void insert_inst_to_default_pipe(const CP_NodeDiskImage &img,
                                   uint64_t index, Op *op) {
    if (getenv("DUMP_MAFIA_PIPE") == 0)
      return ;

    _default_cp.insert_inst(img, index, op);
  }

private:
  DEBUG_CPDG::default_cpdg_t _default_cp;


};

#endif
