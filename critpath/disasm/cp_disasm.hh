
#ifndef CP_DISASM_HH
#define CP_DISASM_HH

#include "cp_dg_builder.hh"
#include "exec_profile.hh"
#include "cp_args.hh"

namespace disasm {

  class cp_disasm : public ArgumentHandler,
                    public CP_DG_Builder<dg_event,
                                         dg_edge_impl_t<dg_event> >
  {
    typedef dg_event T;
    typedef dg_edge_impl_t<dg_event> E;
  public:
    dep_graph_t<Inst_t, T, E> * getCPDG() { return 0; }

    virtual void traceOut(uint64_t index,
                        const CP_NodeDiskImage &img, Op* op) {
    }

    
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {
      if (!op)
        return;
      if(op->bb_pos()==0) {
        std::cout << "BB<" << op->bb()->rpoNum() << ">:\n";
      }
      uint64_t pc  = op->cpc().first;
      uint16_t upc = op->cpc().second;
      std::cout << pc << "," << upc << " : "
                << ExecProfile::getDisasm(pc, upc) << "\n";
      if (op == op->bb()->lastOp())
        std::cout << "\n";
    }
    void handle_argument(const char *name, const char *optarg) {
    }
  };
} // end namespace disasm

#endif
