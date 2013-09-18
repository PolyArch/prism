#ifndef COMPUTED_CP_HH
#define COMPUTED_CP_HH


#include <vector>


#include "critpath.hh"

extern int FETCH_WIDTH;
extern int COMMIT_WIDTH;
extern int IQ_WIDTH;
extern int ROB_SIZE;
extern int LQ_SIZE;
extern int SQ_SIZE;

extern int BR_MISS_PENALTY;


class ComputedCPBase : public CriticalPath {
public:
  ComputedCPBase() : CriticalPath(), _curCycle(0) {}
  virtual ~ComputedCPBase() { }

  std::vector<CP_Node> InstQueue;
  std::vector<CP_Node> LQ;
  std::vector<CP_Node> SQ;

  uint64_t _curCycle;
  virtual void inserted() {
    assert(_nodes.size() > 0);
    CP_Node &node = _nodes[-1];
    //fetch is the cycle
    _curCycle = node.fetch_cycle;
    //remove instruction from IQ
    InstQueue.push_back(node);
    for (std::vector<CP_Node>::iterator I = InstQueue.begin();
         I != InstQueue.end();) {
      if (I->execute_cycle < _curCycle) {
        I = InstQueue.erase(I);
      } else {
        ++I;
      }
    }
    if (node._img._isload)
      LQ.push_back(node);
    if (node._img._isstore)
      SQ.push_back(node);

    for (std::vector<CP_Node>::iterator I = LQ.begin();
         I != LQ.end();) {
      if (I->committed_cycle < _curCycle) {
        I = LQ.erase(I);
      } else {
        ++I;
      }
    }
    for (std::vector<CP_Node>::iterator I = SQ.begin();
         I != SQ.end();) {
      if (I->committed_cycle < _curCycle) {
        I = SQ.erase(I);
      } else {
        ++I;
      }
    }
    CriticalPath::inserted();
  }


  virtual CP_Node &setFetchCycle(CP_Node &n) {
    checkFF(n);
    checkFBW(n);
    checkPF(n);

    checkPipeStalls(n);
    checkIQStalls(n);
    checkLSQStalls(n);
    return n;
  }


  virtual CP_Node &setDispatchCycle(CP_Node &n) {
    checkFD(n);
    checkCD(n);
    return n;
  }


  virtual CP_Node &setReadyCycle(CP_Node &n) {
    checkDR(n);
    checkPR(n);
    return n;
  }

  virtual CP_Node &setExecuteCycle(CP_Node &n) {
    checkRE(n);
    //checkFuncUnits(n);
    return n;
  }

  virtual CP_Node &setCompleteCycle(CP_Node &n) {
    checkEP(n);
    checkPP(n);
    return n;
  }
  virtual CP_Node &setCommittedCycle(CP_Node &n) {

     checkPC(n);
     checkCC(n);
     checkCBW(n);

     return n;
  }


  //==========FETCH ==============
  //Inorder fetch
  virtual CP_Node &checkFF(CP_Node &n) {
    if (_nodes.size() > 0) {
      CP_Node &prev_node = _nodes.back();
      if (n.fetch_cycle < prev_node.fetch_cycle + n.icache_cycle) {
        n.fetch_cycle = prev_node.fetch_cycle + n.icache_cycle;
      }
    }
    return n;
  }

  //Finite fetch bandwidth
  virtual CP_Node &checkFBW(CP_Node &n) {
    if (_nodes.size() > (unsigned)FETCH_WIDTH) {
      CP_Node &prev_node = _nodes[-FETCH_WIDTH]; //should we get rid of n.icache_cycle?
      if (n.fetch_cycle < prev_node.fetch_cycle+1+n.icache_cycle) {
        n.fetch_cycle = prev_node.fetch_cycle+1+n.icache_cycle;
      }
      //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Fetch,
      // n.index, dg_inst<Event_t,Edge_t>::Fetch, 1);
    }
    return n;
  }

  //Control Dep
  virtual CP_Node &checkPF(CP_Node &n) {
    if (_nodes.size() > 0) {
      CP_Node &prev_node = _nodes.back();
      if (prev_node.ctrl_miss) {
        unsigned brPenalty = (BR_MISS_PENALTY);
        if (n.fetch_cycle < (prev_node.complete_cycle + brPenalty
                             +n.icache_cycle)) {
          n.fetch_cycle = (prev_node.complete_cycle
                           + brPenalty
                           + n.icache_cycle);
        }
        //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
        //                  n.index, dg_inst<Event_t,Edge_t>::Fetch, brPenalty);
      }
    }
    return n;
  }

  virtual CP_Node &checkIQStalls(CP_Node &n)
  {
#if 1
    if (InstQueue.size() < (unsigned)IQ_WIDTH)
      return n;

    uint64_t min_issued_cycle = (uint64_t)-1;

    for (std::vector<CP_Node>::iterator I = InstQueue.begin(),
           E = InstQueue.end(); I != E; ++I) {
      if (min_issued_cycle > I->execute_cycle) {
        min_issued_cycle = I->execute_cycle;
      }
    }
    if (n.fetch_cycle < min_issued_cycle + n.icache_cycle + 1) {
      n.fetch_cycle = min_issued_cycle + n.icache_cycle + 1;
    }
    return n;

#else

    if (_nodes.size() < IQ_WIDTH)
      return n;

    uint64_t min_issued_cycle = (uint64_t)-1;
    //number of instructions not executed exceeds the IQ_WIDTH, stall the fetch

    unsigned numInIQ = 0;
    CP_Node min_node;
    const int e = _nodes.size();
    for (int i = 0; i != e; ++i) {
      CP_Node &prev_node = _nodes[-(i+1)];

      if (prev_node.ignored())
        continue;

      if (prev_node.committed_cycle < n.fetch_cycle) {
        //prev node committed before the fetch -- no need to check further
        break;
      }

      if (prev_node.execute_cycle > n.fetch_cycle) {
        //fetch happened for this node when prev_node is in IQ
        ++numInIQ;
        if (min_issued_cycle > prev_node.execute_cycle) {
          min_issued_cycle = prev_node.execute_cycle;
          min_node = prev_node;
        }
      }
      assert(numInIQ <= IQ_WIDTH);
      if (numInIQ == IQ_WIDTH) {
        //IQ stall
        assert(min_issued_cycle >= n.fetch_cycle);
        n.fetch_cycle = min_issued_cycle + n.icache_cycle + 1;
        //cpdg.insert_edge(min_node.index, dg_inst<Event_t,Edge_t>::Execute,
        //                 n.index, dg_inst<Event_t,Edge_t>::Fetch, 1);

        break;
      }
    }
#endif
  }


  virtual CP_Node &checkPipeStalls(CP_Node &n) {
    if (_nodes.size() < (unsigned)3*FETCH_WIDTH)
      return n;
    CP_Node &prev_node = _nodes[-(3*FETCH_WIDTH)];
    // 0   -1     -2        -3
    // F -> De -> Rename -> Dispatch
    // F -> De -> Rename -> Dispatch
    // F -> De -> Renane -> Dispatch

    //tjn: should we add n.icache_cycle?
    if (n.fetch_cycle < prev_node.dispatch_cycle + 1) {
      n.fetch_cycle = prev_node.dispatch_cycle + 1;
    }
    //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Dispatch,
    //  n.index, dg_inst<Event_t,Edge_t>::Fetch, 1);

    return n;
  }
  virtual CP_Node &checkLSQStalls(CP_Node &n) {
    //number of load/store instructions not executed exceeds the LSQ_WIDTH,
    //stall the fetch

    if (LQ.size() < (unsigned)LQ_SIZE && SQ.size() < (unsigned)SQ_SIZE)
      return n;

    uint64_t min_issued_cycle = (uint64_t)-1;

    for (std::vector<CP_Node>::iterator I = LQ.begin(),
           E = LQ.end(); I != E; ++I) {
      if (min_issued_cycle > I->committed_cycle) {
        min_issued_cycle = I->committed_cycle;
      }
    }
    for (std::vector<CP_Node>::iterator I = SQ.begin(),
           E = SQ.end(); I != E; ++I) {
      if (min_issued_cycle > I->committed_cycle) {
        min_issued_cycle = I->committed_cycle;
      }
    }
    if (min_issued_cycle > n.fetch_cycle) {
      n.fetch_cycle = min_issued_cycle + 1;
    }
    return n;
  }



  //========== DISPATCH ==============

  //Dispatch follows fetch
  virtual CP_Node &checkFD(CP_Node &n) {
    if (n.dispatch_cycle < n.fetch_cycle + 3)
      n.dispatch_cycle =  n.fetch_cycle + 3;

    //cpdg.insert_edge(n.index, dg_inst<Event_t,Edge_t>::Fetch,
    //                     n.index, dg_inst<Event_t,Edge_t>::Dispatch, 3);

    return n;
  }

  //Finite ROB
  virtual CP_Node &checkCD(CP_Node &n) {
    if (_nodes.size() > (unsigned)ROB_SIZE) {
      CP_Node &prev_node = _nodes[-ROB_SIZE];
      if (n.dispatch_cycle < prev_node.committed_cycle+1) {
        n.dispatch_cycle = prev_node.committed_cycle+1;
      }
      //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Commit,
      //                 n.index, dg_inst<Event_t,Edge_t>::Dispatch, 3);

    }
    return n;
  }

  //========== READY ==============
  //Ready Follows Dispatch
  virtual CP_Node &checkDR(CP_Node &n)
  {
    if (n.ready_cycle < n.dispatch_cycle + n.de_cycle) {
      n.ready_cycle = n.dispatch_cycle + 1; //n.de_cycle;
    }
    //cpdg.insert_edge(n.index, dg_inst<Event_t,Edge_t>::Dispatch,
    //                 n.index, dg_inst<Event_t,Edge_t>::Ready, 1);

    return n;
  }



  //Data dependence
  virtual CP_Node &checkPR(CP_Node &n) {
    //register dependence
    for (int i = 0; i < 7; ++i) {
      int prod = n._img._prod[i];
      if (prod <= 0)
        continue;
      if (_nodes.size() >= (unsigned)prod) {
        CP_Node &prev_node = _nodes[-prod];
        if (n.ready_cycle < prev_node.complete_cycle) {
          n.ready_cycle = prev_node.complete_cycle;
        }
        //cpdg.insert_dd_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
        //                 n.index, dg_inst<Event_t,Edge_t>::Ready, getFUIssueLatency(n));

      }
    }
    //memory dependence
    if (n._img._mem_prod > 0) {
      if (_nodes.size() > n._img._mem_prod) {
        CP_Node &prev_node = _nodes[-n._img._mem_prod];
        if (prev_node._img._isstore && n._img._isload) {
          if (n.ready_cycle < prev_node.complete_cycle)
            n.ready_cycle = prev_node.complete_cycle;
          //cpdg.insert_dd_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
          // n.index, dg_inst<Event_t,Edge_t>::Ready, 0);
        } else if (prev_node._img._isstore && n._img._isstore) {
          if (n.complete_cycle < prev_node.complete_cycle) {
            n.complete_cycle = prev_node.complete_cycle;
          }
          //cpdg.insert_dd_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
          //                 n.index, dg_inst<Event_t,Edge_t>::Complete, 0);

        } else if (prev_node._img._isload && n._img._isstore) {
          if (n.complete_cycle < prev_node.complete_cycle) {
            n.complete_cycle = prev_node.complete_cycle;
          }
          //cpdg.insert_dd_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
          //                    n.index, dg_inst<Event_t,Edge_t>::Complete, 0);

        }
      }
    }
    return n;
  }

  //==========EXECUTION ==============
  //Execution follows Ready

  virtual bool isSameOpClass(int opclass1, int opclass2) {
    if (opclass1 == opclass2)
      return true;
    if (opclass1 > 9 && opclass2 > 9) {
      //all simd units are same
      return true;
    }
    switch(opclass1) {
    default: return false;
    case 2:
    case 3:
      return (opclass2 == 2 || opclass2 ==3);
    case 4:
    case 5:
    case 6:
      return (opclass2 == 4 || opclass2 == 5 || opclass2 == 6);
    case 7:
    case 8:
    case 9:
      return (opclass2 == 7 || opclass2 == 8 || opclass2 == 9);
    }
    return false;
  }

  virtual unsigned getNumFUAvailable(CP_Node &n) {
    switch(n._img._opclass) {
    case 0: //No_OpClass
      return ROB_SIZE+IQ_WIDTH;
    case 1: //IntALU
      return 6;

    case 2: //IntMult
    case 3: //IntDiv
      return 2;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return 4;
    case 7: //FloatMult
    case 8: //FloatDiv
    case 9: //FloatSqrt
      return 2;
    default:
      return 4;
    }
    return 4;
  }

  virtual unsigned getFUIssueLatency(CP_Node &n) {
    switch(n._img._opclass) {
    case 0: //No_OpClass
      return 1;
    case 1: //IntALU
      return 1;

    case 2: //IntMult
      return 1;
    case 3: //IntDiv
      return 19;

    case 4: //FloatAdd
    case 5: //FloatCmp
    case 6: //FloatCvt
      return 1;
    case 7: //FloatMult
      return 1;
    case 8: //FloatDiv
      return 12;
    case 9: //FloatSqrt
      return 24;
    default:
      return 1;
    }
    return 1;
  }


  virtual CP_Node &checkFuncUnits(CP_Node &n) {
    if (n._img._isload || n._img._isstore)
      return n;

    uint64_t min_cycle_available = n.execute_cycle+50;
    unsigned numOfFUAvailable = getNumFUAvailable(n);
    assert(numOfFUAvailable > 0);
    const int e = _nodes.size();
    CP_Node min_node;
    for (int i = 0; i != e; ++i) {
      CP_Node &prev_node = _nodes[-(i+1)]; //seems wrong, b/c inst can be before
      if (prev_node.execute_cycle +25 < n.execute_cycle)
        break;

      if (isSameOpClass(prev_node._img._opclass, n._img._opclass)) {
        -- numOfFUAvailable;

        if (min_cycle_available > (prev_node.execute_cycle+ getFUIssueLatency(n))) {
          min_cycle_available = prev_node.execute_cycle + getFUIssueLatency(n);
          min_node = prev_node;
        }
      }
      if (numOfFUAvailable == 0) {
        break;
      }
    }
    if (numOfFUAvailable != 0) {
      return n;
    }
    if (min_cycle_available > n.execute_cycle) {

      n.execute_cycle = min_cycle_available;
      //cpdg.insert_edge(min_node.index, dg_inst<Event_t,Edge_t>::Execute,
      //                 n.index, dg_inst<Event_t,Edge_t>::Execute, getFUIssueLatency(n));
    }
    return n;
  }


  virtual CP_Node &checkRE(CP_Node &n)
  {
    if (n.execute_cycle < n.ready_cycle) {
      n.execute_cycle = n.ready_cycle;
    }
    //cpdg.insert_edge(n.index, dg_inst<Event_t,Edge_t>::Ready,
    //                 n.index, dg_inst<Event_t,Edge_t>::Execute, 0);

    return n;
  }

  //==========COMPLTE ==============
  //Complete After Execute
  virtual CP_Node &checkEP(CP_Node &n) {
    if (n.complete_cycle < n.execute_cycle + n.ec_cycle)
      n.complete_cycle = n.execute_cycle + n.ec_cycle;

    //cpdg.insert_edge(n.index, dg_inst<Event_t,Edge_t>::Execute,
    //n.index, dg_inst<Event_t,Edge_t>::Complete, n.ec_cycle);

    return n;
  }

  //cache miss dependencies only
  virtual CP_Node &checkPP(CP_Node &n) {
    if (_nodes.size() > n._img._cache_prod) {
      CP_Node &prev_node = _nodes[-n._img._cache_prod];
      if (n.complete_cycle < prev_node.complete_cycle) {
        n.complete_cycle = prev_node.complete_cycle;
      }
      //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Complete,
      //                 n.index, dg_inst<Event_t,Edge_t>::Complete, 0);

    }
    return n;
  }

  //Commit follows complete
  virtual CP_Node &checkPC(CP_Node &n) {
    if (n.committed_cycle < n.complete_cycle + 1)
      n.committed_cycle = n.complete_cycle + 1;

    //cpdg.insert_edge(n.index, dg_inst<Event_t,Edge_t>::Complete,
    //                     n.index, dg_inst<Event_t,Edge_t>::Commit, 1);

    return n;
  }

  //in order commit
  virtual CP_Node &checkCC(CP_Node &n) {
    if (_nodes.size() > 0) {
      CP_Node &prev_node = _nodes.back();
      if (n.committed_cycle < prev_node.committed_cycle) {
        n.committed_cycle = prev_node.committed_cycle;
      }
      //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Commit,
      //n.index, dg_inst<Event_t,Edge_t>::Commit, 0);

    }
    return n;
  }

  //Finite commit width
  virtual CP_Node &checkCBW(CP_Node &n) {
    if (_nodes.size() > (unsigned)COMMIT_WIDTH) {
      CP_Node &prev_node = _nodes[-COMMIT_WIDTH];
      if (n.committed_cycle < prev_node.committed_cycle+1) {
        n.committed_cycle = prev_node.committed_cycle+1;
      }
      //cpdg.insert_edge(prev_node.index, dg_inst<Event_t,Edge_t>::Commit,
      //n.index, dg_inst<Event_t,Edge_t>::Commit, 1);

    }
    return n;

  }

};


#endif
