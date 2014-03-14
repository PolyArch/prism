#include "nla_inst.hh"

std::shared_ptr<NLAInst> DynSubgraph::calcCritCycles() {
  

  //TODO: THIS DOESN'T WORK : )   (when the start of the CFU isn't sync'd)
  uint64_t max_cycle=0, min_cycle=(uint64_t)(-1);
  std::shared_ptr<NLAInst> max_inst;
  for(auto &inst : insts) {
    std::shared_ptr<NLAInst> nla_inst = inst.lock();
    if(min_cycle > nla_inst->cycleOfStage(NLAInst::Execute)) {
      min_cycle = nla_inst->cycleOfStage(NLAInst::Execute);
    }
    if(max_cycle < nla_inst->cycleOfStage(NLAInst::Complete)) {
      max_cycle = nla_inst->cycleOfStage(NLAInst::Complete);
      max_inst  = nla_inst;
    }
  }

  critCycles=max_cycle-min_cycle;
  return max_inst;
}

