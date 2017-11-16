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

class Func {
public:
  BB* head;
  std::map<ADDRINT,BB*> bbs;
  ADDRINT start;
  ADDRINT end;
};

class BB {
public:
  ADDRINT start;
  ADDRINT end;

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

            Func* func = new Func();
            func->start = func_head;
            func->end  = func_tail;
            func_map[func_head]=func;

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
                    //cout << "FAILED!\n";
                  }
                }            
              }

              if(INS_HasFallThrough(end_ins)) {
                ADDRINT addrint = INS_NextAddress(end_ins);
                //cout << "trying to hook: " << bb->start << " to " << addrint << "\n";
                if(addrint >= func_head && addrint <= func_tail) {
                  if(func->bbs.count(addrint)) {
                    bb->add_bb(func->bbs[addrint]);    
                  } else {
                    //cout << "FAILED!\n";
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

    }
    IMG_Close(img);
}
