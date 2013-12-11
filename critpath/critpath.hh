#ifndef CRITICAL_PATH_HH
#define CRITICAL_PATH_HH

#include "cp_dep_graph.hh"
#include "op.hh"
#include "exec_profile.hh"

#include "pugixml/pugixml.hpp"
#include <stdio.h>
#include <ostream>
#include <fstream>


#include "prof.hh"

// abstract class for all critical path
class CriticalPath //: public insert_event_handler<dg_inst<T,E>>
{
protected:
  std::string _run_name = "";
  std::string _name = "";
  uint64_t _last_index;
  bool _isInOrder = false;
  bool _setInOrder = false;

  //careful, these are not necessarily the defaults...
  //we will try to load the values from m5out/config.ini
  //as the defaults through the prof class
  int FETCH_WIDTH = 4;
  int D_WIDTH = 4;
  int ISSUE_WIDTH = 4;
  int PEAK_ISSUE_WIDTH = 6;
  int WRITEBACK_WIDTH = 4;
  int COMMIT_WIDTH = 4;
  int SQUASH_WIDTH = 4;

  int IQ_WIDTH = 64;
  int ROB_SIZE = 192;
  int LQ_SIZE = 32;
  int SQ_SIZE = 32;

  int FETCH_TO_DISPATCH_STAGES = 4;
  int INORDER_EX_DEPTH = 4; 

  int N_ALUS=6;
  int N_MUL=1;
  int N_FPU=2;
  int N_MUL_FPU=2;
  int RW_PORTS=2;

  int IBUF_SIZE=32;
  int PIPE_DEPTH=16;      
  int PHYS_REGS=256;

  bool applyMaxLatToAccel=true;
  int _max_mem_lat=1073741824; //some big numbers that will never make sense
  int _max_ex_lat=1073741824;
  int _nm=22;

  //------- energy events -----------
  uint64_t committed_insts=0, committed_int_insts=0, committed_fp_insts=0;
  uint64_t committed_branch_insts=0, mispeculatedInstructions=0;
  uint64_t committed_load_insts=0, committed_store_insts=0;
  uint64_t squashed_insts=0;

  uint64_t int_ops=0,mult_ops=0,fp_ops=0;
  uint64_t func_calls=0;
  uint64_t idleCycles=0;
  uint64_t nonPipelineCycles=0;


  //icache
  uint64_t icache_read_accesses=0, icache_read_misses=0, icache_conflicts=0;

  //ooo-pipeline
  uint64_t regfile_reads=0, regfile_writes=0, regfile_freads=0, regfile_fwrites=0;
  uint64_t rob_reads=0, rob_writes=0;
  uint64_t iw_reads=0, iw_writes=0, iw_freads=0, iw_fwrites=0;
  uint64_t rename_reads=0, rename_writes=0;
  uint64_t rename_freads=0, rename_fwrites=0;

  uint64_t btb_read_accesses=0, btb_write_accesses=0;

  virtual void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) = 0;


  //cur_fi will always be on
  //cur_li will either be null or have a loop
  FunctionInfo* cur_fi;
  LoopInfo* cur_li;
  std::map<FunctionInfo*,uint64_t> cycleMapFunc;
  std::map<LoopInfo*,uint64_t> cycleMapLoop;
  BB* cur_bb;

  uint64_t  cur_cycles=0;
  //Update cycles spent in this config
public:
  virtual void update_cycles(Op* op) {
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
        //get the current cycle
        uint64_t cycles=this->numCycles();
        uint64_t cycle_diff = cycles - cur_cycles;

        /*
        if(cycles < cur_cycles) {
          if(cycles + 200 < cur_cycles) {
            std::cerr << "ERROR: WEIRD CYCLE COUNTS!" << "\n";
            std::cerr << "previous last cycle: " << cur_cycles << "\n";
            std::cerr << "new last cycle: " << cycles << "\n";
          }
          cycle_diff=0;
        }*/
        
        if(cycles < cur_cycles || cycles     >= cur_cycles+ ((uint64_t)-1)/2 
                               || cycle_diff >= ((uint64_t)-1)/2 ) {
          std::cerr << "ERROR: WEIRD CYCLE COUNTS!" << "\n";
          std::cerr << "previous last cycle: " << cur_cycles << "\n";
          std::cerr << "new last cycle: " << cycles << "\n";
          assert(0);
        }

        cur_cycles=cycles;
  
        std::cout << "BBs: [" << cur_bb->rpoNum() << "->" << op->bb()->rpoNum()  << ")";
       
        std::cout << "contrib " << cycle_diff << " to ";
        if(cur_li) {
          std::cout  << cur_li->nice_name();
          cycleMapLoop[cur_li]+=cycle_diff;
        } else {
          std::cout  << cur_fi->nice_name();
          cycleMapFunc[cur_fi]+=cycle_diff;
        }
        std::cout << "\n";
      }
    } 

    cur_bb=op->bb();
    cur_li=li;
    cur_fi=fi;
  }

  void printRegionBreakdown(std::ostream& outf) {
    //We need to print out loops and funcions
    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      outf << std::setw(30) << fi->nice_name() << " ";
      outf << cycleMapFunc[fi] << "\n";

      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        outf << std::setw(30) << li->nice_name() << " ";
        outf << cycleMapLoop[li] << "\n";
      }
    }


  }

private:
  bool TraceOutputs = false;
  std::ofstream out;
  bool triedToOpenOutOnce = false;

protected:
  bool getTraceOutputs() const { return TraceOutputs; }

  std::ofstream &outs() {
    if (out.good() || triedToOpenOutOnce)
      return out;

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

  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img, Op* op)
  {
    if (!TraceOutputs)
      return;

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
    accelSpecificStats(out, name);
    out << "\n";
  }

  virtual void accelSpecificStats(std::ostream& out, std::string &name) {
  }

  void setTraceOutputs(bool t) {
    TraceOutputs = t;
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

 
    //N_ALUS=std::min(std::max(1,ISSUE_WIDTH*3/4),6);
    //N_FPU=std::min(std::max(1,ISSUE_WIDTH/2),2);
    //RW_PORTS=std::min(std::max(1,ISSUE_WIDTH/2),2);


  }

  virtual ~CriticalPath() {
  }

  void setInOrder(bool inOrder) {
    assert(!_setInOrder),
    _isInOrder=inOrder;
    _setInOrder=true;
  }

 

  bool isInOrder() {assert(_setInOrder); return _isInOrder;}

  virtual void setWidth(int i) {
    return;
  }

  virtual void insert(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    insert_inst(img,index,op);
    _last_index=index;
    if(out.good()) {
      traceOut(index, img, op);
    }
  }

  virtual uint64_t numCycles() = 0;
  virtual uint64_t finish() {return numCycles();} //default does nothing

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

    //pugi::xml_parse_result result = doc.load_file("tony-ooo.xml");
    if(result) {
      setEnergyEvents(doc,nm);
    } else {
      std::cerr << "XML Malformed\n";
      return;
    }
    doc.save_file(filename);
  }

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

    //base stuff
    sa(system_node,"total_cycles",Prof::get().numCycles);
    sa(system_node,"idle_cycles",Prof::get().idleCycles);
    sa(system_node,"busy_cycles",busyCycles);

    pugi::xml_node core_node =
              system_node.find_child_by_attribute("name","core0");

    //set params:
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

    sa(core_node,"memory_ports", RW_PORTS);
    sa(core_node,"RAS_size", Prof::get().RASSize);

    //set stats:
    if(isInOrder()) {
      sa(core_node,"machine_type",1);
    }

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

    ss.str("");
    ss << PIPE_DEPTH << "," << PIPE_DEPTH;
    sa(core_node,"pipeline_depth",ss.str());

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
    sa(icache_node,"conflicts",Prof::get().icacheReplacements);

    // ---------- dcache --------------

    pugi::xml_node dcache_node =
              core_node.find_child_by_attribute("name","dcache");

    ss.str("");
    ss << Prof::get().dcache_size << "," << Prof::get().cache_line_size
      << "," << Prof::get().dcache_assoc << "," << 1 /*banks*/ << "," << 4 /*thr*/
      << "," << Prof::get().dcache_response_latency
      << "," << 32 /*out*/ << "," << 1 /*policy*/;
    sa(dcache_node,"dcache_config",ss.str());

    ss.str("");
    ss << Prof::get().dcache_mshrs << "," << Prof::get().dcache_mshrs << ","
       << Prof::get().dcache_mshrs << "," << Prof::get().dcache_write_buffers;
    sa(dcache_node,"buffer_sizes",ss.str());

    sa(dcache_node,"read_accesses",Prof::get().dcacheReads);
    sa(dcache_node,"write_accesses",Prof::get().dcacheWrites);
    sa(dcache_node,"read_misses",Prof::get().dcacheReadMisses);
    sa(dcache_node,"write_misses",Prof::get().dcacheWriteMisses);
    sa(dcache_node,"conflicts",Prof::get().dcacheReplacements);

    // ---------- L2 --------------
    pugi::xml_node l2_node =
              system_node.find_child_by_attribute("name","L20");

    ss.str("");
    ss << Prof::get().l2_size << "," << Prof::get().cache_line_size
      << "," << Prof::get().l2_assoc << "," << 8 /*banks*/ << "," << 8 /*thr*/
      << "," << Prof::get().l2_response_latency
      << "," << 32 /*out*/ << "," << 1 /*policy*/;
    sa(l2_node,"L2_config",ss.str());

    ss.str("");
    ss << Prof::get().l2_mshrs << "," << Prof::get().l2_mshrs << ","
       << Prof::get().l2_mshrs << "," << Prof::get().l2_write_buffers;
    sa(l2_node,"buffer_sizes",ss.str());

    sa(l2_node,"read_accesses",Prof::get().l2Reads);
    sa(l2_node,"write_accesses",Prof::get().l2Writes);
    sa(l2_node,"read_misses",Prof::get().l2ReadMisses);
    sa(l2_node,"write_misses",Prof::get().l2WriteMisses);
    sa(l2_node,"conflicts",Prof::get().l2Replacements);
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
