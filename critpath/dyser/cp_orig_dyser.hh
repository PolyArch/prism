
#ifndef CP_ORIG_DYSER_HH
#define CP_ORIG_DYSER_HH

#include "cp_args.hh"
#include "origcp.hh"
#include "cp_utils.hh"

#include "prof.hh"

namespace DySER {

class cp_orig_dyser: public OrigCP {

public:
  cp_orig_dyser() : OrigCP() {}

  void insert_inst(const CP_NodeDiskImage &img, uint64_t index, Op* op) {
    OrigCP::insert_inst(img, index, op);
  }

  virtual void calcAccelEnergy(std::string fname_base, int nm) {
    std::string fname=fname_base + std::string(".accel");

    std::string outf = fname + std::string(".out");
    std::cout <<  " dyser accel(" << nm << "nm)... ";
    std::cout.flush();

    execMcPAT(fname, outf);
    float ialu  = std::stof(grepF(outf,"Integer ALUs",7,5));
    float fpalu = std::stof(grepF(outf,"Floating Point Units",7,5));
    float calu  = std::stof(grepF(outf,"Complex ALUs",7,5));
    float reg   = std::stof(grepF(outf,"Register Files",7,5)) * 4;
    float total = ialu + fpalu + calu + reg;
    std::cout << total << "  (ialu: " <<ialu << ", fp: " << fpalu << ", mul: " << calu << ", reg: " << reg << ")\n";
  }


  // Handle enrgy events for McPAT XML DOC
  virtual void printAccelMcPATxml(std::string fname_base, int nm) {
      uint64_t _dyser_int_ops =
        Prof::get().getStatFromMap<uint64_t>("dyIntOps");

      uint64_t _dyser_fp_ops =
        Prof::get().getStatFromMap<uint64_t>("dyFloatOps");
      uint64_t _dyser_mult_ops =
        Prof::get().getStatFromMap<uint64_t>("dyMultOps");
      uint64_t _dyser_regfile_reads =
        Prof::get().getStatFromMap<uint64_t>("numDyIntReads");
      uint64_t _dyser_regfile_writes =
        Prof::get().getStatFromMap<uint64_t>("numDyIntWrites");
      uint64_t _dyser_floatreg_reads =
        Prof::get().getStatFromMap<uint64_t>("numDyFloatReads");
      uint64_t _dyser_floatreg_writes =
        Prof::get().getStatFromMap<uint64_t>("numDyFloatWrites");

   #include "mcpat-defaults.hh"
    pugi::xml_document accel_doc;
    std::istringstream ss(xml_str);
    pugi::xml_parse_result result = accel_doc.load(ss);

    if (!result) {
      std::cerr << "XML Malformed\n";
      return;
    }

      pugi::xml_node system_node =
        accel_doc.child("component").find_child_by_attribute("name", "system");

      //set the total_cycles so that we get power correctly
      sa(system_node, "total_cycles", numCycles());
      sa(system_node, "busy_cycles", 0);
      sa(system_node, "idle_cycles", numCycles());

      sa(system_node, "core_tech_node", nm);
      sa(system_node, "device_type", 0);


      pugi::xml_node core_node =
        system_node.find_child_by_attribute("name", "core0");
      sa(core_node, "total_cycles", numCycles());
      sa(core_node, "busy_cycles", 0);
      sa(core_node, "idle_cycles", numCycles());

      sa(core_node, "ALU_per_core", Prof::get().int_alu_count);
      sa(core_node, "MUL_per_core", Prof::get().mul_div_count);
      sa(core_node, "FPU_per_core", Prof::get().fp_alu_count);

      sa(core_node, "ialu_accesses", _dyser_int_ops);
      sa(core_node, "fpu_accesses", _dyser_fp_ops);
      sa(core_node, "mul_accesses", _dyser_mult_ops);

      sa(core_node, "archi_Regs_IRF_size", 8);
      sa(core_node, "archi_Regs_FRF_size", 8);
      sa(core_node, "phy_Regs_IRF_size", 64);
      sa(core_node, "phy_Regs_FRF_size", 64);

      sa(core_node, "int_regfile_reads", _dyser_regfile_reads);
      sa(core_node, "int_regfile_writes", _dyser_regfile_writes);
      sa(core_node, "float_regfile_reads", _dyser_floatreg_reads);
      sa(core_node, "float_regfile_writes", _dyser_floatreg_writes);

      std::string fname=fname_base + std::string(".accel");
      accel_doc.save_file(fname.c_str());
    }

};
} // End namespace DySER

#endif
