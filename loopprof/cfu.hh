#ifndef CFU_HH
#define CFU_HH

#include <set>
#include <string>
#include <iostream>
#include "assert.h"
#include "op.hh"

class CFU;
class CFU_node {
  public:
  enum CFU_type { ALU, MEM, MPY, SHF, MAX };
  static std::string kindName(CFU_type kind) {
    switch(kind) {
      case ALU: return std::string("ALU"); break;
      case MEM: return std::string("MEM"); break;
      case MPY: return std::string("MPY"); break;
      case SHF: return std::string("SHF"); break;
      case MAX: assert(0); break;
    }
    return std::string(); // won't happen
  }

  static CFU_type kindOf(int opclass) {
    switch(opclass) {
    case 0: //No_OpClass
      return ALU;
    case 1: //IntALU
      return ALU;

    case 2: //IntMult
    case 3: //IntDiv
      return MPY;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return ALU;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return ALU;
    case 30: //MemRead
    case 31: //MemWrite
      return MEM;

    default:
      return ALU; //hopefully this never happens
    }
    return ALU; //and this!
  }
 
  static void print_kinds(std::ostream& s) {
    s << "set k/NONE";
    for(int i = 0; i < MAX; ++i) {
      s << "," << kindName((CFU_node::CFU_type)i);
    }
    s << "/;\n";
  }

  static bool kind_match(Op* op, CFU_node* cfu_node) {
    return kindOf(op->opclass()) == cfu_node->_type;
  }


private:
  std::set<CFU_node*> _ins;
  std::set<CFU_node*> _outs;
  CFU_type _type;
  bool _in_reg;
  bool _out_reg;
  int _ind;
  CFU* _cfu;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _ins;
    ar & _outs;
    ar & _type;
    ar & _in_reg;
    ar & _out_reg;
    ar & _ind;
    ar & _cfu;
  }

public:
  CFU_node() {} // For serializer : (

  CFU_node(CFU* cfu, int ind, CFU_type t, bool in_reg, bool out_reg, 
           std::initializer_list<CFU_node*> ins) {
    _ind=ind;
    _type=t;
    _in_reg=in_reg;
    _out_reg=out_reg;
    _cfu=cfu;

    if(t==SHF) {
      _type=ALU;
    }

    for(auto& in : ins) {
      _ins.insert(in);
      in->_outs.insert(this); 
    }
  }
  std::string kindName() {
    return kindName(_type);
  }
  CFU_type type() {return _type;}

  std::set<CFU_node*>::iterator ins_begin()  {return _ins.begin();}
  std::set<CFU_node*>::iterator ins_end()    {return _ins.end();}
  std::set<CFU_node*>::iterator outs_begin() {return _outs.begin();}
  std::set<CFU_node*>::iterator outs_end()   {return _outs.end();}
  int ind() {return _ind;}
  bool in_reg() {return _in_reg;}
  bool out_reg() {return _in_reg;}

};

class CFU {
  std::set<CFU_node*> _nodes;
  int _ind;
  std::map<int,CFU_node*> _map;

  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _nodes;
    ar & _ind;
    ar & _map;
  }

  public:
  CFU() {}

  CFU(int ind) {
    _ind=ind;
  }

  void add(CFU_node* cfu_node) {
    _nodes.insert(cfu_node);
    _map[cfu_node->ind()]=cfu_node;
  }

  CFU_node* getByInd(int i) {
    return _map[i];
  }

  ~CFU() {
    for(auto i=_nodes.begin(),e=_nodes.end();i!=e;++i) {
      delete *i;
    }
  }

  std::set<CFU_node*>::iterator nodes_begin() {return _nodes.begin();}
  std::set<CFU_node*>::iterator nodes_end()   {return _nodes.end();}
  int ind() {return _ind;}

  CFU_node* getCFUNode(int i) {
    return _map[i];
  }

};

class CFU_set {
  std::set<CFU*> _cfus;
  int ind = 1, cfu_ind = 1;

  std::map<int,CFU*> _map;


  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _cfus;
    ar & ind;
    ar & cfu_ind;
    ar & _map;
  }

  void addCFU(CFU* cfu) {
    _cfus.insert(cfu);
    _map[cfu->ind()]=cfu;
  }

public:
  std::set<CFU*>::iterator cfus_begin() {return _cfus.begin();}
  std::set<CFU*>::iterator cfus_end()   {return _cfus.end();}


  CFU_set() {}
  void old_set() {
    CFU_node* n1,*n2,*n3,*n4,*n5,*n6;
    CFU* cfu;

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n3}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{n2}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n3}));
    cfu->add(n5=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n4}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n2}));
    cfu->add(n5=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n3,n4}));
    cfu->add(n6=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n5}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MPY,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{n3}));
  }

  void beret_set() {
    CFU_node* n1,*n2,*n3,*n4,*n5;
    CFU* cfu;

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1,n2}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MPY,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1,}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1,n3}));

    addCFU(cfu=new CFU(cfu_ind++));
//    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{}));
//    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
//    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n1,n2}));
//    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n3}));

//    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
//    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
//    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MEM,false,true,{n1}));
//    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::MEM,false,true,{n1,n2}));

    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MEM,false,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n2,n3}));
    cfu->add(n5=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n4}));

    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{n1,n3}));
//    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::SHF,true,true,{n2}));

    addCFU(cfu=new CFU(cfu_ind++));
//    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{}));
//    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MEM,true,false,{n1}));
//    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));
//    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n3}));
//    cfu->add(n5=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n4}));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1,n2}));


    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n1}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{n2}));
    cfu->add(n5=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n3,n4}));
//    cfu->add(n6=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n5}));

    addCFU(cfu=new CFU(cfu_ind++));
//    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{}));
//    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MPY,true,true,{n1}));
//    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));
//    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
//    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::MPY,true,true,{n1}));
//    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n2}));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::MEM,true,true,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::MPY,true,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n3}));


    addCFU(cfu=new CFU(cfu_ind++));
    cfu->add(n1=new CFU_node(cfu,ind++,CFU_node::SHF,true,false,{}));
    cfu->add(n2=new CFU_node(cfu,ind++,CFU_node::ALU,true,true,{n1}));
    cfu->add(n3=new CFU_node(cfu,ind++,CFU_node::ALU,false,true,{n1,n2}));
    cfu->add(n4=new CFU_node(cfu,ind++,CFU_node::SHF,false,true,{n1,n3}));
  }




  void print_to_stream(std::ostream& s) {
    s << "set n/n1*n" << ind-1 << ",nreg/;\n";
    s << "set fu(n)/n1*n" << ind-1 << "/;\n";
    s << "set Hnn(n,n)/nreg.nreg";

    for(auto cfui = _cfus.begin(), cfue = _cfus.end(); cfui != cfue; ++cfui) {
      CFU* cfu = *cfui;
      for(auto ni = cfu->nodes_begin(), ne = cfu->nodes_end(); ni != ne; ++ni) {
        CFU_node* cfu_node = *ni;
        if(cfu_node->in_reg()) {
          s << ", nreg.n" << cfu_node->ind();
        }
        for(auto ii=cfu_node->ins_begin(), ee=cfu_node->ins_end(); ii!=ee; ++ii) {
          CFU_node* input_cfu_node = *ii;
          s << ", n" << input_cfu_node->ind() << ".n" << cfu_node->ind();
        }
        if(cfu_node->out_reg()) {
          s << ", n" << cfu_node->ind() << ".nreg";
        }
      }
    }
    s << "/;\n";

    s << "set s/s1*s" << cfu_ind -1 << "/;\n";
    s << "set cfu(s,n)/";

    bool first=false;
    for(auto cfui = _cfus.begin(), cfue = _cfus.end(); cfui != cfue; ++cfui) {
      CFU* cfu = *cfui;
      for(auto ni = cfu->nodes_begin(), ne = cfu->nodes_end(); ni != ne; ++ni) {
        CFU_node* cfu_node = *ni;
        if(!first) {
          first=true;
        } else {
          s<< ",";
        }
        s << "s" << cfu->ind() << ".n" << cfu_node->ind();
      }
    }
    s << "/;\n";

    s << "set kn(n,k)/nreg.NONE";
    for(auto cfui = _cfus.begin(), cfue = _cfus.end(); cfui != cfue; ++cfui) {
      CFU* cfu = *cfui;
      for(auto ni = cfu->nodes_begin(), ne = cfu->nodes_end(); ni != ne; ++ni) {
        CFU_node* cfu_node = *ni;
        s << ",n" << cfu_node->ind() << "." << cfu_node->kindName();
      }
    }
    s << "/;\n";
  }

  ~CFU_set() {
    for(auto i=_cfus.begin(),e=_cfus.end();i!=e;++i) {
      delete *i;
    }
  }

  CFU* getCFU(int i) {
    return _map[i];
  }

  int numCFUs() {return _map.size();}
};




#endif
