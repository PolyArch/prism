#ifndef CRITPATH_CPNODE_HH
#define CRITPATH_CPNODE_HH

#include <stdint.h>

#include <cassert>

#define STANDALONE_CRITPATH 1
#include "../../src/cpu/crtpath/crtpathnode.hh"

class CP_Node {

public:
  uint64_t fetch_cycle;
  uint64_t dispatch_cycle;
  uint64_t ready_cycle;
  uint64_t execute_cycle;
  uint64_t complete_cycle;
  uint64_t committed_cycle;
  uint64_t index;
  bool ctrl_miss;

  uint64_t ff_cycle;
  uint64_t icache_cycle;
  uint64_t other_fetch_cycle;
  uint64_t fd_cycle;
  uint64_t dr_cycle;
  uint64_t re_cycle;
  uint64_t ec_cycle;
  uint64_t cc_cycle;

  bool ignore;

  CP_NodeDiskImage _img;
  CP_Node(): fetch_cycle(0),
             dispatch_cycle(0), execute_cycle(0),
             complete_cycle(0), committed_cycle(0), index(0),
             ctrl_miss(false),
             ff_cycle(0), icache_cycle(0), other_fetch_cycle(0),
             fd_cycle(0), dr_cycle(0),
             re_cycle(0), ec_cycle(0), cc_cycle(0)
  {}
  CP_Node(const CP_NodeDiskImage &img, uint64_t index):
    fetch_cycle(0),
    dispatch_cycle(0),
    ready_cycle(0),
    execute_cycle(0), complete_cycle(0),
    committed_cycle(0), index(index),
    ctrl_miss(img._ctrl_miss),
    ff_cycle(img._fc),
    icache_cycle(img._icache_lat),
    other_fetch_cycle(img._fc - img._icache_lat),
    fd_cycle(img._dc),
    dr_cycle(img._rc - img._dc),
    re_cycle(img._ec - img._rc),
    ec_cycle(img._cc - img._ec),
    cc_cycle(img._cmpc - img._cc), _img(img)
  {}
  void print_to_stream(std::ostream &out) {
    out << fetch_cycle << "::"
        << dispatch_cycle << " :: " << committed_cycle << "\n";
  }
  void setIgnore() { ignore = true; }
  bool ignored() const { return ignore;}
};

#endif
