#include "nla_inst.hh"
#include "functioninfo.hh"

using namespace std;

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

void DynSubgraph::addDep(std::shared_ptr<DynSubgraph> a, 
                   std::shared_ptr<DynSubgraph> b, bool ignore_if_cycle,
                   const char* m, NLAInst& i1, NLAInst& i2) {
  assert(a!=b);

  /*
  std::cout <<   i1._op->id() << " d:" << i1._index << " sg:" << a->static_sg->id() 
    << " -> " << i2._op->id() << " d:" << i2._index << " sg:" << b->static_sg->id()
    << "(" << m << ")\n";*/

  bool bs_use_has_a=false,as_dep_has_b=false;
  for(auto& i : b->use_subgraphs) {
    if(i.lock() == a) {
      bs_use_has_a=true;
      break;
    }
  }
  for(auto& i : a->dep_subgraphs) {
    if(i.lock() == b) {
      as_dep_has_b=true;
      break;
    }
  }

  static int num_errors=1000;

  if(bs_use_has_a && as_dep_has_b) {
    if(ignore_if_cycle) {
      return;
    } else {
      if(num_errors>0) {
        num_errors-=1;
        cout << "ERROR/HUGE PROBLEM: cycle created!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
        cout << a->insts.begin()->lock()->_op->func()->nice_name() << " ";
        cout << a->insts.begin()->lock()->_index << " " << a->insts.begin()->lock()->_op->id() << "\n";
        cout << b->insts.begin()->lock()->_index << " " << b->insts.begin()->lock()->_op->id() << "\n";
        assert(0&& "cycle created\n");
      }
    }
  }
  b->dep_subgraphs.push_back(a);
  a->use_subgraphs.push_back(b);
  //std::cout << "dep " << a->static_sg->id() << "->" << b->static_sg->id() << "\n";
}



