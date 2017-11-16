#ifndef CRITICAL_PATH_HH
#define CRITICAL_PATH_HH

#include "cp_dep_graph.hh"
#include "op.hh"
#include "exec_profile.hh"

#include "pugixml/pugixml.hpp"
#include <stdio.h>
#include <ostream>
#include <fstream>

#include "mcpat/xmlParser.h"
#include "mcpat/XML_Parse.h"
#include "mcpat/processor.h"
#include "mcpat/globalvar.h"


#include "prof.hh"

class critFuncEdges {
public:
  std::unordered_map<Op*,std::map<Op*,std::map<unsigned,double>>> critEdges;
  float total_weight=0;

  void add_edge(Op* src_op, Op* dest_op, unsigned type, float weight) {
    critEdges[src_op][dest_op][type]+=weight;
    total_weight+=weight;
  }
};





struct MemEvents {
  //icache
  uint64_t icache_read_accesses=0, icache_read_misses=0, icache_conflicts=0;

  //dcache
  uint64_t l1_hits=0, l1_misses=0, l2_hits=0, l2_misses=0;
  uint64_t l1_wr_hits=0, l1_wr_misses=0, l2_wr_hits=0, l2_wr_misses=0;

  //icache
  uint64_t icache_read_accesses_acc=0, icache_read_misses_acc=0, icache_conflicts_acc=0;

  //dcache
  uint64_t l1_hits_acc=0, l1_misses_acc=0, l2_hits_acc=0, l2_misses_acc=0;
  uint64_t l1_wr_hits_acc=0, l1_wr_misses_acc=0, l2_wr_hits_acc=0, l2_wr_misses_acc=0;
};

struct CoreEvents { 
  //------- energy events -----------
  uint64_t committed_insts=0, committed_int_insts=0, committed_fp_insts=0;
  uint64_t committed_branch_insts=0, mispeculatedInstructions=0;
  uint64_t committed_load_insts=0, committed_store_insts=0;
  uint64_t squashed_insts=0;

  uint64_t int_ops=0,mult_ops=0,fp_ops=0;
  uint64_t func_calls=0;
  uint64_t idleCycles=0;
  uint64_t nonPipelineCycles=0;

  //ooo-pipeline
  uint64_t regfile_reads=0, regfile_writes=0;
  uint64_t regfile_freads=0, regfile_fwrites=0;
  uint64_t rob_reads=0, rob_writes=0;
  uint64_t iw_reads=0, iw_writes=0, iw_freads=0, iw_fwrites=0;
  uint64_t rename_reads=0, rename_writes=0;
  uint64_t rename_freads=0, rename_fwrites=0;
  uint64_t btb_read_accesses=0, btb_write_accesses=0;

  //------- energy events accumulated -----------
  uint64_t committed_insts_acc=0, committed_int_insts_acc=0, committed_fp_insts_acc=0;
  uint64_t committed_branch_insts_acc=0, mispeculatedInstructions_acc=0;
  uint64_t committed_load_insts_acc=0, committed_store_insts_acc=0;
  uint64_t squashed_insts_acc=0;

  uint64_t int_ops_acc=0,mult_ops_acc=0,fp_ops_acc=0;
  uint64_t func_calls_acc=0;
  uint64_t idleCycles_acc=0;
  uint64_t nonPipelineCycles_acc=0;

  //ooo-pipeline
  uint64_t regfile_reads_acc=0, regfile_writes_acc=0;
  uint64_t regfile_freads_acc=0, regfile_fwrites_acc=0;
  uint64_t rob_reads_acc=0, rob_writes_acc=0;
  uint64_t iw_reads_acc=0, iw_writes_acc=0, iw_freads_acc=0, iw_fwrites_acc=0;
  uint64_t rename_reads_acc=0, rename_writes_acc=0;
  uint64_t rename_freads_acc=0, rename_fwrites_acc=0;
  uint64_t btb_read_accesses_acc=0, btb_write_accesses_acc=0;

};


// abstract class for all critical path
class CriticalPath {
protected:
  std::string _run_name = "";
  std::string _name = "";
  bool _elide_mem = false;
  bool _isInOrder = false;
  bool _scale_freq = false;
  //careful, these are not necessarily the defaults...
  //we will try to load the values from m5out/config.ini
  //as the defaults through the prof class
  
  MemEvents*  _mev = NULL;
  CoreEvents* _cev = NULL;

  unsigned FETCH_WIDTH = 4;
  unsigned D_WIDTH = 4;
  unsigned ISSUE_WIDTH = 4;
  unsigned PEAK_ISSUE_WIDTH = 6;
  unsigned WRITEBACK_WIDTH = 4;
  unsigned COMMIT_WIDTH = 4;
  unsigned SQUASH_WIDTH = 4;

  unsigned IQ_WIDTH = 64;
  unsigned ROB_SIZE = 192;
  unsigned LQ_SIZE = 32;
  unsigned SQ_SIZE = 32;

  unsigned FETCH_TO_DISPATCH_STAGES = 4;
  unsigned COMMIT_TO_COMPLETE_STAGES = 2;
  unsigned INORDER_EX_DEPTH = 4; 

  unsigned N_ALUS=6;
  unsigned N_MUL=1;
  unsigned N_FPU=2;
  unsigned N_MUL_FPU=2;
  unsigned RW_PORTS=2;

  unsigned L1_MSHRS=4;

  unsigned IBUF_SIZE=32;
  unsigned PIPE_DEPTH=16;      
  unsigned PHYS_REGS=256;

  unsigned CORE_CLOCK=2000;

  bool applyMaxLatToAccel=true;
  int _max_mem_lat=1073741824; //some big numbers that will never make sense
  int _max_ex_lat=1073741824;
  int _nm=22;

  unsigned _cpu_wakeup_cycles=20;  //cycles to wake CPU

  //Revolver Stuff
  bool _enable_revolver=false;
  bool _prev_cycle_revolver_active=false;
  bool _revolver_active=false;

  //  std::map<LoopInfo*,std::pair<std::vector<BB*>,int> > revolver_prof_map;
  std::unordered_map<LoopInfo*,int> _revolver_profit;
  std::unordered_map<LoopInfo*,std::set<BB*>> _revolver_bbs;
  std::set<BB*> _revolver_prev_trace;
  LoopInfo* _cur_revolver_li=NULL;

  virtual void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) = 0;

  uint64_t cycles_acc=0;


  //cur_fi will always be on
  //cur_li will either be null or have a loop
  FunctionInfo* cur_fi;
  LoopInfo* cur_li;
  int cur_is_acc;

  //Accessed only from critpath.hh
  uint64_t n_br=0;
  uint64_t n_br_miss=0;
  uint64_t n_mem=0;
  uint64_t n_flt=0;
  uint64_t n_l1_miss=0;
  uint64_t n_l2_miss=0; 
  uint64_t revolver_insts=0;

  class RegionStats {
  public:
    uint64_t cycles=0;
    uint64_t insts=0;

    uint64_t m_br=0;
    uint64_t m_br_miss=0;
    uint64_t m_mem=0;
    uint64_t m_flt=0;
    uint64_t m_l1_miss=0;
    uint64_t m_l2_miss=0; 

    double total=0;
    bool in_accel=false;
    int on_id=0;
    CumWeights cum_weights;

    static double get_leakage(Component* comp, bool long_channel) {
      return (long_channel?comp->power.readOp.longer_channel_leakage:
              comp->power.readOp.leakage) + comp->power.readOp.gate_leakage;
    }

    //breakdown
    double core=0, icache=0, dcache=0, l2=0, itlb=0, dtlb=0,
           stateful_core_static=0, nonstateful_core_static=0, cache_static=0, 
           accel=0, accel_static=0;

    void fill(uint64_t cycle_diff, uint64_t inst_diff, PrismProcessor* mc_proc, 
              uint64_t br, uint64_t br_miss, uint64_t flt,
              uint64_t mem, uint64_t l1_miss, uint64_t l2_miss,
              double accel_en, double accel_leakage_power,
              int is_on, bool cpu_power_gated, bool revolver_active) {
      cycles+=cycle_diff;
      insts+=inst_diff;

      m_br+=br;
      m_br_miss+=br_miss;
      m_mem+=mem;
      m_flt+=flt;
      m_l1_miss+=l1_miss;
      m_l2_miss+=l2_miss;

      double dcache_en = mc_proc->cores[0]->lsu->dcache.rt_power.readOp.dynamic; //energy
      double icache_en = mc_proc->cores[0]->ifu->icache.rt_power.readOp.dynamic; //energy

      double dtlb_en = mc_proc->cores[0]->mmu->dtlb->rt_power.readOp.dynamic; //energy
      double itlb_en = mc_proc->cores[0]->mmu->itlb->rt_power.readOp.dynamic; //energy
      double core_en_raw = mc_proc->core.rt_power.readOp.dynamic; //energy

      //this contains the icache_en
      double ifu_en = mc_proc->cores[0]->ifu->rt_power.readOp.dynamic; //energy

      double core_en = core_en_raw - dcache_en - itlb_en - dtlb_en; //add back later

      if(core_en < -10 || core_en > 100000)  {
        assert(0);
      }
       
      //adujust differently depending on whether core is active
      if(revolver_active) {
        icache_en = 0; //set to zero b/c we add it back later
        core_en = core_en - ifu_en; //ifu is superset of icache
      } else {
        core_en = core_en - icache_en; //add back the icache_en later
      }

      if(core_en< 0) {
        core_en=0;
      }


      double l2_en =mc_proc->l2.rt_power.readOp.dynamic;
      double ex_freq = mc_proc->XML->sys.target_core_clockrate*1e6;
      bool long_channel = mc_proc->XML->sys.longer_channel_device;
      
      double total_leakage_power = get_leakage(mc_proc,long_channel);
//      double icache_leakage = get_leakage(&mc_proc->cores[0]->ifu->icache,long_channel);
      double dcache_leakage = get_leakage(&mc_proc->cores[0]->lsu->dcache,long_channel);
      double l2_leakage     = get_leakage(&mc_proc->l2,long_channel);

      double itlb_leakage    = get_leakage(mc_proc->cores[0]->mmu->itlb,long_channel);
      double dtlb_leakage    = get_leakage(mc_proc->cores[0]->mmu->dtlb,long_channel);
      double mmu_leakage     = itlb_leakage+dtlb_leakage;

      double reg_leakage    = get_leakage(mc_proc->cores[0]->exu->rfu,long_channel);

      double cache_leakage_power=dcache_leakage+l2_leakage;
      double stateful_core_leakage_power=mmu_leakage+reg_leakage;

      double nonstateful_core_leakage_power=total_leakage_power
                                  -cache_leakage_power-stateful_core_leakage_power;

      if(cpu_power_gated) {
        nonstateful_core_leakage_power*=0.00; //TODO: WHAT IS THE RIGHT FACTOR TO KEEP?
      } else {
        nonstateful_core_leakage_power*=0.9; //TODO: WHAT IS THE RIGHT fudge factor?
      }

      double nonstateful_core_leakage_en = nonstateful_core_leakage_power*(cycle_diff/ex_freq);
      double stateful_core_leakage_en = stateful_core_leakage_power*(cycle_diff/ex_freq);
      double cache_leakage_en = cache_leakage_power*(cycle_diff/ex_freq);

      double accel_leakage_en = 0;
      if(is_on!=0) {
        on_id=is_on;
        accel_leakage_en = accel_leakage_power*(cycle_diff/ex_freq); //same freq
      } else {
        accel_en=0;
      }

      core+=core_en;
      icache+=icache_en;
      dcache+=dcache_en;
      itlb+=itlb_en;
      dtlb+=dtlb_en;
      l2+=l2_en;
      accel+=accel_en;
      accel_static+=accel_leakage_en;
      stateful_core_static+=stateful_core_leakage_en;
      nonstateful_core_static+=nonstateful_core_leakage_en;
      cache_static+=cache_leakage_en;

      total += core_en + icache_en + dcache_en + itlb_en + dtlb_en + l2_en+
        stateful_core_leakage_en+nonstateful_core_leakage_en+cache_leakage_en+
        accel_en+accel_leakage_en;
 
    }

    static const int pc=8;

    static void printHeader(std::ostream& out) {
      out << std::setw(45) << "Func Name";
      out << std::setw(9)  << "Cycles"   << " ";
      out << std::setw(10) << "O-Insts"  << " ";
      out << std::setw(pc) << "O-IPC"    << " ";

      out << std::setw(pc) << "perBr"    << " ";
      out << std::setw(pc) << "perBrMs"  << " ";
      out << std::setw(pc) << "perFlt"   << " ";
      out << std::setw(pc) << "perMem"   << " ";
      out << std::setw(pc) << "perL1Ms"  << " ";
      out << std::setw(pc) << "perL2Ms"  << " ";

      out << std::setw(pc) << "Total"    << " ";
      out << std::setw(pc) << "Lk_sCore" << " ";
      out << std::setw(pc) << "Lk_oCore" << " ";
      out << std::setw(pc) << "Lk_Cache" << " ";
      out << std::setw(pc) << "Core"     << " ";
      out << std::setw(pc) << "itlb"     << " ";
      out << std::setw(pc) << "dtlb"     << " ";
      out << std::setw(pc) << "Icache"   << " ";
      out << std::setw(pc) << "DCache"   << " ";
      out << std::setw(pc) << "L2"       << " ";
      out << std::setw(pc) << "Accel"    << " ";
      out << std::setw(pc) << "Lk_Accel" << " ";
      out << std::setw(5)  << "Acc?"     << " ";
    }

    void printPower(double en, std::ostream& out, double ex_freq) {
      double ex_time = cycles/(ex_freq);  

      if(ex_time != 0 && en / ex_time != 0) {
        out << std::fixed << std::setw(pc) << std::setprecision(4)  << (en / ex_time) * 1.0 << " ";
      } else {
        out << std::setw(pc) << 0 << " ";
      }
    }

    //safe ration
    static double sr(double d1, double d2) {
      double value =d1/d2;
      if(value!=value) {
        return 0;
      }
      return value;
    }

    void print(std::string name, std::ostream& out, double ex_freq) {
      out << std::setw(45) << name << " ";
      out << std::setw(9)  << cycles  << " ";
      out << std::setw(10) << insts  << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(2) << sr(insts,cycles) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_br,insts) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_br_miss,m_br) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_flt,insts) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_mem,insts) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_l1_miss,m_mem) << " ";
      out << std::setw(pc) << std::fixed << std::setprecision(3) << sr(m_l2_miss,m_mem) << " ";

      printPower(total,out,ex_freq);
      printPower(stateful_core_static,out,ex_freq);
      printPower(nonstateful_core_static,out,ex_freq);
      printPower(cache_static,out,ex_freq);
      printPower(core,out,ex_freq);
      printPower(itlb,out,ex_freq);
      printPower(dtlb,out,ex_freq);
      printPower(icache,out,ex_freq);
      printPower(dcache,out,ex_freq);
      printPower(l2,out,ex_freq);
      printPower(accel,out,ex_freq);
      printPower(accel_static,out,ex_freq);
      out << std::setw(5) << on_id << " ";
    }

  };

  std::map<FunctionInfo*,RegionStats> cycleMapFunc;
  std::map<LoopInfo*,    RegionStats> cycleMapLoop;
  BB* cur_bb;

  CumWeights* curWeights() {
    if(cur_li) {
      return &cycleMapLoop[cur_li].cum_weights;
    } else if(cur_fi) {
      return &cycleMapFunc[cur_fi].cum_weights;
    }
    return NULL;
  }

  uint64_t  cur_cycles=0, cur_insts=0;
  //Update cycles spent in this config
public:
  virtual double accel_leakage()    {return 0;}
  virtual double accel_region_en()  {return 0;}

  //Region Breakdown Thing
  virtual void update_cycles() {
    //get the current cycle
    uint64_t cycles=this->numCycles();
    uint64_t insts =Prof::get().dId();

    uint64_t cycle_diff = cycles - cur_cycles;
    uint64_t insts_diff = insts  - cur_insts;

    if(cycles < cur_cycles || cycles     >= cur_cycles+ ((uint64_t)-1)/2 
                           || cycle_diff >= ((uint64_t)-1)/2 ) {
      std::cerr << "ERROR: WEIRD CYCLE COUNTS!" << "\n";
      std::cerr << "previous last cycle: " << cur_cycles << "\n";
      std::cerr << "new last cycle: " << cycles << "\n";
      assert(0);
    }

    pumpMcPAT(); // pump both models

    if(cur_li) {
      cycleMapLoop[cur_li].fill(cycle_diff,insts_diff,mc_proc,
                                n_br,  n_br_miss, n_flt, n_mem, n_l1_miss, n_l2_miss,
                                accel_region_en(),accel_leakage(),
                                cur_is_acc,cpu_power_gated(),
                                _prev_cycle_revolver_active);
    } else {
      cycleMapFunc[cur_fi].fill(cycle_diff,insts_diff,mc_proc,
                                n_br,  n_br_miss, n_flt, n_mem, n_l1_miss, n_l2_miss,
                                accel_region_en(),accel_leakage(),
                                cur_is_acc,cpu_power_gated(),
                                _prev_cycle_revolver_active);
    }

    n_br=0;
    n_br_miss=0;
    n_mem=0;
    n_flt=0;
    n_l1_miss=0;
    n_l2_miss=0;
    cur_cycles=cycles; //update cur cycles for next time
    cur_insts=insts;
  }

  virtual void print_edge_weights(std::ostream& out, AnalysisType analType) { } 

  virtual bool cpu_power_gated() {return false;}

  virtual void check_update_cycles(Op* op) {
    if(op->bb_pos()!=0) {
      return;
    }

    FunctionInfo* fi = op->func();
    //LoopInfo* li = op->func()->getLoop(op->bb());
    LoopInfo* li = Prof::get().curFrame()->curLoop();

    assert(fi);

    if(cur_fi) {
      // If there was a change
      if(li!=cur_li || cur_fi!=fi) {
        update_cycles();
      }
    } 

    cur_bb=op->bb();
    cur_li=li;
    cur_fi=fi;
    cur_is_acc=is_accel_on();
  }

  //return true if accel, and return the id as int
  virtual int is_accel_on() {
    return false;
  }

  virtual void printAccelHeader(std::ostream& outs,bool hole) {}

  void printCritAnal(CumWeights* cw, ostream& out) {
    out << "Weighted:(";
    cw->print_edge_weights(out,AnalysisType::Weighted);
    out << ")";

    out << "Unweighted:(";
    cw->print_edge_weights(out,AnalysisType::Unweighted);
    out << ")";
    
    out << "Offset:(";
    cw->print_edge_weights(out,AnalysisType::Offset);
    out << ")";
  }

  virtual void printAccelRegionStats(int id, std::ostream& outs) {}

  void printLoopRecursive(LoopInfo* li,std::ostream& outf,double ex_freq) {
    std::string quote_name = std::string("\"") 
                             + li->nice_name_full() + std::string("\"");  

    cycleMapLoop[li].print(quote_name,outf,ex_freq);
    printAccelRegionStats(cycleMapLoop[li].on_id,outf);
    printCritAnal(&cycleMapLoop[li].cum_weights,outf);
    outf << " "; Prof::get().print_loop_loc(outf,li);
    outf << "\n";

    for(auto i=li->iloop_begin(),e=li->iloop_end();i!=e;++i) {
      LoopInfo* inner_li = *i;
      printLoopRecursive(inner_li,outf,ex_freq);
    }
  }

  void printRegionBreakdown(std::ostream& outf) {
    RegionStats::printHeader(outf);
    printAccelHeader(outf,false);
    outf<<"\n";

    double ex_freq = mc_proc->XML->sys.target_core_clockrate*1e6;  

    //We need to print out loops and funcions
    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      std::string quote_name = std::string("\"") + fi->nice_name() + std::string("\"");
      cycleMapFunc[fi].print(quote_name,outf,ex_freq);
      printAccelHeader(outf,true);  //fills gap
      printCritAnal(&cycleMapFunc[fi].cum_weights,outf);

      outf << "\n";

      /*
      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        std::string quote_name = std::string("\"") + li->nice_name() + std::string("\"");
        cycleMapLoop[fi]->print(quote_name,outf);
      }*/

      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        if(li->depth()==1) { //only outer loops
          printLoopRecursive(li,outf,ex_freq);
        }
      }

    }

    //bonus
    std::cout << "Revolver Insts:" << revolver_insts << "\n";

  }

private:
  bool TraceOutputs = false;
  int TraceCycleGranularity=false;
  uint64_t last_traced_index=0, last_traced_cycle=0;
  std::ofstream out,outc;
  bool triedToOpenOutOnce = false, triedToOpenOutCycleOnce=false;

protected:
  bool getTraceOutputs() const { return TraceOutputs; }
  int getTraceCycleGranularity() const { return TraceCycleGranularity; }


  std::ofstream &outs() {
    if (out.good() && triedToOpenOutOnce) {
      return out;
    }

    triedToOpenOutOnce = true;
    std::string trace_out = _name;
    if (trace_out == "") {
      char buf[64];
      sprintf(buf, "cp-trace-%p.txt", (void*)this);
      trace_out = std::string(buf);
    } else {
      trace_out = _name + std::string(".txt");
    }
    std::cout << "opening trace file: \"" << trace_out << "\"\n";
    out.open(trace_out.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!out.good()) {
      std::cerr << "Cannot open file: " << trace_out << "\n";
    }
    return out;
  }

  std::ofstream &outcs() {
    if (outc.good() && triedToOpenOutCycleOnce) {
      return outc;
    }

    triedToOpenOutCycleOnce = true;
    std::string trace_out = _name;
    if (trace_out == "") {
      char buf[64];
      sprintf(buf, "stats/%p.cyc-trace", (void*)this);
      trace_out = std::string(buf);
    } else {
      trace_out = std::string("stats/");
      if(!_run_name.empty()) {
        trace_out+=_run_name+".";
      }
      trace_out+= _name + ".cyc-trace";
    }
    std::cout << "opening cycle trace file: \"" << trace_out << "\"\n";
    outc.open(trace_out.c_str(), std::ofstream::out | std::ofstream::trunc | ios::binary);
    if (!outc.good()) {
      std::cerr << "Cannot open file: " << trace_out << "\n";
    }
    int num = TraceCycleGranularity;
    outc.write(reinterpret_cast<const char *>(&num), sizeof(num));
    return outc;
  }

  void traceCycleOut(uint64_t index, const CP_NodeDiskImage &img, Op* op) {
    if (!TraceCycleGranularity) {
      return;
    }
    if(index >= last_traced_index + TraceCycleGranularity) {
      uint64_t new_cycle = this->numCycles();
      int cycle_diff =  new_cycle - last_traced_cycle;
      outcs().write(reinterpret_cast<const char *>(&cycle_diff), sizeof(cycle_diff));
      last_traced_cycle=new_cycle;
      last_traced_index=index;
    }
  }


  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img, Op* op) {
    if (!TraceOutputs) {
      return;
    }

    outs() << (img._isctrl ? "C" : "");
    outs() << (img._isreturn ? "Ret" : "");
    outs() << (img._iscall ? "Func" : "");
    outs() << (img._isload ? "L" : "");
    outs() << (img._isstore ? "S" : "");
    outs() << (img._ctrl_miss ? "ms" : "");
    outs() << (img._spec_miss ? "msSPC" : "");
    outs() << (img._serialBefore ? "sb!" : "");
    outs() << (img._serialAfter ? "sa!" : "");
    outs() << (img._nonSpec ? "ns!" : "");
    outs() << (img._storeCond ? "sc!" : "");
    outs() << (img._prefetch ? "pref!" : "");
    outs() << (img._squashAfter ? "sqa!" : "");
    outs() << (img._writeBar ? "wbar!" : "");
    outs() << (img._memBar  ? "mbar!" : "");
    outs() << (img._syscall ? "sys!" : "");
    outs() << (img._floating ? "FP" : "");

    if (img._mem_prod != 0) {
      outs() << "md(" << img._mem_prod << ")";
    }

    if (img._cache_prod != 0) {
      outs() << "cd(" << img._cache_prod << ")";
    }

    if (img._icache_lat >0) {
      outs() << "I$" << img._icache_lat;
    }

    if(op) {
      outs() << (op->shouldIgnoreInAccel()? " +ig ":"");
      outs() << (op->plainMove()? " +pm ":"");
      outs() << (op->isConstLoad()? " +cm ":"");
      outs() << (op->isStack()? " +ss ":"");
      
      LoopInfo* li = op->func()->innermostLoopFor(op->bb());
      if(li) {
        outs() << li->nice_name_full();
      } else {
        outs() << op->func()->nice_name();
      }
    }

  }

public:
  void setRunName(std::string run_name) {
    _run_name=run_name;
  }

  virtual void printResults(std::ostream& out,
                            std::string name,
                            uint64_t baseline_cycles) {

    uint64_t cycles = this->numCycles();
    out << "Number of cycles [" << name << "]: " << cycles;

    if (cycles != 0) {
      out << " " << (double)baseline_cycles/ (double)cycles;
    }
    loopsgDots(out, name);
    accelSpecificStats(out, name);
    out << "\n";
  
    out << "Weighted Critical Edges: ";
    print_edge_weights(out, AnalysisType::Weighted);
    out << "\n";

    out << "Unweighted Critical Edges: ";
    print_edge_weights(out, AnalysisType::Unweighted);
    out << "\n";

    out << "Offset Critical Edges: ";
    print_edge_weights(out, AnalysisType::Offset);
    out << "\n";
  }


  virtual void accelSpecificStats(std::ostream& out, std::string &name) {}

  virtual bool removes_uops() {return false;}
  virtual void loopsgDots(std::ostream& out, std::string &name) {}


  void setTraceOutputs(bool t, int c) {
    TraceOutputs = t;
    TraceCycleGranularity = c;
  }

  void setName(std::string &name) {
    _name = name;
  }

  virtual void setDefaultsFromProf() {
    FETCH_WIDTH = Prof::get().fetchWidth;
    D_WIDTH = Prof::get().dispatchWidth;
    ISSUE_WIDTH = Prof::get().issueWidth;
    PEAK_ISSUE_WIDTH = Prof::get().issueWidth+2;
    WRITEBACK_WIDTH = Prof::get().wbWidth;
    COMMIT_WIDTH = Prof::get().commitWidth;
    SQUASH_WIDTH = Prof::get().squashWidth;
  
    IQ_WIDTH = Prof::get().numIQEntries;
    ROB_SIZE = Prof::get().numROBEntries;
    LQ_SIZE =  Prof::get().LQEntries;
    SQ_SIZE =  Prof::get().SQEntries;
  
    FETCH_TO_DISPATCH_STAGES = Prof::get().fetchToDecodeDelay +
                                   Prof::get().decodeToRenameDelay +
                                   Prof::get().renameToIEWDelay;

    PIPE_DEPTH=20;

    N_ALUS=Prof::get().int_alu_count;
    N_MUL=Prof::get().mul_div_count;
    N_FPU=Prof::get().fp_alu_count;
    N_MUL_FPU=Prof::get().fp_mul_div_sqrt_count;
    RW_PORTS=Prof::get().read_write_port_count;
    L1_MSHRS=Prof::get().dcache_mshrs;


 
    //N_ALUS=std::min(std::max(1,ISSUE_WIDTH*3/4),6);
    //N_FPU=std::min(std::max(1,ISSUE_WIDTH/2),2);
    //RW_PORTS=std::min(std::max(1,ISSUE_WIDTH/2),2);
  }

  virtual ~CriticalPath() {
  }

  virtual void setInOrder(bool inOrder) {
    _isInOrder=inOrder;
  }

  virtual void initialize() {}
  virtual void setupComplete() {}

  virtual void set_elide_mem(bool elideMem) {
    _elide_mem=elideMem;
    if(_elide_mem) {
       RW_PORTS=3;
       //set_max_mem_lat(4);
    }
  } 

  bool isInOrder() {return _isInOrder;}

  virtual void setWidth(int i,bool scale_freq, bool match_simulator, 
                        bool revolver, int mem_ports, int num_L1_MSHRs) {
    return;
  }

  virtual void insert(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    insert_inst(img,index,op);
    
    n_br+=img._isctrl;
    n_br_miss+=img._ctrl_miss;
    n_flt+=img._floating;
    n_mem += img._isload;
    n_mem += img._isstore;
    revolver_insts += _revolver_active;
    if(img._isload || img._isstore) {
      if(img._miss_level>=1) {
        n_l1_miss++;
      } 
      if(img._miss_level>=2) {
        n_l2_miss++;
      } 
    }
    if(out.good()) {
      traceOut(index, img, op);
    }
    if(outc.good()) {
      traceCycleOut(index, img, op);
    }

  }

  virtual uint64_t numCycles() = 0;
  virtual uint64_t finish() {
    outc.flush();
    return numCycles();
  } 

  virtual void calcAccelEnergy(std::string fname_base,int nm) {
    return;
  }

  std::string mcpat_xml_fname;
  std::string mcpat_xml_accel_fname;
  virtual void printMcPATxml(const char* filename,int nm) {
    #include "mcpat-defaults.hh"
    pugi::xml_document doc;
    std::istringstream ss(xml_str);
    pugi::xml_parse_result result = doc.load(ss);
    mcpat_xml_fname=std::string(filename);

    if(result) {
      setEnergyEvents(doc,nm);
    } else {
      std::cerr << "XML Malformed\n";
      return;
    }
    doc.save_file(filename);
  }

  PrismProcessor* mc_proc = NULL; // this is a mcpat processor that accumulates energy
  virtual void setupMcPAT(const char* filename, int nm) {
    _nm=nm;
    printMcPATxml(filename,nm);
    ParseXML* mcpat_xml= new ParseXML(); //Another XML Parser : )
    mcpat_xml->parse((char*)filename);
    mc_proc = new PrismProcessor(mcpat_xml); 
    pumpCoreEnergyEvents(mc_proc,0); //reset all energy stats to 0
  } 

  virtual uint64_t getIntervalBewteenEnergyMeasurements() {
    uint64_t maxCycle = numCycles();
    uint64_t totalCycles = maxCycle - cycles_acc;
    assert(totalCycles >=0);
    cycles_acc = maxCycle;
    return totalCycles;
  }

  virtual void pumpCoreEnergyEvents(PrismProcessor*,uint64_t){}
  virtual void pumpAccelEnergyEvents(uint64_t){}

  virtual void accCoreEnergyEvents(){}
  virtual void accAccelEnergyEvents(){}

  virtual void printAccEnergyEvents(){} //this is for core

  virtual void pumpAccelMcPAT(uint64_t) {}
  virtual void pumpCoreMcPAT(uint64_t totalCycles) {
    pumpCoreEnergyEvents(mc_proc,totalCycles);
    mc_proc->computeEnergy();
  }

  /* use this to set the counters for McPAT */
  virtual void pumpMcPAT() {
    uint64_t totalCycles = getIntervalBewteenEnergyMeasurements();
    pumpCoreMcPAT(totalCycles);
    pumpAccelMcPAT(totalCycles);
  }

  virtual void printMcPAT() {
    mc_proc->computeAccPower(); //need to compute the accumulated power
    double total_dyn_power = mc_proc->core_acc_power.rt_power.readOp.dynamic + 
                      mc_proc->l2_acc_power.rt_power.readOp.dynamic;

    bool long_channel = mc_proc->XML->sys.longer_channel_device;
    
    double total_leakage = (long_channel?mc_proc->power.readOp.longer_channel_leakage:mc_proc->power.readOp.leakage) + mc_proc->power.readOp.gate_leakage;

    cout << _name << " Dynamic Power(" << _nm << ")... " 
                  << total_dyn_power    <<       " " << total_leakage << "\n";

    //mc_proc->displayAccEnergy();
    printMcPAT_Accel();
  }

  virtual void printMcPAT_Accel() { }


  virtual void printAccelMcPATxml(std::string filename,int nm) {
    return;
  }
  //sets a particular energy event attribute of the mcpat xml doc
  //"sa" for conciseness (set attribute)
  static void sa(pugi::xml_node& node, const char* attr, uint64_t val) {
    pugi::xml_node temp;
    temp = node.find_child_by_attribute("name",attr);
    std::stringstream ss;
    ss << val;
    temp.attribute("value").set_value(ss.str().c_str());
  }

  //sets a particular energy event attribute of the mcpat xml doc
  //"sa" for conciseness (set attribute)
  static void sa(pugi::xml_node& node, const char* attr, std::string sval) {
    pugi::xml_node temp;
    temp = node.find_child_by_attribute("name",attr);
    temp.attribute("value").set_value(sval.c_str());
  }

  virtual void set_max_mem_lat(int maxMem) {_max_mem_lat=maxMem;}
  virtual void  set_max_ex_lat(int maxEx)  {_max_ex_lat=maxEx;}
  virtual void  set_nm(int nm)  {_nm=nm;}

  virtual void setEnergyEvents(pugi::xml_document& doc,int nm) {
    std::stringstream ss;
    pugi::xml_node system_node =
      doc.child("component").find_child_by_attribute("name","system");

    uint64_t busyCycles=Prof::get().numCycles-Prof::get().idleCycles;

    sa(system_node,"core_tech_node",nm);
    sa(system_node,"target_core_clockrate",CORE_CLOCK);

    //base stuff
    sa(system_node,"total_cycles",Prof::get().numCycles);
    sa(system_node,"idle_cycles",Prof::get().idleCycles);
    sa(system_node,"busy_cycles",busyCycles);

    pugi::xml_node core_node =
              system_node.find_child_by_attribute("name","core0");

    //set params:
    sa(core_node,"clock_rate",CORE_CLOCK);
    sa(core_node,"fetch_width",FETCH_WIDTH);
    sa(core_node,"decode_width",D_WIDTH);
    sa(core_node,"issue_width",ISSUE_WIDTH);
    sa(core_node,"peak_issue_width",PEAK_ISSUE_WIDTH);
    sa(core_node,"commit_width",COMMIT_WIDTH);
    sa(core_node,"fp_issue_width",N_FPU);

    sa(core_node,"ALU_per_core", N_ALUS);
    sa(core_node,"MUL_per_core", N_MUL);
    sa(core_node,"FPU_per_core", N_FPU);

    sa(core_node,"instruction_window_size", IQ_WIDTH);
    sa(core_node,"fp_instruction_window_size", IQ_WIDTH);
    sa(core_node,"ROB_size", ROB_SIZE);

    sa(core_node,"phy_Regs_IRF_size", PHYS_REGS);
    sa(core_node,"phy_Regs_FRF_size", PHYS_REGS);

    sa(core_node,"store_buffer_size", SQ_SIZE);
    sa(core_node,"load_buffer_size", LQ_SIZE);

    sa(core_node,"instruction_buffer_size", IBUF_SIZE);
    sa(core_node,"decoded_stream_buffer_size", IBUF_SIZE);

    if(!_elide_mem) {
      sa(core_node,"memory_ports", RW_PORTS);
    }
    sa(core_node,"RAS_size", Prof::get().RASSize);

    ss.str("");
    ss << PIPE_DEPTH << "," << PIPE_DEPTH;
    sa(core_node,"pipeline_depth",ss.str());

    if(isInOrder()) {
      sa(core_node,"machine_type",1);
    }

    //set stats:
    sa(core_node,"total_instructions",Prof::get().totalInsts);
    sa(core_node,"int_instructions",Prof::get().intOps);
    sa(core_node,"fp_instructions",Prof::get().fpOps);
    sa(core_node,"branch_instructions",Prof::get().branchPredictions);
    sa(core_node,"branch_mispredictions",Prof::get().mispredicts);
    sa(core_node,"load_instructions",Prof::get().loadOps);
    sa(core_node,"store_instructions",Prof::get().storeOps);

    sa(core_node,"committed_instructions",Prof::get().commitInsts);
    //sa(core_node,"committed_int_instructions",Prof::get().commitIntInsts);
    sa(core_node,"committed_int_instructions",Prof::get().commitInsts-
                                              Prof::get().commitFPInsts);
    sa(core_node,"committed_fp_instructions",Prof::get().commitFPInsts);

    sa(core_node,"total_cycles",Prof::get().numCycles);
    sa(core_node,"idle_cycles",Prof::get().idleCycles);
    sa(core_node,"busy_cycles",busyCycles);

    sa(core_node,"ROB_reads",Prof::get().rob_reads);
    sa(core_node,"ROB_writes",Prof::get().rob_writes);

    sa(core_node,"rename_reads",Prof::get().rename_reads);
    sa(core_node,"rename_writes",Prof::get().rename_writes);
    sa(core_node,"fp_rename_reads",Prof::get().fp_rename_reads);
    sa(core_node,"fp_rename_writes",Prof::get().fp_rename_writes);

    sa(core_node,"inst_window_reads",Prof::get().int_iw_reads);
    sa(core_node,"inst_window_writes",Prof::get().int_iw_writes);
    sa(core_node,"inst_window_wakeup_accesses",Prof::get().int_iw_wakeups);

    sa(core_node,"fp_inst_window_reads",Prof::get().fp_iw_reads);
    sa(core_node,"fp_inst_window_writes",Prof::get().fp_iw_writes);
    sa(core_node,"fp_inst_window_wakeup_accesses",Prof::get().fp_iw_wakeups);

    sa(core_node,"int_regfile_reads",Prof::get().int_regfile_reads);
    sa(core_node,"int_regfile_writes",Prof::get().int_regfile_writes);
    sa(core_node,"float_regfile_reads",Prof::get().fp_regfile_reads);
    sa(core_node,"float_regfile_writes",Prof::get().fp_regfile_writes);

    sa(core_node,"function_calls",Prof::get().func_calls);

    sa(core_node,"ialu_accesses",Prof::get().ialu_ops);
    sa(core_node,"fpu_accesses",Prof::get().fp_alu_ops);
    sa(core_node,"mul_accesses",Prof::get().multOps);

    sa(core_node,"cdb_alu_accesses",Prof::get().ialu_ops);
    sa(core_node,"cdb_fpu_accesses",Prof::get().fp_alu_ops);
    sa(core_node,"cdb_mul_accesses",Prof::get().multOps);

    // ---------- icache --------------

    pugi::xml_node icache_node =
              core_node.find_child_by_attribute("name","icache");
    ss.str("");
    // capacity, block_width, associativity, bank, throughput w.r.t. core clock,
    // latency w.r.t. core clock,output_width, cache policy
    // cache_policy: 0 -- no write or write-though with non-write allocate
    //               1 -- write-back with write-allocate

    ss << Prof::get().icache_size << "," << Prof::get().cache_line_size
      << "," << Prof::get().icache_assoc << "," << 1 /*banks*/ << "," << 4 /*thr*/
      << "," << Prof::get().icache_response_latency
      << "," << 32 /*out*/ << "," << 0 /*policy*/;
    sa(icache_node,"icache_config",ss.str());

    ss.str("");
    //miss_buffer_size(MSHR),fill_buffer_size,
    //prefetch_buffer_size,wb_buffer_size
    ss << Prof::get().icache_mshrs << "," << Prof::get().icache_mshrs << ","
       << Prof::get().icache_mshrs << "," << Prof::get().icache_write_buffers;
    sa(icache_node,"buffer_sizes",ss.str());

    sa(icache_node,"read_accesses",Prof::get().icacheLinesFetched);
    sa(icache_node,"read_misses",Prof::get().icacheMisses);
    //sa(icache_node,"conflicts",Prof::get().icacheReplacements);

    // ---------- dcache --------------

    pugi::xml_node dcache_node = core_node.find_child_by_attribute("name","dcache");

    ss.str("");
    ss << Prof::get().dcache_size << "," << Prof::get().cache_line_size
      << "," << Prof::get().dcache_assoc << "," << 1 /*banks*/ << "," << 4 /*thr*/
      << "," << Prof::get().dcache_response_latency
      << "," << 32 /*out*/ << "," << 1 /*policy*/;
    sa(dcache_node,"dcache_config",ss.str());

    ss.str("");
    ss << L1_MSHRS << "," << L1_MSHRS << "," << L1_MSHRS << "," 
       << Prof::get().dcache_write_buffers;
    sa(dcache_node,"buffer_sizes",ss.str());

    sa(dcache_node,"read_accesses",Prof::get().dcacheReads);
    sa(dcache_node,"write_accesses",Prof::get().dcacheWrites);
    sa(dcache_node,"read_misses",Prof::get().dcacheReadMisses);
    sa(dcache_node,"write_misses",Prof::get().dcacheWriteMisses);
    //sa(dcache_node,"conflicts",Prof::get().dcacheReplacements);

    // ---------- L2 --------------
    pugi::xml_node l2_node = system_node.find_child_by_attribute("name","L20");

    ss.str("");
    ss << Prof::get().l2_size << "," << Prof::get().cache_line_size
      << "," << Prof::get().l2_assoc << "," << 8 /*banks*/ << "," << 8 /*thr*/
      << "," << Prof::get().l2_response_latency
      << "," << 32 /*out*/ << "," << 1 /*policy*/;
    sa(l2_node,"L2_config",ss.str());
    //sa(l2_node,"clockrate",CORE_CLOCK);

    ss.str("");
    ss << Prof::get().l2_mshrs << "," << Prof::get().l2_mshrs << ","
       << Prof::get().l2_mshrs << "," << Prof::get().l2_write_buffers;
    sa(l2_node,"buffer_sizes",ss.str());

    sa(l2_node,"read_accesses",Prof::get().l2Reads);
    sa(l2_node,"write_accesses",Prof::get().l2Writes);
    sa(l2_node,"read_misses",Prof::get().l2ReadMisses);
    sa(l2_node,"write_misses",Prof::get().l2WriteMisses);
    //sa(l2_node,"conflicts",Prof::get().l2Replacements);
  }

  template <class T, class E>
  void dumpInst(std::shared_ptr<dg_inst<T, E> >inst) {
    dumpInst(inst.get());
  }


  template <class T, class E>
  void dumpInst(dg_inst<T, E> *inst) {
    if (!inst) {
      std::cout << "<null>\n";
      return;
    }
    for (unsigned j = 0; j < inst->numStages(); ++j) {
      std::cout << std::setw(5) << inst->cycleOfStage(j) << " ";
    }

    std::cout << (inst->_isload?"L":" ")
              << (inst->_isstore?"S":" ")
              << ((inst->_cache_prod && inst->_true_cache_prod)
                  ?"T"
                  :" ")
              << (inst->_isctrl?"C":" ")
              << (inst->_ctrl_miss?"M": " ")
              << (inst->_floating?"f": " ")
              << " ";
    if (inst->hasDisasm()) {
      std::cout << inst->getDisasm() << "\n";
      return ;
    }

    printDisasmPC(inst->_pc, inst->_upc);
  }


  virtual void printDisasmPC(uint64_t pc, int upc) {
    std::cout << pc << "," << upc << " : "
              << ExecProfile::getDisasm(pc, upc) << "\n";
  }

};

#endif
