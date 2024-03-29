#ifndef BERET_INST
#define BERET_INST

class BeretInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;  

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      Execute = 0,
      Complete = 1,
      Writeback = 2,
      NumStages
  };

private:
  T events[NumStages];

public:
  std::shared_ptr<T> startSEB;
  std::shared_ptr<T> endSEB;

  BeretInst(Op* op) {
    this->_op=op;
  }

  virtual ~BeretInst() { 
    //std::cout << "deleting beret inst" << _index << "\n";
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }*/
  }

  E* ex_edge, *st_edge;

  void updateImg(const CP_NodeDiskImage &img) {
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _true_cache_prod=img._true_cache_prod;
    _ex_lat=img._ep_lat;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._st_lat;
    _floating=img._floating;
    _iscall=img._iscall;
    _eff_addr=img._eff_addr;
  }

  BeretInst(const CP_NodeDiskImage &img, uint64_t index, Op* op):
              dg_inst_base<T,E>(index){
    updateImg(img);
    this->_op=op;
    this->isAccelerated=true;
  }

  BeretInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
  }

  void updateLat(uint16_t lat) {
    //HACK: see cp_beret.hh
    if(ex_edge->_len==1) {
      ex_edge->_len=lat;
    } else {
      ex_edge->_len+=lat;
    }
  }

  void updateStLat(uint16_t lat) {
    st_edge->_len=lat;
  }

  uint16_t ex_lat() {
    return ex_edge->_len;
  }


  void reCalculate() {
    startSEB->reCalculate();
    for(int i = 0; i < NumStages; ++i) {
      events[i].reCalculate();
    }
    endSEB->reCalculate();
  }

  virtual unsigned numStages() {
    return NumStages;
  }
  virtual uint64_t cycleOfStage(const unsigned i) {
    return events[i].cycle(); 
  }
  virtual unsigned eventComplete() {
    return Complete; 
  }
  virtual unsigned eventReady() { //this is when to delay the begining of execution
    return Execute; 
  }

  virtual unsigned memComplete() {
    if(this->_isload) {
      return Complete; //TODO: Complete or commit?
    } else if (this->_isstore) {
      return Writeback;
    } else {
      assert(0 && "no mem access allowed");
    }
  }


/*  virtual unsigned memComplete() {
    return Complete;
  }*/

};

#endif
