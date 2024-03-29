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
#include <unordered_set>
#include <bitset>
#include <string>
#include <assert.h>

#include "cpu/crtpath/crtpathnode.hh"
#define MAX_OPS 2048

#include "op.hh"
#include "bb.hh"
#include "functioninfo.hh"
#include "loopinfo.hh"
#include "durhisto.hh"

#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>
//#include <boost/serialization/unordered_map.hpp>

#include <memory> //for shared ptr
#include "elfparse.hh"

/*
class stackDep {
  Op* st_inst;
  std::vector<Op*> ld_inst;
}*/

struct MemDep {
  uint32_t dId;
  Op* op;
};

class PathProf;

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

  uint64_t _trace_duration=0;
  uint64_t _prev_trace_interval=0;

  int _cur_trace_id=-1;

  uint64_t  _cur_i_iloop_start=0, _cur_i_oloop_start=0; //inlinable loops
  LoopInfo* _cur_i_iloop=NULL, *_cur_i_oloop=NULL; //inlinable loops
  int _trace_start=0;

  std::unordered_map<Op*,uint64_t> stack_op_addr;
  std::unordered_map<uint64_t,MemDep> giganticMemDepTable;  //who last wrote this
  std::unordered_map<uint64_t,MemDep> giganticMemLoadTable; //who last read this

  void fixup(uint64_t dId, PathProf* prof) {
    hot_trace_gran(dId, prof, true);
    FunctionInfo* fi = funcInfo();
    for(auto ii = fi->li_begin(), ee = fi->li_end(); ii!=ee; ++ii) {
      LoopInfo* li = ii->second;  
      checkLoopGranComplete(dId, li, prof);
    } 
  }

  void checkIfStackSpill(Op* st_op, Op* ld_op, uint64_t addr) {
    assert(st_op && ld_op);
    //looks like stack -- robustify later?
    if(!onStack(addr)) {
      return;
    }
    
    LoopInfo* st_li = _funcInfo->innermostLoopFor(st_op->bb());
    LoopInfo* ld_li = _funcInfo->innermostLoopFor(ld_op->bb());

    if((stack_op_addr.count(st_op) && stack_op_addr[st_op]!=addr) ||
       (stack_op_addr.count(ld_op) && stack_op_addr[ld_op]!=addr
       && st_li == ld_li ) ||
       st_op->isSpcMem() || ld_op->isSpcMem()) {
      _funcInfo->not_stack_candidate(st_op);
      _funcInfo->not_stack_candidate(ld_op);
    } else {
      _funcInfo->add_stack_candidate(ld_op, st_op);
     // std::cout << "load added: " << ld_op->id() << " " << addr << "\n";
    }

    stack_op_addr[st_op]=addr;
    stack_op_addr[ld_op]=addr;
  }

public:
  void hot_trace_gran(uint64_t did, PathProf* prof, bool stack_changed);
  void checkLoopGranBegin(uint64_t dId, LoopInfo* li, PathProf* prof);
  void checkLoopGranComplete(uint64_t dId, LoopInfo* li,PathProf* prof);

  void returning(uint64_t dId, PathProf* prof) {
    fixup(dId,prof);
  }

  static bool onStack(uint64_t addr) {
    return addr >= 0x7f0000000000;
  }

  void check_for_stack(Op* op, uint64_t dId, uint64_t addr, uint8_t acc_size);

  StackFrame(FunctionInfo* fi, uint32_t dId) : _funcInfo(fi), _prevBB(NULL), _pathIndex(0) {
    //create dummy _iterStack for non-loops
    _iterStack.emplace_back(new LoopIter(_loopStack,fi->instance_num()));
    //also, put a marker in the _iterMap, at the begining
    _iterMap.emplace(std::piecewise_construct,
                     std::forward_as_tuple(dId),
		     std::forward_as_tuple(_iterStack.back()));
  }

  int getLoopIterNum() {
    if ( _iterStack.back()->relevantLoopIter() != -1) {
      #if 0
      std::cout << _iterStack.back()->relevantLoop()
                << ":"
                <<  _iterStack.back()->relevantLoop()->curIter()
                <<"\n";
      #endif
      return _iterStack.back()->relevantLoop()->curIter();
    }
    return -1;
  }

  LoopInfo* curLoop() {
    if(!isLooping()) {
      return NULL;
    } else {
      return _loopStack.back();
    }
  }

  bool isLooping() {
    return _loopStack.size()>0;
  }

  FunctionInfo* funcInfo() {
    return _funcInfo;
  }

  Op* getOp(CPC cpc);

  void processBB_phase1(CPC headCPC, CPC tailCPC) {
    _prevBB=_funcInfo->addBB(_prevBB,headCPC,tailCPC);
  }

  void setRecursing(bool b) {
    _isRecursing=b;
  }

  bool isRecursing() {return _isRecursing;}

  void setDirectRecursing(bool b) {
    _isDirectRecursing=b;
  }
  bool isDirectRecursing() {return _isDirectRecursing;}

  void dyn_dep(Op* dep_op, Op* op, uint64_t dep_dId, uint64_t did, bool isMem);

  //Process op, determine if it is a bb or not
  Op* processOp_phase2(uint32_t dId, CPC cpc,uint64_t addr, uint8_t acc_size);
  void processBB_phase2(uint32_t dId, BB* bb, int profile=1, PathProf* prof=NULL);

  Op* processOp_phase3(uint32_t dId, CPC cpc,PathProf* prof, bool extra);

  
};


class StaticBB {
public:
  uint64_t head,tail;
  int line=0,srcno=-1;
  std::set<std::pair<uint64_t,uint64_t>> next_static_bb;
  StaticBB(uint64_t head_, uint64_t tail_, int line_, int srcno_) {
    head = head_;
    tail = tail_;
    line = line_;
    srcno = srcno_;
  }
};

class StaticFunction {
public:
  int line=0;
  std::string filename;
  std::string name;
  std::map<uint64_t, StaticBB*> static_bbs;
};

class StaticCFG {
public:
  std::unordered_map<uint64_t,StaticFunction*> static_funcs;
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
  StaticCFG static_cfg;
  std::vector<std::string> static_func_names;

  void print_loop_loc(std::ostream& out, LoopInfo* li) {
    std::map<int,std::set<int>> f2bb;
    for(auto ii = li->body_begin(), ee = li->body_end();ii!=ee;++ii) {
      BB* bb = *ii;
      f2bb[bb->src_number()].insert(bb->line_number());
    }
    for(auto i : f2bb) {
      if(static_func_names.size() > (unsigned)i.first) {
        out << "\"" << static_func_names[i.first] << "\"";
        for(auto j : i.second) {
          out << " " << j;
        }
      }
    }
  }

  uint64_t dId() {return _dId;}

  //some stats for loops/recursion
  uint64_t insts_in_beret=0;
  uint64_t insts_in_simple_inner_loop=0;
  uint64_t insts_in_inner_loop=0;
  uint64_t insts_in_all_loops=0;
  uint64_t non_loop_insts_direct_recursion=0;
  uint64_t non_loop_insts_any_recursion=0;

  enum structure {
    ST_TRACE, ST_ILOOP, ST_OLOOP, ST_NUM
  };

  std::vector<DurationHisto> _gran_duration;
  std::map<LoopInfo*,uint64_t> longer_loops_d16384_s256;
  std::map<LoopInfo*,uint64_t> longer_loops_d16384_s256_i;

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

  //stats from m5out/config.ini
  //[system]
  int cache_line_size=0;
  //[system.cpu.dcache]
  int dcache_assoc=2, dcache_hit_latency=2, dcache_mshrs=4, dcache_response_latency=2;
  int dcache_size=65536, dcache_tgts_per_mshr=20, dcache_write_buffers=8;

  //[system.cpu.icache]
  int icache_assoc, icache_hit_latency=2, icache_mshrs=4, icache_response_latency=2;
  int icache_size=32768, icache_tgts_per_mshr=20, icache_write_buffers=8;
  
  //[system.l2]
  int l2_assoc=2, l2_hit_latency=20, l2_mshrs=20, l2_response_latency=20;
  int l2_size=2097152, l2_tgts_per_mshr=12, l2_write_buffers=8;
  
  //[system.switch_cpus]
  int LQEntries=32, LSQDepCheckShift=0, SQEntries=32, cachePorts=2;
  int commitToDecodeDelay=1, commitToFetchDelay=1;
  int commitToIEWDelay=1, commitToRenameDelay=1;
  int fetchWidth=4, decodeWidth=4, dispatchWidth=4, renameWidth=4;
  int issueWidth=4, commitWidth=4, squashWidth=4;
  int decodeToFetchDelay=1, decodeToRenameDelay=1;
  int fetchToDecodeDelay=1, fetchTrapLatency=1;
  int iewToDecodeDelay=1, iewToFetchDelay=1,iewToRenameDelay=1;
  int issueToExecuteDelay=1;
  bool needsTSO=false;
  int numIQEntries=64, numROBEntries=192;
  int numPhysFloatRegs=256, numPhysIntRegs=256;
  int renameToDecodeDelay=1, renameToFetchDelay=1, renameToIEWDelay=2;
  int renameToROBDelay=1;
  int wbDepth=16, wbWidth=4;

  //[system.switch_cpus.branchPred]
  int BTBEntries=4096, BTBTagSize=16, RASSize=16;

  // ... this is hokey, but it will work for now ...
  //[system.switch_cpus.fuPool.FUList0]
  int int_alu_count=6;
  
  //[system.switch_cpus.fuPool.FUList0.opList]
  int int_alu_issueLat=1;
  int int_alu_opLat=1;

  //[system.switch_cpus.fuPool.FUList1]
  int mul_div_count=2;

  //[system.switch_cpus.fuPool.FUList1.opList0]
  int mul_issueLat=1;
  int mul_opLat=3;

  //[system.switch_cpus.fuPool.FUList1.opList1]
  int div_issueLat=19;
  int div_opLat=20;

  //[system.switch_cpus.fuPool.FUList2]
  int fp_alu_count=4;

  //[system.switch_cpus.fuPool.FUList2.opList0]
  int fadd_issueLat=1;
  int fadd_opLat=2;

  //[system.switch_cpus.fuPool.FUList2.opList1]
  int fcmp_issueLat=1;
  int fcmp_opLat=2;

  //[system.switch_cpus.fuPool.FUList2.opList2]
  int fcvt_issueLat=1;
  int fcvt_opLat=2;

  //[system.switch_cpus.fuPool.FUList3]
  int fp_mul_div_sqrt_count=2; 

  //[system.switch_cpus.fuPool.FUList3.opList0]
  int fmul_issueLat=1;
  int fmul_opLat=4;

  //[system.switch_cpus.fuPool.FUList3.opList1]
  int fdiv_issueLat=12;
  int fdiv_opLat=12;

  //[system.switch_cpus.fuPool.FUList3.opList2]
  int fsqrt_issueLat=24;
  int fsqrt_opLat=24;

  //[system.switch_cpus.fuPool.FUList7]
  int read_write_port_count=2;

  //[system.switch_cpus.fuPool.FUList7.opList0]
  int read_port_issueLat=1;
  int read_port_opLat=1;

  //[system.switch_cpus.fuPool.FUList7.opList1]
  int write_port_issueLat=1;
  int write_port_opLat=1;

  CFU_set* beret_cfus() {return &_beret_cfus;}


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
    ar & static_func_names;
    ar & _beret_cfus;
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
//    ar & _gran_duration;
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

  CFU_set _beret_cfus;
  FuncMap _funcMap;

  std::list<StackFrame> _callStack;

  //Phase 1 Processing
  CPC _prevHead;
  std::unordered_map<CPC,uint64_t> const_loads; // 0 value means not constant
  std::unordered_map<CPC,Op*> const_load_ops; // keep cpc->op mapping for these
  std::unordered_map<uint64_t,CPC> const_loads_backwards; // 0 value means not constant

  void checkRecursion();
  bool adjustStack(CPC newCPC, bool isCall, bool isRet);

  //Phase 2 Processing
  int _dId;
  Op* _op_buf[MAX_OPS];
  std::unordered_set<Op*> _ctrlFreeMem;

  //get a token from a stream
  static bool getToken(std::istringstream& iss, std::string& thing, char c=' ') {
    bool valid = true; 
    do {
      valid = iss.good();
      std::getline( iss, thing , c);
    } while(valid && thing.size() == 0);
    return valid;
  }

  static bool getStat(const char* tag_str, const std::string& tag,
                      const std::string& val, uint64_t& stat, bool eq = false) {
    if (eq) {
      if (tag == std::string(tag_str)) {
        stat = std::stoul(val);
        return true;
      }
      return false;
    }
    if (tag.find(tag_str) != std::string::npos) {
      stat = std::stoul(val);
      return true;
    }
    return false;
  }

  static bool getStat(const char* tag_str, std::string& tag,
                      std::string& val, int& stat, bool eq = false) {
    if (eq) {
      if (tag == std::string(tag_str)) {
        stat = std::stoi(val);
        return true;
      }
      return false;
    }

    if (tag.find(tag_str) != std::string::npos) {
      stat = std::stoi(val);
      return true;
    }
    return false;
  }

  static bool getStat(const char* tag_str, std::string& tag,
                      std::string& val, bool& stat, bool eq = false) {

    if (eq) {
      if (tag == std::string(tag_str)) {
        stat = val.find("false") != std::string::npos;
        return true;
      }
      return false;
    }

    if (tag.find(tag_str) != std::string::npos) {
      stat = val.find("false")!=std::string::npos;
      return true;
    }
    return false;
  }



  static bool to_bool(std::string const& s) {
    return s != "0";
  }

  std::map<std::string, std::string> statMap;
public:
  template<class T>
  T getStatFromMap(const char *tag) {
    T val = 0;
    for (auto I = statMap.begin(), E = statMap.end();
         I != E; ++I) {
      if (I->first.find(tag) == std::string::npos)
        continue;
      getStat(tag, I->first, I->second, val);
    }
    return val;
  }


public:
  PathProf() {
    _gran_duration.resize(ST_NUM);
    _beret_cfus.beret_set();
    _dId=0;
  }

  StackFrame* curFrame() {
    return &(_callStack.back());
  }

  void procSymTab(const char* filename) {
    ELF_parser::read_symbol_tables(filename,sym_tab);
  }

  void procStaticCFG(const char* filename);
  void procConfigFile(const char* filename);
  void procStatsFile(const char* filename);
  void procStackFile(const char* filename);

  FunctionInfo* getOrAddFunc(CPC newCPC);
  void setStartInst(uint64_t count) {startInst=count;}
  void setStopInst(uint64_t count) {stopInst=count;}
  void setSkipInsts(uint64_t count) {skipInsts=count;}

  bool isBB(CPC cpc);
  void processOpPhase1(CPC prevCPC, CPC newCPC, bool isCall, bool isRet);
  void processAddr(CPC cpc, uint64_t addr, bool is_load, bool is_store);

  void runAnalysis();
  void runAnalysis2(bool no_gams, bool gams_details, bool size_based_cfus,
                    uint64_t total_dyn_insts);

  void processOpPhase2(CPC prevCPC, CPC newCPC, bool isCall, bool isRet,
                       CP_NodeDiskImage& img);
  Op* processOpPhase3(CPC newCPC, bool wasCalled, bool wasReturned);
  Op* processOpPhaseExtra(CPC newCPC, bool wasCalled, bool wasReturned);


  FuncMap::iterator fbegin() {return _funcMap.begin();}
  FuncMap::iterator fend() {return _funcMap.end();}


  //look up the entire stack, find the "biggest" ccore available
  //not used, but w/e
  std::pair<LoopInfo*,FunctionInfo*> upStack(Op* op,
                                            std::set<FunctionInfo*>& fiSet, 
                                            std::set<LoopInfo*>& liSet) {

    //FunctionInfo* cur_fi = op->func_info();
    //assert(fi);
    //LoopInfo* cur_li = cur_fi->innermostLoopFor(op->bb());
    Op* cur_op = op;

    LoopInfo* li_option;
    FunctionInfo* fi_option;

    for(auto i=_callStack.rbegin(),e=_callStack.rend();i!=e;) {
      StackFrame& sf = *i;
      FunctionInfo* fi = sf.funcInfo();
      assert(cur_op->func() == fi);

      LoopInfo* cur_li = fi->innermostLoopFor(cur_op->bb());
      while((cur_li = cur_li->parentLoop())) {
        if(liSet.count(cur_li)) {
          li_option=cur_li;
          fi_option=NULL;
        }
      }      
      if(fiSet.count(fi)) {
        li_option=NULL;
        fi_option=fi;
      }

      //peek up the call stack
      i++;
      if(i==e) {
        break;
      }
      StackFrame& up_sf = *i;
      FunctionInfo* up_fi = up_sf.funcInfo();

      std::vector<Op*> op_vec = up_fi->opsWhichCalledFunc(fi);
      if(op_vec.size()==0) {
        break;
      } else { //should I check if only one?  otherwise i could get a bad ccore
        cur_op = op_vec[0]; 
      }
    }
    return std::make_pair(li_option, fi_option);
  }



  //looks down the stack, stops when finds something
  std::pair<LoopInfo*,FunctionInfo*> findEntry(Op* op,
                                            std::set<FunctionInfo*>& fiSet, 
                                            std::set<LoopInfo*>& liSet) {
    for(auto i=_callStack.begin(),e=_callStack.end();i!=e;++i) {
      StackFrame& sf = *i;
      FunctionInfo* fi = sf.funcInfo();
      //first check loops
      for(auto ii = fi->li_begin(), ee = fi->li_end(); ii!=ee; ++ii) {
        LoopInfo* li = ii->second;
        if(liSet.count(li)) {
          return std::make_pair(li,(FunctionInfo*)NULL);
        }
      } 
   
      //then check functions
      if(fiSet.count(fi)) {
        return std::make_pair((LoopInfo*) NULL,fi);
      }
    }

    return std::make_pair((LoopInfo*) NULL, (FunctionInfo*)NULL);
  }

  std::list<StackFrame>::iterator cs_begin() {return _callStack.begin();}
  std::list<StackFrame>::iterator cs_end()   {return _callStack.end();}

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

      //filename "/func_" << fi.id() << "_" << fi.calls(
      std::string file_base = fi.nice_filename(); 

      std::ofstream ofs;
      filename << dir << "/" << file_base << ".dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile(ofs);
      ofs.close();

      filename.str("");
      filename << dir << "/" << file_base << ".det.dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile_detailed(ofs);
      ofs.close();

      filename.str("");
      filename << dir << "/" << file_base << ".rec.dot";
      ofs.open(filename.str().c_str(), std::ofstream::out | std::ofstream::trunc);
      fi.toDotFile_record(ofs);
      ofs.close();

      for(auto I=fi.li_begin(), E=fi.li_end(); I!=E; ++I) {
        LoopInfo* li = I->second;
        if(li->hasSubgraphs()) { //beret
	  filename.str("");
	  filename << dir << "/" << file_base << ".loopsg" << li->id() << ".dot"; 
          std::ofstream dotOutFile(filename.str().c_str()); 

          li->printSubgraphDot(dotOutFile,false/*nla*/,true/*just sgs*/,li);
	}
        if(li->hasSubgraphs(true)) { //nla
	  filename.str("");
	  filename << dir << "/" << file_base << ".NLA.loopsg" << li->id() << ".dot"; 
          std::ofstream dotOutFile(filename.str().c_str()); 
          li->printSubgraphDot(dotOutFile,true/*nla*/,true/*just sgs*/,li);
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
