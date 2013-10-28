
#ifndef CP_OPDG_BUILDER_HH
#define CP_OPDG_BUILDER_HH

#include "cp_dg_builder.hh"

template<typename T, typename E>
class CP_OPDG_Builder : public CP_DG_Builder<T, E> {

  typedef dg_inst<T, E> Inst_t;
  typedef std::shared_ptr<Inst_t> InstPtr;

public:
  CP_OPDG_Builder() : CP_DG_Builder<T, E> () {}
  virtual ~CP_OPDG_Builder() {}

  void pushPipe(InstPtr &inst) {
    CP_DG_Builder<T, E>::pushPipe(inst);
    _lastInst = inst;
  }

  virtual dep_graph_t<Inst_t, T, E> *getCPDG() = 0;

  virtual Inst_t &checkRegisterDependence(Inst_t &n) {
    if (!useOpDependence())
      return CP_DG_Builder<T, E>::checkRegisterDependence(n);

    Op *op = getOpForInst(n);
    assert(op);

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
    if (!useOpDependence())
      return CP_DG_Builder<T, E>::checkMemoryDependence(n);

    Op *op = getOpForInst(n);
    assert(op);

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

  virtual bool useOpDependence() const = 0;

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

  virtual Op* getOpForInst(Inst_t &n) {
    auto I2Op = _inst2Op.find(&n);
    if (I2Op != _inst2Op.end())
      return I2Op->second;
    assert(0 && "inst2Op map does not have inst??");
    return 0;
  }

  virtual InstPtr getInstForOp(Op *op) {
    auto Op2I = _op2InstPtr.find(op);
    if (Op2I != _op2InstPtr.end())
      return Op2I->second;
    return 0;
  }
};

#endif
