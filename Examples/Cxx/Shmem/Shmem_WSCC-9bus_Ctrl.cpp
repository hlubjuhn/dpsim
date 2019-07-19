/** CIM Test
 *
 * @author Markus Mirz <mmirz@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <DPsim.h>

using namespace DPsim;
using namespace CPS;
using namespace CPS::DP;
using namespace CPS::DP::Ph1;
using namespace CPS::Signal;

int main(int argc, char *argv[]) {

	CommandLineArgs args(argc, argv);

	// Find CIM files
	std::list<fs::path> filenames;
	if (argc <= 1) {
		filenames = Utils::findFiles({
			"WSCC-09_RX_DI.xml",
			"WSCC-09_RX_EQ.xml",
			"WSCC-09_RX_SV.xml",
			"WSCC-09_RX_TP.xml"
		}, "Examples/CIM/WSCC-09_RX", "CIMPATH");
	}
	else {
		filenames = args.filenames;
	}

	String simName = "Shmem_WSCC-9bus_Ctrl";

	CIM::Reader reader(simName, Logger::Level::info, Logger::Level::info);
	SystemTopology sys = reader.loadCIM(60, filenames);

	// Extend system with controllable load (Profile)
	auto load_profile = PQLoadCS::make("load_cs_profile");
	load_profile->connect({ sys.node<DP::Node>("BUS7") });
	load_profile->setParameters(0, 0, 230000);
	sys.mComponents.push_back(load_profile);

	// Extend system with controllable load
	auto load = PQLoadCS::make("load_cs");
	load->connect({ sys.node<DP::Node>("BUS4") });
	load->setParameters(0, 0, 230000);
	sys.mComponents.push_back(load);

	// Controllers and filter
	std::vector<Real> coefficients_profile = std::vector<Real>(2000, 1./2000);
	std::vector<Real> coefficients = std::vector<Real>(100, 1./100);

	auto filtP_profile = FIRFilter::make("filter_p_profile", coefficients_profile, 0, Logger::Level::info);
	filtP_profile->setPriority(1);
	load_profile->setAttributeRef("P", filtP_profile->attribute<Real>("output"));

	sys.mComponents.push_back(filtP_profile);

	auto filtP = FIRFilter::make("filter_p", coefficients, 0, Logger::Level::info);
	filtP->setPriority(1);
	load->setAttributeRef("P", filtP->attribute<Real>("output"));
	sys.mComponents.push_back(filtP);

	RealTimeSimulation sim(simName, sys, args.timeStep, args.duration, args.solver.domain, args.solver.type, args.logLevel, true);

	// Create shmem interface
	Interface::Config conf;
	conf.samplelen = 64;
	conf.queuelen = 1024;
	conf.polling = false;
	String in  = "/villas-dpsim1";
	String out = "/dpsim1-villas";
	Interface intf(out, in, &conf, false);

	// Register exportable node voltages
	UInt o = 0;
	for (auto n : sys.mNodes) {
		UInt i;
		if (sscanf(n->name().c_str(), "BUS%u", &i) != 1) {
			std::cerr << "Failed to determine bus no of bus: " << n->name() << std::endl;
			continue;
		}

		i--;

		auto n_dp = std::dynamic_pointer_cast<CPS::DP::Node>(n);
		auto v = n_dp->attributeMatrix<Complex>("v");
		auto v0 = v->coeffComplex(0,0);

		std::cout << "Signal " << (i*2)+0 << ": Mag  " << n->name() << std::endl;
		std::cout << "Signal " << (i*2)+1 << ": Phas " << n->name() << std::endl;

		intf.addExport(v0->mag(),   (i*2)+0); o++;
		intf.addExport(v0->phase(), (i*2)+1); o++;
	}

	// TODO gain by 20e8
	filtP->setInput(intf.importReal(0));
	filtP_profile->setInput(intf.importReal(1));

	intf.addExport(load->attribute<Real>("P"), o++);
	intf.addExport(load_profile->attribute<Real>("P"), o++);

	sim.addInterface(&intf, false);
	sim.run();

	return 0;
}
