#ifndef CRITICAL_PATH_HH
#define CRITICAL_PATH_HH

//#include "cpnode.hh"
//#include "myqueue_impl.hh"
#include "cp_dep_graph.hh"
#include "op.hh"

#include "pugixml/pugixml.hpp"
#include "mcpat-defaults.hh"

#include <stdio.h>
#include <ostream>
#include <fstream>

extern int TraceOutputs; 

#include "prof.hh"

// abstract class for all critical path
class CriticalPath //: public insert_event_handler<dg_inst<T,E>>
{
protected:
  uint64_t _last_index;
  bool _isInOrder;
  bool _setInOrder=false;

  //------- energy events -----------
  uint64_t committed_insts=0, committed_int_insts=0, committed_fp_insts=0;
  uint64_t committed_branch_insts=0, mispeculatedInstructions=0;
  uint64_t committed_load_insts=0, committed_store_insts=0;
  uint64_t squashed_insts=0;

  uint64_t mult_ops=0;
  uint64_t func_calls=0;
  uint64_t idleCycles=0;

  //icache
  uint64_t icache_read_accesses=0, icache_read_misses=0, icache_conflicts=0;

  //ooo-pipeline
  uint64_t regfile_reads=0, regfile_writes=0, regfile_freads=0, regfile_fwrites=0;
  uint64_t rob_reads=0, rob_writes=0;
  uint64_t iw_reads=0, iw_writes=0, iw_freads=0, iw_fwrites=0;
  uint64_t rename_reads=0, rename_writes=0;
  uint64_t rename_freads=0, rename_fwrites=0;

  uint64_t btb_read_accesses=0, btb_write_accesses=0;

  std::ofstream out;
  
  virtual void insert_inst(const CP_NodeDiskImage &img, uint64_t index,Op* op) = 0;

  virtual void traceOut(uint64_t index, const CP_NodeDiskImage &img, 
                        Op* op) {
    if(TraceOutputs) {
/*      if(img._wc) {
        out << img._wc  << " ";
        out << img._xc  << " ";
      }*/

      out << (img._isctrl ? "C" : "");
      out << (img._isreturn ? "Ret" : "");
      out << (img._iscall ? "Func" : "");
      out << (img._isload ? "L" : "");
      out << (img._isstore ? "S" : "");
      out << (img._ctrl_miss ? "ms" : "");
      out << (img._spec_miss ? "msSPC" : "");
      out << (img._serialBefore ? "sb!" : "");
      out << (img._serialAfter ? "sa!" : "");
      out << (img._nonSpec ? "ns!" : "");
      out << (img._storeCond ? "sc!" : "");
      out << (img._prefetch ? "pref!" : "");
      out << (img._squashAfter ? "sqa!" : "");
      out << (img._writeBar ? "wbar!" : "");
      out << (img._memBar  ? "mbar!" : "");
      out << (img._syscall ? "sys!" : "");
      out << (img._floating ? "FP" : "");
     

      if(img._mem_prod != 0) {
        out << "md(" << img._mem_prod << ")";
      }

      if(img._icache_lat >0) {
        out << "I$" << img._icache_lat;
      }
    }
  }

public:
  virtual void accelSpecificStats(std::ostream& out) {
  }

  void setupOutFile(std::string file) {
    out.open(file.c_str(), std::ofstream::out | std::ofstream::trunc);
  }
  virtual ~CriticalPath() {
  }

  void setInOrder(bool inOrder) {
    assert(!_setInOrder), 
    _isInOrder=inOrder; 
    _setInOrder=true;
  }
  bool isInOrder() {assert(_setInOrder); return _isInOrder;}

  virtual void insert(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    insert_inst(img,index,op);
    _last_index=index;
    if(out.good()) {
      traceOut(index, img, op);
    }
  }

  virtual uint64_t numCycles()=0;

  virtual void printMcPATxml(const char* filename) {
    pugi::xml_document doc;
    std::istringstream ss(xml_str);
    pugi::xml_parse_result result = doc.load(ss);

    //pugi::xml_parse_result result = doc.load_file("tony-ooo.xml");
    if(result) {
      setEnergyEvents(doc);
    } else {
      std::cerr << "XML Malformed\n";
      return;
    }
    doc.save_file(filename);
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
  
  virtual void setEnergyEvents(pugi::xml_document& doc) {
    pugi::xml_node system_node = doc.child("component").find_child_by_attribute("name","system");

    uint64_t busyCycles=Prof::get().numCycles-Prof::get().idleCycles;

    //base stuff 
    sa(system_node,"total_cycles",Prof::get().numCycles);
    sa(system_node,"idle_cycles",Prof::get().idleCycles);
    sa(system_node,"busy_cycles",busyCycles);

    pugi::xml_node core_node = 
              system_node.find_child_by_attribute("name","core0");

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

    pugi::xml_node icache_node = 
              core_node.find_child_by_attribute("name","icache");

    sa(icache_node,"read_accesses",Prof::get().icacheLinesFetched);
    sa(icache_node,"read_misses",Prof::get().icacheMisses);
    sa(icache_node,"conflicts",Prof::get().icacheReplacements);

    pugi::xml_node dcache_node = 
              core_node.find_child_by_attribute("name","dcache");

    sa(dcache_node,"read_accesses",Prof::get().dcacheReads);
    sa(dcache_node,"write_accesses",Prof::get().dcacheWrites);
    sa(dcache_node,"read_misses",Prof::get().dcacheReadMisses);
    sa(dcache_node,"write_misses",Prof::get().dcacheWriteMisses);
    sa(dcache_node,"conflicts",Prof::get().dcacheReplacements);

    pugi::xml_node l2_node = 
              system_node.find_child_by_attribute("name","L20");

    sa(l2_node,"read_accesses",Prof::get().l2Reads);
    sa(l2_node,"write_accesses",Prof::get().l2Writes);
    sa(l2_node,"read_misses",Prof::get().l2ReadMisses);
    sa(l2_node,"write_misses",Prof::get().l2WriteMisses);
    sa(l2_node,"conflicts",Prof::get().l2Replacements);

  }

};

#endif
