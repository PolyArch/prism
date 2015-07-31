
#ifndef CP_NPU_HH
#define CP_NPU_HH

#include "cp_args.hh"
#include "cp_dg_builder.hh"

#include "npu_inst.hh"
namespace npu {

  class cp_npu : public ArgumentHandler,
                 public CP_DG_Builder<dg_event,
                                      dg_edge_impl_t<dg_event> >
  {
  public:

    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;

    cp_npu() : CP_DG_Builder<T, E> () {
      getCPDG()->no_horizon();
    }

    virtual ~cp_npu() {}

    dep_graph_impl_t<Inst_t, T, E> cpdg;

    dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

    std::string _npu_func_name = "";
    unsigned _npu_exec_latency = 32;
    uint64_t _num_npu_exec_inst = 0;

    void handle_argument(const char *name,
                         const char *optarg) {
      if (strcmp(name, "npu-func") == 0) {
        _npu_func_name = std::string(optarg);
      }
      if (strcmp(name, "npu-exec-latency") == 0) {
        _npu_exec_latency = atoi(optarg);
      }
    }

    bool isNPUFunc(FunctionInfo *func) const {
      if (!func)
        return false;
      auto sym = func->symbol();
      if (!sym)
        return false;
      return (sym->name == _npu_func_name);
    }
    std::string getFuncName(FunctionInfo *func) const {
      if (!func)
        return "";
      auto sym = func->symbol();
      if (!sym)
        return "";
      return sym->name;
    }

    void accelSpecificStats(std::ostream& out, std::string &name) {
      out << " num_npu_invoked: " << _num_npu_exec_inst;
    }

  private:
    std::map<Op*, InstPtr> _op2InstPtr;
    InstPtr prev_npu_inst=NULL;
  public:
    virtual void addDeps(InstPtr &inst, Op *op = NULL) {
      if (op) {
        assert(inst.get());
        _op2InstPtr[op] = inst;
      }
      CP_DG_Builder<T, E>::addDeps(inst, op);
      if (getenv("MAFIA_NPU_DUMP_PIPE")) {
        dumpInst(inst);
      }
    }

    bool insideNPU = false;

    std::vector<InstPtr> _npu_inputs;
    InstPtr _current_npu_exec_inst = 0;
    Op *prevOp = 0;
    std::set<std::string> CalledFuncs;
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {
      bool insert_npu_exec = false;
      // are we inside the function
      if (getenv("MAFIA_NPU_DUMP_CALLED_FUNC")) {
        if (op && op->isCall()) {
          std::string fname = getFuncName(op->getCalledFunc());
          if (!CalledFuncs.count(fname)) {
            CalledFuncs.insert(fname);
            std::cout << "NPU:: Called func: "
                      << fname
                      << ":::" << op->getCalledFuncName()
                      << "\n";
          }
        }
      }
      if (!insideNPU) {
        if (op
            && op->isCall()
            && isNPUFunc(op->getCalledFunc())) {
          insideNPU = true;
          insert_npu_exec = true;
        }
      } else {
        if (prevOp->isReturn()
            && isNPUFunc(prevOp->func())) {
          // just return from _npu_func_name
          // FIXME:: how about recursion...
          insideNPU = false;
          _current_npu_exec_inst = 0;
        }
      }

      prevOp = op;
      if (!insideNPU) {
        CP_DG_Builder<T, E>::insert_inst(img, index, op);
        return;
      }

      if (insideNPU && insert_npu_exec) {
        // now, create a node for NPU execution
        InstPtr inst = InstPtr(new npu_inst(_npu_exec_latency));
        getCPDG()->addInst(inst, index);

        // create edges from its arguments;
        FunctionInfo *func = op->getCalledFunc();
        func->computeArguments();
        for (auto I = func->arg_op_begin(), E = func->arg_op_end(); I != E; ++I) {
          if (!_op2InstPtr.count(I->first))
            continue;
          InstPtr depInst = _op2InstPtr[I->first];
          assert(depInst.get());
          getCPDG()->insert_edge(*depInst,depInst->eventComplete(),
                                 *inst, Inst_t::Ready,
                                 0, E_NPUPR);
        }

        //Tony: make sure we don't get free NPU pipelining!
        if(prev_npu_inst!=NULL) {
           getCPDG()->insert_edge(*prev_npu_inst,prev_npu_inst->eventComplete(),
                                 *inst, Inst_t::Ready,
                                 1, E_NPUPR);
        }
        prev_npu_inst=inst;

        addDeps(inst);
        pushPipe(inst);
        inserted(inst);
        _current_npu_exec_inst = inst;
        ++ _num_npu_exec_inst;
        return;
      }

      // We are inside NPU.
      // do not push instruction to pipe
      // But keep make all finish executing same time as the
      // NPU exec node.

      InstPtr inst = InstPtr(new Inst_t(img, index));
      getCPDG()->addInst(inst, index);
      assert(_current_npu_exec_inst.get() != 0);

      dg_inst_base<T, E> &prev_inst = getCPDG()->queryNodes(index-1);

      // addDeps ()
      for (int stage = 0; stage < dg_inst<T, E>::NumStages; ++stage)
        getCPDG()->insert_edge(prev_inst, stage,
                               *inst, stage, 0, E_NPUFE);
      // No push pipe
      if (getenv("MAFIA_NPU_DUMP_ELIDED")) {
        this->dumpInst(inst);
      }
      inserted(inst);
    }

  };

} // end namespace npu
#endif
