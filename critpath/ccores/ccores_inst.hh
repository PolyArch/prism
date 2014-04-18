#ifndef CCORES_INST_HH
#define CCORES_INST_HH

class CCoresInst : public dg_inst_base<dg_event,dg_edge_impl_t<dg_event>> {
  typedef dg_event T;
  typedef dg_edge_impl_t<T> E;

  typedef T* TPtr;
  typedef E* EPtr;

public:
  //Events:
  enum NodeTy {
      BBReady = 0,
      Execute = 1,
      Complete = 2,
      Writeback = 3,
      NumStages
  };
private:
  T events[NumStages];

public:
 std::shared_ptr<T>   endBB;
 std::shared_ptr<T> startBB;

  virtual ~CCoresInst() { 
    /*for (int i = 0; i < 3; ++i) {
      events[i].remove_all_edges();
    }
    endBB->remove_all_edges();
    startBB->remove_all_edges();*/
  }


  bool _isctrl=0;
  bool _ctrl_miss=0;
  uint16_t _icache_lat=0;
  uint16_t _prod[MAX_SRC_REGS]={0,0,0,0,0,0,0,0};
  uint16_t _mem_prod=0;
  uint16_t _cache_prod=0;
  uint64_t _true_cache_prod=false;
  uint16_t _ex_lat=0;
  bool _serialBefore=0;
  bool _serialAfter=0;
  bool _nonSpec=0;
  uint16_t _st_lat=0;
  uint64_t _pc=0;
  uint16_t _upc=0;
  uint64_t _eff_addr;
  bool _floating=false;
  bool _iscall=false;

  CCoresInst(const CP_NodeDiskImage &img, uint64_t index,Op* op):
              dg_inst_base<T,E>(index){
    _opclass=img._opclass;
    _isload=img._isload;
    _isstore=img._isstore;
    _isctrl=img._isctrl;
    _ctrl_miss=img._ctrl_miss;
    _icache_lat=img._icache_lat;
    std::copy(std::begin(img._prod), std::end(img._prod), std::begin(_prod));
    _mem_prod=img._mem_prod;
    _cache_prod=img._cache_prod;
    _true_cache_prod=img._true_cache_prod;
    _ex_lat=img._cc-img._ec;
    _serialBefore=img._serialBefore;
    _serialAfter=img._serialAfter;
    _nonSpec=img._nonSpec;
    _st_lat=img._xc-img._wc;
    _pc=img._pc;
    _upc=img._upc;
    _floating=img._floating;
    _iscall=img._iscall;
    _op=op;
  }

  CCoresInst() : dg_inst_base<T,E>() {}

  T& operator[](const unsigned i) {
    assert(i < NumStages);
    return events[i];
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
  virtual unsigned eventReady() {
    return Execute; 
  }
/*  virtual unsigned memComplete() {
    return Complete;
  }*/
  virtual unsigned memComplete() {
    if(this->_isload) {
      return Complete; //TODO: Complete or commit?
    } else if (this->_isstore) {
      return Writeback;
    } else {
      assert(0 && "no mem access allowed");
    }
  }

};


#endif //CCORES_INST_HH

