/* Copyright 2017-2020 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#include <cps/EMT/EMT_Ph3_Resistor.h>

using namespace CPS;

EMT::Ph3::Resistor::Resistor(String uid, String name, Logger::Level logLevel)
	: SimPowerComp<Real>(uid, name, logLevel) {
	mPhaseType = PhaseType::ABC;
	setTerminalNumber(2);
	mIntfVoltage = Matrix::Zero(3, 1);
	mIntfCurrent = Matrix::Zero(3, 1);

	addAttribute<Matrix>("R", &mResistance, Flags::read | Flags::write);
}

SimPowerComp<Real>::Ptr EMT::Ph3::Resistor::clone(String name) {
	auto copy = Resistor::make(name, mLogLevel);
	copy->setParameters(mResistance);
	return copy;
}

void EMT::Ph3::Resistor::initializeFromNodesAndTerminals(Real frequency) {

	// IntfVoltage initialization for each phase
	MatrixComp vInitABC = Matrix::Zero(3, 1);
	vInitABC(0, 0) = RMS3PH_TO_PEAK1PH * initialSingleVoltage(1) - RMS3PH_TO_PEAK1PH * initialSingleVoltage(0);
	vInitABC(1, 0) = vInitABC(0, 0) * SHIFT_TO_PHASE_B;
	vInitABC(2, 0) = vInitABC(0, 0) * SHIFT_TO_PHASE_C;
	mIntfVoltage = vInitABC.real();
	mConductance = mResistance.inverse();
	mIntfCurrent = (mConductance * vInitABC).real();

	mSLog->info("\nResistance [Ohm]: {:s}"
				"\nConductance [S]: {:s}", 
				Logger::matrixToString(mResistance), 
				Logger::matrixToString(mConductance));
	mSLog->info(
		"\n--- Initialization from powerflow ---"
		"\nVoltage across: {:s}"
		"\nCurrent: {:s}"
		"\nTerminal 0 voltage: {:s}"
		"\nTerminal 1 voltage: {:s}"
		"\nResistance Matrix: {:s}"
		"\n--- Initialization from powerflow finished ---",
		Logger::matrixToString(mIntfVoltage),
		Logger::matrixToString(mIntfCurrent),
		Logger::phasorToString(RMS3PH_TO_PEAK1PH * initialSingleVoltage(0)),
		Logger::phasorToString(RMS3PH_TO_PEAK1PH * initialSingleVoltage(1)),
		Logger::matrixToString(mResistance));
	mSLog->flush();
}

void EMT::Ph3::Resistor::mnaInitialize(Real omega, Real timeStep, Attribute<Matrix>::Ptr leftVector) {
	MNAInterface::mnaInitialize(omega, timeStep);

	updateMatrixNodeIndices();
	mMnaTasks.push_back(std::make_shared<MnaPostStep>(*this, leftVector));
}

void EMT::Ph3::Resistor::mnaApplySystemMatrixStamp(Matrix& systemMatrix) {
	// Set diagonal entries
	if (terminalNotGrounded(0)) {
		// set upper left block, 3x3 entries
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(0, 0), mConductance(0, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(0, 1), mConductance(0, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(0, 2), mConductance(0, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(0, 0), mConductance(1, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(0, 1), mConductance(1, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(0, 2), mConductance(1, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(0, 0), mConductance(2, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(0, 1), mConductance(2, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(0, 2), mConductance(2, 2));
	}
	if (terminalNotGrounded(1)) {
		// set buttom right block, 3x3 entries
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(1, 0), mConductance(0, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(1, 1), mConductance(0, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(1, 2), mConductance(0, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(1, 0), mConductance(1, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(1, 1), mConductance(1, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(1, 2), mConductance(1, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(1, 0), mConductance(2, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(1, 1), mConductance(2, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(1, 2), mConductance(2, 2));
	}
	// Set off diagonal blocks, 2x3x3 entries
	if (terminalNotGrounded(0) && terminalNotGrounded(1)) {
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(1, 0), -mConductance(0, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(1, 1), -mConductance(0, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 0), matrixNodeIndex(1, 2), -mConductance(0, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(1, 0), -mConductance(1, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(1, 1), -mConductance(1, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 1), matrixNodeIndex(1, 2), -mConductance(1, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(1, 0), -mConductance(2, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(1, 1), -mConductance(2, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(0, 2), matrixNodeIndex(1, 2), -mConductance(2, 2));


		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(0, 0), -mConductance(0, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(0, 1), -mConductance(0, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 0), matrixNodeIndex(0, 2), -mConductance(0, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(0, 0), -mConductance(1, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(0, 1), -mConductance(1, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 1), matrixNodeIndex(0, 2), -mConductance(1, 2));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(0, 0), -mConductance(2, 0));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(0, 1), -mConductance(2, 1));
		Math::addToMatrixElement(systemMatrix, matrixNodeIndex(1, 2), matrixNodeIndex(0, 2), -mConductance(2, 2));
	}

	mSLog->info(
		"\nConductance matrix: {:s}",
		Logger::matrixToString(mConductance));
}

void EMT::Ph3::Resistor::mnaAddPostStepDependencies(AttributeBase::List &prevStepDependencies, AttributeBase::List &attributeDependencies, AttributeBase::List &modifiedAttributes, Attribute<Matrix>::Ptr &leftVector) {
	attributeDependencies.push_back(leftVector);
	modifiedAttributes.push_back(attribute("v_intf"));
	modifiedAttributes.push_back(attribute("i_intf"));
}

void EMT::Ph3::Resistor::mnaPostStep(Real time, Int timeStepCount, Attribute<Matrix>::Ptr &leftVector) {
	mnaUpdateVoltage(*leftVector);
	mnaUpdateCurrent(*leftVector);
}


void EMT::Ph3::Resistor::mnaUpdateVoltage(const Matrix& leftVector) {
	// v1 - v0
	mIntfVoltage = Matrix::Zero(3,1);
	if (terminalNotGrounded(1)) {
		mIntfVoltage(0, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 0));
		mIntfVoltage(1, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 1));
		mIntfVoltage(2, 0) = Math::realFromVectorElement(leftVector, matrixNodeIndex(1, 2));
	}
	if (terminalNotGrounded(0)) {
		mIntfVoltage(0, 0) = mIntfVoltage(0, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 0));
		mIntfVoltage(1, 0) = mIntfVoltage(1, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 1));
		mIntfVoltage(2, 0) = mIntfVoltage(2, 0) - Math::realFromVectorElement(leftVector, matrixNodeIndex(0, 2));
	}
	mSLog->debug(
		"\nVoltage: {:s}",
		Logger::matrixToString(mIntfVoltage)
	);
	mSLog->flush();
}

void EMT::Ph3::Resistor::mnaUpdateCurrent(const Matrix& leftVector) {
	mIntfCurrent = mConductance * mIntfVoltage;
	mSLog->debug(
		"\nCurrent: {:s}",
		Logger::matrixToString(mIntfCurrent)
	);
	mSLog->flush();
}


// #### DAE Section ####

/// Initialize state and derivative of state variables
void EMT::Ph3::Resistor::daeInitialize(double time, double state[], double dstate_dt[], int& counter){
	updateMatrixNodeIndices();
	mSLog->info(
		"\n--- daeInitialize ---"
		"\nno variable was added by the 3Ph-resistor '{:s}' to the state vector"
		"\n--- daeInitialize finished ---",
		this->name());
}

///Residual Function for DAE Solver
void EMT::Ph3::Resistor::daeResidual(double time, 
	const double state[], const double dstate_dt[], 
	double resid[], std::vector<int>& off){
	
	// positive current flows from noded 1 into node 0
	
	if (terminalNotGrounded(0)) {
		resid[matrixNodeIndex(0, 0)] += state[matrixNodeIndex(0, 0)]*mConductance(0, 0);
		resid[matrixNodeIndex(0, 1)] += state[matrixNodeIndex(0, 1)]*mConductance(1, 1);
		resid[matrixNodeIndex(0, 2)] += state[matrixNodeIndex(0, 2)]*mConductance(2, 2);

		resid[matrixNodeIndex(1, 0)] -= state[matrixNodeIndex(0, 0)]*mConductance(0, 0);
		resid[matrixNodeIndex(1, 1)] -= state[matrixNodeIndex(0, 1)]*mConductance(1, 1);
		resid[matrixNodeIndex(1, 2)] -= state[matrixNodeIndex(0, 2)]*mConductance(2, 2);
	}
	if (terminalNotGrounded(1)) {
		resid[matrixNodeIndex(1, 0)] += state[matrixNodeIndex(1, 0)]*mConductance(0, 0);
		resid[matrixNodeIndex(1, 1)] += state[matrixNodeIndex(1, 1)]*mConductance(1, 1);
		resid[matrixNodeIndex(1, 2)] += state[matrixNodeIndex(1, 2)]*mConductance(2, 2);

		resid[matrixNodeIndex(0, 0)] -= state[matrixNodeIndex(1, 0)]*mConductance(0, 0);
		resid[matrixNodeIndex(0, 1)] -= state[matrixNodeIndex(1, 1)]*mConductance(1, 1);
		resid[matrixNodeIndex(0, 2)] -= state[matrixNodeIndex(1, 2)]*mConductance(2, 2);
	}

	mSLog->debug(
		"\n\n--- daeResidual 3Ph-Resistor name: {:s} - SimStep = {:f} ---"
		"\nupdate nodal equations of node 0"
		"\nresid[matrixNodeIndex(0, 0)] += (state[matrixNodeIndex(0, 0)]-state[matrixNodeIndex(1, 0))*mConductance(0, 0) = ({:f} - {:f})*{:f} = {:f}"
		"\nresid[matrixNodeIndex(0, 1)] += (state[matrixNodeIndex(0, 1)]-state[matrixNodeIndex(1, 1))*mConductance(1, 1) = ({:f} - {:f})*{:f} = {:f}"
		"\nresid[matrixNodeIndex(0, 2)] += (state[matrixNodeIndex(0, 2)]-state[matrixNodeIndex(1, 2))*mConductance(2, 2) = ({:f} - {:f})*{:f} = {:f}"

		"\nupdate nodal equations of node 1"
		"\nresid[matrixNodeIndex(1, 0)] += (state[matrixNodeIndex(1, 0)]-state[matrixNodeIndex(0, 0))*mConductance(0, 0) = ({:f} - {:f})*{:f} = {:f}"
		"\nresid[matrixNodeIndex(1, 1)] += (state[matrixNodeIndex(1, 1)]-state[matrixNodeIndex(0, 1))*mConductance(1, 1) = ({:f} - {:f})*{:f} = {:f}"
		"\nresid[matrixNodeIndex(1, 2)] += (state[matrixNodeIndex(1, 2)]-state[matrixNodeIndex(0, 2))*mConductance(2, 2) = ({:f} - {:f})*{:f} = {:f}",

		this->name(), time,
		state[matrixNodeIndex(0, 0)], state[matrixNodeIndex(1, 0)], mConductance(0, 0), resid[matrixNodeIndex(0, 0)],
		state[matrixNodeIndex(0, 1)], state[matrixNodeIndex(1, 1)], mConductance(1, 1), resid[matrixNodeIndex(0, 1)],
		state[matrixNodeIndex(0, 2)], state[matrixNodeIndex(1, 2)], mConductance(2, 2), resid[matrixNodeIndex(0, 2)],
		state[matrixNodeIndex(1, 0)], state[matrixNodeIndex(0, 0)], mConductance(0, 0), resid[matrixNodeIndex(1, 0)],
		state[matrixNodeIndex(1, 1)], state[matrixNodeIndex(0, 1)], mConductance(1, 1), resid[matrixNodeIndex(1, 1)],
		state[matrixNodeIndex(1, 2)], state[matrixNodeIndex(0, 2)], mConductance(2, 2), resid[matrixNodeIndex(1, 2)]
	);
}

///
void EMT::Ph3::Resistor::daePostStep(double Nexttime, const double state[], const double dstate_dt[], int& counter) {
	mIntfVoltage(0, 0) = 0.0;
	mIntfVoltage(1, 0) = 0.0;
	mIntfVoltage(2, 0) = 0.0;
	if (terminalNotGrounded(0)) {
		mIntfVoltage(0, 0) -= state[matrixNodeIndex(0, 0)];
		mIntfVoltage(1, 0) -= state[matrixNodeIndex(0, 1)];
		mIntfVoltage(2, 0) -= state[matrixNodeIndex(0, 2)];
	}
	if (terminalNotGrounded(1)) {
		mIntfVoltage(0, 0) += state[matrixNodeIndex(1, 0)];
		mIntfVoltage(1, 0) += state[matrixNodeIndex(1, 1)];
		mIntfVoltage(2, 0) += state[matrixNodeIndex(1, 2)];
	}
	
	mIntfCurrent(0,0) = mIntfVoltage(0,0)*mConductance(0, 0);
	mIntfCurrent(1,0) = mIntfVoltage(1,0)*mConductance(1, 1);
	mIntfCurrent(2,0) = mIntfVoltage(2,0)*mConductance(2, 2);
}