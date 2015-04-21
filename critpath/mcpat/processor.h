/*****************************************************************************
 *                                McPAT
 *                      SOFTWARE LICENSE AGREEMENT
 *            Copyright 2012 Hewlett-Packard Development Company, L.P.
 *                          All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.”
 *
 ***************************************************************************/
#ifndef PROCESSOR_H_
#define PROCESSOR_H_

#include "XML_Parse.h"
#include "area.h"
#include "decoder.h"
#include "parameter.h"
#include "array.h"
#include "arbiter.h"
#include <vector>
#include "basic_components.h"
#include "core.h"
#include "memoryctrl.h"
#include "router.h"
#include "sharedcache.h"
#include "noc.h"
#include "iocontrollers.h"

class Processor : public Component
{
  public:
	ParseXML *XML;
	vector<Core *> cores;
    vector<SharedCache *> l2array;
    vector<SharedCache *> l3array;
    vector<SharedCache *> l1dirarray;
    vector<SharedCache *> l2dirarray;
    vector<NoC *>  nocs;
    MemoryController * mc;
    NIUController    * niu;
    PCIeController   * pcie;
    FlashController  * flashcontroller;
    InputParameter interface_ip;
    ProcParam procdynp;
    //wire	globalInterconnect;
    //clock_network globalClock;
    Component core, l2, l3, l1dir, l2dir, noc, mcs, cc, nius, pcies,flashcontrollers;
    int  numCore, numL2, numL3, numNOC, numL1Dir, numL2Dir;
    Processor(ParseXML *XML_interface);
    //void compute();
    virtual void set_proc_param();

    virtual void displayEnergy(uint32_t indent = 0,int plevel = 100, bool is_tdp=true);
    virtual void displayDeviceType(int device_type_, uint32_t indent = 0);
    virtual void displayInterconnectType(int interconnect_type_, uint32_t indent = 0);

    ~Processor();
};

class PrismProcessor : public Processor
{
public:
  PrismProcessor(ParseXML* XML_interface);

  virtual void computeEnergy(); 
  virtual void computeAccPower(); 

  virtual void displayAccEnergy();

  double executionTime_acc;  
  
  //overall output energy
  Component overall_acc, core_acc, ifu_acc, icache_acc, lsu_acc, dcache_acc, mmu_acc, exu_acc, iiw_acc, fiw_acc, isel_acc, rnu_acc, corepipe_acc, undiffCore_acc, l2cache_acc, ialu_acc, fpu_acc, mul_acc, rfu_acc, l2_acc, l3_acc, l1dir_acc, l2dir_acc, noc_acc, cc_acc, nius_acc, pcies_acc, flashcontrollers_acc;

  //overall output power
  Component overall_acc_power, core_acc_power, 
            ifu_acc_power, icache_acc_power, lsu_acc_power, dcache_acc_power, mmu_acc_power, exu_acc_power, iiw_acc_power, fiw_acc_power, isel_acc_power, rnu_acc_power, corepipe_acc_power, undiffCore_acc_power, l2cache_acc_power,
            ialu_acc_power, fpu_acc_power,  mul_acc_power, rfu_acc_power, l2_acc_power, l3_acc_power, l1dir_acc_power, l2dir_acc_power, noc_acc_power, cc_acc_power, nius_acc_power, pcies_acc_power, flashcontrollers_acc_power;
};



class NLAProcessor : public PrismProcessor
{
public:
  NLAProcessor(ParseXML* XML_interface);

  vector<NLAU *> nlas;


  virtual void computeEnergy(); 
  virtual void computeAccPower(); 
  virtual void displayAccEnergy();

  //current energy
  Component nla, imu;

  //overall output energy
  Component nla_acc, imu_acc, nla_net_acc;

  //overall output power
  Component nla_acc_power, imu_acc_power, nla_net_acc_power;

};




#endif /* PROCESSOR_H_ */
