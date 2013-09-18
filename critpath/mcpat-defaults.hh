#ifndef MCPAT_DEFAULT_HH
#define MCPAT_DEFAULT_HH

const char * xml_str = R"(
<?xml version="1.0" ?>
<component id="root" name="root">
	<component id="system" name="system">
		<!--McPAT will skip the components if number is set to 0 -->
		<param name="number_of_cores" value="1"/>
		<param name="number_of_L1Directories" value="0"/>
		<param name="number_of_L2Directories" value="0"/>
		<param name="number_of_L2s" value="1"/> <!-- This number means how many L2 clusters in each cluster there can be multiple banks/ports -->
		<param name="Private_L2" value="0"/><!--1 Private, 0 shared/coherent -->
		<param name="number_of_L3s" value="0"/> <!-- This number means how many L3 clusters -->
		<param name="number_of_NoCs" value="1"/>
		<param name="homogeneous_cores" value="1"/><!--1 means homo -->
		<param name="homogeneous_L2s" value="1"/>
		<param name="homogeneous_L1Directories" value="1"/>
		<param name="homogeneous_L2Directories" value="1"/>
		<param name="homogeneous_L3s" value="1"/>
		<param name="homogeneous_ccs" value="1"/><!--cache coherence hardware -->
		<param name="homogeneous_NoCs" value="1"/>
		<param name="core_tech_node" value="22"/><!-- nm -->
		<param name="target_core_clockrate" value="3700"/><!--MHz -->
		<param name="temperature" value="380"/> <!-- Kelvin -->
		<param name="number_cache_levels" value="2"/>
		<param name="interconnect_projection_type" value="0"/><!--0: aggressive wire technology; 1: conservative wire technology -->
		<param name="device_type" value="0"/><!--0: HP(High Performance Type); 1: LSTP(Low standby power) 2: LOP (Low Operating Power)  -->
		<param name="longer_channel_device" value="1"/><!-- 0 no use; 1 use when aggressive -->
		<param name="power_gating" value="1"/><!-- 0 not enabled; 1 enabled -->
		<param name="machine_bits" value="64"/>
		<param name="virtual_address_width" value="64"/>
		<param name="physical_address_width" value="52"/>
		<param name="virtual_memory_page_size" value="4096"/>
		<!-- address width determines the tag_width in Cache, LSQ and buffers in cache controller 
			default value is machine_bits, if not set --> 
		<stat name="total_cycles" value="0"/>
		<stat name="idle_cycles" value="0"/>
		<stat name="busy_cycles"  value="0"/>
			<!--This page size(B) is complete different from the page size in Main memo section. this page size is the size of 
			virtual memory from OS/Archi perspective; the page size in Main memo section is the actual physical line in a DRAM bank  -->
		<!-- *********************** cores ******************* -->
		<component id="system.core0" name="core0">
			<!-- Core property -->
			<param name="clock_rate" value="3700"/>
			<!-- for cores with unknown timing, set to 0 to force off the opt flag -->
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<param name="opt_local" value="1"/>
			<param name="instruction_length" value="32"/>
			<param name="opcode_width" value="16"/>
			<param name="x86" value="1"/>
			<param name="micro_opcode_width" value="8"/>
			<param name="machine_type" value="0"/>
			<!-- inorder/OoO; 1 inorder; 0 OOO-->
			<param name="number_hardware_threads" value="1"/>
			<!-- number_instruction_fetch_ports(icache ports) is always 1 in single-thread processor,
			it only may be more than one in SMT processors. BTB ports always equals to fetch ports since 
			branch information in consecutive branch instructions in the same fetch group can be read out from BTB once.--> 
			<param name="fetch_width" value="4"/>
			<!-- fetch_width determines the size of cachelines of L1 cache block -->
			<param name="number_instruction_fetch_ports" value="1"/>
			<param name="decode_width" value="4"/>
			<!-- decode_width determines the number of ports of the 
			renaming table (both RAM and CAM) scheme -->
			<param name="issue_width" value="4"/>
			<param name="peak_issue_width" value="6"/><!--As shown in Wiki figure which has max 5 ports, store data/address is modeled 
														  as a single port.-->
			<!-- issue_width determines the number of ports of Issue window and other logic 
			as in the complexity effective processors paper; issue_width==dispatch_width -->
			<param name="commit_width" value="4"/>
			<!-- commit_width determines the number of ports of register files -->
			<param name="fp_issue_width" value="4"/>
			<param name="prediction_width" value="1"/> 
			<!-- number of branch instructions can be predicted simultaneously-->
			<!-- Current version of McPAT does not distinguish int and floating point pipelines 
			Theses parameters are reserved for future use.--> 
			<param name="pipelines_per_core" value="1,1"/>
			<!--integer_pipeline and floating_pipelines, if the floating_pipelines is 0, then the pipeline is shared-->
			<param name="pipeline_depth" value="14,14"/>
			<!-- pipeline depth of int and fp, if pipeline is shared, the second number is the average cycles of fp ops -->
			<!-- issue and exe unit-->
			<param name="ALU_per_core" value="6"/>
			<!-- contains an adder, a shifter, and a logical unit -->
			<param name="MUL_per_core" value="1"/>
			<!-- For MUL and Div -->
			<param name="FPU_per_core" value="2"/>		
			<!-- buffer between IF and ID stage -->
			<param name="instruction_buffer_size" value="32"/><!--Inst. + micro-op -->
			<!-- buffer between ID and sche/exe stage -->
			<param name="decoded_stream_buffer_size" value="16"/>
			<param name="instruction_window_scheme" value="0"/><!-- 0 PHYREG based, 1 RSBASED-->
			<!-- McPAT support 2 types of OoO cores, RS based and physical reg based-->
			<param name="instruction_window_size" value="64"/>
			<param name="fp_instruction_window_size" value="64"/>
			<!-- the instruction issue Q as in Alpha 21264; The RS as in Intel P6 -->
			<param name="ROB_size" value="192"/>
			<!-- each in-flight instruction has an entry in ROB -->
			<!-- registers -->
			<param name="archi_Regs_IRF_size" value="16"/><!-- X86-64 has 16GPR -->			
			<param name="archi_Regs_FRF_size" value="32"/><!-- MMX + XMM -->
			<!--  if OoO processor, phy_reg number is needed for renaming logic, 
			renaming logic is for both integer and floating point insts.  -->
			<param name="phy_Regs_IRF_size" value="256"/>
			<param name="phy_Regs_FRF_size" value="256"/>
			<!-- rename logic -->
			<param name="rename_scheme" value="0"/>
			<!-- can be RAM based(0) or CAM based(1) rename scheme 
			RAM-based scheme will have free list, status table;
			CAM-based scheme have the valid bit in the data field of the CAM 
			both RAM and CAM need RAM-based checkpoint table, checkpoint_depth=# of in_flight instructions;
			Detailed RAT Implementation see TR -->
			<param name="register_windows_size" value="0"/>
			<!-- how many windows in the windowed register file, sun processors;
			no register windowing is used when this number is 0 -->
			<!-- In OoO cores, loads and stores can be issued whether inorder(Pentium Pro) or (OoO)out-of-order(Alpha),
			They will always try to execute out-of-order though. -->
			<param name="LSU_order" value="inorder"/>
			<param name="store_buffer_size" value="96"/>
			<!-- By default, in-order cores do not have load buffers -->
			<param name="load_buffer_size" value="48"/>	
			<!-- number of ports refer to sustain-able concurrent memory accesses --> 
			<param name="memory_ports" value="2"/>	
			<!-- max_allowed_in_flight_memo_instructions determines the # of ports of load and store buffer
			as well as the ports of Dcache which is connected to LSU -->	
			<!-- dual-pumped Dcache can be used to save the extra read/write ports -->
			<param name="RAS_size" value="32"/>						
			<!-- general stats, defines simulation periods;require total, idle, and busy cycles for sanity check  -->
			<!-- please note: if target architecture is X86, then all the instructions refer to (fused) micro-ops -->
			<stat name="total_instructions" value="0"/>
			<stat name="int_instructions" value="0"/>
			<stat name="fp_instructions" value="0"/>
			<stat name="branch_instructions" value="0"/>
			<stat name="branch_mispredictions" value="0"/>
			<stat name="load_instructions" value="0"/>
			<stat name="store_instructions" value="0"/>
			<stat name="committed_instructions" value="0"/>
			<stat name="committed_int_instructions" value="0"/>
			<stat name="committed_fp_instructions" value="0"/>
			<stat name="pipeline_duty_cycle" value="1"/><!--<=1, runtime_ipc/peak_ipc; averaged for all cores if homogeneous -->
			<!-- the following cycle stats are used for heterogeneous cores only, 
				please ignore them if homogeneous cores -->
			<stat name="total_cycles" value="0"/>
		    <stat name="idle_cycles" value="0"/>
		    <stat name="busy_cycles"  value="0"/>
			<!-- instruction buffer stats -->
			<!-- ROB stats, both RS and Phy based OoOs have ROB
			performance simulator should capture the difference on accesses,
			otherwise, McPAT has to guess based on number of committed instructions. -->
			<stat name="ROB_reads" value="0"/>
			<stat name="ROB_writes" value="0"/>
			<!-- RAT accesses -->
			<stat name="rename_reads" value="0"/> <!--lookup in renaming logic -->
			<stat name="rename_writes" value="0"/><!--update dest regs. renaming logic -->
			<stat name="fp_rename_reads" value="0"/>
			<stat name="fp_rename_writes" value="0"/>
			<!-- decode and rename stage use this, should be total ic - nop -->
			<!-- Inst window stats -->
			<stat name="inst_window_reads" value="0"/>
			<stat name="inst_window_writes" value="0"/>
			<stat name="inst_window_wakeup_accesses" value="0"/>
			<stat name="fp_inst_window_reads" value="0"/>
			<stat name="fp_inst_window_writes" value="0"/>
			<stat name="fp_inst_window_wakeup_accesses" value="0"/>
			<!--  RF accesses -->
			<stat name="int_regfile_reads" value="0"/>
			<stat name="float_regfile_reads" value="0"/>
			<stat name="int_regfile_writes" value="0"/>
			<stat name="float_regfile_writes" value="0"/>
			<!-- accesses to the working reg -->
			<stat name="function_calls" value="0"/>
			<stat name="context_switches" value="0"/>
			<!-- Number of Windows switches (number of function calls and returns)-->
			<!-- Alu stats by default, the processor has one FPU that includes the divider and 
			 multiplier. The fpu accesses should include accesses to multiplier and divider  -->
			<stat name="ialu_accesses" value="0"/>			
			<stat name="fpu_accesses" value="0"/>
			<stat name="mul_accesses" value="0"/>
			<stat name="cdb_alu_accesses" value="0"/>
			<stat name="cdb_mul_accesses" value="0"/>
			<stat name="cdb_fpu_accesses" value="0"/>
			<!-- multiple cycle accesses should be counted multiple times, 
			otherwise, McPAT can use internal counter for different floating point instructions 
			to get final accesses. But that needs detailed info for floating point inst mix -->
			<!--  currently the performance simulator should 
			make sure all the numbers are final numbers, 
			including the explicit read/write accesses, 
			and the implicit accesses such as replacements and etc.
			Future versions of McPAT may be able to reason the implicit access
			based on param and stats of last level cache
			The same rule applies to all cache access stats too!  -->
			<!-- following is AF for max power computation. 
				Do not change them, unless you understand them-->
			<stat name="IFU_duty_cycle" value="0.25"/>			
			<stat name="LSU_duty_cycle" value="0.25"/>
			<stat name="MemManU_I_duty_cycle" value="0.25"/>
			<stat name="MemManU_D_duty_cycle" value="0.25"/>
			<stat name="ALU_duty_cycle" value="1"/>
			<stat name="MUL_duty_cycle" value="0.3"/>
			<stat name="FPU_duty_cycle" value="0.3"/>
			<stat name="ALU_cdb_duty_cycle" value="1"/>
			<stat name="MUL_cdb_duty_cycle" value="0.3"/>
			<stat name="FPU_cdb_duty_cycle" value="0.3"/>
			<param name="number_of_BPT" value="2"/>
			<component id="system.core0.predictor" name="PBT">
				<!-- branch predictor; tournament predictor see Alpha implementation -->
				<param name="local_predictor_size" value="10,3"/>
				<param name="local_predictor_entries" value="1024"/>
				<param name="global_predictor_entries" value="4096"/>
				<param name="global_predictor_bits" value="2"/>
				<param name="chooser_predictor_entries" value="4096"/>
				<param name="chooser_predictor_bits" value="2"/>
				<!-- These parameters can be combined like below in next version
				<param name="load_predictor" value="10,3,1024"/>
				<param name="global_predictor" value="4096,2"/>
				<param name="predictor_chooser" value="4096,2"/>
				-->
			</component>
			<component id="system.core0.itlb" name="itlb">
				<param name="number_entries" value="128"/>
				<stat name="total_accesses" value="0"/>
				<stat name="total_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
				<!-- there is no write requests to itlb although writes happen to itlb after miss, 
				which is actually a replacement -->
			</component>
			<component id="system.core0.icache" name="icache">
				<!-- there is no write requests to itlb although writes happen to it after miss, 
				which is actually a replacement -->
				<param name="icache_config" value="32768,32,8,1,4,4,32,0"/>
				<!-- the parameters are capacity,block_width, associativity, bank, throughput w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy,  -->
				<!-- cache_policy;//0 no write or write-though with non-write allocate;1 write-back with write-allocate -->
				<param name="buffer_sizes" value="16, 16, 16,0"/>
				<!-- cache controller buffer sizes: miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_size,wb_buffer_size--> 
				<stat name="read_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="conflicts" value="0"/>				
			</component>
			<component id="system.core0.dtlb" name="dtlb">
				<param name="number_entries" value="256"/><!--dual threads-->
				<stat name="total_accesses" value="0"/>
				<stat name="total_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
			</component>
			<component id="system.core0.dcache" name="dcache">
			        <!-- all the buffer related are optional -->
				<param name="dcache_config" value="32768,32,8,1, 4,6, 32,1 "/>
				<param name="buffer_sizes" value="16, 16, 16, 16"/>
				<!-- cache controller buffer sizes: miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_size,wb_buffer_size-->	
				<stat name="read_accesses" value="0"/>
				<stat name="write_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="write_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
			</component>
			<param name="number_of_BTB" value="2"/>
			<component id="system.core0.BTB" name="BTB">
			        <!-- all the buffer related are optional -->
				<param name="BTB_config" value="5120,4,2,1, 1,3"/> <!--should be 4096 + 1024 -->
				<!-- the parameters are capacity,block_width,associativity,bank, throughput w.r.t. core clock, latency w.r.t. core clock,-->
				<stat name="read_accesses" value="0"/> <!--See IFU code for guideline -->
				<stat name="write_accesses" value="0"/>
			</component>
	</component>
		<component id="system.L1Directory0" name="L1Directory0">
				<param name="Directory_type" value="0"/>
			    <!--0 cam based shadowed tag. 1 directory cache -->	
				<param name="Dir_config" value="4096,2,0,1,100,100, 8"/>
				<!-- the parameters are capacity,block_width, associativity,bank, throughput w.r.t. core clock, latency w.r.t. core clock,-->
			    <param name="buffer_sizes" value="8, 8, 8, 8"/>	
				<!-- all the buffer related are optional -->
			    <param name="clockrate" value="3400"/>
				<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
				<param name="ports" value="1,1,1"/>
				<!-- number of r, w, and rw search ports -->
				<param name="device_type" value="0"/>
				<!-- although there are multiple access types, 
				Performance simulator needs to cast them into reads or writes
				e.g. the invalidates can be considered as writes -->
				<stat name="read_accesses" value="0"/>
				<stat name="write_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="write_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
		</component>
		<component id="system.L2Directory0" name="L2Directory0">
				<param name="Directory_type" value="1"/>
			    <!--0 cam based shadowed tag. 1 directory cache -->	
				<param name="Dir_config" value="1048576,16,16,1,2, 100"/>
				<!-- the parameters are capacity,block_width, associativity,bank, throughput w.r.t. core clock, latency w.r.t. core clock,-->
			    <param name="buffer_sizes" value="8, 8, 8, 8"/>	
				<!-- all the buffer related are optional -->
			    <param name="clockrate" value="3400"/>
				<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
				<param name="ports" value="1,1,1"/>
				<!-- number of r, w, and rw search ports -->
				<param name="device_type" value="0"/>
				<!-- altough there are multiple access types, 
				Performance simulator needs to cast them into reads or writes
				e.g. the invalidates can be considered as writes -->
				<stat name="read_accesses" value="0"/>
				<stat name="write_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="write_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
		</component>
		<component id="system.L20" name="L20">
			<!-- all the buffer related are optional -->
				<param name="L2_config" value="6291456,64, 16, 8, 8, 23, 32, 1"/> 
				<!-- the parameters are capacity,block_width, associativity, bank, throughput w.r.t. core clock, latency w.r.t. core clock,output_width, cache policy -->
				<param name="buffer_sizes" value="16, 16, 16, 16"/>
				<!-- cache controller buffer sizes: miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_size,wb_buffer_size-->	
				<param name="clockrate" value="3700"/>
				<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
				<param name="ports" value="1,1,1"/>
				<!-- number of r, w, and rw ports -->
				<param name="device_type" value="0"/>
				<stat name="read_accesses" value="0"/>
				<stat name="write_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="write_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
			    <stat name="duty_cycle" value="0.5"/>	
		</component>
		
<!--**********************************************************************-->
<component id="system.L30" name="L30">
				<param name="L3_config" value="16777216,64,16, 16, 16, 100,1"/>
				<!-- the parameters are capacity,block_width, associativity,bank, throughput w.r.t. core clock, latency w.r.t. core clock,-->
				<param name="clockrate" value="850"/>
				<param name="ports" value="1,1,1"/>
				<!-- number of r, w, and rw ports -->
				<param name="device_type" value="0"/>
				<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
				<param name="buffer_sizes" value="16, 16, 16, 16"/>
				<!-- cache controller buffer sizes: miss_buffer_size(MSHR),fill_buffer_size,prefetch_buffer_size,wb_buffer_size-->	
				<stat name="read_accesses" value="0"/>
				<stat name="write_accesses" value="0"/>
				<stat name="read_misses" value="0"/>
				<stat name="write_misses" value="0"/>
				<stat name="conflicts" value="0"/>	
				<stat name="duty_cycle" value="1.0"/>	
		</component>
<!--**********************************************************************-->
		<component id="system.NoC0" name="noc0">
			<param name="clockrate" value="3400"/>
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<param name="type" value="0"/>
			<!--0:bus, 1:NoC , for bus no matter how many nodes sharing the bus
				at each time only one node can send req -->
			<param name="horizontal_nodes" value="1"/>
			<param name="vertical_nodes" value="1"/>
			<param name="has_global_link" value="0"/>
			<!-- 1 has global link, 0 does not have global link -->
			<param name="link_throughput" value="1"/><!--w.r.t clock -->
			<param name="link_latency" value="1"/><!--w.r.t clock -->
			<!-- throughput >= latency -->
			<!-- Router architecture -->
			<param name="input_ports" value="1"/>
			<param name="output_ports" value="1"/>
			<!-- For bus the I/O ports should be 1 -->
			<param name="flit_bits" value="256"/>
			<param name="chip_coverage" value="1"/>
			<!-- When multiple NOC present, one NOC will cover part of the whole chip. 
				chip_coverage <=1 -->
			<param name="link_routing_over_percentage" value="0.5"/>
			<!-- Links can route over other components or occupy whole area.
				by default, 50% of the NoC global links routes over other 
				components -->
			<stat name="total_accesses" value="0"/>
			<!-- This is the number of total accesses within the whole network not for each router -->
			<stat name="duty_cycle" value="1"/>
		</component>		
<!--**********************************************************************-->
		<component id="system.mc" name="mc">
			<!-- Memory controllers are for DDR(2,3...) DIMMs -->
			<!-- current version of McPAT uses published values for base parameters of memory controller
			improvements on MC will be added in later versions. -->
			<param name="type" value="0"/> <!-- 1: low power; 0 high performance -->
			<param name="mc_clock" value="200"/><!--DIMM IO bus clock rate MHz  --> 
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<param name="peak_transfer_rate" value="3200"/><!--MB/S-->
			<param name="block_size" value="64"/><!--B-->
			<param name="number_mcs" value="0"/>
			<!-- current McPAT only supports homogeneous memory controllers -->
			<param name="memory_channels_per_mc" value="1"/>
			<param name="number_ranks" value="2"/>
			<param name="withPHY" value="0"/>
			<!-- # of ranks of each channel-->
			<param name="req_window_size_per_channel" value="32"/>
			<param name="IO_buffer_size_per_channel" value="32"/>
			<param name="databus_width" value="128"/>
			<param name="addressbus_width" value="51"/>
			<!-- McPAT will add the control bus width to the address bus width automatically -->
			<stat name="memory_accesses" value="0"/>
			<stat name="memory_reads" value="0"/>
			<stat name="memory_writes" value="0"/>
			<!-- McPAT does not track individual mc, instead, it takes the total accesses and calculate 
			the average power per MC or per channel. This is sufficient for most application. 
			Further track down can be easily added in later versions. -->  			
		</component>
<!--**********************************************************************-->
		<component id="system.niu" name="niu">
			<!-- On chip 10Gb Ethernet NIC, including XAUI Phy and MAC controller  -->
			<!-- For a minimum IP packet size of 84B at 10Gb/s, a new packet arrives every 67.2ns. 
				 the low bound of clock rate of a 10Gb MAC is 150Mhz -->
			<param name="type" value="0"/> <!-- 1: low power; 0 high performance -->
			<param name="clockrate" value="350"/>
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<param name="number_units" value="0"/> <!-- unlike PCIe and memory controllers, each Ethernet controller only have one port -->
			<stat name="duty_cycle" value="1.0"/> <!-- achievable max load <= 1.0 -->
			<stat name="total_load_perc" value="0.7"/> <!-- ratio of total achieved load to total achieve-able bandwidth  -->
			<!-- McPAT does not track individual nic, instead, it takes the total accesses and calculate 
			the average power per nic or per channel. This is sufficient for most application. -->  			
		</component>
<!--**********************************************************************-->
		<component id="system.pcie" name="pcie">
			<!-- On chip PCIe controller, including Phy-->
			<!-- For a minimum PCIe packet size of 84B at 8Gb/s per lane (PCIe 3.0), a new packet arrives every 84ns. 
				 the low bound of clock rate of a PCIe per lane logic is 120Mhz -->
			<param name="type" value="0"/> <!-- 1: low power; 0 high performance -->
			<param name="withPHY" value="1"/>
			<param name="clockrate" value="350"/>
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<param name="number_units" value="0"/>
			<param name="num_channels" value="8"/> <!-- 2 ,4 ,8 ,16 ,32 -->
			<stat name="duty_cycle" value="1.0"/> <!-- achievable max load <= 1.0 -->
			<stat name="total_load_perc" value="0.7"/> <!-- Percentage of total achieved load to total achieve-able bandwidth  -->
			<!-- McPAT does not track individual pcie controllers, instead, it takes the total accesses and calculate 
			the average power per pcie controller or per channel. This is sufficient for most application. -->  			
		</component>
<!--**********************************************************************-->
		<component id="system.flashc" name="flashc">
		    <param name="number_flashcs" value="0"/>
			<param name="type" value="1"/> <!-- 1: low power; 0 high performance -->
            <param name="withPHY" value="1"/>
			<param name="peak_transfer_rate" value="200"/><!--Per controller sustain-able peak rate MB/S -->
			<param name="vdd" value="0"/><!-- 0 means using ITRS default vdd -->
			<stat name="duty_cycle" value="1.0"/> <!-- achievable max load <= 1.0 -->
			<stat name="total_load_perc" value="0.7"/> <!-- Percentage of total achieved load to total achieve-able bandwidth  -->
			<!-- McPAT does not track individual flash controller, instead, it takes the total accesses and calculate 
			the average power per fc or per channel. This is sufficient for most application -->  			
		</component>
<!--**********************************************************************-->

		</component>
</component>
)";

#endif //MCPAT_DEFAULT_HH
