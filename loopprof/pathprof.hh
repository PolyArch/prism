#ifndef PATHPROF_HH
#define PATHPROF_HH

#include <unordered_map>
#include <utility>
#include <stdint.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <ostream>
#include <algorithm>

#include <map>
#include <list>
#include <vector>
#include <set>
#include <bitset>
#include <string>
#include <assert.h>
#include "cpu/crtpath/crtpathnode.hh"
#define MAX_OPS 1000

#include "op.hh"
#include "bb.hh"
#include "functioninfo.hh"
#include "loopinfo.hh"

#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
//#include <boost/serialization/unordered_map.hpp>

#include <memory> //for shared ptr
#include "elfparse.hh"

/*
class TransInfo {
private:
  Op* _op;
  BB* _bb;
  FunctionInfo* _func;

  bool 
public:
  Trans

  ol funcCalled() {return _funcCalled == NULL;}
};*/

struct MemDep {
  uint32_t dId;
};

class StackFrame {
public:

private:
  FunctionInfo* _funcInfo;

  //phase 1 info
  BB* _prevBB=NULL;

  //phase 2 info
  LoopIter::LoopStack _loopStack;
  std::vector<std::shared_ptr<LoopIter>> _iterStack;
  std::map<uint32_t,std::shared_ptr<LoopIter>> _iterMap; //maps dId to LoopIteration
  
  LoopInfo::BBvec _loopPath;
  int _pathIndex=0;

  bool _isRecursing=false;
  bool _isDirectRecursing=false;

  std::unordered_map<uint64_t,MemDep> giganticMemDepTable;

public:
  StackFrame(FunctionInfo* fi, uint32_t dId) : _funcInfo(fi), _prevBB(NULL), _pathIndex(0) {
    //create dummy _iterStack for non-loops
    _iterStack.emplace_back(new LoopIter(_loopStack));
    //also, put a marker in the _iterMap, at the begining
    _iterMap.emplace(std::piecewise_construct,
                     std::forward_as_tuple(dId),
		     std::forward_as_tuple(_iterStack.back()));
  }
  
  bool isLooping() {
    return _loopStack.size()>0;
  }

  FunctionInfo* funcInfo() {
    return _funcInfo;
  }

  void processBB_phase1(CPC headCPC, CPC tailCPC) {
    _prevBB=_funcInfo->addBB(_prevBB,headCPC,tailCPC);
  }

  void setRecursing(bool b) {_isRecursing=b;}
  bool isRecursing() {return _isRecursing;}

  void setDirectRecursing(bool b) {_isDirectRecursing=b;}
  bool isDirectRecursing() {return _isDirectRecursing;}

  void dyn_dep(Op* op, uint32_t ind, bool isMem);

  //Process op, determine if it is a bb or not
  Op* processOp_phase2(uint32_t dId, CPC cpc,uint64_t addr, uint8_t acc_size);
  void processBB_phase2(uint32_t dId, BB* bb,bool profile=true);

  Op* processOp_phase3(uint32_t dId, CPC cpc);

};


//Path Profiler
class PathProf {
public:
  uint32_t maxOps=0;
  uint32_t maxBBs=0;
  uint32_t maxLoops=0;
  uint32_t maxFuncs=0;

  uint64_t startInst=0;
  uint64_t stopInst=0;
  uint64_t skipInsts=0;
 
  typedef std::map<CPC,FunctionInfo*>  FuncMap;
  SYM_TAB sym_tab;

  //some stats for loops/recursion
  uint64_t insts_in_beret=0;
  uint64_t insts_in_simple_inner_loop=0;
  uint64_t insts_in_inner_loop=0;
  uint64_t insts_in_all_loops=0;
  uint64_t non_loop_insts_direct_recursion=0;
  uint64_t non_loop_insts_any_recursion=0;

  //stats from m5out/stats.txt -- memory stats must be put here
  uint64_t numCycles=0,idleCycles=0,totalInsts=0;
  uint64_t loadOps=0,storeOps=0,nOps=0,aluOps=0,multOps=0,divOps=0;
  uint64_t faddOps=0,fcvtOps=0,fcmpOps=0,fmultOps=0,fdivOps=0,fsqrtOps=0;
  uint64_t branchPredictions=0, mispredicts=0;
  uint64_t rob_reads=0,rob_writes=0;
  uint64_t rename_reads=0,rename_writes=0,fp_rename_reads=0,fp_rename_writes=0;
  uint64_t int_iw_reads=0,int_iw_writes=0,int_iw_wakeups=0;
  uint64_t fp_iw_reads=0,fp_iw_writes=0,fp_iw_wakeups=0;
  uint64_t int_regfile_reads=0,int_regfile_writes=0;
  uint64_t fp_regfile_reads=0,fp_regfile_writes=0;
  uint64_t func_calls=0;
  uint64_t ialu_ops=0, fp_alu_ops=0;
  uint64_t icacheLinesFetched=0; 
  uint64_t commitInsts=0,commitIntInsts=0,commitFPInsts=0;
  uint64_t commitBranches=0, commitBranchMispredicts=0;
  uint64_t commitLoads=0,commitStores=0,commitMemRefs=0;
  uint64_t icacheMisses=0,icacheReplacements=0;
  uint64_t dcacheReads=0,dcacheWrites=0;
  uint64_t dcacheReadMisses=0,dcacheWriteMisses=0,dcacheReplacements=0;
  uint64_t l2Reads=0,l2Writes=0;
  uint64_t l2ReadMisses=0,l2WriteMisses=0,l2Replacements=0;
  uint64_t intOps=0,fpOps=0;

private:
  //KNOWN_ISSUE: FunctionInfo's are not deleted

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    maxOps   = Op::_idcounter;
    maxBBs   = BB::_idcounter;
    maxLoops = LoopInfo::_idcounter;
    maxFuncs = FunctionInfo::_idcounter;
    ar & sym_tab;
    ar & _funcMap;
    ar & maxOps;
    ar & maxBBs;
    ar & maxLoops;
    ar & maxFuncs;
    ar & startInst;
    ar & stopInst;
    ar & skipInsts;
    ar & _origstack;
    ar & _origPrevCtrl;
    ar & _origPrevCall;
    ar & _origPrevRet;
    ar & _origPrevCPC;
    Op::_idcounter           = maxOps;
    BB::_idcounter           = maxBBs; 
    LoopInfo::_idcounter     = maxLoops;
    FunctionInfo::_idcounter = maxFuncs;
  }

  bool _origPrevCtrl;
  bool _origPrevCall;
  bool _origPrevRet;
  CPC _origPrevCPC;
  std::vector<uint64_t> _origstack;

  FuncMap _funcMap;
  std::list<StackFrame> _callStack;

  //Phase 1 Processing
  CPC _prevHead;

  void checkRecursion();
  bool adjustStack(CPC newCPC, bool isCall, bool isRet);

  //Phase 2 Processing
  int _dId;
  Op* _op_buf[MAX_OPS];

  //get a token from a stream
  static bool getToken(std::istringstream& iss, std::string& thing) {
    bool valid = true; 
    do {
      valid = std::getline( iss, thing , ' ');
    } while(valid && thing.size() == 0);
    return valid;
  }

  static void getStat(const char* tag_str, std::string& tag, 
               std::string& val, uint64_t& stat) {
    if(tag.find(tag_str) != std::string::npos) {
      stat = std::stoul(val);
    }
  }

  static bool to_bool(std::string const& s) {
    return s != "0";
  }

public:
  PathProf() {
    _dId=0;
  }

  void procSymTab(char* filename) {
    ELF_parser::read_symbol_tables(filename,sym_tab);
  }

  void procStatsFile(const char* filename) {
    std::string line;
    std::ifstream ifs(filename);

    if(!ifs.good()) {
      std::cerr << filename << " doesn't look good";
      return;
    }

    while(std::getline(ifs, line)) {
      std::istringstream iss( line );
     
      std::string tag,val;
      if( getToken(iss, tag) && getToken(iss,val) ) {
        if( tag.find("switch_cpus") != std::string::npos ) {
          getStat("numCycles",tag,val,numCycles);
          getStat("idleCycles",tag,val,idleCycles);

          getStat("iq.FU_type_0::total",tag,val,totalInsts);

          getStat("iq.FU_type_0::MemRead",tag,val,loadOps);
          getStat("iq.FU_type_0::MemWrite",tag,val,storeOps);

          getStat("iq.FU_type_0::No_OpClass",tag,val,nOps);
          getStat("iq.FU_type_0::IntAlu",tag,val,aluOps);
          getStat("iq.FU_type_0::IntMult",tag,val,multOps);
          getStat("iq.FU_type_0::IntDiv",tag,val,divOps);

          getStat("iq.FU_type_0::FloatAdd",tag,val,faddOps);
          getStat("iq.FU_type_0::FloatCmp",tag,val,fcmpOps);
          getStat("iq.FU_type_0::FloatCvt",tag,val,fcvtOps);
          getStat("iq.FU_type_0::FloatMult",tag,val,fmultOps);
          getStat("iq.FU_type_0::FloatDiv",tag,val,fdivOps);
          getStat("iq.FU_type_0::FloatSqrt",tag,val,fsqrtOps);

          getStat("branchPred.lookups",tag,val,branchPredictions);
          getStat("branchPred.condIncorrect",tag,val,mispredicts);

          getStat("rob.rob_reads",tag,val,rob_reads);
          getStat("rob.rob_writes",tag,val,rob_writes);

          getStat("rename.int_rename_lookups",tag,val,rename_reads);
          getStat("rename.int_rename_operands",tag,val,rename_writes);

          getStat("rename.fp_rename_lookups",tag,val,fp_rename_reads);
          getStat("rename.fp_rename_operands",tag,val,fp_rename_writes);

          getStat("iq.int_inst_queue_reads",tag,val,int_iw_reads);
          getStat("iq.int_inst_queue_writes",tag,val,int_iw_writes);
          getStat("iq.int_inst_queue_wakeup_accesses",tag,val,int_iw_wakeups);

          getStat("iq.fp_inst_queue_reads",tag,val,fp_iw_reads);
          getStat("iq.fp_inst_queue_writes",tag,val,fp_iw_writes);
          getStat("iq.fp_inst_queue_wakeup_accesses",tag,val,fp_iw_wakeups); 

          getStat("int_regfile_reads",tag,val,int_regfile_reads);
          getStat("int_regfile_writes",tag,val,int_regfile_writes);

          getStat("fp_regfile_reads",tag,val,fp_regfile_reads);
          getStat("fp_regfile_writes",tag,val,fp_regfile_writes);

          getStat("function_calls",tag,val,func_calls);

          getStat("iq.int_alu_accesses",tag,val,ialu_ops);
          getStat("iq.fp_alu_accesses",tag,val,fp_alu_ops);

          getStat("fetch.CacheLines",tag,val,icacheLinesFetched);

          getStat("committedOps",tag,val,commitInsts);
          getStat("commit.int_insts",tag,val,commitIntInsts);
          getStat("commit.fp_insts",tag,val,commitFPInsts);
          getStat("commit.branches",tag,val,commitBranches);
          getStat("commit.branchMispredicts",tag,val,commitBranchMispredicts);
          getStat("commit.loads",tag,val,commitLoads);
          getStat("commit.refs",tag,val,commitMemRefs);
        } else {
          getStat("icache.overall_misses",tag,val,icacheMisses);
          getStat("icache.replacements",tag,val,icacheReplacements);

          getStat("dcache.ReadReq_accesses",tag,val,dcacheReads);
          getStat("dcache.WriteReq_accesses",tag,val,dcacheWrites);
          getStat("dcache.ReadReq_misses",tag,val,dcacheReadMisses);
          getStat("dcache.WriteReq_misses",tag,val,dcacheWriteMisses);
          getStat("dcache.replacements",tag,val,dcacheReplacements);

          getStat("l2.ReadReq_accesses",tag,val,l2Reads); 
          getStat("l2.WriteReq_accesses",tag,val,l2Writes); 
          getStat("l2.ReadReq_misses",tag,val,l2ReadMisses); 
          getStat("l2.WriteReq_misses",tag,val,l2WriteMisses); 
          getStat("l2.replacements",tag,val,l2Replacements); 
        }
      }
    }
    intOps=nOps+aluOps+multOps+divOps;
    fpOps=faddOps+fcmpOps+fcvtOps+fmultOps+fdivOps+fsqrtOps;
    commitStores=commitMemRefs-commitLoads;

    #if 0
    std::cout << "Example " << int_iw_reads << ", ";
    std::cout << dcacheReads << ", ";
    std::cout << storeOps << ","; 
    std::cout << "\n";
    #endif
  }


  void procStackFile(const char* filename) {
    std::string line,line1,line2;
    std::ifstream ifs(filename);

    if(!ifs.good()) {
      std::cerr << filename << " doesn't look good";
      return;
    }

    std::getline(ifs,line1);
    std::getline(ifs,line2);
    std::cout << "line1: \"" << line1 << "\"\n";
    _origPrevCPC = std::make_pair(std::stoul(line1), (uint16_t)std::stoi(line2));

    std::getline(ifs,line1);
    _origPrevCtrl = to_bool(line1);

    std::getline(ifs,line1);
    _origPrevCall = to_bool(line1);

    std::getline(ifs,line1);
    _origPrevRet = to_bool(line1);

    if(ifs) {
      while(std::getline(ifs, line)) {
        _origstack.push_back(std::stoul(line)); 
      }
    }
  }

  FunctionInfo* getOrAddFunc(CPC newCPC);
  void setStartInst(uint64_t count) {startInst=count;}
  void setStopInst(uint64_t count) {stopInst=count;}
  void setSkipInsts(uint64_t count) {skipInsts=count;}

  void processOpPhase1(CPC prevCPC, CPC newCPC, bool isCall, bool isRet);
  void runAnalysis();
  void runAnalysis2(bool no_gams, bool gams_details);

  void processOpPhase2(CPC prevCPC, CPC newCPC, bool isCall, bool isRet,
                       CP_NodeDiskImage& img);
  Op* processOpPhase3(CPC newCPC, bool wasCalled, bool wasReturned);

  FuncMap::iterator fbegin() {return _funcMap.begin();}
  FuncMap::iterator fend() {return _funcMap.end();}

  //void initCPC(CPC cpc) {_prevHead=cpc;}
  void printInfo();
  void printCFGs(std::string& dir) {
    std::cout << "Printing " 
              << FunctionInfo::_idcounter << " Functions, "
              << LoopInfo::_idcounter << " Loops, "
              << BB::_idcounter << " BBs, "
              << Op::_idcounter << " Ops\n";
 
    for(auto i=_funcMap.begin(),e=_funcMap.end();i!=e;++i) {
      FunctionInfo& fi = *i->second;
      std::stringstream filename;

      std::ofstream ofs;
      filename << dir << "/func_" << fi.id() << "_" << fi.calls() << ".dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile(ofs);
      ofs.close();

      filename.str("");
      filename << dir << "/func_" << fi.id() << "_" << fi.calls() << ".det.dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile_detailed(ofs);
      ofs.close();

      filename.str("");
      filename << dir << "/func_" << fi.id() << "_" << fi.calls() << ".rec.dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile_record(ofs);
      ofs.close();

      for(auto I=fi.li_begin(), E=fi.li_end(); I!=E; ++I) {
        LoopInfo* li = I->second;
        if(li->hasSubgraphs()) {
	  filename.str("");
	  filename << dir << "/func_" << fi.id() << "_" << fi.calls() << ".loopsg" << li->id() << ".dot"; 
          std::ofstream dotOutFile(filename.str().c_str()); 

          li->printSubgraphDot(dotOutFile);
	}
      }
    }
  }

  void resetStack(CPC& prevCPC, bool& prevCtrl, bool& prevCall, bool& prevRet) {
    _callStack.clear();
    _dId=0;
    if(_origstack.size()==0) {
      return;
    }
    prevCPC=_origPrevCPC;
    prevCtrl=_origPrevCtrl;
    prevCall=_origPrevCall;
    prevRet=_origPrevRet;
    for(auto i = _origstack.begin(),e=_origstack.end();i!=e;++i) {
      uint64_t pc = *i;
      CPC newCPC = std::make_pair(pc,(uint16_t)0);
      FunctionInfo* funcInfo = getOrAddFunc(newCPC);
      _callStack.emplace_back(funcInfo,_dId);
    }

 };
};

#endif
