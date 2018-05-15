/** CIM Test
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * DPsim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <iostream>
#include <list>

#include "DPsim.h"
#include "cps/Interfaces/ShmemInterface.h"
#include "cps/CIM/Reader.h"

using namespace DPsim;
using namespace CPS;
using namespace CPS::Components::DP;

int main(int argc, char *argv[]) {
	// Testing the interface with a simple circuit,
	// but the load is simulated in a different instance.
	// Values are exchanged using the ideal transformator model: an ideal
	// current source on the supply side and an ideal voltage source on the
	// supply side, whose values are received from the respective other circuit.
	// Here, the two instances directly communicate with each other without using
	// VILLASnode in between.

	struct shmem_conf conf;
	conf.samplelen = 4;
	conf.queuelen = 1024;
	conf.polling = false;

	if (argc < 2) {
		std::cerr << "not enough arguments (either 0 or 1 for the test number)" << std::endl;
		std::exit(1);
	}

	String in, out;

	if (String(argv[1]) == "0") {
		in  = "/dpsim10";
		out = "/dpsim01";
	}
	else if (String(argv[1]) == "1") {
		in  = "/dpsim01";
		out = "/dpsim10";
	}

	ShmemInterface shmem(in, out, &conf);
	Real timeStep = 0.000150;

	if (String(argv[1]) == "0") {
		// Specify CIM files
#ifdef _WIN32
		String path("..\\..\\..\\..\\dpsim\\Examples\\CIM\\WSCC-09_Neplan_RX\\");
#elif defined(__linux__) || defined(__APPLE__)
		String path("Examples/CIM/IEEE-09_Neplan_RX/");
#endif

		std::list<String> filenames = {
			path + "WSCC-09_Neplan_RX_DI.xml",
			path + "WSCC-09_Neplan_RX_EQ.xml",
			path + "WSCC-09_Neplan_RX_SV.xml",
			path + "WSCC-09_Neplan_RX_TP.xml"
		};

		CIM::Reader reader(50, Logger::Level::INFO, Logger::Level::INFO);
		SystemTopology sys = reader.loadCIM(filenames);

		Simulation sim("Shmem_WSCC-9bus_CIM", sys, 0.0001, 0.1,
			Solver::Domain::DP, Solver::Type::MNA, Logger::Level::DEBUG);

		// Create shmem interface
		struct shmem_conf conf;
		conf.samplelen = 64;
		conf.queuelen = 1024;
		conf.polling = false;
		String in  = "/dpsim10";
		String out = "/dpsim01";
		ShmemInterface shmem(out, in, &conf);

		// Register exportable node voltages
		shmem.registerExportedAttribute(sys.mNodes[0]->findAttribute<Complex>("voltage"), 1.0, 0, 1);
		shmem.registerExportedAttribute(sys.mNodes[1]->findAttribute<Complex>("voltage"), 1.0, 1, 2);
		shmem.registerExportedAttribute(sys.mNodes[2]->findAttribute<Complex>("voltage"), 1.0, 3, 4);
		shmem.registerExportedAttribute(sys.mNodes[3]->findAttribute<Complex>("voltage"), 1.0, 5, 6);
		shmem.registerExportedAttribute(sys.mNodes[4]->findAttribute<Complex>("voltage"), 1.0, 7, 8);
		shmem.registerExportedAttribute(sys.mNodes[5]->findAttribute<Complex>("voltage"), 1.0, 9, 10);
		shmem.registerExportedAttribute(sys.mNodes[6]->findAttribute<Complex>("voltage"), 1.0, 11, 12);
		shmem.registerExportedAttribute(sys.mNodes[7]->findAttribute<Complex>("voltage"), 1.0, 13, 14);
		shmem.registerExportedAttribute(sys.mNodes[8]->findAttribute<Complex>("voltage"), 1.0, 15, 16);

		// Extend system with controllable load
		auto ecs = CurrentSource::make("i_intf", Node::List{sys.mNodes[3], GND}, Complex(0, 0), Logger::Level::DEBUG);
		// Register interface current source and voltage drop 
		shmem.registerControlledAttribute(ecs->findAttribute<Real>("current_ref"), 1.0, 0);
		shmem.registerExportedAttribute(ecs->findAttribute<Complex>("comp_voltage"), 1.0, 0, 1);

		sim.addInterface(&shmem);
		sim.run();
	}

	if (String(argv[1]) == "1") {
		// Nodes
		auto n1 = Node::make("n1");

		// Add interface voltage source
		auto evs = VoltageSource::make("v_intf", Node::List{GND, n1}, Complex(0, 0), Logger::Level::DEBUG);
		// Register voltage source reference and current flowing through source 
		// multiply with -1 to consider passive sign convention
		shmem.registerControlledAttribute(evs->findAttribute<Complex>("voltage_ref"), 1.0, 0, 1);
		shmem.registerExportedAttribute(evs->findAttribute<Complex>("comp_current"), -1.0, 0, 1);
		
		// Extend system with controllable load
		auto load = PQLoadCS::make("load_cs", Node::List{n1}, 0, 0, 230000);
		// Register controllable load
		shmem.registerControlledAttribute(load->findAttribute<Real>("active_power"), 1.0, 0);

		auto sys = SystemTopology(50, Node::List{n1}, ComponentBase::List{evs, load});
		auto sim = Simulation("ShmemDistributedDirect_2", sys, timeStep, 0.1);

		sim.addInterface(&shmem);
		sim.run();
	}

	return 0;
}