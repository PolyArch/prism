
#ifndef CP_SIMD_HH
#define CP_SIMD_HH

#include "cp_dg_builder.hh"
#include "cp_registry.hh"

#include "simd_inst.hh"
#include "exec_profile.hh"

namespace simd {

  class cp_simd : public ArgumentHandler,
                  public CP_DG_Builder<dg_event,
                                       dg_edge_impl_t<dg_event> >

  {

    typedef dg_event T;
    typedef dg_edge_impl_t<T> E;
    typedef dg_inst<T, E> Inst_t;
    typedef std::shared_ptr<Inst_t> InstPtr;
    typedef std::shared_ptr<simd_inst> SimdInstPtr;

    dep_graph_impl_t<Inst_t, T, E> cpdg;

    enum ModelState {
      IN_SCALAR, // instructions execute in scalar mode
      IN_SIMD    // instructions execute in simd fashion
    };

    unsigned _simd_len = 4;

  public:
    cp_simd() : CP_DG_Builder<T, E> () {
    }

    virtual ~cp_simd() {}

    void handle_argument(const char *name, const char *optarg) {
      if (strcmp(name, "simd-len") == 0) {
        _simd_len = atoi(optarg);
        if (_simd_len == 0)
          _simd_len = 4;
      }
    }

    virtual dep_graph_t<Inst_t, T, E>* getCPDG() {
      return &cpdg;
    }

    LoopInfo *CurLoop = 0;
    bool simd_state  = IN_SCALAR;

    LoopInfo *getLoop(Op *op) {
      // Is this instruction first in a basic block?
      if (op->bb_pos() == 0) {
        // is this bb starts an inner loop?
        CurLoop = op->func()->getLoop(op->bb());
      }
      return CurLoop;
    }

    std::map<Op*, unsigned> _op2Count;
    std::map<LoopInfo *, bool> isVectorizableMap;
    bool isVectorizable(LoopInfo *li) {

      if (isVectorizableMap.count(li))
        return isVectorizableMap[li];

      bool shouldPrint =  !li_printed.count(li);
      printLoop(li);

      bool canVectorize = true;

      for (auto BBI = li->body_begin(), BBE = li->body_end(); BBI != BBE; ++BBI) {
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end(); I != E; ++I) {
          if (!(*I)->isLoad() && !(*I)->isStore())
            continue;
          int stride = 0;
          if (!(*I)->getStride(&stride)) {
            if (shouldPrint) {
              printDisasm((*I)->cpc().first, (*I)->cpc().second);
              std::cout << "    stride is not constant or unknown\n";
              (*I)->printEffAddrs();
              std::cout << "\n";
            }
            canVectorize = false;
          }
          for (auto DI = (*I)->m_begin(), DE = (*I)->m_end(); DI != DE; ++DI) {
            Op *DepOp = *DI;
            // Same -- broadcast load should handle this
            if (DepOp == (*I))
              continue;
            if (DepOp->isLoad() && (*I)->isLoad())
              continue;
            // Same memory touched by two ops in the same loop...
            if (li == DepOp->func()->getLoop(DepOp->bb())) {
              if (shouldPrint) {
                printDisasm((*I)->cpc().first, (*I)->cpc().second);
                (*I)->printEffAddrs();
                std::cout << "\n";

                std::cout << " conflicts with \n";
                printDisasm(DepOp->cpc().first,  DepOp->cpc().second);
                DepOp->printEffAddrs();
                std::cout << "\n";
              }
              canVectorize = false;
            }
          }
        }
      }
      isVectorizableMap[li] = canVectorize;
      return canVectorize;
    }
    unsigned getSIMDPos(InstPtr inst, Op *op, LoopInfo *li) {
      // No loop -- scalar
      if (!li)
        return 0;

      // No inner loop -- scalar
      if (!li->isInnerLoop())
        return 0;

      // Not vectorizable -- scalar
      if (!isVectorizable(li))
        return 0;

      // Vectorizable
      unsigned pos  = _op2Count[op];
      _op2Count[op] = (pos+1)% _simd_len;

      return pos;
    }

    static std::map<LoopInfo*, bool> li_printed;

    void printDisasm(uint64_t pc, int upc) {
      std::cout << pc << "," << upc << " : "
                << exec_profile::getDisasm(pc, upc) << "\n";
    }

    void printLoop(LoopInfo *li) {
      if (li_printed.count(li))
        return;
      li_printed[li] = true;

      std::cout << "======================================\n";
      std::cout << "================" << li << "==========\n";
      for (auto BBI = li->body_begin(), BBE = li->body_end(); BBI != BBE; ++BBI) {
        std::cout << "BB::" << *BBI << "\n";
        for (auto I = (*BBI)->op_begin(), E = (*BBI)->op_end(); I != E; ++I) {
          Op *op = *I;
          uint64_t pc = op->cpc().first;
          int upc = op->cpc().second;
          std::cout << pc << "," <<upc << " : " <<
            exec_profile::getDisasm(pc, upc) << "\n";
        }
        std::cout << "\n";
      }
      std::cout << "================" << li << "==========\n";
      std::cout << "======================================\n";
    }

    InstPtr addShuffleInst(InstPtr inst)
    {
      return inst;
    }

    //
    // Override insert_inst to transform to SIMD graph
    //
    void insert_inst(const CP_NodeDiskImage &img, uint64_t index,
                     Op *op) {

      LoopInfo *li = getLoop(op);
      // Create the instruction
      InstPtr inst = InstPtr(new Inst_t(img, index));
      // Add to the graph to do memory management
      getCPDG()->addInst(inst, index);

      unsigned vecPos = getSIMDPos(inst, op, li);
      if (vecPos == 0) {
        // Add dependency to the instruction
        addDeps(*inst, op);
        // and add to the pipe
        pushPipe(inst);

        // handle broadcast_loads
        //  load followed by shuffles ...
        //if (op->isLoad() && isStrideAccess(0)) {
        //  Inst = addShuffleInst(inst);
        //}
      }

      // Keep track of the instruction..
      inserted(inst);

    }

#if 0
      // Is this instruction first in a basic block?
      if (op->bb_pos() == 0) {
        // is this bb starts an inner loop?
        li = op->func()->getLoop(op->bb());
        if (simd_state == IN_SIMD) {
          if (CurLoop && CurLoop != li) {
            // Switch back to scalar mode
            // We are in a outer loop or not in a loop anymore.
            CurLoop = 0;
            simd_state = IN_SCALAR;
          }
        } else {
          // We are in IN_SCALAR Mode
          if (li && isVectorizable(li)) {
            // We are in a inner Loop
            // and vectorizable
            CurLoop = li;
            simd_state = IN_SIMD;
          }
        }
      }


      if (simd_state == IN_SIMD) {
        if (shouldExecuteInPipe(inst, CurLoop) ) {
          addDeps(*inst->get(), op);
          pushPipe(inst);
        } else {
          // No, we already executed as part of existing instruction.
        }
      } else {
        // Always execute in Pipe
        addDeps(*inst->get(), op);
        pushPipe(inst);
      }
      // Keep track of the instruction..
      inserted(inst);
    }

#endif
  };

};

#endif
