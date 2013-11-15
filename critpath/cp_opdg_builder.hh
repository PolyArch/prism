
#ifndef CP_OPDG_BUILDER_HH
#define CP_OPDG_BUILDER_HH

#include "cp_dg_builder.hh"
#include "exec_profile.hh"

namespace DEBUG_CPDG {
  class default_cpdg_t : public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>>
  {
public:
    bool _dump_inst_flag = false;

    default_cpdg_t() {
      _isInOrder = false;
      _setInOrder = true;
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

  typedef dg_inst<T, E> Inst_t;
  typedef std::shared_ptr<Inst_t> InstPtr;
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

  void insert_inst(const CP_NodeDiskImage &img,
                   uint64_t index, Op* op) {
    InstPtr sh_inst = createInst(img, index, op);
    getCPDG()->addInst(sh_inst, index);
    this->addDeps(sh_inst, op);
    this->pushPipe(sh_inst);
    this->inserted(sh_inst);
  }

  std::vector<InstPtr> _producerInsts;

  virtual dep_graph_t<Inst_t, T, E> *getCPDG() = 0;

  virtual Inst_t &checkRegisterDependence(Inst_t &n) {
    if (!(useOpDependence() || usePipeDependence()
          || useInstListDependence()
          || n.hasOperandInsts()))
      return CP_DG_Builder<T, E>::checkRegisterDependence(n);

    if (usePipeDependence()) {
      const int NumProducer = 7; // X86 dep
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

    if (n.hasOperandInsts()) {
      for (auto I = n.op_begin(), IE = n.op_end(); I != IE; ++I) {
        auto inst = *I;
        getCPDG()->insert_edge(*inst, inst->eventComplete(),
                               n, Inst_t::Ready, 0, E_RDep);
      }
      return n;
    }

    Op *op = getOpForInst(n);
    if (!op) {
      assert(n.hasDisasm() && n.getDisasm() == "dyser_config");
      return n;
    }

    for (auto I = op->d_begin(), IE = op->d_end(); I != IE; ++I) {
      Op *DepOp = *I;
      InstPtr depInst = getInstForOp(DepOp);
      if (!depInst.get())
        continue;
      getCPDG()->insert_edge(*depInst, depInst->eventComplete(),
                             n, Inst_t::Ready, 0, E_RDep);
    }
    return n;
  }

  // Override DataDependence Check
  virtual Inst_t &checkMemoryDependence(Inst_t &n) {
    if (!useOpDependence() && !n.hasMemOperandInsts())
      return CP_DG_Builder<T, E>::checkMemoryDependence(n);

    if (n.hasMemOperandInsts()) {
      for (auto I = n.mem_op_begin(), IE = n.mem_op_end(); I != IE; ++I) {
        std::shared_ptr<dg_inst_base<T, E> > inst = *I;
        Inst_t *depInst = dynamic_cast<Inst_t*>(inst.get());
        this->insert_mem_dep_edge(*depInst, n);
      }
      return n;
    }

    Op *op = getOpForInst(n);

    if (!op) {
      assert(n.hasDisasm() && n.getDisasm() == "dyser_config");
      return n;
    }


    for (auto I = op->m_begin(), IE = op->m_end(); I != IE; ++I) {
      Op *DepOp = *I;
      InstPtr depInst = getInstForOp(DepOp);
      Inst_t *depInstPtr = depInst.get();
      if (!depInstPtr)
        continue;
      this->insert_mem_dep_edge(*depInstPtr, n);
    }
    return n;
  }

protected:
  InstPtr _lastInst = 0;

  std::map<Op *, InstPtr> _op2InstPtr;
  std::map<Inst_t *, Op*> _inst2Op;


  virtual InstPtr createInst(const CP_NodeDiskImage &img,
                             uint64_t index,
                             Op *op)
  {
    InstPtr ret = InstPtr(new Inst_t(img, index));
    if (op)
      keepTrackOfInstOpMap(ret, op);
    return ret;
  }

  void remove_instr(dg_inst_base<T, E> *p) {
    Inst_t *ptr = dynamic_cast<Inst_t*>(p);
    assert(ptr);
    _inst2Op.erase(ptr);
  }

  virtual void keepTrackOfInstOpMap(InstPtr ret, Op *op) {
    _op2InstPtr[op] = ret;
    _inst2Op[ret.get()] = op;
  }

  virtual Op* getOpForInst(Inst_t &n, bool allowNull = false) {
    auto I2Op = _inst2Op.find(&n);
    if (I2Op != _inst2Op.end())
      return I2Op->second;
    if (!allowNull) {
      if (n.hasDisasm())
        return 0; // Fake instructions ....
      assert(0 && "inst2Op map does not have inst??");
    }
    return 0;
  }

  virtual InstPtr getInstForOp(Op *op) {
    auto Op2I = _op2InstPtr.find(op);
    if (Op2I != _op2InstPtr.end())
      return Op2I->second;
    return 0;
  }


protected:

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
  std::map<Op*, uint16_t> _cacheLat;
  std::map<Op*, bool> _trueCacheProd;
  std::map<Op*, bool> _ctrlMiss;

  virtual void trackLoopInsts(LoopInfo *li, Op *op, InstPtr inst,
                              const CP_NodeDiskImage &img) {
    _loop_InstTrace.push_back(_InstInfo<Op*, InstPtr, CP_NodeDiskImage>(op,
                                                                        inst,
                                                                        img));
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

      // TODO: do something for _cache_prod
    }
    if (op->isCtrl()) {
      // if one missed, simd misses.
      if (_ctrlMiss.count(op))
        _ctrlMiss[op] |= inst->_ctrl_miss;
      else
        _ctrlMiss[op] = inst->_ctrl_miss;
    }

    // cache the op in the insts
    {
      const int NumProducer = 7;
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
  }

  virtual void updateInstWithTraceInfo(Op *op, InstPtr inst,
                                       bool useInst) {

    if (op->isLoad()) {
      uint16_t inst_lat = useInst ? inst->_ex_lat : 0;
      inst->_ex_lat = std::max(inst_lat, _cacheLat[op]);
    }
    if (op->isStore()) {
      uint16_t inst_lat = useInst ? inst->_st_lat : 0;
      inst->_st_lat = std::max(inst_lat, _cacheLat[op]);
    }

    if (getenv("MAFIA_SIMD_NO_TRUE_CACHEPROD") == 0) {
      if (op->isLoad() || op->isStore()) {
        if (useInst)
          inst->_true_cache_prod &= _trueCacheProd[op];
        else
          inst->_true_cache_prod = _trueCacheProd[op];
      }
    }

    if (op->isCtrl()) {
      if (useInst)
        inst->_ctrl_miss |= _ctrlMiss[op];
      else
        inst->_ctrl_miss = _ctrlMiss[op];
    }

  }

  virtual void cleanupLoopInstTracking() {
    for (auto I = _loop_InstTrace.begin(), IE = _loop_InstTrace.end();
         I != IE; ++I) {
      InstPtr inst = I->second;
      inst->clearOperandInsts();
    }
    _loop_InstTrace.clear();
    _cacheLat.clear();
    _trueCacheProd.clear();
    _ctrlMiss.clear();
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
      inst->clearOperandInsts();

      _loop_InstTrace.pop_front();
    }
    redoTrackLoopInsts(li, (unsigned)-1);
  }

  virtual void redoTrackLoopInsts(LoopInfo *li, unsigned iter) {

    _cacheLat.clear();
    _trueCacheProd.clear();
    _ctrlMiss.clear();

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
