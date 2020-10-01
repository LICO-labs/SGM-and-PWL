/* #############################################
 *             This file is part of
 *                    ZERO
 *
 *             Copyright (c) 2020
 *     Released under the Creative Commons
 *        Zero v1.0 Universal License
 *
 *              Find out more at
 *        https://github.com/ds4dm/ZERO
 * #############################################*/


#include "interfaces/epec_models.h"
#include "zero.h"
#include <armadillo>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdlib>
#include <gurobi_c++.h>
#include <iostream>
#include <iterator>
#include <math.h>

using namespace std;
namespace logging = boost::log;
using namespace boost::program_options;
namespace po = boost::program_options;

int main(int argc, char **argv) {
  string resFile, instanceFile, logFile;
  int    writeLevel = 0, nThreads = 0, verbosity = 0, algorithm = 0, aggressiveness = 0, add{0},
		recover      = 0;
  double timeLimit = NAN, boundBigM = NAN, devtol = NAN;
  bool   bound = 0, pure = 0;

  po::options_description desc("ZERO-EPEC: Allowed options");
  desc.add_options()("help,h", "Shows this help message")("version,v", "Shows ZERO version")(
		"input,i",
		po::value<string>(&instanceFile),
		"Sets the input path/filename of the instance file (.json appended "
		"automatically)")("pure,p",
								po::value<bool>(&pure)->default_value(false),
								"Controls whether the Algorithm should seek for a pure NE or not. If "
								"Algorithm is CombinatorialPNE, this is automatically true.")(
		"recover,r",
		po::value<int>(&recover)->default_value(0),
		"If InnerApproximation is used along with PureNashEquilibrium, which "
		"strategy should "
		"be used to retrieve a pure NE. 0: IncrementalEnumeration, "
		"1:CombinatorialPNE")("Algorithm,a",
									 po::value<int>(&algorithm),
									 "Sets the Algorithm. 0:FullEnumeration, "
									 "1:InnerApproximation, 2:CombinatorialPNE, 3:OuterApproximation")(
		"solution,s",
		po::value<string>(&resFile)->default_value("dat/Solution"),
		"Sets the output path/filename of the solution file (.json appended "
		"automatically)")("log,l",
								po::value<string>(&logFile)->default_value("dat/Results.csv"),
								"Sets the output path/filename of the csv log file")(
		"timelimit,tl",
		po::value<double>(&timeLimit)->default_value(-1.0),
		"Sets the timelimit for solving the Nash Equilibrium model")(
		"writelevel,w",
		po::value<int>(&writeLevel)->default_value(0),
		"Sets the writeLevel param. 0: only Json. 1: only human-readable. 2: "
		"both")("message,m",
				  po::value<int>(&verbosity)->default_value(0),
				  "Sets the verbosity level for info and warning messages. 0: "
				  "warning and critical. 1: info. 2: debug. 3: trace")(
		"Threads,t",
		po::value<int>(&nThreads)->default_value(1),
		"Sets the number of Threads for Gurobi. (int): number of Threads. 0: "
		"auto (number of processors)")(
		"aggr,ag",
		po::value<int>(&aggressiveness)->default_value(1),
		"Sets the Aggressiveness for the InnerApproximation, namely the number "
		"of Random polyhedra added if no deviation is found. (int)")(
		"bound,bo",
		po::value<bool>(&bound)->default_value(false),
		"Decides whether primal variables should be bounded or not.")(
		"devtol,dt",
		po::value<double>(&devtol)->default_value(-1.0),
		"Sets the deviation tolerance.")("BoundBigM,bbm",
													po::value<double>(&boundBigM)->default_value(1e5),
													"Set the bounding BigM related to the parameter --bound")(
		"add,ad",
		po::value<int>(&add)->default_value(0),
		"Sets the Game::EPECAddPolyMethod for the InnerApproximation. 0: "
		"Sequential. "
		"1: ReverseSequential. 2:Random.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
  po::notify(vm);

  if (vm.count("help")) {
	 cout << desc;
	 return EXIT_SUCCESS;
  }
  if (vm.count("version") || verbosity >= 2) {
	 arma::arma_version ver;
	 int                major = 0, minor = 0, technical = 0;
	 string             M, m, p;
	 ZEROVersion(M, m, p);
	 BOOST_LOG_TRIVIAL(info) << "ZERO Version: " << M << "." << m << "." << p;
	 if (vm.count("version"))
		return EXIT_SUCCESS;
  }

  if (instanceFile == "") {
	 cout << "-i [--input] option missing.\n Use with --help for help on list "
				"of arguments\n";
	 return EXIT_SUCCESS;
  }
  switch (verbosity) {
  case 0:
	 logging::core::get()->set_filter(logging::trivial::severity > logging::trivial::info);
	 break;
  case 1:
	 logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
	 break;
  case 2:
	 logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::debug);
	 break;
  case 3:
	 logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::trace);
	 break;
  default:
	 BOOST_LOG_TRIVIAL(warning) << "Invalid option for --message (-m). Setting default value: 0";
	 verbosity = 0;
	 logging::core::get()->set_filter(logging::trivial::severity > logging::trivial::info);
	 break;
  }
  // --------------------------------
  // LOADING INSTANCE
  // --------------------------------
  Models::EPEC::EPECInstance instance(instanceFile);
  if (instance.Countries.empty()) {
	 cerr << "Error: instance is empty\n";
	 return 1;
  }

  // --------------------------------
  // TEST STARTS
  // --------------------------------
  auto timeStart = std::chrono::high_resolution_clock::now();
  try {
	 GRBEnv env = GRBEnv();

	 // OPTIONS
	 //------------
	 Models::EPEC::EPEC epec(&env);
	 // Num Threads
	 if (nThreads != 0)
		epec.setNumThreads(nThreads);
	 // Pure NE
	 if (pure)
		epec.setPureNashEquilibrium(true);
	 // TimeLimit
	 epec.setTimeLimit(timeLimit);
	 // bound QPs
	 if (bound) {
		epec.setBoundPrimals(true);
	 }
	 if (devtol > 0)
		epec.setDeviationTolerance(devtol);

	 // Algorithm

	 switch (algorithm) {
	 case 1: {
		epec.setAlgorithm(Data::EPEC::Algorithms::InnerApproximation);
		if (aggressiveness != 1)
		  epec.setAggressiveness(aggressiveness);
		switch (add) {
		case 1:
		  epec.setAddPolyMethod(Data::LCP::PolyhedraStrategy::ReverseSequential);
		  break;
		case 2:
		  epec.setAddPolyMethod(Data::LCP::PolyhedraStrategy::Random);
		  break;
		default:
		  epec.setAddPolyMethod(Data::LCP::PolyhedraStrategy::Sequential);
		}
		if (recover != 0)
		  epec.setRecoverStrategy(Data::EPEC::RecoverStrategy::Combinatorial);
		break;
	 }
	 case 2: {
		epec.setAlgorithm(Data::EPEC::Algorithms::CombinatorialPne);
		break;
	 }
	 case 3: {
		epec.setAlgorithm(Data::EPEC::Algorithms::OuterApproximation);
		break;
	 }
	 default:
		epec.setAlgorithm(Data::EPEC::Algorithms::FullEnumeration);
	 }

	 //------------

	 for (unsigned int j = 0; j < instance.Countries.size(); ++j)
		epec.addCountry(instance.Countries.at(j));
	 epec.addTranspCosts(instance.TransportationCosts);
	 epec.finalize();
	 epec.findNashEq();

	 auto                          timeStop      = std::chrono::high_resolution_clock::now();
	 std::chrono::duration<double> timeDiff      = timeStop - timeStart;
	 double                        wallClockTime = timeDiff.count();
	 int realThreads = nThreads > 0 ? env.get(GRB_IntParam_Threads) : nThreads;

	 // --------------------------------
	 // WRITING STATISTICS AND SOLUTION
	 // --------------------------------
	 auto stat = epec.getStatistics();
	 if (stat.Status.get() == ZEROStatus::NashEqFound)
		epec.writeSolution(writeLevel, resFile);
	 ifstream      existCheck(logFile);
	 std::ofstream results(logFile, ios::app);

	 if (!existCheck.good()) {
		results << "instance;Algorithm;Countries;Followers;isPureNE;RequiredPureNE;"
					  "Status;"
					  "numFeasiblePolyhedra;"
					  "NumVar;NumConstraints;NumNonZero;ClockTime"
					  "(s);Threads;Indicators;numInnerIterations;LostIntermediateEq;"
					  "Aggressiveness;"
					  "AddPolyMethod;NumericalIssues;bound;BoundBigM;"
					  "recoveryStrategy\n";
	 }
	 existCheck.close();

	 stringstream polyT;
	 copy(stat.AlgorithmData.FeasiblePolyhedra.get().begin(),
			stat.AlgorithmData.FeasiblePolyhedra.get().end(),
			ostream_iterator<int>(polyT, " "));

	 results << instanceFile << ";" << to_string(stat.AlgorithmData.Algorithm.get()) << ";"
				<< instance.Countries.size() << ";[";
	 for (auto &countrie : instance.Countries)
		results << " " << countrie.n_followers;

	 results << " ];" << to_string(epec.getStatistics().PureNashEquilibrium.get()) << ";"
				<< to_string(pure) << ";" << to_string(stat.Status.get()) << ";[ " << polyT.str()
				<< "];" << stat.NumVar.get() << ";" << stat.NumConstraints.get() << ";"
				<< stat.NumNonZero.get() << ";" << wallClockTime << ";" << realThreads << ";"
				<< to_string(stat.AlgorithmData.IndicatorConstraints.get());
	 if (stat.AlgorithmData.Algorithm.get() == Data::EPEC::Algorithms::InnerApproximation) {
		results << ";" << stat.NumIterations.get() << ";"
				  << epec.getStatistics().AlgorithmData.LostIntermediateEq.get() << ";"
				  << stat.AlgorithmData.Aggressiveness.get() << ";"
				  << to_string(stat.AlgorithmData.PolyhedraStrategy.get()) << ";"
				  << stat.NumericalIssues.get() << ";"
				  << to_string(stat.AlgorithmData.BoundPrimals.get()) << ";"
				  << stat.AlgorithmData.BoundBigM.get() << ";"
				  << to_string(stat.AlgorithmData.RecoverStrategy.get());
	 } else {
		results << ";-;-;-;-;-;-;-;-";
	 }
	 results << "\n";
	 results.close();
  } catch (ZEROException &e) {
	 std::cerr << "" << e.what() << "--" << e.more();
  }

  return EXIT_SUCCESS;
}
