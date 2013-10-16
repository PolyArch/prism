
#ifndef CP_VERBOSE_HH
#define CP_VERBOSE_HH

#include "cp_dg_builder.hh"
#include "exec_profile.hh"
#include "cp_args.hh"

namespace verbose {

  class cp_verbose : public CP_DG_Builder<dg_event,
                                          dg_edge_impl_t<dg_event> >
  {
    typedef dg_event T;
    typedef dg_edge_impl_t<dg_event> E;
    unsigned _count = 0;

  public:
    dep_graph_t<Inst_t, T, E> * getCPDG() { return 0; }

   virtual void traceOut(uint64_t index,
                        const CP_NodeDiskImage &img, Op* op) {
   }

    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {
      ++_count;

      std::cout << _count << ":  ";
      img.write_to_stream(std::cout);
      std::cout << "\n";
    }

    void printResults(std::ostream &out, std::string name,
                      uint64_t baseline_cycles) {
    }
  };
} // end namespace verbose

#endif
