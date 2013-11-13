#include "op.hh"
#include "bb.hh"
#include "functioninfo.hh"

uint32_t Op::_idcounter=0;
FunctionInfo* Op::func()   {return _bb->func();}
std::string Op::getCalledFuncName() const
{
  return _calledFunc->nice_name();
}


