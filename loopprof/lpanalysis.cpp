

#include "cpu/crtpath/crtpathnode.hh"

#include "lpanalysis.hh"
#include "gzstream.hh"
#include "exec_profile.hh"

#include <iostream>
#include <utility>

bool doLoopProfAnalysis(const char *trace_fname,
                        uint64_t max_inst,
                        int winsize,
                        bool verbose,
                        bool size_based_cfus,
                        bool no_gams,
                        bool gams_details,
                        uint64_t &count,
                        bool extra_pass,
                        PathProf &pathProf)
{

  //We'll put some files here for looking at later
  system("mkdir -p stats/");

  CP_NodeDiskImage* cp_array = new CP_NodeDiskImage[winsize];

  //Two Passes
  for (int pass = 1; pass <= 2+extra_pass; ++pass) {
    igzstream inf(trace_fname, std::ios::in | std::ios::binary);

    if (!inf.is_open()) {
      std::cerr << "Cannot open file " << trace_fname << "\n";
      delete[] cp_array;
      return false;
    }

    bool prevCtrl = true;
    bool prevCall = true;
    bool prevRet = false;

    CPC prevCPC;
    prevCPC.first = 0;
    prevCPC.second = 0;

    pathProf.resetStack(prevCPC, prevCtrl, prevCall, prevRet);

    std::cout << "LoopProf PASS " << pass << "\n";

    count = 0;
    while (!inf.eof()) {
      int ind = (count % winsize);
      cp_array[ind] = CP_NodeDiskImage::read_from_file(inf);
      CP_NodeDiskImage& img = cp_array[ind];

      CPC cpc = std::make_pair(cp_array[ind]._pc,
                               (uint8_t)cp_array[ind]._upc);

      if(cpc.first==0) {
        break; //unreal instructions should be ignored
      }

      ++count;


      #if 0
      if (pass == 2) {
        std::cout << cpc.first << "," << cpc.second << " : "
                  << ExecProfile::getDisasm(cpc.first, cpc.second) << "\n";
      }
      #endif

      if (verbose && pass == 2) {
        std::cout << count << ":  ";
        img.write_to_stream(std::cout);
        std::cout << "\n";
      }

      if (pass == 1) {
        static int skipInsts = 0;
        //This condition determines whether we start a new bb
        if(cp_array[ind]._upc==0 && (prevCtrl || pathProf.isBB(cpc))) { 
        //old way: if (prevCtrl) {
          if (skipInsts > 0) {
            //cout << "------------------skip ---------" << skipInsts << "\n";
            pathProf.setSkipInsts(skipInsts);
            skipInsts = -1;
          }
          //Only pass control instructions to phase 1
          pathProf.processOpPhase1(prevCPC, cpc, prevCall, prevRet);

        } else if (skipInsts != -1) {
          skipInsts++;
        }
        pathProf.processAddr(cpc, img._eff_addr, img._isload, img._isstore);

      } else if (pass == 2) {
        //Pass all instructions to phase 2
        pathProf.processOpPhase2(prevCPC, cpc, prevCall, prevRet, img);
      } else if (pass == 3) {
        //Pass all instructions to phase 3 -- this is what will be
	//used in the transform pass, used for debugging purposes only
        pathProf.processOpPhaseExtra(cpc, prevCall, prevRet);
      }

      prevCPC = cpc;
      prevCtrl = img._isctrl;
      prevCall = img._iscall;
      prevRet = img._isreturn;

      if (count == max_inst) {
        break;
      }
      if (count && (count % 10000000 == 0)) {
        std::cout << "\rpass " << pass << " processed " << count << " instructions";
        std::cout.flush();
      }
    }
    std::cout << "\n";

    if (pass == 1) {
      pathProf.runAnalysis();
    } else if (pass == 2) {
      pathProf.runAnalysis2(no_gams, gams_details, size_based_cfus, count);
    }
    inf.close();
  }
  delete[] cp_array;

  pathProf.setStopInst(count);

  return true;
}
