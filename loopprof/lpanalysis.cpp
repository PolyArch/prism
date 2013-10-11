

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
                        bool no_gams,
                        bool gams_details,
                        uint64_t &count,
                        PathProf &pathProf)
{

  CP_NodeDiskImage* cp_array = new CP_NodeDiskImage[winsize];

  //Two Passes
  for (int pass = 1; pass <= 2; ++pass) {
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

    printf("LoopProf PASS %d\n", pass);

    count = 0;
    while (!inf.eof()) {
      int ind = (count % winsize);
      cp_array[ind] = CP_NodeDiskImage::read_from_file(inf);
      CP_NodeDiskImage& img = cp_array[ind];

      CPC cpc = std::make_pair(cp_array[ind]._pc,
                               cp_array[ind]._upc);
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
        if (prevCtrl) {
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
      } else if (pass == 2) {
        //Pass all instructions to phase 2
        pathProf.processOpPhase2(prevCPC, cpc, prevCall, prevRet, img);
      } else if (pass == 3) {
        //Pass all instructions to phase 3 -- this is what will be
	//used in the transform pass, used for debugging purposes only
        pathProf.processOpPhase3(cpc, prevCall, prevRet);
      }

      prevCPC = cpc;
      prevCtrl = img._isctrl;
      prevCall = img._iscall;
      prevRet = img._isreturn;

      if (count == max_inst) {
        break;
      }
      if (count && (count % 10000000 == 0)) {
        std::cout << "pass " << pass << " processed " << count << " instructions\n";
      }
    }

    if (pass == 1) {
      pathProf.runAnalysis();
    } else if (pass == 2) {
      pathProf.runAnalysis2(no_gams, gams_details);
    }
    inf.close();
  }
  delete[] cp_array;

  return true;
}
