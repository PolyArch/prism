/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2014 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include <iomanip>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "pin.H"
#include <map>
#include <set>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>
#include <string.h>

using namespace std;


class BB;

class InstInfo {
public:
  bool memRead, memWrite;
  xed_iclass_enum_t iclass;
  xed_category_enum_t category;
  bool hasImm;
  bool is_fp;
  ADDRINT dir_branch_addr;
  std::vector<LEVEL_BASE::REG> read_regs;
  std::vector<LEVEL_BASE::REG> write_regs;
  ADDRINT callee_func_addr;
  BB* bb;
  string dis;
  ADDRINT addr;

  InstInfo(INS ins, BB* in_bb) {
    hasImm=false;
    bb=in_bb;
    callee_func_addr=0;

    addr = INS_Address(ins);
    dis = INS_Disassemble(ins);

//            cout << dis;
//            if(INS_Category(ins) == XED_CATEGORY_COND_BR) {
//              cout << "(cond ctrl)";
//              if(!INS_HasFallThrough(ins)) {
//                //if(func->start==0x400530) {
//                  cout << INS_Address(ins) << " has no fall through";
//                //}
//              }
//            }
//            cout << "\n";


    if(INS_IsDirectCall(ins)) { 
      callee_func_addr = INS_DirectBranchOrCallTargetAddress(ins);
    }

    if(INS_IsDirectBranch(ins)) {
      dir_branch_addr=INS_DirectBranchOrCallTargetAddress(ins);
    } else {
      dir_branch_addr=0;
    } 

    for(unsigned r_ind = 0; r_ind < INS_MaxNumRRegs(ins); r_ind++) {
      REG read_reg = INS_RegR(ins,r_ind);
      read_regs.push_back(read_reg);
    }

    for(unsigned r_ind = 0; r_ind < INS_MaxNumWRegs(ins); r_ind++) {
      REG write_reg = INS_RegW(ins,r_ind);
      write_regs.push_back(write_reg);
    }


    //calculate if the instruction has any immediates
    for(UINT32 ii = 0; ii < INS_OperandCount(ins); ii++)
    {
      if (INS_OperandIsImmediate(ins,ii)) {
        hasImm=true;
      }

      /* Useful code, remember for later
      if (INS_OperandWritten(ins,ii)) {
        REG reg = INS_OperandReg(ins, ii); 
        if (((int)reg > 0 )
          && ((int)reg < 32)) {
            trace_op->dst = ((int) reg)%NUM_REG; 
        }
      }
      if (INS_OperandRead(ins,ii)) {
        REG reg = INS_OperandReg(ins, ii); 
        if (REG_is_fr(reg)) trace_op->is_fp = true; 
        if (((int)reg > 0 )
          && ((int)reg < 32)) { 
            trace_op->src[MIN2(num_src,2)] = ((int) reg)%NUM_REG; 
            num_src++; 
        }
      }*/
    }

    memRead=INS_IsMemoryRead(ins);
    memWrite=INS_IsMemoryWrite(ins);
    iclass = static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
    category = static_cast<xed_category_enum_t>(INS_Category(ins));
  }
};


class Func {
public:
  BB* head;
  std::map<ADDRINT,BB*> bbs;
  std::map<ADDRINT,InstInfo*> insts;
  std::set<Func*> callees;
  ADDRINT start;
  ADDRINT end;
  std::set <ADDRINT> start_insts;
  std::set <ADDRINT> stop_insts;
  std::set <ADDRINT> weird_lock_insts;
  std::string name;
};

class BB {
public:
  ADDRINT start;
  ADDRINT end;
  std::vector<InstInfo*> insts;

  std::set<ADDRINT> callee_funcs;

  void add_bb_r(BB* bb) {
    prev_bb.push_back(bb);
  }
  void add_bb(BB* bb) {
    next_bb.push_back(bb);
    bb->add_bb_r(this);
  }

  vector<BB*> next_bb, prev_bb;
};

std::map <uint64_t,Func*> func_map;

void get_callable(BB* orig_bb, BB* bb, std::set<Func*>& callable, std::set<BB*>& seen, std::set<BB*>& starts, bool& path_found) {
  if(seen.count(bb)) {
//    cout << "  SEENIT!\n";
    return;
  }

  seen.insert(bb);

  std::set<ADDRINT>::iterator ii,ee;
  for(ii=bb->callee_funcs.begin(), ee=bb->callee_funcs.end(); ii!=ee;++ii) {
    ADDRINT callee_func_addr = *ii;
    if(func_map.count(callee_func_addr)) {
      Func* func = func_map[callee_func_addr];
      callable.insert(func);
    }
  }

  if(starts.count(bb)) {
    if(orig_bb == bb) {
      //not possible for a call to occur in here -- ignore

    } else {
  //    cout << "  Path Found\n";
      path_found=true;
      return;
    }
  }

  std::vector<BB*>::iterator i,e;
  for(i=bb->prev_bb.begin(),e=bb->prev_bb.end();i!=e;++i) {
    BB* prev_bb = *i;
    //cout << prev_bb->start << "\n";;
    get_callable(orig_bb,prev_bb,callable,seen,starts,path_found);
  }
  return;
}

//Traverse CFG Bottom up to try to find start instruction.  Everything in between is callable
void get_callable(Func* func, set<Func*>& callable, set<BB*>& callable_from) {
  std::set<ADDRINT>::iterator i,e;

  std::set<BB*> start_bbs;
  bool path_found=true;

  for(i=func->start_insts.begin(),e=func->start_insts.end();i!=e;++i) {
    ADDRINT ins_addr = *i;
    InstInfo* inst_info = func->insts[ins_addr];
    start_bbs.insert(inst_info->bb);
  }

  for(i=func->stop_insts.begin(),e=func->stop_insts.end();i!=e;++i) {
    ADDRINT ins_addr=  *i;
    InstInfo* inst_info = func->insts[ins_addr]; 
    BB* bb = inst_info->bb;
    std::set<BB*> seen;
    path_found=false;
    get_callable(bb,bb,callable,seen,start_bbs,path_found);

    callable_from.insert(seen.begin(),seen.end());

/*    if(path_found==false) {
      cout << "For func:" << func->start;
      cout << " ----  WARNING, NO PATH FROM STOP BACK TO STARTS\n";
    }*/

    if(path_found==false) {
      break;
    }
  }

  if(path_found==false) {
    cout << "For func: " << func->start;
    cout << " -- WARNING, NO PATH FROM STOP BACK TO STARTS\n";
    cout << "Conservatively Applying Callable Analysis for Entire Function!\n";
    set<Func*>::iterator ii,ee;
    for(ii=func->callees.begin(),ee=func->callees.end();ii!=ee;++ii) {
      callable.insert(*ii);
    }
    std::map<ADDRINT,BB*>::iterator I,E;
    for(I=func->bbs.begin(),E=func->bbs.end();I!=E;++I) {
      BB* bb = I->second;
      callable_from.insert(bb);
    }
  }

}

void trans_helper(Func* func, std::set<Func*>& t_callable) {
  if(t_callable.count(func)) {
    return;
  }

  t_callable.insert(func);

  set<Func*>::iterator i,e;
  for(i=func->callees.begin(),e=func->callees.end();i!=e;++i) {
    trans_helper(*i,t_callable);
  }
  return;
}

//Traverse CFG Bottom up to try to find start instruction.  Everything in between is callable
void get_transitively_callable(Func* func, const set<Func*>& callable, set<Func*>& t_callable) {
  set<Func*>::iterator i,e;
  for(i=callable.begin(),e=callable.end();i!=e;++i) {
    Func* callee_func=*i;
    trans_helper(callee_func,t_callable);
  }
}



KNOB<string> KnobInputFile(KNOB_MODE_WRITEONCE, "pintool",
    "i", "<imagename>", "specify an image to read");

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool disassembles an image." << endl << endl;
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

typedef struct 
{
    ADDRINT start;
    ADDRINT end;
}RTN_INTERNAL_RANGE;

vector< RTN_INTERNAL_RANGE> rtnInternalRangeList;
/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(INT32 argc, CHAR **argv)
{

/*    int status = mkdir("pin-cfgs", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(status && status != EEXIST && status !=-1) {
      perror("Error: ");
      cout << "ERROR: mkdir pin-cfgs failed! " << status << " \n";
      return status;
    }*/

    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    IMG img = IMG_Open(KnobInputFile);

    if (!IMG_Valid(img))
    {
        std::cout << "Could not open " << KnobInputFile.Value() << endl;
        exit(1);
    }
    
    std::ofstream out;
    //std::string outfile = "pin-cfgs/" +  std::string( basename(KnobInputFile.Value().c_str()) )  + ".cfg";
    std::string outfile = std::string( basename(KnobInputFile.Value().c_str()) )  + ".cfg";
    cout << "writing to file: " << outfile << "\n";
    out.open(outfile, std::ofstream::out | std::ofstream::trunc);

    


    std::map<string,int> filenames;
    std::vector<string> filenames_vec;

    int counter=0;
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);
           
            if (!INS_Valid(RTN_InsHead(rtn)))
            {
                RTN_Close(rtn);
                continue;
            }

            ADDRINT func_head = INS_Address(RTN_InsHead(rtn));
            ADDRINT func_tail = INS_Address(RTN_InsTail(rtn));

            Func* func = new Func();
            func->start = func_head;
            func->end  = func_tail;
            func_map[func_head]=func;

            func->name = RTN_Name(rtn);

            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
              INT32 col=0,line=0;
              string filename;
              LEVEL_PINCLIENT::PIN_GetSourceLocation(INS_Address(ins),&col,&line,&filename);
              if(filenames.count(filename)==0) {
                filenames[filename]=counter++;
                filenames_vec.push_back(filename);
              }

            }

            RTN_Close(rtn);
        }
    }

    for(unsigned i = 0; i < filenames_vec.size(); i++) {
      string thing = filenames_vec[i];
      if(thing.size()==0) {
        thing=string("no-filename");
      }
      out << "FNAME: \"" << thing << "\"\n";
    }

    std::cout << hex;

    rtnInternalRangeList.clear();

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        out << "Section: " << setw(8) << SEC_Address(sec) << " " << SEC_Name(sec) << endl;
                
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            INT32 col=0,line=0;
            string filename;
            LEVEL_PINCLIENT::PIN_GetSourceLocation(RTN_Address(rtn),&col,&line,&filename);

            if(filename.size()==0) {
              filename=string("no-filename");
            }
            out << "Function: " << setw(8) << hex << RTN_Address(rtn) << " \"" << RTN_Name(rtn) << "\" \"" << filename <<  "\" " << dec << line << endl;
            //string path;
            //INT32 line;
            //PIN_GetSourceLocation(RTN_Address(rtn), NULL, &line, &path);

            //if (path != "")
            //{
            //    std::cout << "File " << path << " Line " << line << endl; 
            //}

            RTN_Open(rtn);
           
            if (!INS_Valid(RTN_InsHead(rtn)))
            {
                RTN_Close(rtn);
                continue;
            }

            ADDRINT func_head = INS_Address(RTN_InsHead(rtn));
            ADDRINT func_tail = INS_Address(RTN_InsTail(rtn));

            Func* func = func_map[func_head];

            RTN_INTERNAL_RANGE rtnInternalRange;
            rtnInternalRange.start = INS_Address(RTN_InsHead(rtn));
            rtnInternalRange.end 
              = INS_Address(RTN_InsHead(rtn)) + INS_Size(RTN_InsHead(rtn));
            INS lastIns = INS_Invalid();


            // ------------------------ ORIGINAL INTEGRITY CHECK ----------------------------------------------
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                //std::cout << "    " << setw(8) << hex << INS_Address(ins) << " " << INS_Disassemble(ins) << endl;
                if (INS_Valid(lastIns))
                {
                    if ((INS_Address(lastIns) + INS_Size(lastIns)) == INS_Address(ins))
                    {
                        rtnInternalRange.end = INS_Address(ins)+INS_Size(ins);
                    }
                    else
                    { 
                        rtnInternalRangeList.push_back(rtnInternalRange);
                        //std::cout << "  rtnInternalRangeList.push_back " << setw(8) << hex << rtnInternalRange.start << " " << setw(8) << hex << rtnInternalRange.end << endl;
                        // make sure this ins has not already appeared in this RTN
                        for (vector<RTN_INTERNAL_RANGE>::iterator ri = rtnInternalRangeList.begin(); ri != rtnInternalRangeList.end(); ri++)
                        {
                            if ((INS_Address(ins) >= ri->start) && (INS_Address(ins)<ri->end))
                            {
                                std::cerr << "***Error - above instruction already appeared in this RTN\n";
                                std::cerr << "  in rtnInternalRangeList " << setw(8) << hex << ri->start << " " << setw(8) << hex << ri->end << endl;
                                exit (1);
                            }
                        }
                        rtnInternalRange.start = INS_Address(ins);
                        rtnInternalRange.end = INS_Address(ins) + INS_Size(ins);
                    }
                }
                lastIns = ins;
            }
            // ------------------- END ORIGINAL INTEGRITY CHECK ------------------------------------------------

            std::map<ADDRINT, bool> bbs_brs;
            std::map<ADDRINT, INS>  inses;

            //Iterate through routine and figure out basic blocks and branches
            bbs_brs[func_head]=true; //mark function head as a BB
            bbs_brs[func_tail]=false; //mark function head as a BB

            //cout << "Func head tail " << func_head << " to " << func_tail << "\n";

            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {

              ADDRINT addr = INS_Address(ins);
              inses[addr]=ins;

              //Update Call Graph
              if(INS_IsDirectCall(ins)) { 
                ADDRINT callee_addr = INS_DirectBranchOrCallTargetAddress(ins);
                if(func_map.count(callee_addr)) { 
                  Func* callee_func = func_map[callee_addr];
                  func->callees.insert(callee_func); //yee haw
                }
              }

              //find start INST
              //CHANGE_THIS!
              if(static_cast<xed_iclass_enum_t>(INS_Opcode(ins))==XED_ICLASS_XCHG && INS_RegR(ins,0) == 38 && INS_RegR(ins,1) == 38) {
                //cerr << "Saw xchg bx,bx\n";
                //cerr << hex << INS_Address(ins) << ":" << INS_Disassemble(ins);
                //cerr << "\n";
                func->start_insts.insert(addr);
              } 
              //find stop INST
              //CHANGE_THIS!
              if(static_cast<xed_iclass_enum_t>(INS_Opcode(ins))==XED_ICLASS_XCHG && INS_RegR(ins,0) == 32 && INS_RegR(ins,1) == 32) {
                //cerr << "Saw xchg cx,cx\n";
                //cerr << hex << INS_Address(ins) << ":" << INS_Disassemble(ins);
                //cerr << "\n";
                func->stop_insts.insert(addr);
              } 

              if(INS_LockPrefix(ins)) {
                func->weird_lock_insts.insert(addr);
              }

              if(INS_IsBranch(ins)) {
                if(bbs_brs.count(addr)==0) {
                  bbs_brs[addr]=false; // mark branches as branches
                  //cout << addr << " is branch\n";
                }

                //fall through case
                if(INS_HasFallThrough(ins)) {
                  ADDRINT bb_addr = INS_NextAddress(ins);
                  if(bb_addr>=func_head && bb_addr <= func_tail && bb_addr != 0) {
                    bbs_brs[bb_addr]=true; // mark fall throughs as BBs
                    //cout << bb_addr << " is fall through BB\n";
                  }
                }

                //direct branch case
                if(INS_IsDirectBranchOrCall(ins)) {
                  ADDRINT bb_addr = INS_DirectBranchOrCallTargetAddress(ins);
                  if(bb_addr>=func_head && bb_addr <= func_tail && bb_addr != 0) {
                    bbs_brs[bb_addr]=true; // mark branch targets as BBs
                    //cout << bb_addr << " is br target BB\n";
                  }
                }
              }

              if(INS_IsDirectCall(ins)) {
                ADDRINT bb_addr = INS_NextAddress(ins);
                if(bb_addr>=func_head && bb_addr <= func_tail && bb_addr != 0) {
                  bbs_brs[bb_addr]=true; // mark instruction after call as BB
                  //cout << bb_addr << " is br target BB\n";
                }
              }


            }

            //create a basic block object for each marked BB
            std::map<ADDRINT,bool>::iterator i,e;
            ADDRINT prev_addrint = 0;
            bool    prev_is_bb = false;
            for (i=bbs_brs.begin(),e=bbs_brs.end();i!=e;++i) {
              ADDRINT addrint = i->first;
              bool is_bb = i->second;

              if(!inses.count(addrint)) {
                //cout << "ERROR: " << addrint << " not found\n";
                //cout << (is_bb ? "  is_bb" : "  not_is_bb") << "\n";
                continue;
              }

              if(prev_addrint && prev_is_bb) {
                INS prev_ins = inses[prev_addrint];
                INS ins = inses[addrint];
                INS final_ins = ins;

                //At this point, ins could be a branch or a basic block -- adjust final ins so that its the terminator
                if(is_bb) {
                  final_ins = INS_Prev(ins);
                }

                
                if(prev_is_bb) {
                  //I now need to integrity check to make sure we don't create any bad BBs
                  
                  INS ie=inses[prev_addrint];


                  //cout << "BB " << INS_Address(prev_ins) << "\n";
                  //cout << "E  " << INS_Address(final_ins) << "\n";
                  //cout << "Nx " << INS_Address(ins) << "\n";


                  while(INS_Valid(ie) && INS_HasFallThrough(ie) && INS_Address(ie) < INS_Address(final_ins)) {
                    ie=INS_Next(ie);
                  }

                  //cout << "RE " << INS_Address(ie) << "\n";


                  bool failed=false;
                  INS ia=inses[prev_addrint];
                  INS i1=ia;
                  INS i2=INS_Next(i1);
                  for(; INS_HasFallThrough(i1) && INS_Address(i1) < INS_Address(ie); i1=i2,i2=INS_Next(i2)) {
                    if((INS_Address(i1) + INS_Size(i1)) != INS_Address(i2) ) {
                      failed=true;
                      break;
                    }
                    //cout << "  S " << INS_Address(i1) << "\n";
                    //cout << "  N " << INS_Address(i2) << "\n";

                  }

                  if(!failed) {
                    BB* bb = new BB();
                    bb->start = INS_Address(prev_ins);
                    bb->end = INS_Address(ie);
                    func->bbs[bb->start]=bb;
                  } else {
                    //cout << "still failed!\n";
                  }
                  //cout << "bb " << INS_Address(prev_ins) << "\n";
                  //cout << "ia " << INS_Address(ia) << "\n";
                  //cout << "i1 " << INS_Address(i1) << "\n";
                  //cout << "i2 " << INS_Address(i2) << "\n";
                  //cout << "ie " << INS_Address(ie) << "\n";
                  //cout << "F  " << INS_Address(final_ins) << "\n";



#if 0                  
                  for(; INS_Valid(i2) && INS_Address(i2) < INS_Address(final_ins); i1=i2,i2=INS_Next(i2)) {
                    if((INS_Address(i1) + INS_Size(i1)) != INS_Address(i2)) {
                      failed=true;
                      //cout << "failed!\n";
                      break;
                    }
                    //cout << "  S " << INS_Address(i1) << "\n";
                    //cout << "  N " << INS_Address(i2) << "\n";
                  }

                  if(!failed && (INS_Address(i2)==INS_Address(final_ins) || INS_Address(prev_ins) == INS_Address(final_ins))) {
                    BB* bb = new BB();
                    bb->start = INS_Address(prev_ins);
                    bb->end = INS_Address(final_ins);
                    func->bbs[bb->start]=bb;
                  } else {
                    //cout << "still failed!\n";
                    //cout << "bb " << INS_Address(prev_ins) << "\n";
                    //cout << "i1 " << INS_Address(i1) << "\n";
                    //cout << "i2 " << INS_Address(i2) << "\n";
                    //cout << "F  " << INS_Address(final_ins) << "\n";
                  }
#endif        

                }
              }

              prev_addrint=addrint;
              prev_is_bb=is_bb;
            }

            if(prev_is_bb) { //if final instruction is its own BB
              BB* bb = new BB();
              bb->start = prev_addrint;
              bb->end = prev_addrint;
              func->bbs[bb->start]=bb;
            }
            
            //Now go through all BBs and hook up targets
            map<ADDRINT,BB*>::iterator ii,ee;
            for(ii=func->bbs.begin(),ee=func->bbs.end();ii!=ee;++ii) {
              BB* bb = ii->second;
              INS end_ins = inses[bb->end];

              if(!INS_Valid(end_ins)) {
                cerr << "ERROR: TAIL INST INVALID " << INS_Address(end_ins) << "(" << func_head << "," << func_tail << ")\n";
                return 1;
              }

              
              if(INS_IsDirectBranchOrCall(end_ins)) {
                ADDRINT addrint = INS_DirectBranchOrCallTargetAddress(end_ins);
                //cout << "trying to hook: " << bb->start << " to " << addrint << "\n";
                if(addrint >= func_head && addrint <= func_tail) {
                  if(func->bbs.count(addrint)) {
                    bb->add_bb(func->bbs[addrint]);    
                  } else {
                    if(func_map.count(addrint) && func_map[addrint]!=func) {
                      //don't do anything
                    } else {
                      //check if target is a lock instruction which we are branching into the middoe lf
                      if(func->weird_lock_insts.count(addrint-1) && func->bbs.count(addrint-1)) {
                        bb->add_bb(func->bbs[addrint-1]);    
                      } else {
                        cerr << "Branch Target FAILED! -- ";
                        cerr << INS_Disassemble(end_ins) << " --- ";
                        cerr << "func:" << hex << func->start << ": target:" << addrint << " origin_bb:" << bb->start << "\n";
                      }
                    }
                  }
                }            
              }

              if(INS_HasFallThrough(end_ins)) {
                ADDRINT addrint = INS_NextAddress(end_ins);
//                if(func->start==0x400530) {
//                  cout << "trying to hook: " << bb->start << " to " << addrint << "\n";
//                }
                if(addrint >= func_head && addrint <= func_tail) {
                  if(func->bbs.count(addrint)) {
                    bb->add_bb(func->bbs[addrint]);    
                  } else {
                    if(func_map.count(addrint) && func_map[addrint]!=func) {
                      //don't do anything
                    } else {
                      cerr << "Fall Through FAILED! -- ";
                      cerr << INS_Disassemble(end_ins) << " --- ";
                      cerr << "func:" << hex << func->start << ": target:" << addrint << " origin_bb:" << bb->start << "\n";
                    }
                  }
                }            
              } else {
//                if(func->start==0x400530) {
//                  cout << "no fall through\n";
//                }
              }

              if(INS_IsDirectCall(end_ins)) {
                ADDRINT addrint = INS_NextAddress(end_ins);
                if(addrint >= func_head && addrint <= func_tail) {
                  if(func->bbs.count(addrint)) {
                    bb->add_bb(func->bbs[addrint]);    
                  } else {
                    if(func_map.count(addrint) && func_map[addrint]!=func) {
                      //don't do anything
                    } else {
                      cerr << "Next After Call FAILED!\n";
                      cerr << INS_Disassemble(end_ins) << " --- ";
                      cerr << "func:" << func->start << ": target:" << addrint << " origin_bb:" << bb->start << "\n";
                    }
                  }
                }
              }

            }

            for(ii=func->bbs.begin(),ee=func->bbs.end();ii!=ee;++ii) {
              BB* bb = ii->second;

              INT32 col=0,line=0;
              string filename;
              LEVEL_PINCLIENT::PIN_GetSourceLocation(bb->start,&col,&line,&filename);
              int findex = filenames[filename];
              out << dec << line << "," << findex << ": ";

              out << hex << bb->start << "." << bb->end << "  ->   ";
              
              vector<BB*>::iterator i,e;
              for(i=bb->next_bb.begin(),e=bb->next_bb.end();i!=e;++i) {
                BB* next_bb = *i;
                out << next_bb->start << "." << next_bb->end << ", ";
              }
              out << "\n";
            }

            RTN_Close(rtn);
            rtnInternalRangeList.clear();
        }


        //go through instructions in a function, and associate with a BB
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn))
        {
            RTN_Open(rtn);
            if (!INS_Valid(RTN_InsHead(rtn)))  {
                RTN_Close(rtn);
                continue;
            }

            ADDRINT func_head = INS_Address(RTN_InsHead(rtn));
            Func* func = func_map[func_head];

            BB* cur_bb=NULL;
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
              ADDRINT ins_addr = INS_Address(ins);

              if(func->bbs.count(ins_addr)) {
                cur_bb=func->bbs[ins_addr];
              }

              if(cur_bb) {
                InstInfo* inst_info = new InstInfo(ins,cur_bb);
                cur_bb->insts.push_back(inst_info);
                func->insts[ins_addr]=inst_info;
                if(inst_info->callee_func_addr) {
                  cur_bb->callee_funcs.insert(inst_info->callee_func_addr);
                }
              }
            }

            RTN_Close(rtn);
        }


    }

    //Print out Full Cfg

    std::ofstream outf;
    //std::string outfile = "pin-cfgs/" +  std::string( basename(KnobInputFile.Value().c_str()) )  + ".cfg";
    std::string outfile_full = std::string( basename(KnobInputFile.Value().c_str()) )  + ".full-cfg";
    cout << "writing to file: " << outfile_full << "\n";
    outf.open(outfile_full, std::ofstream::out | std::ofstream::trunc);
               
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        outf << "Section: " << setw(8) << SEC_Address(sec) << " " << SEC_Name(sec) << endl;

        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
          RTN_Open(rtn);

          INT32 col=0,line=0;
          string filename;
          LEVEL_PINCLIENT::PIN_GetSourceLocation(RTN_Address(rtn),&col,&line,&filename);

          if(filename.size()==0) {
            filename=string("no-filename");
          }
          outf << "Function: " << setw(8) << hex << RTN_Address(rtn) << " \"" << RTN_Name(rtn) << "\" \"" << filename <<  "\" " << dec << line << endl;

          outf << hex;
          ADDRINT func_head = INS_Address(RTN_InsHead(rtn));
          Func* func = func_map[func_head];

          std::map<ADDRINT,BB*>::iterator i,e;
          for(i=func->bbs.begin(),e=func->bbs.end();i!=e;++i) {
            ADDRINT bb_addr = i->first;
            BB* bb = i->second;
            outf << hex << bb_addr << ":";
  
            std::vector<BB*>::iterator ii,ee;
            for(ii=bb->next_bb.begin(),ee=bb->next_bb.end();ii!=ee;++ii) {
              BB* next_bb = *ii;
              outf << hex << next_bb->start << " ";
            }
            outf << "  (";
            for(ii=bb->prev_bb.begin(),ee=bb->prev_bb.end();ii!=ee;++ii) {
              BB* prev_bb = *ii;
              outf << hex << prev_bb->start << " ";
            }            
            outf << ")\n";
  
            std::vector<InstInfo*>::iterator I,E;
            for(I=bb->insts.begin(),E=bb->insts.end();I!=E;++I) {
              InstInfo* ins= *I;
              outf << "     " << hex << ins->addr << ":" << ins->dis << "\n";
            }
          }

          RTN_Close(rtn);
          outf << "------------------------------------------------------------\n\n";
        }
    }
    outf.close();

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec))
    {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
          RTN_Open(rtn);
          ADDRINT func_head = INS_Address(RTN_InsHead(rtn));
          Func* func = func_map[func_head];
          
          std::set<Func*> callable;
          std::set<BB*> callable_from;
          get_callable(func,callable,callable_from);
          std::set<Func*>::iterator i,e;

          std::set<Func*> t_callable;
          get_transitively_callable(func,callable,t_callable);

          //if(callable_from.size() > 0) {
          if(t_callable.size() > 0) {
            cout << "critical regions of  \"" << func->name << "\"  can call:  ";
            for(i=t_callable.begin(),e=t_callable.end();i!=e;++i) {
              Func* callee_func = *i;
              cout << "\"" << callee_func->name << "\" ";
            }
            cout << "\n";
            cout << "from bbs: ";
            set<BB*>::iterator I,E;
            for(I=callable_from.begin(),E=callable_from.end();I!=E;++I) {
              BB* bb = *I;
              cout << hex << bb->start << " ";
            }
            cout << "\n";
          }

         RTN_Close(rtn);
        }
    }

    //Go through each stop instruction, find set of instructions which could be 


                    //cerr << hex << INS_Address(ins) << ":" << INS_Disassemble(ins);


    IMG_Close(img);
}
