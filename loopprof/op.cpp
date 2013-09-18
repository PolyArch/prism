#include "op.hh"
#include "bb.hh"


uint32_t Op::_idcounter=0;
FunctionInfo* Op::func()   {return _bb->func();}


