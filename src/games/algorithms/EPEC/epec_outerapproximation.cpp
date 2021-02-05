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


#include "games/algorithms/EPEC/epec_outerapproximation.h"

#include <chrono>
#include <gurobi_c++.h>
#include <set>
#include <string>

/**
 * @brief Overrides Algorithms::EPEC::PolyBase::isSolved with a custom method.
 * @param tol Numerical tolerance. Currently not useful
 * @return True if the current outer approximation solution is feasible (then, it is solved)
 */
bool Algorithms::EPEC::OuterApproximation::isSolved(double tol) const { return this->Feasible; }


/**
 * @brief Checks whether the current outer approximation equilibrium is feasible and solves the
 * problem. Otherwise, it adde cuts or generate useful information for thext iterations
 * @param addedCuts [out] is true if at least a cut has been added
 * @return
 */
bool Algorithms::EPEC::OuterApproximation::isFeasible(bool &addedCuts) {


  // First, we have a NE from Games::computeNashEq
  if (!this->EPECObject->NashEquilibrium)
	 return false;

  // Then, the feasibility is implied also by the deviations
  bool      result = {true};
  arma::vec bestResponse;
  arma::vec currentPayoffs =
		this->EPECObject->TheNashGame->computeQPObjectiveValues(this->EPECObject->SolutionX, true);
  for (unsigned int i = 0; i < this->EPECObject->NumPlayers; ++i) {
	 LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation:: Payoff of " << i << " is "
					 << currentPayoffs.at(i);
	 this->Trees.at(i)->resetFeasibility();
	 double val = this->EPECObject->respondSol(bestResponse, i, this->EPECObject->SolutionX);
	 if (val == GRB_INFINITY) {
		LOG_S(1) << "Algorithms::EPEC::OuterApproximation:: Unbounded deviation for " << i;
		addedCuts = false;
		return false;
	 }
	 // minimization standard
	 // @todo check the direction of the inequality
	 if (std::abs(currentPayoffs.at(i) - val) > this->Tolerance) {
		// Discrepancy between payoffs! Need to investigate.
		if (currentPayoffs.at(i) - val > this->Tolerance) {
		  // It means the current payoff is more than then optimal response. Then
		  // this is not a best response. Theoretically, this cannot happen from
		  // an outer approximation. This if case is a warning case then
		  //@todo can this happen?

		  LOG_S(WARNING) << "Algorithms::EPEC::OuterApproximation::"
								  "isFeasible: No best response for Player "
							  << i << " (" << currentPayoffs.at(i) << " vs " << val << ")";

		  throw ZEROException(ZEROErrorCode::Numeric,
									 "Invalid payoffs relation (better best response)");
		  // throw;
		  // throw;
		} else {
		  // if ((val - currentPayoffs.at(i) ) > tol)
		  // It means the current payoff is less than the optimal response. The
		  // approximation is not good, and this point is infeasible. Then, we can
		  // generate a value-cut
		  arma::vec xMinusI;
		  this->EPECObject->getXMinusI(this->EPECObject->SolutionX, i, xMinusI);
		  this->addValueCut(i, val, xMinusI);
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::isFeasible: "
							  "Value cut at for Player "
						  << i;
		  result = false;
		}
	 } else {
		// Here we have a best response whose payoff coincides with the one of the
		// equilibrium. The strategy might not be feasible, though.
		arma::vec xOfI;
		this->EPECObject->getXofI(this->EPECObject->SolutionX, i, xOfI, false);

		// Check if we need to add the point to the vertex storage.
		arma::vec vertex = bestResponse.subvec(0, xOfI.size() - 1);
		//@todo debug
		// vertex.print("Best Response");
		if (!Utils::containsRow(*this->Trees.at(i)->getV(), vertex, this->Tolerance)) {
		  this->Trees.at(i)->addVertex(vertex);
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::isFeasible: "
							  "Adding vertex as of best response for Player "
						  << i << " (Best Response)";
		} else {
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::isFeasible: "
							  "Already known best response for Player "
						  << i << " (Best Response)";
		}

		if (!Utils::isZero(xOfI - bestResponse.subvec(0, xOfI.size() - 1), this->Tolerance)) {
		  // Then, if the answers do not coincide, we need to refine the
		  // approximation or determine if this strategy is anyhow feasible.
		  // We search for a convex combination of best responses so that we can
		  // certify the answer is inside the convex-hull (or not).

		  int budget = 15;
		  if (!this->separationOracle(xOfI, this->EPECObject->SolutionX, i, budget, addedCuts)) {
			 LOG_S(1) << "Algorithms::EPEC::OuterApproximation::isFeasible: "
							 "Oracle gave a negative answer for Player "
						 << i;
			 result = false;
		  }

		} else {
		  this->Trees.at(i)->setFeasible();
		  this->Trees.at(i)->setPure();
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::isFeasible: "
							  "Feasible strategy for Player "
						  << i << " (Best Response)";
		}
	 }
  }
  return result;
}


/**
 * @brief Updates the membership linear-program in the relative
 * Algorithms::EPEC::OuterApproximation::Trees for the player @p player
 * @param player The player index
 * @param xOfI  The point for which the membership LP should be updated for
 * @param normalization  True if the LP is normalized
 */
void Algorithms::EPEC::OuterApproximation::updateMembership(const unsigned int &player,
																				const arma::vec &   xOfI,
																				bool                normalization) {


  auto PlayerTree = Trees.at(player);
  MathOpt::getDualMembershipLP(PlayerTree->MembershipLP,
										 PlayerTree->VertexCounter,
										 PlayerTree->V,
										 PlayerTree->RayCounter,
										 PlayerTree->R,
										 xOfI,
										 normalization);
}

bool Algorithms::EPEC::OuterApproximation::separationOracle(
	 arma::vec &xOfI, arma::vec &x, unsigned int player, int budget, bool &addedCuts) {

  for (int k = 0; k < budget; ++k) {
	 // First, we check whether the point is a convex combination of feasible
	 // KNOWN points

	 auto V = this->Trees.at(player)->V;

	 //@todo debug
	 // xOfI.print("Point to separate: ");

	 this->updateMembership(player, xOfI, true);
	 auto convexModel = *this->Trees.at(player)->MembershipLP;
	 convexModel.optimize();

	 int status = convexModel.get(GRB_IntAttr_Status);
	 LOG_S(1) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
					 "MermbershipLP status is "
				 << status;
	 if (status == GRB_OPTIMAL && convexModel.get(GRB_IntAttr_SolCount) == 1) {
		convexModel.set(GRB_IntParam_SolutionNumber, 0);
		arma::vec sol(xOfI.size(), arma::fill::zeros);
		for (unsigned int i = 0; i < xOfI.size(); i++)
		  sol.at(i) =
				std::abs(convexModel.getVarByName("y_" + std::to_string(i)).get(GRB_DoubleAttr_X));

		if (convexModel.getObjective().getValue() == 0 && sol.max() == 0) {
		  // this->Trees.at(player)->addVertex(xOfI);
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
							  "The point is a convex combination of known points! Player "
						  << player;

		  this->Trees.at(player)->setFeasible();

		  arma::vec support;
		  support.zeros(this->Trees.at(player)->getVertexCount());
		  auto test = convexModel.getVarByName("x").get(GRB_DoubleAttr_X);
		  for (unsigned int v = 0; v < this->Trees.at(player)->getVertexCount(); ++v) {
			 // abs to avoid misunderstanding with sign conventions
			 support.at(v) =
				  convexModel.getConstrByName("V_" + std::to_string(v)).get(GRB_DoubleAttr_Pi);
		  }
		  //@todo debug
		  // support.print("MNE Support: ");
		  if (support.max() == 1)
			 this->Trees.at(player)->setPure();
		  return true;
		}
	 }

	 // Else, the status should be OPTIMAL but without the objective of zero
	 if (status == GRB_OPTIMAL) {
		// Get the Farkas' in the form of the unbounded ray of the dual of the
		// dualMembershipLP (the primal)
		LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
							"The point is NOT a convex combination of known points! Found "
						<< convexModel.get(GRB_IntAttr_SolCount) << " solutions. Player " << player;
		for (int z = 0; z < convexModel.get(GRB_IntAttr_SolCount); ++z) {
		  convexModel.set(GRB_IntParam_SolutionNumber, z);
		  arma::vec cutLHS;
		  cutLHS.zeros(xOfI.size());

		  for (unsigned int i = 0; i < xOfI.size(); i++)
			 cutLHS.at(i) = convexModel.getVarByName("y_" + std::to_string(i)).get(GRB_DoubleAttr_X);
		  //@todo debug
		  // cutLHS.print("Separating hyperplane: ");

		  // Optimize the resulting inequality over the original feasible set
		  auto       leaderModel = this->EPECObject->respond(player, x);
		  GRBLinExpr expr        = 0;
		  for (unsigned int i = 0; i < xOfI.size(); ++i)
			 expr += cutLHS.at(i) * leaderModel->getVarByName("x_" + std::to_string(i));

		  leaderModel->setObjective(expr, GRB_MAXIMIZE);
		  leaderModel->update();
		  leaderModel->set(GRB_IntParam_InfUnbdInfo, 1);
		  leaderModel->set(GRB_IntParam_DualReductions, 0);
		  leaderModel->set(GRB_IntParam_OutputFlag, 0);
		  // leaderModel->write("dat/LeaderModel" + std::to_string(player) + ".lp");
		  leaderModel->optimize();
		  status = leaderModel->get(GRB_IntAttr_Status);

		  if (status == GRB_OPTIMAL) {
			 double cutV = leaderModel->getObjective().getValue();
			 LOG_S(1) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
							 "LeaderModel status = "
						 << std::to_string(status) << " with objective=" << cutV << " for Player "
						 << player;
			 arma::vec val  = cutLHS.t() * xOfI; // c^T xOfI
			 arma::vec val2 = cutLHS.t() * V.row(0).t();
			 LOG_S(1) << "Algorithms::EPEC::OuterApproximation::separationOracle: c^Tv=" << cutV
						 << " -- c^TxOfI=" << val.at(0) << " -- c^TV(0)=" << val2.at(0);
			 if (cutV - val.at(0) < -this->Tolerance) {
				// False, but we have a cut :-)
				// Ciao Moni
				cutV              = cutV;
				arma::sp_mat cutL = Utils::resizePatch(
					 arma::sp_mat{cutLHS}.t(), 1, this->PolyLCP.at(player)->getNumCols());
				if (this->PolyLCP.at(player)->containsCut(
						  Utils::resizePatch(cutLHS, this->PolyLCP.at(player)->getNumCols()), cutV)) {
				  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
									  "cut already added for Player "
								  << player;
				  // throw;
				  break;

				} else {
				  this->PolyLCP.at(player)->addCustomCuts(cutL, arma::vec{cutV});
				  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
									  "adding cut for Player "
								  << player;
				  addedCuts = true;
				  return false;
				}
			 } else {
				// We found a new vertex
				arma::vec v;
				v.zeros(V.n_cols);
				for (unsigned int i = 0; i < V.n_cols; ++i) {
				  v[i] = leaderModel->getVarByName("x_" + std::to_string(i)).get(GRB_DoubleAttr_X);
				}

				//@todo debug
				// v.print("Vertex found: ");
				if (Utils::containsRow(*this->Trees.at(player)->getV(), v, this->Tolerance)) {
				  LOG_S(WARNING) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
										  "duplicate vertex for  player "
									  << player;
				  //@todo
				  break;
				  // throw;
				} else {
				  this->Trees.at(player)->addVertex(v);
				  //@todo debug
				  // v.print("Vertex");
				  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
									  "adding vertex for Player. "
								  << (budget - k - 1) << " iterations left for player " << player;
				  break;
				}
			 }

		  } // status optimal for leaderModel
		  else if (status == GRB_UNBOUNDED) {
			 // Check for a new ray
			 if (!Utils::containsRow(*this->Trees.at(player)->getR(), cutLHS, this->Tolerance)) {
				LOG_S(WARNING) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
										"new ray for  player "
									<< player;
				this->Trees.at(player)->addRay(cutLHS);
				break;
			 } else {
				LOG_S(WARNING) << "Algorithms::EPEC::OuterApproximation::separationOracle: "
										"duplicate ray for player "
									<< player;
				break;
			 }

		  } // status unbounded for leaderModel

		  else
			 throw ZEROException(ZEROErrorCode::Assertion,
										"Unknown status for leaderModel for player " +
											 std::to_string(player));
		} // end for
		  // no separation
	 } else {
		throw ZEROException(ZEROErrorCode::Assertion,
								  "Unknown status for convexModel for player " + std::to_string(player));
	 }
  }
  return false;
}


/**
 * @brief Adds a value cut to @p player MathOpt::LCP
 * @param player The index of the player
 * @param RHS The RHS of the value cut
 * @param xMinusI The strategies of the other players
 */
void Algorithms::EPEC::OuterApproximation::addValueCut(const unsigned int player,
																		 const double       RHS,
																		 arma::vec          xMinusI) {

  arma::vec LHS = this->EPECObject->LeaderObjective.at(player)->c +
						this->EPECObject->LeaderObjective.at(player)->C * xMinusI;
  arma::sp_mat cutLHS =
		Utils::resizePatch(arma::sp_mat{LHS}.t(), 1, this->PolyLCP.at(player)->getNumCols());
  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::addValueCut: "
					  "adding cut for Player "
				  << player;
  if (!this->PolyLCP.at(player)->containsCut(LHS, -RHS, this->Tolerance))
	 this->PolyLCP.at(player)->addCustomCuts(-cutLHS, arma::vec{-RHS});
}

/**
 * @brief Given the Game::EPEC instance, this methos solves it through the outer approximation
 * scheme.
 */
void Algorithms::EPEC::OuterApproximation::solve() {
  // Set the initial point for all countries as 0 and solve the respective LCPs?
  this->EPECObject->SolutionX.zeros(this->EPECObject->NumVariables);
  bool solved = {false};
  if (this->EPECObject->Stats.AlgorithmData.TimeLimit.get() > 0)
	 this->EPECObject->InitTime = std::chrono::high_resolution_clock::now();

  this->EPECObject->Stats.NumIterations.set(0);

  // Initialize Trees
  // We actually do not use the complex tree structure for this vanilla-version, but we nevertheless
  // give the user the capability of doing so.
  this->Trees     = std::vector<OuterTree *>(this->EPECObject->NumPlayers, 0);
  this->Incumbent = std::vector<OuterTree::Node *>(this->EPECObject->NumPlayers, 0);
  for (unsigned int i = 0; i < this->EPECObject->NumPlayers; i++) {
	 Trees.at(i)     = new OuterTree(this->PolyLCP.at(i)->getNumRows(), this->Env);
	 Incumbent.at(i) = Trees.at(i)->getRoot();
  }

  bool branch = true;
  // In this case, branchingLocations is a vector of locations with the length
  // of this->EPECObject->NumPlayers
  std::vector<int>      branchingLocations;
  std::vector<int>      branchingCandidatesNumber;
  unsigned int          cumulativeBranchingCandidates = 0;
  std::vector<long int> branches;
  while (!solved) {
	 branchingLocations.clear();
	 this->EPECObject->Stats.NumIterations.set(this->EPECObject->Stats.NumIterations.get() + 1);
	 LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: Iteration "
					 << std::to_string(this->EPECObject->Stats.NumIterations.get());

	 branchingLocations            = std::vector<int>(this->EPECObject->NumPlayers, -1);
	 branchingCandidatesNumber     = std::vector<int>(this->EPECObject->NumPlayers, 0);
	 cumulativeBranchingCandidates = 0;

	 for (int j = 0; j < this->EPECObject->NumPlayers; ++j) {
		branchingCandidatesNumber.at(j) =
			 Trees.at(j)->getEncodingSize() - Incumbent.at(j)->getCumulativeBranches();
		cumulativeBranchingCandidates += branchingCandidatesNumber.at(j);
	 }

	 bool infeasibilityDetection = false;
	 if (branch) {
		for (int j = 0; j < this->EPECObject->NumPlayers && !infeasibilityDetection; ++j) {
		  // Check if we can branch
		  if (branchingCandidatesNumber.at(j) != 0) {
			 // In the first iteration, no complex branching rule.
			 if (this->EPECObject->Stats.NumIterations.get() == 1) {
				branchingLocations.at(j) = this->getFirstBranchLocation(j, Incumbent.at(j));
				// Check if we detected infeasibility
				if (branchingLocations.at(j) < 0) {
				  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
									  "firstBranching proves infeasibility for player  "
								  << j;
				  infeasibilityDetection = true;
				  break;
				}
			 } else {
				branchingLocations.at(j) = this->hybridBranching(j, Incumbent.at(j));
				// Check if we detected infeasibility
				if (branchingLocations.at(j) == -2) {
				  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
									  "hybridBranching proves infeasiblity for player "
								  << j;
				  infeasibilityDetection = true;
				  break;
				}
			 }
		  }
		}

		if (infeasibilityDetection) {
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "Solved without any equilibrium. Proven infeasibility";
		  this->EPECObject->Stats.Status.set(ZEROStatus::NashEqNotFound);
		  solved = true;
		  break;
		}

		// Check at least a player has at least a branching candidate
		if (cumulativeBranchingCandidates == 0) {
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "Solved without any equilibrium.";
		  this->EPECObject->Stats.Status.set(ZEROStatus::NashEqNotFound);
		  solved = true;
		  break;
		}


		// Check that there is at least a player has a branching selection with
		// hybrid branching
		if (*std::max_element(branchingLocations.begin(), branchingLocations.end()) < 0) {

		  // No branching candidates.
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "No more hybrid branching candidates for "
							  "any player. Checking if "
							  "any complementarities are left.";
		  this->printCurrentApprox();
		  for (int j = 0; j < this->EPECObject->NumPlayers; ++j)
			 branchingLocations.at(j) = this->getFirstBranchLocation(j, Incumbent.at(j));

		  if (*std::max_element(branchingLocations.begin(), branchingLocations.end()) < 0) {
			 LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
								 "No more branching candidates.";
			 this->EPECObject->Stats.Status.set(ZEROStatus::NashEqNotFound);
			 break;
		  }
		}
	 }

	 for (int j = 0; j < this->EPECObject->NumPlayers; ++j) {
		unsigned int test = 0;
		if (branchingLocations.at(j) > -1) {
		  branches           = Trees.at(j)->singleBranch(branchingLocations.at(j), *Incumbent.at(j));
		  auto childEncoding = this->Trees.at(j)->getNodes()->at(branches.at(0)).getEncoding();
		  test += this->PolyLCP.at(j)->outerApproximate(childEncoding, true);
		  // By definition of hybrid branching, the node should be feasible
		  Incumbent.at(j) = &(this->Trees.at(j)->getNodes()->at(branches.at(0)));
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "branching candidate for player "
						  << j << " is " << branchingLocations.at(j);
		} else if (!branch) {
		  // if we don't branch.
		  test += this->PolyLCP.at(j)->outerApproximate(Incumbent.at(j)->getEncoding(), true);
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "No branching for player "
						  << j;
		}
	 }

	 this->printCurrentApprox();
	 this->EPECObject->makePlayersQPs();
	 // To make computeNashEq skip any feasibility check
	 this->Feasible = true;
	 if (this->EPECObject->Stats.AlgorithmData.TimeLimit.get() > 0) {
		// Then we should take care of time. Also, let's use an heuristic to compute the time for the
		// current outer approximation.
		const std::chrono::duration<double> timeElapsed =
			 std::chrono::high_resolution_clock::now() - this->EPECObject->InitTime;
		const double timeRemaining =
			 this->EPECObject->Stats.AlgorithmData.TimeLimit.get() - timeElapsed.count();

		double timeForNextIteration = timeRemaining * 0.98;
		if ((cumulativeBranchingCandidates - 1) > 0)
		  timeForNextIteration = (timeRemaining * 0.2) / (cumulativeBranchingCandidates - 1);

		LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: Allocating "
						<< timeForNextIteration << "s for the next iteration ("
						<< cumulativeBranchingCandidates << " complementarities left).";
		this->EPECObject->computeNashEq(
			 this->EPECObject->Stats.AlgorithmData.PureNashEquilibrium.get(), timeForNextIteration);
	 } else {
		this->EPECObject->computeNashEq(
			 this->EPECObject->Stats.AlgorithmData.PureNashEquilibrium.get());
	 }

	 this->Feasible = false;
	 if (this->EPECObject->NashEquilibrium) {
		bool addedCuts{false};
		if (this->isFeasible(addedCuts)) {
		  this->Feasible = true;
		  this->EPECObject->Stats.Status.set(ZEROStatus::NashEqFound);
		  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
							  "Solved. ";
		  this->after();
		  return;
		} else {
		  if (addedCuts) {
			 branch = false;
			 LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::solve: "
								 "Cuts were added. Skipping next branching phase. ";
		  } else {
			 branch = true;
		  }
		}
	 } else {
		branch = true;
	 }
	 if (this->EPECObject->Stats.AlgorithmData.TimeLimit.get() > 0) {
		const std::chrono::duration<double> timeElapsed =
			 std::chrono::high_resolution_clock::now() - this->EPECObject->InitTime;
		const double timeRemaining =
			 this->EPECObject->Stats.AlgorithmData.TimeLimit.get() - timeElapsed.count();
		if (timeRemaining <= 0) {
		  this->EPECObject->Stats.Status.set(ZEROStatus::TimeLimit);
		  this->after();
		  return;
		}
	 }
  }
  this->after();
}


/**
 * @brief Given the player index @p player, gets a feasibility quadratic problem enforcing @p x to
 * be in the feasible region of the Game::EPEC::PlayersQP
 * @param player  The player index
 * @param x The strategy for the player
 * @return A Gurobi pointer to the model
 */
std::unique_ptr<GRBModel> Algorithms::EPEC::OuterApproximation::getFeasQP(const unsigned int player,
																								  const arma::vec    x) {


  // this->EPECObject->getXMinusI(this->EPECObject->SolutionX, player, xMinusI);
  arma::vec zeros;
  // Dummy vector of zeros associated to x^{-i}
  zeros.zeros(this->EPECObject->PlayersQP.at(player)->getNx());
  auto model = this->EPECObject->PlayersQP.at(player)->solveFixed(zeros, false);
  // Enforce QP::y to be x, namely the point to belong to the feasible region
  for (unsigned int j = 0; j < x.size(); j++)
	 model->addConstr(model->getVarByName("y_" + std::to_string(j)),
							GRB_EQUAL,
							x.at(j),
							"Fix_y_" + std::to_string(j));
  // Reset the objective
  model->setObjective(GRBLinExpr{0}, GRB_MINIMIZE);
  // model->write("dat/test.lp");
  return model;
}


/**
 * @brief Given @p player -- containing the id of the player, returns the branching
 * decision for that node given by a hybrid branching rule. In
 * particular, the method return the complementarity id maximizing a
 * combination of constraint violations and number of violated constraints.
 * @p node contains the tree's node. It isn't const since a branching candidate
 * can be pruned if infeasibility is detected. Note that if the problem is infeasible, namely one
 * complementarity branching candidate results in an infeasible relaxation, then all branching
 * candidates are removed from the list of branching candidates.
 * @param player The player id
 * @param node The pointer to the incumbent OuterTree::Node
 * @return The branching candidate. -1 if none. -2 if infeasible.
 **/
int Algorithms::EPEC::OuterApproximation::hybridBranching(const unsigned int player,
																			 OuterTree::Node *  node) {

  LOG_S(INFO) << "OuterApproximation::hybridBranching: Player " << player;

  int bestId = -1;
  if (this->EPECObject->NashEquilibrium) {
	 arma::vec zeros, x;

	 this->EPECObject->getXofI(this->EPECObject->SolutionX, player, x);
	 if (x.size() != this->EPECObject->LeaderObjective.at(player)->c.n_rows)
		throw ZEROException(ZEROErrorCode::Assertion, "wrong dimensioned x^i");

	 auto              currentEncoding = node->getEncoding();
	 std::vector<bool> incumbentApproximation;
	 double            bestScore = -1.0;

	 for (unsigned int i = 0; i < currentEncoding.size(); i++) {
		// For each complementarity
		if (node->getAllowedBranchings().at(i)) {
		  // Consider it if it is a good candidate for branching (namely, we
		  // didn't branch on it, or it wasn't proven to be infeasible)
		  incumbentApproximation = currentEncoding;
		  // Include this complementarity in the approximation
		  incumbentApproximation.at(i) = true;
		  // Build the approximation
		  this->PolyLCP.at(player)->outerApproximate(incumbentApproximation, true);
		  // If the approximation is infeasible, the
		  if (!this->PolyLCP.at(player)->getFeasOuterApp()) {
			 // The problem is infeasible!
			 LOG_S(INFO) << "OuterApproximation::hybridBranching: Player " << player
							 << " has an infeasible problem (outer relaxation induction)";
			 for (unsigned int j = 0; j < currentEncoding.size(); j++) {
				Trees.at(player)->denyBranchingLocation(*node, j);
			 }
			 return -2;
		  } else {
			 // In this case, we can check if the solution belongs to the outer
			 // approximation
			 this->EPECObject->makePlayerQP(player);
			 // Get the QP model with other players decision QP::x fixed to zero
			 // (since they only appear in the objective);
			 auto model = this->getFeasQP(player, x);
			 model->optimize();
			 const int status = model->get(GRB_IntAttr_Status);
			 if (status == GRB_INFEASIBLE) {
				// If the status is infeasible, bingo! We want to get a measure of
				// the constraint violations given by the current x
				model->feasRelax(0, false, false, true);
				model->optimize();
				if (model->getObjective().getValue() > bestScore) {
				  bestId    = i;
				  bestScore = model->getObjective().getValue();
				  LOG_S(INFO) << "OuterApproximation::hybridBranching: Player " << player
								  << " has violation of " << bestScore << " with complementarity " << i;
				}
			 } else {
				LOG_S(INFO) << "OuterApproximation::hybridBranching: Player " << player
								<< " has no violation with complementarity " << i;
			 }
		  }
		}
	 }
  }
  return bestId;
}



/**
 * @brief Given @p player -- containing the id of the player, returns the branching
 * decision for that node, where the complementarity is the most (possibly)
 * infeasible one (with both x and z positive). In particular, the method
 * return the (positive) id of the complementarity equation if there is a
 * feasible branching decision at @p node, and a negative value otherwise.
 * @param player The player id
 * @param node The pointer to the incumbent OuterTree::Node
 * @return The branching candidate. Negative if none
 */
int Algorithms::EPEC::OuterApproximation::infeasibleBranching(const unsigned int     player,
																				  const OuterTree::Node *node) {

  int result = -1;
  if (this->EPECObject->NashEquilibrium) {
	 // There exists a Nash Equilibrium for the outer approximation, which is not
	 // a Nash Equilibrium for the game
	 arma::vec x, z;
	 this->EPECObject->getXWithoutHull(this->EPECObject->SolutionX, x);
	 z                                      = this->PolyLCP.at(player)->zFromX(x);
	 std::vector<short int> currentSolution = this->PolyLCP.at(player)->solEncode(x);

	 double maxInfeas = 0;

	 //"The most infeasible" branching
	 for (unsigned int i = 0; i < currentSolution.size(); i++) {
		unsigned int varPos = i >= this->PolyLCP.at(player)->getLStart()
										  ? i + this->PolyLCP.at(player)->getNumberLeader()
										  : i;
		if (x(varPos) > 0 && z(i) > 0 && node->getAllowedBranchings().at(i) &&
			 currentSolution.at(i) == 0) {
		  if ((x(varPos) + z(i)) > maxInfeas) {
			 maxInfeas = x(varPos) + z(i);
			 result    = i;
		  }
		}
	 }
  }
  return result;
}

/**
 * @brief Given @p player -- containing the id of the player, returns the branching
 * decision for that node, where the complementarity helps to cut off a
 * possible deviation . In particular, the method return the (positive) id of
 * the complementarity equation if there is a feasible branching decision at
 * @p node, and a negative value otherwise.
 * @param player The player id
 * @param node The pointer to the incumbent OuterTree::Node
 * @return The branching candidate. Negative if none
 */
int Algorithms::EPEC::OuterApproximation::deviationBranching(const unsigned int     player,
																				 const OuterTree::Node *node) {


  int result = -1;
  if (this->EPECObject->NashEquilibrium) {
	 // There exists a Nash Equilibrium for the outer approximation, which is not
	 // a Nash Equilibrium for the game
	 arma::vec dev;
	 arma::vec x;
	 this->EPECObject->getXWithoutHull(this->EPECObject->SolutionX, x);
	 std::vector<short int> currentSolution = this->PolyLCP.at(player)->solEncode(x);
	 this->EPECObject->respondSol(dev, player, this->EPECObject->SolutionX);
	 auto encoding = this->PolyLCP.at(player)->solEncode(dev);

	 for (unsigned int i = 0; i < encoding.size(); i++) {
		if (encoding.at(i) > 0 && node->getAllowedBranchings().at(i) && currentSolution.at(i) == 0) {
		  result = i;
		}
	 }
  }
  return result;
}


/**
 * @brief Given @p player -- containing the id of the player, returns the branching
 * decision for that node, with no complementarity condition enforced. In
 * particular, the method return the (positive) id of the complementarity
 * equation if there is a feasible branching decision at @p node, and a
 * negative value otherwise.
 * @param player The player id
 * @param node The pointer to the incumbent OuterTree::Node
 * @return The branching candidate. Negative if none
 */
int Algorithms::EPEC::OuterApproximation::getFirstBranchLocation(const unsigned int player,
																					  OuterTree::Node *  node) {
  /**
	* Given @p player -- containing the id of the player, returns the branching
	* decision for that node. In
	* particular, the method return the (positive) id of the complementarity
	* equation if there is a feasible branching decision at @p node, and a
	* negative value otherwise. Note that if the problem is infeasible, namely one
	* complementarity branching candidate results in an infeasible relaxation, then all branching
	* candidates are removed from the list of branching candidates.
	* @return a positive int with the id of the complementarity to branch on, or
	* a negative value if none exists.
	*/

  if (node->getCumulativeBranches() == Trees.at(player)->getEncodingSize())
	 return -1;
  auto         model = this->PolyLCP.at(player)->LCPasMIP(true);
  unsigned int nR    = this->PolyLCP.at(player)->getNumRows();
  int          pos   = -nR;
  arma::vec    z, x;
  if (this->PolyLCP.at(player)->extractSols(
			 model.get(), z, x, true)) // If already infeasible, nothing to branch!
  {
	 std::vector<short int> v1 = this->PolyLCP.at(player)->solEncode(z, x);

	 double       maxvalx{-1}, maxvalz{-1};
	 unsigned int maxposx{0}, maxposz{0};
	 for (unsigned int i = 0; i < nR; i++) {
		unsigned int varPos = i >= this->PolyLCP.at(player)->getLStart()
										  ? i + this->PolyLCP.at(player)->getNumberLeader()
										  : i;
		if (x(varPos) > maxvalx && node->getAllowedBranchings().at(i)) {
		  maxvalx = x(varPos);
		  maxposx = i;
		}
		if (z(i) > maxvalz && node->getAllowedBranchings().at(i)) {
		  maxvalz = z(i);
		  maxposz = i;
		}
	 }
	 pos = maxvalz > maxvalx ? maxposz : maxposx;
  } else {
	 // The problem is infeasible!
	 LOG_S(INFO) << "OuterApproximation::getFirstBranchLocation: Player " << player
					 << " has an infeasible problem (outer relaxation induction)";
	 for (unsigned int j = 0; j < node->getEncoding().size(); j++) {
		Trees.at(player)->denyBranchingLocation(*node, j);
	 }
	 return -1;
  }
  return pos;
}



/**
 * @brief Given @p player -- containing the id of the player -- and @p node
 * containing a node, returns the branching decision for that node, with
 * respect to the current node. In particular, the method return the
 * (positive) id of the complementarity equation if there is a feasible
 * branching decision at @p node, and a negative value otherwise.
 * @param player The player id
 * @param node The pointer to the incumbent OuterTree::Node
 * @return A vector of 4 ints with the branching location given by the most
 * Algorithms::EPEC::OuterApproximation::infeasibleBranching,
 * Algorithms::EPEC::OuterApproximation::deviationBranching,
 * Algorithms::EPEC::OuterApproximation::hybridBranching, and
 * Algorithms::EPEC::OuterApproximation::getFirstBranchLocation, respectively. If an int is
 * negative, there is no real candidate.
 */
[[maybe_unused]] std::vector<int>
Algorithms::EPEC::OuterApproximation::getNextBranchLocation(const unsigned int player,
																				OuterTree::Node *  node) {

  std::vector<int> decisions = {-1, -1, -1, -1};
  decisions.at(0)            = this->infeasibleBranching(player, node);
  decisions.at(1)            = this->deviationBranching(player, node);
  decisions.at(2)            = this->hybridBranching(player, node);

  if (decisions.at(0) < 0 && decisions.at(1) < 0 && decisions.at(2) < 0) {
	 LOG_S(INFO) << "Player " << player
					 << ": branching with FirstBranchLocation is the only available choice";
	 decisions.at(3) = this->getFirstBranchLocation(player, node);
  }

  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::getNextBranchinglocation: "
					  "given decisions are: ";
  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::"
					  "getNextBranchinglocation:\t Infeasible="
				  << decisions.at(0);
  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::"
					  "getNextBranchinglocation:\t Deviation="
				  << decisions.at(1);
  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::"
					  "getNextBranchinglocation:\t Hybrid="
				  << decisions.at(2);
  LOG_S(INFO) << "Algorithms::EPEC::OuterApproximation::"
					  "getNextBranchinglocation:\t First="
				  << decisions.at(3);
  return decisions;
}


/**
 * @brief Prints a log message containing the encoding at the current outer
 * approximation iteration
 */
void Algorithms::EPEC::OuterApproximation::printCurrentApprox() {
  LOG_S(INFO) << "Current Node Approximation:";
  for (unsigned int p = 0; p < this->EPECObject->NumPlayers; ++p) {
	 std::stringstream msg;
	 msg << "\tPlayer " << p << ":";
	 for (unsigned int i = 0; i < this->Incumbent.at(p)->getEncoding().size(); i++) {
		msg << "\t" << this->Incumbent.at(p)->getEncoding().at(i);
	 }
	 LOG_S(INFO) << msg.str();
  }
}

/**
 * @brief  Given the vector of branching candidates from
 * Algorithms::EPEC::OuterApproximation::getNextBranchLocation, prints a sum up of them
 * @param vector Output of Algorithms::EPEC::OuterApproximation::getNextBranchLocation
 */
void Algorithms::EPEC::OuterApproximation::printBranchingLog(std::vector<int> vector) {
  LOG_S(INFO) << "Current Branching Log:";
  LOG_S(INFO) << "\tInfeasibleBranching: " << vector.at(0);
  LOG_S(INFO) << "\tDeviationBranching: " << vector.at(1);
  LOG_S(INFO) << "\tHybridBranching: " << vector.at(2);
  LOG_S(INFO) << "\tFirstAvail: " << vector.at(3);
}

/**
 * @brief Checks whether the current solution is a pure-strategy nash equilibrium
 * @param tol A numerical tolerance. Currently not used
 * @return True if the strategy is a pure nash equilibrium
 */
bool Algorithms::EPEC::OuterApproximation::isPureStrategy(double tol) const {
  if (!this->Feasible)
	 return false;
  else {
	 for (unsigned int i = 0; i < this->EPECObject->NumPlayers; ++i)
		if (!Trees.at(i)->getPure())
		  return false;

	 return true;
  }
}
void Algorithms::EPEC::OuterApproximation::after() {
  bool                      pureStrategy = true;
  std::vector<unsigned int> numComps;
  for (unsigned int i = 0; i < this->EPECObject->getNumPlayers(); ++i) {
	 if (!this->Trees.at(i)->getPure()) {
		pureStrategy = false;
	 }
	 unsigned int counter = 0;
	 for (unsigned int j = 0; j < this->Incumbent.at(i)->getEncoding().size(); j++)
		counter += this->Incumbent.at(i)->getEncoding().at(j);
	 numComps.push_back(counter);
  }
  this->EPECObject->Stats.PureNashEquilibrium.set(pureStrategy);
  this->EPECObject->Stats.AlgorithmData.OuterComplementarities.set(numComps);
  LOG_S(3) << "Algorithms::EPEC::OuterApproximation::after: post-processing results.";
}



/**
 * @brief Given the parent node address @p parent, the @p idComp to branch
	on, and the @p id, creates a new node
 * @param parent The parent node
 * @param idComp The id of the node
 * @param id The The branching candidate
 */
Algorithms::EPEC::OuterTree::Node::Node(Node &parent, unsigned int idComp, unsigned long int id) {
  this->IdComps                      = std::vector<unsigned int>{idComp};
  this->Encoding                     = parent.Encoding;
  this->Encoding.at(idComp)          = true;
  this->AllowedBranchings            = parent.AllowedBranchings;
  this->AllowedBranchings.at(idComp) = false;
  this->Id                           = id;
  this->Parent                       = &parent;
}

/**
 * @brief Constructor for the root node, given the encoding size, namely the number of
 * complementarity equations
 * @param encSize The number of complementarities
 */
Algorithms::EPEC::OuterTree::Node::Node(unsigned int encSize) {
  this->Encoding          = std::vector<bool>(encSize, 0);
  this->Id                = 0;
  this->AllowedBranchings = std::vector<bool>(encSize, true);
}


/**
 * @brief If a complementarity equation @p location  has proven to be infeasible
 * or it isn't a candidate for branching, this method prevents any further
 * branching on it for the node @p node.
 * @param node The node pointer
 * @param location The denied branching location
 */
void Algorithms::EPEC::OuterTree::denyBranchingLocation(Algorithms::EPEC::OuterTree::Node &node,
																		  const unsigned int &location) {
  if (location >= this->EncodingSize)
	 throw ZEROException(ZEROErrorCode::OutOfRange, "idComp is larger than the encoding size");
  if (!node.AllowedBranchings.at(location))
	 LOG_S(WARNING) << "Algorithms::EPEC::OuterTree::denyBranchingLocation: location "
							 "has been already denied.";
  node.AllowedBranchings.at(location) = false;
}


/**
 * @brief Given the @p idComp and the parent node @p t, creates a single
 * child by branching on @p idComp.
 * @param idComp The branching id for the complementarity
 * @param t The pointer to the node
 * @return The node counter stored in a single-element vector
 */
std::vector<long int>
Algorithms::EPEC::OuterTree::singleBranch(const unsigned int                 idComp,
														Algorithms::EPEC::OuterTree::Node &t) {
  if (idComp >= this->EncodingSize)
	 throw ZEROException(ZEROErrorCode::OutOfRange, "idComp is larger than the encoding size");
  if (t.Encoding.at(idComp) != 0) {
	 LOG_S(WARNING) << "OuterTree: cannot branch on this complementary, since it already "
							 "has been processed.";
	 return std::vector<long int>{-1};
  }
  auto child = Node(t, idComp, this->nextIdentifier());

  this->Nodes.push_back(child);
  return std::vector<long int>{this->NodeCounter - 1};
}

/**
 * @brief Adds a vertex to OuterTree::V
 * @param vertex The vector containing the vertex
 */
void Algorithms::EPEC::OuterTree::addVertex(arma::vec vertex) {
  if (vertex.size() != this->V.n_cols && this->V.n_rows > 0)
	 throw ZEROException(ZEROErrorCode::OutOfRange, "Ill-dimensioned vertex");
  this->V = arma::join_cols(this->V, arma::sp_mat{vertex.t()});
}

/**
 * @brief Adds a ray to OuterTree::R
 * @param ray The vector containing the ray
 */
void Algorithms::EPEC::OuterTree::addRay(arma::vec ray) {
  if (ray.size() != this->R.n_cols && this->R.n_rows > 0)
	 throw ZEROException(ZEROErrorCode::OutOfRange, "Ill-dimensioned ray");
  this->R = arma::join_cols(this->R, arma::sp_mat{ray.t()});
}

/**
 * @brief Given the parent node address @p parent, the @p idsComp to branch
 * on (containing all the complementarities ids), and the @p id, creates a
 * new node
 * @param parent The parent node pointer
 * @param idsComp  The vector of branching locations
 * @param id  The node id for the children
 */
Algorithms::EPEC::OuterTree::Node::Node(Node &            parent,
													 std::vector<int>  idsComp,
													 unsigned long int id) {
  this->IdComps           = std::vector<unsigned int>();
  this->Encoding          = parent.Encoding;
  this->AllowedBranchings = parent.AllowedBranchings;
  for (auto &idComp : idsComp) {
	 if (idComp < 0)
		throw ZEROException(ZEROErrorCode::Assertion, "idComp is negative");
	 this->Encoding.at(idComp)          = true;
	 this->AllowedBranchings.at(idComp) = false;
	 this->IdComps.push_back(idComp);
  }
  this->Id     = id;
  this->Parent = &parent;
}
