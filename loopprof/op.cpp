#include "op.hh"
#include "bb.hh"
#include "functioninfo.hh"

uint32_t Op::_idcounter=1;
FunctionInfo* Op::func()   {return _bb->func();}
std::string Op::getCalledFuncName() const {
  if(_calledFunc) {
    return _calledFunc->nice_name();
  } else {
    return std::string("unknown");
  }
}


