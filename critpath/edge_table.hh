#ifndef EDGE_TABLE_NAMES_HH
#define EDGE_TABLE_NAMES_HH

#include "cp_dep_graph.hh"

#define X(a, b, c) b,
static const char *edge_name[] = {
  EDGE_TABLE
};
#undef X

#endif
