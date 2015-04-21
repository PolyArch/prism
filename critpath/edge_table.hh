#ifndef EDGE_TABLE_NAMES_HH
#define EDGE_TABLE_NAMES_HH

#define EDGE_TABLE                                      \
  X(DEF,  "DEF", "Default Type")                        \
  X(NONE, "NONE","Type Not Specified")                  \
  X(FF,   "FF",  "Fetch to Fetch")                      \
  X(IC,   "IC",  "ICache Miss")                         \
  X(FBW,  "FBW", "Fetch Bandwidth")                     \
  X(FPip, "FPip","Frontend Pipe BW")                    \
  X(LQTF, "LQTF","Load/Store Queue To Fetch")           \
  X(BP,   "BP",  "Branch Predict (in-order)")           \
  X(CM,   "CM",  "Control Miss (OoO)")                  \
  X(IQ,   "IQ",  "Instruction Queue Full")              \
  X(LSQ,  "LSQ", "LoadStore Queue Full")                \
  X(FD,   "FD",  "Fetch to Dispatch")                   \
  X(DBW,  "DBW", "Dispatch Bandwidth")                  \
  X(DD,   "DD",  "Dispatch Inorder")                    \
  X(ROB,  "ROB", "ROB Full")                            \
  X(DR,   "DR",  "Dispatch to Ready")                   \
  X(NSpc, "NSpc","non-speculative inst")                \
  X(RDep, "RDep","Register Dependence")                 \
  X(MDep, "MDep","Memory Dependnece")                   \
  X(EPip, "EPip","Execution Pipeline BW")               \
  X(EE,   "EE",  "Execute to Execute")                  \
  X(IBW,  "IBW", "Issue Width")                         \
  X(RE,   "RE",  "Ready to Execute")                    \
  X(FU,   "FU",  "Functional Unit Hazard")              \
  X(EP,   "EP",  "Execute to Complete")                 \
  X(WBBW, "WBBW","WriteBack BandWidth")                 \
  X(MSHR, "MSHR","MSHR Resource")                       \
  X(MP,   "MP",  "Memory Port Resource")                \
  X(WB,   "WB",  "Writeback")                           \
  X(PP,   "PP",  "Cache Dep")                           \
  X(PPS,   "PPS",  "Cache Dep (store)")                 \
  X(PPO,   "PPO",  "Cache Dep (only one)")              \
  X(PPN,   "PPN",  "Cache Dep (new edge)")              \
  X(PPW,   "PPW",  "Cache Dep (post-swap)")             \
  X(PP_,   "PP_",  "Cache Dep (orig)")                  \
  X(PC,   "PC",  "Complete To Commit")                  \
  X(SQUA, "SQUA","Squash Penalty")                      \
  X(CC,   "CC",  "Commit to Commit")                    \
  X(CBW,  "CBW", "Commit B/W")                          \
  X(SER,  "SER", "serialize this instruction")          \
  X(CXFR, "CXFR","CCores Control Transfer")             \
  X(CSBB, "CSBB","CCores Seralize BB Activation")       \
  X(BBA,  "BBA", "CCores Activate Basic Block")         \
  X(BBE,  "BBR", "CCores Basic Block Ready")            \
  X(BBC,  "BBC", "CCores Basic Block Complete")         \
  X(BBN,  "BBN", "CCores Basic Block Not sure")         \
  X(SEBB, "SEBB", "SEB Region Begin")                   \
  X(SEBA, "SEBA", "SEB Activate")                       \
  X(SEBW, "SEBW", "SEB Writeback")                      \
  X(SEBS, "SEBS", "SEB Serialization")                  \
  X(SSDF, "SSDF", "SEB Serialization, dataflow")        \
  X(SSMD, "SSMD", "SEB Serialization, memory dep")        \
  X(SEBL, "SEBL", "SEB Done")    \
  X(SEB,  "SEB", "???")                                 \
  X(SEBD, "SEBD", "Intra-SEB Data Dependence")                \
  X(SEBX, "SEBX", "Inter-SEB Data Dependence")                \
  X(BREP, "BREP", "Beret Replay")                               \
  X(BXFR, "BXFR", "Beret Data Transfer")                        \
  X(CHT,  "CHT",  "cheat edge for super insts")                 \
  X(DyDep, "DyDep", "DySER Dependence")                         \
  X(DyRR,  "DyRR", "DySER Functional unit ready queue")         \
  X(DyRE,  "DyRE",  "DySER Ready to Execute")                   \
  X(DyFU,  "DyFU", "DySER Functional unit pipelined")           \
  X(DyEP,  "DyEP", "DySER Functional Execute to complete")      \
  X(DyPP,  "DyPP", "DySER Complete To Complete")                \
  X(DyCR,  "DyCR", "DySER Commit to ready")                     \
  X(NPUPR, "NPUPR", "NPU Complete to ready")                    \
  X(NPUFE, "NPUFE", "NPU Fake Edges")                    \
  X(NCFG,  "NCFG",  "NLA Config") \
  X(NCPU,  "NCPU",  "NLA to CPU Time") \
  X(NSER,  "NSER",  "NLA CFU Serialization") \
  X(NCTL,  "NCTL",  "NLA CTRL Serialization") \
  X(NSLP,  "NSLP",  "NLA Serialize Loop") \
  X(NITR,  "NITR",  "NLA Iteration Limit") \
  X(NNET,  "NNET",  "NLA Network Conflict") \
  X(CFUR,  "CFUR",  "CFU Ready (operands ready)") \
  X(CFUB,  "CFUB",  "CFU Begin to Execute") \
  X(CFUE,  "CFUE",  "CFU Complete to End") \
  X(NFWD,  "NFWD",  "NLA Forward") \
  X(NDWR,  "NDWR",  "NLA Delay CFU Writes") \
  X(NMTK,  "NMTK",  "NLA Memory Token") \
  X(HORZ,  "HORZ",  "The HORIZON")                    \
  X(NUM,   "NUM",  "LAST (should not see)")        

#define X(a, b, c) E_ ## a,
enum EDGE_TYPE {
  EDGE_TABLE
};
#undef X

#define X(a, b, c) b,
static const char *edge_name[] = {
  EDGE_TABLE
};
#undef X

#endif
