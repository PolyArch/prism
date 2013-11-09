
#include "sliceinfo.hh"

std::map<LoopInfo*, DySER::SliceInfo*> DySER::SliceInfo::_info_cache;

bool DySER::SliceInfo::mapInternalControlToDySER = true;
