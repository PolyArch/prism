
#include "cp_disasm.hh"
#include "cp_registry.hh"

static RegisterCP<disasm::cp_disasm> DIS("disasm", true,
                                         false, false, true);

__attribute__((__constructor__))
static void init()
{
}
