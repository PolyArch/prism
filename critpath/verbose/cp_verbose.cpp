
#include "cp_verbose.hh"
#include "cp_registry.hh"

static RegisterCP<verbose::cp_verbose> Verbose("verbose",
                                               true,
                                               false,
                                               false,
                                               true);

__attribute__((__constructor__))
static void init()
{
}
