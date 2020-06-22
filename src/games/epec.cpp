#include "games/epec.h"
#include "algorithms/algorithms.h"
#include "algorithms/combinatorialpne.h"
#include "algorithms/fullenumeration.h"
#include "algorithms/innerapproximation.h"
#include "algorithms/outerapproximation.h"
#include <algorithm>
#include <armadillo>
#include <array>
#include <boost/log/trivial.hpp>
#include <iostream>
#include <memory>

void Game::EPEC::preFinalize()
/**
  @brief Empty function - optionally reimplementable in derived class
@details This function can be optionally implemented by
 the derived class. Code in this class will be run <i>before</i>
 calling Game::EPEC::finalize().
*/
{}

void Game::EPEC::postFinalize()
/**
  @brief Empty function - optionally reimplementable in derived class
@details This function can be optionally implemented by
 the derived class. Code in this class will be run <i>after</i>
 calling Game::EPEC::finalize().
*/
{}

void Game::EPEC::finalize()
/**
 * @brief Finalizes the creation of a Game::EPEC object.
 * @details Performs a bunch of job after all data for a Game::EPEC object are
 * given, namely.
 * Models::EPEC::computeLeaderLocations -	Adds the required dummy
 * variables to each leader's problem so that a game among the leaders can be
 * defined. Calls Game::EPEC::addDummyLead
 * 	-	Makes the market clearing constraint in each country. Calls
 */
{
  if (this->Finalized)
    std::cerr << "Warning in Game::EPEC::finalize: Model already Finalized\n";

  this->NumPlayers = this->getNumLeaders();
  /// Game::EPEC::preFinalize() can be overridden, and that code will run before
  /// calling Game::EPEC::finalize()
  this->preFinalize();

  try {
    this->ConvexHullVariables = std::vector<unsigned int>(this->NumPlayers, 0);
    this->Stats.FeasiblePolyhedra =
        std::vector<unsigned int>(this->NumPlayers, 0);
    this->computeLeaderLocations(this->numMCVariables);
    // Initialize leader objective and PlayersQP
    this->LeaderObjective =
        std::vector<std::shared_ptr<Game::QP_Objective>>(NumPlayers);
    this->LeaderObjectiveConvexHull =
        std::vector<std::shared_ptr<Game::QP_Objective>>(NumPlayers);
    this->PlayersQP = std::vector<std::shared_ptr<Game::QP_Param>>(NumPlayers);
    this->PlayersLCP = std::vector<std::shared_ptr<Game::LCP>>(NumPlayers);
    this->SizesWithoutHull = std::vector<unsigned int>(NumPlayers, 0);

    for (unsigned int i = 0; i < this->NumPlayers; i++) {
      this->addDummyLead(i);
      this->LeaderObjective.at(i) = std::make_shared<Game::QP_Objective>();
      this->LeaderObjectiveConvexHull.at(i) =
          std::make_shared<Game::QP_Objective>();
      this->makeObjectivePlayer(i, *this->LeaderObjective.at(i).get());
      // this->PlayersLCP.at(i) =std::shared_ptr<Game::PolyLCP>(new
      // PolyLCP(this->Env,*this->PlayersLowerLevels.at(i).get()));
      this->SizesWithoutHull.at(i) = *this->LocEnds.at(i);
    }

  } catch (const char *e) {
    std::cerr << e << '\n';
    throw;
  } catch (std::string &e) {
    std::cerr << "String in Game::EPEC::finalize : " << e << '\n';
    throw;
  } catch (GRBException &e) {
    std::cerr << "GRBException in Game::EPEC::finalize : " << e.getErrorCode()
              << ": " << e.getMessage() << '\n';
    throw;
  } catch (std::exception &e) {
    std::cerr << "Exception in Game::EPEC::finalize : " << e.what() << '\n';
    throw;
  }

  this->Finalized = true;

  /// Game::EPEC::postFinalize() can be overridden, and that code will run after
  /// calling Game::EPEC::finalize()
  this->postFinalize();
}

void Game::EPEC::addDummyLead(
    const unsigned int i ///< The leader to whom dummy variables should be added
) {
  /// Adds dummy variables to the leader of an EPEC - useful after computing the
  /// convex hull.
  const unsigned int nEPECvars = this->NumVariables;
  const unsigned int nThisCountryvars = *this->LocEnds.at(i);
  // this->Locations.at(i).at(Models::LeaderVars::End);

  if (nEPECvars < nThisCountryvars)
    throw("String in Game::EPEC::addDummyLead: Invalid variable counts " +
          std::to_string(nEPECvars) + " and " +
          std::to_string(nThisCountryvars));

  try {
    this->PlayersLowerLevels.at(i).get()->addDummy(nEPECvars -
                                                   nThisCountryvars);
  } catch (const char *e) {
    std::cerr << e << '\n';
    throw;
  } catch (std::string &e) {
    std::cerr << "String in Game::EPEC::add_Dummy_All_Lead : " << e << '\n';
    throw;
  } catch (GRBException &e) {
    std::cerr << "GRBException in Game::EPEC::add_Dummy_All_Lead : "
              << e.getErrorCode() << ": " << e.getMessage() << '\n';
    throw;
  } catch (std::exception &e) {
    std::cerr << "Exception in Game::EPEC::add_Dummy_All_Lead : " << e.what()
              << '\n';
    throw;
  }
}

void Game::EPEC::computeLeaderLocations(const unsigned int addSpaceForMC) {
  this->LeaderLocations = std::vector<unsigned int>(this->NumPlayers);
  this->LeaderLocations.at(0) = 0;
  for (unsigned int i = 1; i < this->NumPlayers; i++) {
    this->LeaderLocations.at(i) =
        this->LeaderLocations.at(i - 1) + *this->LocEnds.at(i - 1);
  }
  this->NumVariables =
      this->LeaderLocations.back() + *this->LocEnds.back() + addSpaceForMC;
}

void Game::EPEC::getXMinusI(const arma::vec &x, const unsigned int &i,
                            arma::vec &solOther) const {
  const unsigned int nEPECvars = this->NumVariables;
  const unsigned int nThisCountryvars = *this->LocEnds.at(i);
  const unsigned int nThisCountryHullVars = this->ConvexHullVariables.at(i);
  const unsigned int nConvexHullVars = static_cast<const unsigned int>(
      std::accumulate(this->ConvexHullVariables.rbegin(),
                      this->ConvexHullVariables.rend(), 0));

  solOther.zeros(nEPECvars -        // All variables in EPEC
                 nThisCountryvars - // Subtracting this country's variables,
                 // since we only want others'
                 nConvexHullVars + // We don't want any convex hull variables
                 nThisCountryHullVars); // We subtract the hull variables
  // associated to the ith player
  // convex hull vars, since we double subtracted

  for (unsigned int j = 0, count = 0, current = 0; j < this->NumPlayers; ++j) {
    if (i != j) {
      current = *this->LocEnds.at(j) - this->ConvexHullVariables.at(j);
      solOther.subvec(count, count + current - 1) =
          x.subvec(this->LeaderLocations.at(j),
                   this->LeaderLocations.at(j) + current - 1);
      count += current;
    }
  }
  // We need to keep track of MC_vars also for this country
  for (unsigned int j = 0; j < this->numMCVariables; j++)
    solOther.at(solOther.n_rows - this->numMCVariables + j) =
        x.at(this->NumVariables - this->numMCVariables + j);
}

void Game::EPEC::getXofI(const arma::vec &x, const unsigned int &i,
                         arma::vec &solI, bool hull) const {
  /**
   * Given the player id @p i and the solution @p x, the method returns in @p
   * xWithoutHull the x vector for the given player, with the convex-hull's
   * variables in case @p hull is false. Also, no MC variables are included
   */
  const unsigned int nThisCountryvars = *this->LocEnds.at(i);
  const unsigned int nThisCountryHullVars = this->ConvexHullVariables.at(i);

  unsigned int vars = 0, current = 0;
  if (hull) {
    vars = nThisCountryvars;
    current = *this->LocEnds.at(i);
  } else {
    vars = nThisCountryvars - nThisCountryHullVars;
    current = *this->LocEnds.at(i) - this->ConvexHullVariables.at(i);
  }
  solI.zeros(vars);
  solI.subvec(0, vars - 1) = x.subvec(
      this->LeaderLocations.at(i), this->LeaderLocations.at(i) + current - 1);
}

void Game::EPEC::getXWithoutHull(const arma::vec &x,
                                 arma::vec &xWithoutHull) const {
  /**
   * Given the the solution @p x, the method returns in @p
   * xWithoutHull the x vector without the convex-hull's
   * variables.  Also, no MC variables are included
   *
   */
  const unsigned int nEPECvars = this->NumVariables;
  const unsigned int nConvexHullVars = static_cast<const unsigned int>(
      std::accumulate(this->ConvexHullVariables.rbegin(),
                      this->ConvexHullVariables.rend(), 0));

  xWithoutHull.zeros(nEPECvars -       // All variables in EPEC
                     nConvexHullVars); // We subtract the hull variables
  // associated to the convex hull
  // convex hull vars

  for (unsigned int j = 0, count = 0, current = 0; j < this->NumPlayers; ++j) {
    current = *this->LocEnds.at(j) - this->ConvexHullVariables.at(j);
    xWithoutHull.subvec(count, count + current - 1) = x.subvec(
        this->LeaderLocations.at(j), this->LeaderLocations.at(j) + current - 1);
    count += current;
  }
}

std::unique_ptr<GRBModel> Game::EPEC::respond(const unsigned int i,
                                              const arma::vec &x) const {
  if (!this->Finalized)
    throw("Error in Game::EPEC::respond: Model not Finalized");

  if (i >= this->NumPlayers)
    throw("Error in Game::EPEC::respond: Invalid country number");

  arma::vec solOther;
  this->getXMinusI(x, i, solOther);
  if (this->LeaderObjective.at(i)->Q.n_nonzero > 0)
    return this->PlayersLCP.at(i).get()->MPECasMIQP(
        this->LeaderObjective.at(i)->Q, this->LeaderObjective.at(i)->C,
        this->LeaderObjective.at(i)->c, solOther, true);
  else
    return this->PlayersLCP.at(i).get()->MPECasMILP(
        this->LeaderObjective.at(i)->C, this->LeaderObjective.at(i)->c,
        solOther, true);
}

double Game::EPEC::respondSol(
    arma::vec &sol,      ///< [out] Optimal response
    unsigned int player, ///< Player whose optimal response is to be computed
    const arma::vec &x,  ///< A std::vector of pure strategies (either for all
    ///< players or all other players
    const arma::vec &prevDev
    //< [in] if any, the std::vector of previous deviations.
) const {
  /**
   * @brief Returns the optimal objective value that is obtainable for the
   * player @p player given the decision @p x of all other players.
   * @details
   * Calls Game::EPEC::respond and obtains the std::unique_ptr to GRBModel of
   * best response by player @p player. Then solves the model and returns the
   * appropriate objective value.
   * @returns The optimal objective value for the player @p player.
   */
  auto model = this->respond(player, x);
  BOOST_LOG_TRIVIAL(trace) << "Game::EPEC::respondSol: Writing dat/RespondSol" +
                              std::to_string(player) + ".lp to disk";
  model->write("dat/RespondSol" + std::to_string(player) + ".lp");
  const int status = model->get(GRB_IntAttr_Status);
  if (status == GRB_UNBOUNDED || status == GRB_OPTIMAL) {
    unsigned int Nx = this->PlayersLCP.at(player)->getNumCols();
    sol.zeros(Nx);
    for (unsigned int i = 0; i < Nx; ++i)
      sol.at(i) =
          model->getVarByName("x_" + std::to_string(i)).get(GRB_DoubleAttr_X);

    if (status == GRB_UNBOUNDED) {
      BOOST_LOG_TRIVIAL(warning) << "Game::EPEC::respondSol: deviation is "
                                    "unbounded.";
      GRBLinExpr obj = 0;
      model->setObjective(obj);
      model->optimize();
      if (!prevDev.empty()) {
        BOOST_LOG_TRIVIAL(trace)
            << "Generating an improvement basing on the extreme ray.";
        // Fetch objective function coefficients
        GRBQuadExpr QuadObj = model->getObjective();
        arma::vec objcoeff;
        for (unsigned int i = 0; i < QuadObj.size(); ++i)
          objcoeff.at(i) = QuadObj.getCoeff(i);

        // Create objective function objects
        arma::vec objvalue = prevDev * objcoeff;
        arma::vec newobjvalue{0};
        bool improved{false};

        // improve following the unbounded ray
        while (!improved) {
          for (unsigned int i = 0; i < Nx; ++i)
            sol.at(i) =
                sol.at(i) + model->getVarByName("x_" + std::to_string(i))
                    .get(GRB_DoubleAttr_UnbdRay);
          newobjvalue = sol * objcoeff;
          if (newobjvalue.at(0) < objvalue.at(0))
            improved = true;
        }
        return newobjvalue.at(0);

      } else {
        return model->get(GRB_DoubleAttr_ObjVal);
      }
    }
    if (status == GRB_OPTIMAL) {
      return model->get(GRB_DoubleAttr_ObjVal);
    }
  } else {
    return GRB_INFINITY;
  }
  return GRB_INFINITY;
}

const void Game::EPEC::makePlayerQP(const unsigned int i)
/**
 * @brief Makes the Game::QP_Param corresponding to the @p i-th country.
 * @details
 *  - First gets the Game::LCP object from @p Game::EPEC::PlayersLowerLevels and
 * makes a Game::QP_Param with this LCP as the lower level
 *  - This is achieved by calling LCP::makeQP and using the objective value
 * object in @p Game::EPEC::LeaderObjective
 *  - Finally the locations are updated owing to the complete convex hull
 * calculated during the call to LCP::makeQP
 * @note Overloaded as Models::EPEC::makePlayerQP()
 */
{
  // BOOST_LOG_TRIVIAL(info) << "Starting Convex hull computation of the country
  // "
  // << this->AllLeadPars[i].name << '\n';
  if (!this->Finalized)
    throw("Error in Game::EPEC::makePlayerQP: Model not Finalized");
  if (i >= this->NumPlayers)
    throw("Error in Game::EPEC::makePlayerQP: Invalid country number");
  // if (!this->PlayersQP.at(i).get())
  {
    this->PlayersQP.at(i) = std::make_shared<Game::QP_Param>(this->Env);
    const auto &origLeadObjec = *this->LeaderObjective.at(i).get();

    this->LeaderObjectiveConvexHull.at(i).reset(new Game::QP_Objective{
        origLeadObjec.Q, origLeadObjec.C, origLeadObjec.c});
    this->PlayersLCP.at(i)->makeQP(*this->LeaderObjectiveConvexHull.at(i).get(),
                                   *this->PlayersQP.at(i).get());
  }
}

void Game::EPEC::makePlayersQPs()
/**
 * @brief Makes the Game::QP_Param for all the countries
 * @details
 * Calls are made to Models::EPEC::makePlayerQP(const unsigned int i) for
 * each valid @p i
 * @note Overloaded as EPEC::makePlayerQP(unsigned int)
 */
{
  for (unsigned int i = 0; i < this->NumPlayers; ++i) {
    this->Game::EPEC::makePlayerQP(i);
  }
  for (unsigned int i = 0; i < this->NumPlayers; ++i) {
    // LeadLocs &Loc = this->Locations.at(i);
    // Adjusting "stuff" because we now have new convHull variables
    unsigned int originalSizeWithoutHull =
        this->LeaderObjective.at(i)->Q.n_rows;
    unsigned int convHullVarCount =
        this->LeaderObjectiveConvexHull.at(i)->Q.n_rows -
        originalSizeWithoutHull;

    BOOST_LOG_TRIVIAL(trace)
        << "Game::EPEC::makePlayerQP: Added " << convHullVarCount
        << " convex hull variables to QP #" << i;

    // Location details
    this->ConvexHullVariables.at(i) = convHullVarCount;
    // All other players' QP
    try {
      if (this->NumPlayers > 1) {
        for (unsigned int j = 0; j < this->NumPlayers; j++) {
          if (i != j) {
            this->PlayersQP.at(j)->addDummy(
                convHullVarCount, 0,
                this->PlayersQP.at(j)->getNx() -
                this->numMCVariables); // The position to add parameters is
            // towards the end of all parameters,
            // giving space only for the
            // numMCVariables number of market
            // clearing variables
          }
        }
      }
    } catch (const char *e) {
      std::cerr << e << '\n';
      throw;
    } catch (std::string &e) {
      std::cerr << "String in Game::EPEC::makePlayerQP : " << e << '\n';
      throw;
    } catch (GRBException &e) {
      std::cerr << "GRBException in Game::EPEC::makePlayerQP : "
                << e.getErrorCode() << ": " << e.getMessage() << '\n';
      throw;
    } catch (std::exception &e) {
      std::cerr << "Exception in Game::EPEC::makePlayerQP : " << e.what()
                << '\n';
      throw;
    }
  }
  this->updateLocations();
  this->computeLeaderLocations(this->numMCVariables);
}

void ::Game::EPEC::makeTheLCP() {
  if (this->PlayersQP.front() == nullptr) {
    BOOST_LOG_TRIVIAL(error) << "Exception in Game::EPEC::makeTheLCP : "
                                "no country QP has been "
                                "made."
                             << '\n';
    throw;
  }
  // Preliminary set up to get the LCP ready
  int Nvar =
      this->PlayersQP.front()->getNx() + this->PlayersQP.front()->getNy();
  arma::sp_mat MC(0, Nvar), dumA(0, Nvar);
  arma::vec MCRHS, dumb;
  MCRHS.zeros(0);
  dumb.zeros(0);
  this->makeMCConstraints(MC, MCRHS);
  BOOST_LOG_TRIVIAL(trace) << "Game::EPEC::makeTheLCP(): Market Clearing "
                              "constraints are ready";
  this->TheNashGame = std::unique_ptr<Game::NashGame>(
      new Game::NashGame(this->Env, this->PlayersQP, MC, MCRHS, 0, dumA, dumb));
  BOOST_LOG_TRIVIAL(trace) << "Game::EPEC::makeTheLCP(): NashGame is ready";
  this->TheLCP =
      std::unique_ptr<Game::LCP>(new Game::LCP(this->Env, *TheNashGame));
  BOOST_LOG_TRIVIAL(trace) << "Game::EPEC::makeTheLCP(): LCP is ready";
  BOOST_LOG_TRIVIAL(trace) << "Game::EPEC::makeTheLCP(): Indicators set to "
                           << this->Stats.AlgorithmParam.Indicators;
  this->TheLCP->UseIndicators =
      this->Stats.AlgorithmParam.Indicators; // Using indicator constraints

  this->LCPModel = this->TheLCP->LCPasMIP(false);
  //this->LCPModel->setObjective(GRBLinExpr{0}, GRB_MINIMIZE);

  BOOST_LOG_TRIVIAL(trace) << *TheNashGame;
}

bool Game::EPEC::computeNashEq(
    bool pureNE,           ///< True if we search for a PNE
    double localTimeLimit, ///< Allowed time limit to run this function
    bool check ///< If true, the Algorithm will seek for the maximum number of
    ///< NE. Then, it will check they are equilibria for the original
    ///< problem
) {
  /**
   * Given that Game::EPEC::PlayersQP are all filled with a each country's
   * Game::QP_Param problem (either exact or approximate), computes the Nash
   * equilibrium.
   * @returns true if a Nash equilibrium is found
   */
  // Make the Nash Game between countries
  this->NashEquilibrium = false;
  BOOST_LOG_TRIVIAL(trace)
      << " Game::EPEC::computeNashEq: Making the Master LCP";
  this->makeTheLCP();
  BOOST_LOG_TRIVIAL(trace) << " Game::EPEC::computeNashEq: Made the Master LCP";
  if (localTimeLimit > 0) {
    this->LCPModel->set(GRB_DoubleParam_TimeLimit, localTimeLimit);
  }
  if (this->Stats.AlgorithmParam.BoundPrimals) {
    for (unsigned int c = 0; c < this->TheNashGame->getNprimals(); c++) {
      this->LCPModel->getVarByName("x_" + std::to_string(c))
          .set(GRB_DoubleAttr_UB, this->Stats.AlgorithmParam.BoundBigM);
    }
  }

  if (pureNE) {
    BOOST_LOG_TRIVIAL(info)
        << " Game::EPEC::computeNashEq: (PureNashEquilibrium flag is "
           "true) Searching for a pure NE.";
    if (this->Stats.AlgorithmParam.PolyLcp)
      dynamic_cast<Algorithms::PolyBase *>(this->Algorithm.get())
          ->makeThePureLCP(this->Stats.AlgorithmParam.Indicators);
  }

  this->LCPModel->set(GRB_IntParam_OutputFlag, 1);
  if (check)
    this->LCPModel->set(GRB_IntParam_SolutionLimit, GRB_MAXINT);
  this->LCPModel->optimize();
  this->Stats.WallClockTime += this->LCPModel->get(GRB_DoubleAttr_Runtime);

  // Search just for a feasible point
  try { // Try finding a Nash equilibrium for the approximation
    this->NashEquilibrium = this->TheLCP->extractSols(
        this->LCPModel.get(), SolutionZ, SolutionX, true);
  } catch (GRBException &e) {
    BOOST_LOG_TRIVIAL(error)
        << "GRBException in Game::EPEC::computeNashEq : " << e.getErrorCode()
        << ": " << e.getMessage() << " ";
  }
  if (this->NashEquilibrium) { // If a Nash equilibrium is found, then update
    // appropriately
    if (check) {
      int scount = this->LCPModel->get(GRB_IntAttr_SolCount);
      BOOST_LOG_TRIVIAL(info)
          << "Game::EPEC::computeNashEq: number of equilibria is " << scount;
      for (int k = 0, stop = 0; k < scount && stop == 0; ++k) {
        this->LCPModel->getEnv().set(GRB_IntParam_SolutionNumber, k);
        this->NashEquilibrium = this->TheLCP->extractSols(
            this->LCPModel.get(), this->SolutionZ, this->SolutionX, true);
        if (this->Algorithm->isSolved()) {
          BOOST_LOG_TRIVIAL(info)
              << "Game::EPEC::computeNashEq: an Equilibrium has been found";
          stop = 1;
        }
      }
    } else {
      this->NashEquilibrium = true;
      this->SolutionX.save("dat/X.dat", arma::file_type::arma_ascii);
      this->SolutionZ.save("dat/Z.dat", arma::file_type::arma_ascii);
      BOOST_LOG_TRIVIAL(info)
          << "Game::EPEC::computeNashEq: an Equilibrium has been found";
    }

  } else { // If not, then update accordingly
    BOOST_LOG_TRIVIAL(info)
        << "Game::EPEC::computeNashEq: no equilibrium has been found.";
    int status = this->LCPModel->get(GRB_IntAttr_Status);
    if (status == GRB_TIME_LIMIT)
      this->Stats.Status = Game::EPECsolveStatus::TimeLimit;
    else
      this->Stats.Status = Game::EPECsolveStatus::NashEqNotFound;
  }
  return this->NashEquilibrium;
}

bool Game::EPEC::warmstart(const arma::vec x) { //@todo complete implementation

  if (x.size() < this->getNumVar()) {
    BOOST_LOG_TRIVIAL(error)
        << "Exception in Game::EPEC::warmstart: number of variables "
           "does not fit this instance.";
    throw;
  }
  if (!this->Finalized) {
    BOOST_LOG_TRIVIAL(error)
        << "Exception in Game::EPEC::warmstart: EPEC is not Finalized.";
    throw;
  }
  if (this->PlayersQP.front() == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
        << "Game::EPEC::warmstart: Generating QP as of warmstart.";
  }

  this->SolutionX = x;
  std::vector<arma::vec> devns = std::vector<arma::vec>(this->NumPlayers);
  std::vector<arma::vec> prevDevns = std::vector<arma::vec>(this->NumPlayers);
  this->makePlayersQPs();

  arma::vec devn;

  if (this->Algorithm->isSolved())
    BOOST_LOG_TRIVIAL(warning) << "Game::EPEC::warmstart: "
                                  "The loaded solution is optimal.";
  else
    BOOST_LOG_TRIVIAL(warning)
        << "Game::EPEC::warmstart: "
           "The loaded solution is NOT optimal. Trying to repair.";
  /// @todo Game::EPEC::warmstart - to complete implementation?
  return true;
}
bool Game::EPEC::isPureStrategy(double tol) const {
  /**
   * @brief Call the delegated Algorithm and return true if the equilibrium is
   * pure
   */
  return this->Algorithm->isPureStrategy(tol);
}
bool Game::EPEC::isSolved(double tol) const {
  /**
   * @brief Call the delegated Algorithm and return true if the EPEC has been
   * solved.
   */
  return this->Algorithm->isSolved(tol);
}

const void Game::EPEC::findNashEq() {
  /**
   * @brief Computes Nash equilibrium using the Algorithm set in
   * Game::EPEC::Algorithm
   * @details
   * Checks the value of Game::EPEC::Algorithm and delegates the task to
   * appropriate Algorithm wrappers.
   */

  std::stringstream final_msg;
  if (!this->Finalized)
    throw("Error in Game::EPEC::iterativeNash: Object not yet "
          "Finalized. ");

  if (this->Stats.Status != Game::EPECsolveStatus::Uninitialized) {
    BOOST_LOG_TRIVIAL(error)
        << "Game::EPEC::findNashEq: a Nash Eq was "
           "already found. Calling this findNashEq might lead to errors!";
  }

  // Choosing the appropriate algorithm
  switch (this->Stats.AlgorithmParam.Algorithm) {

  case Game::EPECalgorithm::InnerApproximation: {
    final_msg << "Inner approximation Algorithm completed. ";
    this->Algorithm = std::shared_ptr<Algorithms::Algorithm>(
        new class Algorithms::InnerApproximation(this->Env, this));
    this->Algorithm->solve();
  } break;

  case Game::EPECalgorithm::CombinatorialPne: {
    final_msg << "CombinatorialPNE Algorithm completed. ";
    this->Algorithm = std::shared_ptr<Algorithms::Algorithm>(
        new class Algorithms::CombinatorialPNE(this->Env, this));
    this->Algorithm->solve();
  } break;

  case Game::EPECalgorithm::OuterApproximation: {
    final_msg << "Outer approximation Algorithm completed. ";
    this->Algorithm = std::shared_ptr<Algorithms::Algorithm>(
        new class Algorithms::OuterApproximation(this->Env, this));
    this->Algorithm->solve();
  } break;

  case Game::EPECalgorithm::FullEnumeration: {
    final_msg << "Full enumeration Algorithm completed. ";
    this->Algorithm = std::shared_ptr<Algorithms::Algorithm>(
        new class Algorithms::FullEnumeration(this->Env, this));
    this->Algorithm->solve();
  } break;
  }
  // Handing EPECStatistics object to track performance of algorithm
  if (this->LCPModel) {
    this->Stats.NumVar = this->LCPModel->get(GRB_IntAttr_NumVars);
    this->Stats.NumConstraints = this->LCPModel->get(GRB_IntAttr_NumConstrs);
    this->Stats.NumNonZero = this->LCPModel->get(GRB_IntAttr_NumNZs);
  } // Assigning appropriate Status messages after solving

  switch (this->Stats.Status) {
  case Game::EPECsolveStatus::NashEqNotFound:
    final_msg << "No Nash equilibrium exists.";
    break;
  case Game::EPECsolveStatus::NashEqFound: {
    final_msg << "Found a Nash equilibrium ("
              << (this->Stats.PureNashEquilibrium == 0 ? "MNE" : "PNE") << ").";
  } break;
  case Game::EPECsolveStatus::TimeLimit:
    final_msg << "Nash equilibrium not found. Time limit attained";
    break;
  case Game::EPECsolveStatus::Numerical:
    final_msg << "Nash equilibrium not found. Numerical issues might affect "
                 "this result.";
    break;
  default:
    final_msg << "Nash equilibrium not found. Time limit attained";
    break;
  }
  BOOST_LOG_TRIVIAL(info) << "Game::EPEC::findNashEq: " << final_msg.str();
}

void Game::EPEC::setAlgorithm(Game::EPECalgorithm algorithm)
/**
 * Decides the Algorithm to be used for solving the given instance of the
 * problem. The choice of algorithms are documented in Game::EPECalgorithm
 */
{
  this->Stats.AlgorithmParam.Algorithm = algorithm;
}

void Game::EPEC::setRecoverStrategy(Game::EPECRecoverStrategy strategy)
/**
 * Decides the Algorithm to be used for recovering a PNE out of the
 * InnerApproximation procedure.
 */
{
  this->Stats.AlgorithmParam.RecoverStrategy = strategy;
}

unsigned int Game::EPEC::getPositionLeadFoll(const unsigned int i,
                                             const unsigned int j) const {
  /**
   * Get the position of the j-th variable in the i-th leader
   * Querying Game::EPEC::LCPModel for x[return-value] variable gives the
   * appropriate variable
   */
  const auto LeaderStart = this->TheNashGame->getPrimalLoc(i);
  return LeaderStart + j;
}

unsigned int Game::EPEC::getPositionLeadLead(const unsigned int i,
                                             const unsigned int j) const {
  /**
   * Get the position of the j-th Follower variable in the i-th leader
   * Querying Game::EPEC::LCPModel for x[return-value] variable gives the
   * appropriate variable
   */
  const auto LeaderStart = this->TheNashGame->getPrimalLoc(i);
  return LeaderStart + this->PlayersLCP.at(i)->getLStart() + j;
}

double Game::EPEC::getValLeadFoll(const unsigned int i,
                                  const unsigned int j) const {
  /**
   * Get the value of the j-th variable in i-th leader
   */
  if (!this->LCPModel)
    throw std::string("Error in Game::EPEC::getValLeadFoll: "
                      "Game::EPEC::LCPModel not made and solved");
  return this->LCPModel
      ->getVarByName("x_" + std::to_string(this->getPositionLeadFoll(i, j)))
      .get(GRB_DoubleAttr_X);
}

double Game::EPEC::getValLeadLead(const unsigned int i,
                                  const unsigned int j) const {
  /**
   * Get the value of the j-th non-follower variable in i-th leader
   */
  if (!this->LCPModel)
    throw std::string("Error in Game::EPEC::getValLeadLead: "
                      "Game::EPEC::LCPModel not made and solved");
  return this->LCPModel
      ->getVarByName("x_" + std::to_string(this->getPositionLeadLead(i, j)))
      .get(GRB_DoubleAttr_X);
}