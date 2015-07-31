#ifndef CP_MULTI
#define CP_MULTI

#include "../nla/cp_nla.hh"
#include "../simd/cp_simd.hh"
#include "../dyser/cp_vec_dyser.hh"

#include <algorithm>

//
//  Soooo, this is a bit hacky.
//
//
//
class cp_multi : public ArgumentHandler,
      public CP_DG_Builder<dg_event, dg_edge_impl_t<dg_event>>  {

  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  
  typedef dg_inst<T, E> Inst_t;


  //anode: Amdahl Tree Node -------------------------------------------------------------
  struct anode;
  struct anode {
    LoopInfo* li;
    std::map<LoopInfo*,CP_DG_Builder<T,E>*> plan;
    float expected_benefit=0;
    std::set<anode*> a_parents;
    std::map<anode*,int> parent2numcalls;

    std::set<anode*> a_children; //children loops
    int children_done=0;
    bool _done=false;

    int num_calls_into=0;
    uint64_t total_dynamic_insts=0; // all dynamic instructions regardless of *my* callers' context

    anode(LoopInfo* li_in) {
      li=li_in;
      //if(li->isInnerLoop()) {
      //  total_dynamic_insts=totalDynamicInlinedInsts();
      //}
    }

    std::set<anode*> done() {
      _done=true;
      std::set<anode*> done_parents;
      for(auto i=a_parents.begin(),e=a_parents.end();i!=e;++i) {
        anode* parent_an = *i;
        parent_an->children_done++;

        if(parent_an->children_done > parent_an->num_children()) {
          std::cerr << "me: " << li->nice_name_full() << "\n";
          std::cerr << "parent: " << parent_an->li->nice_name_full() << "\n";
          std::cerr << "children_done: " << parent_an->children_done << " "
                    << "num_children: " << parent_an->num_children() << "\n";
          assert(parent_an->children_done <= parent_an->num_children());
        }

        if(parent_an->children_done == parent_an->num_children()) {
          done_parents.insert(parent_an);
        }
      }
      return done_parents;
    }

    void set_benefit(float benefit) {
      expected_benefit=benefit;
    }

    float benefit() {return expected_benefit;}

    uint64_t dyn_insts() {
      if(total_dynamic_insts==0) {
        for(anode* child_an : a_children) {
          total_dynamic_insts+=child_an->dyn_insts_for_parent(this);
        }
        if(num_children()==0) {
          total_dynamic_insts=li->totalDynamicInlinedInsts();
        }
        //TODO: add in my own loop's instructions
      }
      return total_dynamic_insts;     
    }

    void add_child(anode* child, int num_calls=-1) {
      assert(child!=this);
      a_children.insert(child);
      child->add_parent(this, num_calls);
    }

    int num_parents() {return a_parents.size();}
    int num_children() {return a_children.size();}

    uint64_t dyn_insts_for_parent(anode* parent_an) {
      assert(a_parents.count(parent_an));
      if(num_parents()==1) {
        return dyn_insts();
      } else {
        int total_calls = 0;
        for(const auto& I : parent2numcalls) {
          //anode* parent_an = I.first;
          int calls = I.second;
          assert(calls != -1);
          total_calls+=calls;
        }
        return (uint64_t)((parent2numcalls[parent_an]/((double)total_calls)) * dyn_insts());
      }
    }


private:

    void add_parent(anode* parent, int num_calls=-1) {
      assert(parent!=this);
      a_parents.insert(parent);
      parent2numcalls[parent]=num_calls;
    }
  };


  //model_info: Extra informaiton for models  -------------------------------------------
  struct model_info {
  public:
    string name;
    CP_DG_Builder<T,E>* model;
    uint64_t cycles;
    uint64_t insts;
  };

public:
  virtual bool removes_uops() {return true;}
  virtual dep_graph_t<Inst_t,T,E>* getCPDG() {
    return &cpdg;
  };

  void handle_argument(const char *name, const char *arg) {

  }

  dep_graph_impl_t<Inst_t,T,E> cpdg;
  std::map<LoopInfo*,CP_DG_Builder<T,E>*> which_model;

  //Define Models:
  cp_nla seed;
  cp_nla beret;
  simd::cp_simd simd;
  DySER::cp_vec_dyser dyser;
  vector<CP_DG_Builder<T,E>*> models;
  CP_DG_Builder<T,E>* cur_model=NULL;
  std::map<CP_DG_Builder<T,E>*, model_info> m2info;

  void add_model(const char* name, CP_DG_Builder<T,E>* model) {
    models.push_back(model);
    m2info[model].name=name;
  }

  cp_multi() {
    add_model("seed",&seed);
    //add_model("beret",&beret);
    add_model("simd",&simd);
    add_model("dyser",&dyser);

    beret.handle_argument("nla-trace-ctrl","1");
  }
  

  virtual void initialize() override {
     for(const auto& model : models) {
       model->initialize();
       model->setGraph(&cpdg);
     }
  }

  virtual void setWidth(int i,bool scale_freq, bool match_simulator, 
                        bool revolver, int mem_ports, int num_L1_MSHRs) {
    for(const auto& model : models) {
      model->setWidth(i,scale_freq,match_simulator,revolver,mem_ports,num_L1_MSHRs);
    }
    CP_DG_Builder::setWidth(i,scale_freq,match_simulator,revolver,mem_ports,num_L1_MSHRs);
  }

  std::ofstream fout;

  std::map<LoopInfo*,anode*> l2anode;
  std::set<anode*> leaf_anodes;

  virtual void setupComplete() override {
    /* ------------------------------- Decide Loops --------------------------------  */

    //1. Create an ahmdal node for each loop
    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
        LoopInfo* li = i->second;
        if(!li->cantFullyInline()) {
          l2anode[li]=new anode(li);
        }       
      }
    }

    //2. Hook up all anodes, mark starting anodes
    for(auto i : l2anode) {
      LoopInfo* li=i.first;
      anode* an=i.second;

      //hook up any children inside func
      for(auto I=li->iloop_begin(),E=li->iloop_end(); I!=E; ++I) {         
        LoopInfo* inner_li = *I;
        anode* inner_an = l2anode[inner_li];
        an->add_child(inner_an);
      }

      if(li->containsCallReturn()) { //mark as leaf
        for(auto ii=li->callto_begin(),ee=li->callto_end();ii!=ee;++ii) {
          FunctionInfo* called_fi = ii->first.second;
          int num_calls = ii->second;

          for(auto II=called_fi->li_begin(),EE=called_fi->li_end();II!=EE;++II) {
            LoopInfo* called_li = II->second;
            if(called_li->isOuterLoop()) {
              anode* called_an = l2anode[called_li];
              an->add_child(called_an, num_calls);
            }
          }
        }
      }

      if(an->num_children()==0) {
        leaf_anodes.insert(an);  
      }

    }

    //3. Travere the Amdahl Tree!  (ok its a dag, but dag just sounds less cool for some reason)
    std::deque<anode*> worklist;
    for(anode* an : leaf_anodes) {
      worklist.push_back(an);
    }

    while(worklist.size()!=0) {
      anode* a = worklist.front();
      worklist.pop_front();
      LoopInfo* li = a->li;

      //Option 1. Keep what we have at each level (Amdy's Law)
      double unchanged_weight=li->numInsts(); // add in the instructions
      double unchanged_insts=li->numInsts();
      for(auto i=a->a_children.begin(),e=a->a_children.end(); i!=e; ++i) {
        anode* child_an=*i;
        uint64_t insts = child_an->dyn_insts_for_parent(a);
        unchanged_weight+=((double)insts)/child_an->benefit();
        unchanged_insts+=insts;
      }
      double unchanged_benefit=unchanged_insts/unchanged_weight;

      cout << li->nice_name_full() << "  default" << "-" << unchanged_benefit;

      //Option 2. Trade for something better
      float best_benefit=0;
      CP_DG_Builder<T,E>* best_model=NULL;

      for(const auto& model : models) {
        float estimate = model->estimated_benefit(li);
        
        cout << " " << m2info[model].name << "-" << estimate;

        if(best_benefit < estimate) {
          best_benefit = estimate;
          best_model = model;
        }
      }
      cout << "\n";

      if(best_model!=NULL && best_benefit > unchanged_benefit) {
        a->set_benefit(best_benefit);
        which_model[li]=best_model;
      } else {
        a->set_benefit(unchanged_benefit);
      }

      //Now push back the next level up if possible:
      std::set<anode*> try_these = a->done();
      for(anode* parent_an : try_these) {
        worklist.push_back(parent_an);
      }
    }
    






    /*                              Decide Looops     (old_way)                             */
/*    for(auto i=Prof::get().fbegin(),e=Prof::get().fend();i!=e;++i) {
      FunctionInfo* fi = i->second;
      for(auto i=fi->li_begin(),e=fi->li_end();i!=e;++i) {
                                                                                                                                     |    for (auto I = body_begin(), E = body_end(); I != E; ++I) {
                                                                                                                                                 LoopInfo* li = i->second;

        if(!li->isInnerLoop()) {
          continue;
        }
        
        float best_benefit=0;
        CP_DG_Builder<T,E>* best_model=NULL;

        for(const auto& model : models) {
          float estimate = model->estimated_benefit(li);

          cout << " " << m2info[model].name << "-" << estimate;

          if(best_benefit < estimate) {
            best_benefit = estimate;
            best_model = model;
          }
        }
        cout << "\n";
        if(best_benefit > 0) {
          which_model[li]=best_model;
        }

        //if(!li->cantFullyInline()) {
        //  float inlineValue=inline_value(li); 
        //  //std::cout << "->" << li->nice_name() << " (value " << inlineValue << ")\n";
        //  PQL.push(li,inlineValue);
        //} else {
        //  //std::cout << "can't inline " << li->nice_name() << "\n";
        //}

      }
      //if(!fi->cantFullyInline()) {
      //  float inlineValue = inline_value(fi);
      //  // std::cout << "->" << fi->nice_name() << " (value " << inlineValue << ")\n";
      //  PQF.push(fi,inlineValue);
      //} else {
      //  //std::cout << "can't inline " << fi->nice_name() << "\n";
      //}
    } */

    //Open File
    std::string trace_out = "stats/" + _name + "-region.out";

    fout.open(trace_out.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!fout.good()) {
      std::cerr << "Cannot open file: " << trace_out << "\n";
    }


     CP_DG_Builder::setupComplete();
     for(const auto& model : models) {
       model->setupComplete();
     } 
  }

  virtual void setupMcPAT(const char* filename, int nm) override {
     for(const auto& model : models) {
       model->setupMcPAT(filename,nm);
     } 
     CP_DG_Builder::setupMcPAT(filename,nm);
  }
   
  virtual void pumpAccelMcPAT(uint64_t totalCycles) override {
    if(cur_model) {
      cur_model->pumpAccelMcPAT(totalCycles);
    } else {
      CP_DG_Builder::pumpAccelMcPAT(totalCycles);
    }
  }
 
  virtual double accel_leakage() override {
    if(cur_model) {
      return cur_model->accel_leakage();
    } else {
      return CP_DG_Builder::accel_leakage();
    }
  }

  virtual double accel_region_en() override {
    if(cur_model) {
      return cur_model->accel_region_en();
    } else {
      return CP_DG_Builder::accel_region_en();
    }
  }


  virtual int is_accel_on() override {
    if(cur_model) {
      return cur_model->is_accel_on();
    } else {
      return CP_DG_Builder::is_accel_on();
    }
  }

  uint64_t trans_cycle=0;
  uint64_t trans_index=0;
  uint64_t latest_index=0;

  void trace_reg(uint64_t& start_c, uint64_t& start_i, 
                 uint64_t cur_c, uint64_t cur_i, 
                 CP_DG_Builder<T,E>* model,
                 CP_DG_Builder<T,E>* next_model) {
   
    if(next_model) {
      cout << "model:" << m2info[next_model].name << "\n";
    } else {
      cout << "model:" << "base" << "\n";
    }

    uint64_t total_c = cur_c-start_c;
    uint64_t total_i = cur_i-start_i;
    start_c=cur_c;
    start_i=cur_i;
    std::string name = "base";
    if(cur_model) {
      name= m2info[model].name; 
    } 
    fout << name << " " << total_c << " " << total_i << "\n";
  }  

  virtual void insert_inst(const CP_NodeDiskImage &img,
                   uint64_t index, Op* op) {
    //std::cout << img._ep_lat << "\n";

    latest_index=index;
    if(cur_model==NULL) {
      LoopInfo* li=NULL;
      bool inserted_into_accel=false;
      if(op->bb_pos()==0) { //check for loop
        li = op->func()->getLoop(op->bb());
        CP_DG_Builder<T,E>* next=which_model[li];
        if(next) { //found one!
          trace_reg(trans_cycle,trans_index,this->numCycles(),index,cur_model,next);
          cur_model=next;
          cur_model->insert_inst(img,index,op);
          inserted_into_accel=true;
        }
      }
      if(!inserted_into_accel) { 
        // no loop, or no model
        InstPtr sh_inst = createInst(img,index,op,false);
        getCPDG()->addInst(sh_inst,index);
        addDeps(sh_inst,op);
        pushPipe(sh_inst);
        inserted(sh_inst);   

        //Do some tasks
        FunctionInfo* f = op->func();
        if(!func_transition_inst || f!=func_transition_inst->_op->func()) {
          func_transition_inst = sh_inst;
          for(const auto& model : models) {
            model->set_func_trans_inst(sh_inst);
          }
        }
      }

    } else if (cur_model->is_accel_on()) {
      cur_model->insert_inst(img,index,op);
    } else {
      trace_reg(trans_cycle,trans_index,this->numCycles(),index,cur_model,NULL);
      cur_model=NULL;
      CP_DG_Builder::insert_inst(img,index,op);
    }

    if(cur_model) {
      m2info[cur_model].insts++;
    }

  }


  virtual uint64_t finish() {
    trace_reg(trans_cycle,trans_index,this->numCycles(),latest_index,cur_model,NULL);
    if(cur_model) {
      cur_model->finish();
    }
    return numCycles();
  }

  virtual void accelSpecificStats(std::ostream& out, std::string& name) override {
    out << " (";
 
    for(const auto& model : models) {
      model_info& mi= m2info[model];
      out << mi.name << "-only " << mi.cycles << " " 
          << mi.name << "-insts " << mi.insts << " ";
    }  

    out << ")";
  }

};
  


#endif

