package SIMCAN.simulations.s_4serv_1cores; 

import ned.DatarateChannel;
import SIMCAN.Nodes.Racks.*;
import SIMCAN.Nodes.Nodes.*;
import inet.networklayer.autorouting.ipv4.IPv4NetworkConfigurator;
import inet.nodes.ethernet.EtherSwitch;

// ---------------------------------------------------------------
//   Definition of the scenario
// ---------------------------------------------------------------
network s_4serv_1cores{

   // -------------------------------------------------------
   //   Definition of main modules
   // -------------------------------------------------------
   submodules:

	// ---------------------------------------------------------------
	//   Network configurator
	// ---------------------------------------------------------------
	configurator: IPv4NetworkConfigurator;

	// ---------------------------------------------------------------
	//   Definition of switches
	// ---------------------------------------------------------------
	// Switch (GeneralSwitch)[2]
	switch_0:EtherSwitch;

	// ---------------------------------------------------------------
	//   Definition of Racks
	// ---------------------------------------------------------------
	// Rack (cRack)[0]
	rCmp0_cRack:Rack;

	// Rack (cRack)[1]
	rCmp1_cRack:Rack;

	// Rack (sRack)[3]
	rSto2_sRack:Rack;

	// ---------------------------------------------------------------
	//   Connections section
	// ---------------------------------------------------------------
	connections allowunconnected:

	for i=0..3 {
	   rCmp0_cRack.ethg++ <--> Eth1G_channel <--> switch_0.ethg++;
	}
	for i=0..3 {
	   rCmp1_cRack.ethg++ <--> Eth1G_channel <--> switch_0.ethg++;
	}
	for i=0..0 {
	   rSto2_sRack.ethg++ <--> Eth1G_channel <--> switch_0.ethg++;
	}

}