
#include "sliceinfo.hh"
#include "functioninfo.hh"

std::map<LoopInfo*, DySER::SliceInfo*> DySER::SliceInfo::_info_cache;

bool DySER::SliceInfo::mapInternalControlToDySER = true;
bool DySER::SliceInfo::useRPOIndexForOutput = false;

bool DySER::SliceInfo::shouldDySERize(bool vectorizable) {

  if (vectorizable) {
    // try dyserization -- vectorizable provides more benefits
    return true;
  }

  for(auto i=LI->calls_begin(),e=LI->calls_end();i!=e;++i) {
    FunctionInfo* fi = *i;
    if(!fi->isSinCos()) {
      return false; 
    }
  }

  int Total = OpList.size();
  int InSize =  getNumInputs();
  int OutSize = getNumOutputs();
  int lssize =  getNumLoadSlice();

  if (InSize == 0 || OutSize == 0) {
    // no input or output -- no dyser
    return false;
  }

  if (InSize + OutSize > (Total - lssize))
    return false;

  return true;
}

