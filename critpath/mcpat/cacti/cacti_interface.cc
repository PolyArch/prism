/*****************************************************************************
 *                                McPAT/CACTI
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

#include <time.h>
#include <math.h>


#include "area.h"
#include "basic_circuit.h"
#include "component.h"
#include "const.h"
#include "parameter.h"
#include "cacti_interface.h"
#include "Ucache.h"

#include <pthread.h>
#include <iostream>
#include <algorithm>

using namespace std;

PowerDB PowerDatabase::_instance;
bool PowerDatabase::setup = false;
bool PowerDatabase::do_not_load = false;
std::string PowerDatabase::db_file;


  template<class Archive>
  void mem_array::serialize(Archive & ar, const unsigned int version) {
    ar & Ndcm;
    ar & Ndwl;
    ar & Ndbl;
    ar & Nspd;
    ar & deg_bl_muxing;
    ar & Ndsam_lev_1;
    ar & Ndsam_lev_2;
    ar & access_time;
    ar & cycle_time;
    ar & multisubbank_interleave_cycle_time;
    ar & area_ram_cells;
    ar & area;
    ar & power;
    ar & delay_senseamp_mux_decoder;
    ar & delay_before_subarray_output_driver;
    ar & delay_from_subarray_output_driver_to_output;
    ar & height;
    ar & width;

    ar & mat_height;
    ar & mat_length;
    ar & subarray_length;
    ar & subarray_height;

    ar & delay_route_to_bank;
    ar & delay_input_htree;
    ar & delay_row_predecode_driver_and_block;
    ar & delay_row_decoder;
    ar & delay_bitlines;
    ar & delay_sense_amp;
    ar & delay_subarray_output_driver;
    ar & delay_dout_htree;
    ar & delay_comparator;
    ar & delay_matchlines;

    ar & all_banks_height;
    ar & all_banks_width;
    ar & area_efficiency;

    ar & power_routing_to_bank;
    ar & power_addr_input_htree;
    ar & power_data_input_htree;
    ar & power_data_output_htree;
    ar & power_htree_in_search;
    ar & power_htree_out_search;
    ar & power_row_predecoder_drivers;
    ar & power_row_predecoder_blocks;
    ar & power_row_decoders;
    ar & power_bit_mux_predecoder_drivers;
    ar & power_bit_mux_predecoder_blocks;
    ar & power_bit_mux_decoders;
    ar & power_senseamp_mux_lev_1_predecoder_drivers;
    ar & power_senseamp_mux_lev_1_predecoder_blocks;
    ar & power_senseamp_mux_lev_1_decoders;
    ar & power_senseamp_mux_lev_2_predecoder_drivers;
    ar & power_senseamp_mux_lev_2_predecoder_blocks;
    ar & power_senseamp_mux_lev_2_decoders;
    ar & power_bitlines;
    ar & power_sense_amps;
    ar & power_prechg_eq_drivers;
    ar & power_output_drivers_at_subarray;
    ar & power_dataout_vertical_htree;
    ar & power_comparators;

    ar & power_cam_bitline_precharge_eq_drv;
    ar & power_searchline;
    ar & power_searchline_precharge;
    ar & power_matchlines;
    ar & power_matchline_precharge;
    ar & power_matchline_to_wordline_drv;

    ar & arr_min;
    ar & wt;

    // dram stats
    ar & activate_energy; ar & read_energy; ar & write_energy; ar & precharge_energy;
    ar & refresh_power; ar & leak_power_subbank_closed_page; ar & leak_power_subbank_open_page;
    ar & leak_power_request_and_reply_networks;

    ar & precharge_delay;

    //Power-gating stats
    ar & array_leakage;
    ar & wl_leakage;
    ar & cl_leakage;

    ar & sram_sleep_tx_width; ar & wl_sleep_tx_width; ar & cl_sleep_tx_width;
    ar & sram_sleep_tx_area; ar & wl_sleep_tx_area; ar & cl_sleep_tx_area;
    ar & sram_sleep_wakeup_latency; ar & wl_sleep_wakeup_latency; ar & cl_sleep_wakeup_latency; ar & bl_floating_wakeup_latency;

    ar & sram_sleep_wakeup_energy; ar & wl_sleep_wakeup_energy; ar & cl_sleep_wakeup_energy; ar & bl_floating_wakeup_energy;

    ar & num_active_mats;
    ar & num_submarray_mats;

    ar & long_channel_leakage_reduction_periperal;
    ar & long_channel_leakage_reduction_memcell;
  }



bool mem_array::lt(const mem_array * m1, const mem_array * m2)
{
  if (m1->Nspd < m2->Nspd) return true;
  else if (m1->Nspd > m2->Nspd) return false;
  else if (m1->Ndwl < m2->Ndwl) return true;
  else if (m1->Ndwl > m2->Ndwl) return false;
  else if (m1->Ndbl < m2->Ndbl) return true;
  else if (m1->Ndbl > m2->Ndbl) return false;
  else if (m1->deg_bl_muxing < m2->deg_bl_muxing) return true;
  else if (m1->deg_bl_muxing > m2->deg_bl_muxing) return false;
  else if (m1->Ndsam_lev_1 < m2->Ndsam_lev_1) return true;
  else if (m1->Ndsam_lev_1 > m2->Ndsam_lev_1) return false;
  else if (m1->Ndsam_lev_2 < m2->Ndsam_lev_2) return true;
  else return false;
}



void uca_org_t::find_delay()
{
  mem_array * data_arr = data_array2;
  mem_array * tag_arr  = tag_array2;

  // check whether it is a regular cache or scratch ram
  if (g_ip->pure_ram|| g_ip->pure_cam || g_ip->fully_assoc)
  {
    access_time = data_arr->access_time;
  }
  // Both tag and data lookup happen in parallel
  // and the entire set is sent over the data array h-tree without
  // waiting for the way-select signal --TODO add the corresponding
  // power overhead Nav
  else if (g_ip->fast_access == true)
  {
    access_time = MAX(tag_arr->access_time, data_arr->access_time);
  }
  // Tag is accessed first. On a hit, way-select signal along with the
  // address is sent to read/write the appropriate block in the data
  // array
  else if (g_ip->is_seq_acc == true)
  {
    access_time = tag_arr->access_time + data_arr->access_time;
  }
  // Normal access: tag array access and data array access happen in parallel.
  // But, the data array will wait for the way-select and transfer only the
  // appropriate block over the h-tree.
  else
  {
    access_time = MAX(tag_arr->access_time + data_arr->delay_senseamp_mux_decoder,
                      data_arr->delay_before_subarray_output_driver) +
                  data_arr->delay_from_subarray_output_driver_to_output;
  }
}



void uca_org_t::find_energy()
{
  if (!(g_ip->pure_ram|| g_ip->pure_cam || g_ip->fully_assoc))//(g_ip->is_cache)
    power = data_array2->power + tag_array2->power;
  else
    power = data_array2->power;
}



void uca_org_t::find_area()
{
  if (g_ip->pure_ram|| g_ip->pure_cam || g_ip->fully_assoc)//(g_ip->is_cache == false)
  {
    cache_ht  = data_array2->height;
    cache_len = data_array2->width;
  }
  else
  {
    cache_ht  = MAX(tag_array2->height, data_array2->height);
    cache_len = tag_array2->width + data_array2->width;
  }
  area = cache_ht * cache_len;
}

void uca_org_t::adjust_area()
{
  double area_adjust;
  if (g_ip->pure_ram|| g_ip->pure_cam || g_ip->fully_assoc)
  {
    if (data_array2->area_efficiency/100.0<0.2)
    {
    	//area_adjust = sqrt(area/(area*(data_array2->area_efficiency/100.0)/0.2));
    	area_adjust = sqrt(0.2/(data_array2->area_efficiency/100.0));
    	cache_ht  = cache_ht/area_adjust;
    	cache_len = cache_len/area_adjust;
    }
  }
  area = cache_ht * cache_len;
}

void uca_org_t::find_cyc()
{
  if ((g_ip->pure_ram|| g_ip->pure_cam || g_ip->fully_assoc))//(g_ip->is_cache == false)
  {
    cycle_time = data_array2->cycle_time;
  }
  else
  {
    cycle_time = MAX(tag_array2->cycle_time,
                    data_array2->cycle_time);
  }
}

uca_org_t :: uca_org_t()
:tag_array2(0),
 data_array2(0),
 uca_pg_reference(0)
{
	uca_q = vector<uca_org_t * >(0);
}

uca_org_t uca_org_t::deep_copy() {
  uca_org_t holder = *this; //use default copy for everything except arrays
  if(tag_array2) {
    holder.tag_array2 = new mem_array();
    *holder.tag_array2 = *tag_array2;
  }
  if(data_array2) {
    holder.data_array2 = new mem_array();
    *holder.data_array2 = *data_array2;
  }
  //TODO uca_q is not copied properly
  //TODO uca_pg_reference is not copied properly
  return holder;
}


void uca_org_t :: cleanup()
{
	//	uca_org_t * it_uca_org;
	if (data_array2!=0){
		delete data_array2;
		data_array2 =0;
	}

	if (tag_array2!=0){
		delete tag_array2;
		tag_array2 =0;
	}

        return; //these haven't been deep copied yet

	std::vector<uca_org_t * >::size_type sz = uca_q.size();
	for (int i=sz-1; i>=0; i--)
	{
		if (uca_q[i]->data_array2!=0)
		{
			delete uca_q[i]->data_array2;
			uca_q[i]->data_array2 =0;
		}
		if (uca_q[i]->tag_array2!=0){
			delete uca_q[i]->tag_array2;
			uca_q[i]->tag_array2 =0;
		}
		delete uca_q[i];
		uca_q[i] =0;
		uca_q.pop_back();
	}

	if (uca_pg_reference!=0)
	{
		if (uca_pg_reference->data_array2!=0)
		{
			delete uca_pg_reference->data_array2;
			uca_pg_reference->data_array2 =0;
		}
		if (uca_pg_reference->tag_array2!=0){
			delete uca_pg_reference->tag_array2;
			uca_pg_reference->tag_array2 =0;
		}
		delete uca_pg_reference;
		uca_pg_reference =0;
	}
}

uca_org_t :: ~uca_org_t()
{
//	cleanup();
}
