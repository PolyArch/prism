
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

    cp_npu() : CP_DG_Builder<T, E> () { }

    virtual ~cp_npu() {}

    dep_graph_impl_t<Inst_t, T, E> cpdg;

    dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

    std::string _npu_func_name = "";

    void handle_argument(const char *name,
                         const char *optarg) {
    }

    bool insideNPU = false;

    std::vector<InstPtr> _npu_inputs;
    InstPtr _current_npu_exec_inst = 0;
    Op *prevOp = 0;
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {
      // are we inside the function
      if (!insideNPU) {
        if (op
            && op->isCall()
            && op->getCalledFuncName() == _npu_func_name) {
          insideNPU = true;
          // create the node for NPU execution
          // now, create a node for NPU execution
          InstPtr inst = InstPtr(new npu_inst());
          getCPDG()->addInst(inst, index);
          addDeps(inst);
          pushPipe(inst);
          inserted(inst);
          _current_npu_exec_inst = inst;
        }
      } else {
        if (prevOp->isReturn()
            && prevOp->func()->nice_name() == _npu_func_name) {
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

      inserted(inst);
    }

  };

} // end namespace npu
#endif
