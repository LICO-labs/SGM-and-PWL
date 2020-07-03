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


#include "mathopt/mathopt.h"
#include <armadillo>
#include <boost/log/trivial.hpp>
#include <iostream>

unsigned int MathOpt::convexHull(
	 const std::vector<arma::sp_mat *> *Ai, ///< Inequality constraints LHS that define polyhedra
														 ///< whose convex hull is to be found
	 const std::vector<arma::vec *> *bi,    ///< Inequality constraints RHS that define
														 ///< polyhedra whose convex hull is to be found
	 arma::sp_mat &     A,                  ///< Pointer to store the output of the convex hull LHS
	 arma::vec &        b,                  ///< Pointer to store the output of the convex hull RHS
	 const arma::sp_mat Acom,               ///< any common constraints to all the polyhedra - lhs.
	 const arma::vec    bcom                ///< Any common constraints to ALL the polyhedra - RHS.
	 )
/** @brief Computing convex hull of finite union of polyhedra
 * @details Computes the convex hull of a finite union of polyhedra where
 * each polyhedra @f$P_i@f$ is of the form
 * @f{eqnarray}{
 * A^ix &\leq& b^i\\
 * x &\geq& 0
 * @f}
 * This uses Balas' approach to compute the convex hull.
 *
 * <b>Cross reference:</b> Conforti, Michele; Cornuéjols, Gérard; and Zambelli,
 * Giacomo. Integer programming. Vol. 271. Berlin: Springer, 2014. Refer:
 * Eqn 4.31
 */
{
  // Count number of polyhedra and the space we are in!
  const unsigned int nPoly{static_cast<unsigned int>(Ai->size())};
  // Error check
  if (nPoly == 0)
	 throw ZEROException(ZEROErrorCode::Assertion, "There are no polyhedra");
  // consider
  const unsigned int nC{static_cast<unsigned int>(Ai->front()->n_cols)};
  const unsigned int nComm{static_cast<unsigned int>(Acom.n_rows)};

  if (nComm > 0 && Acom.n_cols != nC)
	 throw ZEROException(ZEROErrorCode::Assertion, "Inconsistend number of variables");
  if (nComm > 0 && nComm != bcom.n_rows)
	 throw ZEROException(ZEROErrorCode::Assertion, "Inconsistent number of rows");

  // Count the number of variables in the convex hull.
  unsigned int nFinCons{0}, nFinVar{0};
  if (nPoly != bi->size())
	 throw ZEROException(ZEROErrorCode::Assertion, "Inconsistent number of rows in the polyhedron");
  for (unsigned int i = 0; i != nPoly; i++) {
	 if (Ai->at(i)->n_cols != nC)
		throw ZEROException(ZEROErrorCode::Assertion,
								  "Inconsistent number of variables: " + std::to_string(i) + "; " +
										std::to_string(Ai->at(i)->n_cols) + "!=" + std::to_string(nC));
	 if (Ai->at(i)->n_rows != bi->at(i)->n_rows)
		throw ZEROException(ZEROErrorCode::Assertion,
								  "Inconsistent number of rows: " + std::to_string(i) + ";" +
										std::to_string(Ai->at(i)->n_rows) +
										"!=" + std::to_string(bi->at(i)->n_rows));
	 nFinCons += Ai->at(i)->n_rows;
  }
  // For common constraint copy
  nFinCons += nPoly * nComm;

  const unsigned int FirstCons = nFinCons;

  // 2nd constraint in Eqn 4.31 of Conforti - twice so we have 2 ineq instead of
  // 1 eq constr
  nFinCons += nC * 2;
  // 3rd constr in Eqn 4.31. Again as two ineq constr.
  nFinCons += 2;
  // Common constraints
  // nFinCons += Acom.n_rows;

  nFinVar = nPoly * nC + nPoly + nC; // All x^i variables + delta variables+ original x variables
  A.zeros(nFinCons, nFinVar);
  b.zeros(nFinCons);
  // A.zeros(nFinCons, nFinVar); b.zeros(nFinCons);
  // Implements the first constraint more efficiently using better constructors
  // for sparse matrix
  MathOpt::compConvSize(A, nFinCons, nFinVar, Ai, bi, Acom, bcom);

  // Counting rows completed
  /****************** SLOW LOOP BEWARE *******************/
  for (unsigned int i = 0; i < nPoly; i++) {
	 BOOST_LOG_TRIVIAL(trace) << "MathOpt::convexHull: Handling Polyhedron " << i + 1 << " out of "
									  << nPoly;
	 // First constraint in (4.31)
	 // A.submat(complRow, i*nC, complRow+nConsInPoly-1, (i+1)*nC-1) =
	 // *Ai->at(i); // Slowest line. Will arma improve this? First constraint RHS
	 // A.submat(complRow, nPoly*nC+i, complRow+nConsInPoly-1, nPoly*nC+i) =
	 // -*bi->at(i); Second constraint in (4.31)
	 for (unsigned int j = 0; j < nC; j++) {
		A.at(FirstCons + 2 * j, nC + (i * nC) + j)     = 1;
		A.at(FirstCons + 2 * j + 1, nC + (i * nC) + j) = -1;
	 }
	 // Third constraint in (4.31)
	 A.at(FirstCons + nC * 2, nC + nPoly * nC + i)     = 1;
	 A.at(FirstCons + nC * 2 + 1, nC + nPoly * nC + i) = -1;
  }
  /****************** SLOW LOOP BEWARE *******************/
  // Second Constraint RHS
  for (unsigned int j = 0; j < nC; j++) {
	 A.at(FirstCons + 2 * j, j)     = -1;
	 A.at(FirstCons + 2 * j + 1, j) = 1;
  }
  // Third Constraint RHS
  b.at(FirstCons + nC * 2)     = 1;
  b.at(FirstCons + nC * 2 + 1) = -1;
  return nPoly; ///< Perform increasingly better inner approximations in
					 ///< iterations
}

void MathOpt::compConvSize(
	 arma::sp_mat &                     A,        ///< Output parameter
	 const unsigned int                 nFinCons, ///< Number of rows in final matrix A
	 const unsigned int                 nFinVar,  ///< Number of columns in the final matrix A
	 const std::vector<arma::sp_mat *> *Ai, ///< Inequality constraints LHS that define polyhedra
														 ///< whose convex hull is to be found
	 const std::vector<arma::vec *> *bi,    ///< Inequality constraints RHS that define
														 ///< polyhedra whose convex hull is to be found
	 const arma::sp_mat &Acom,              ///< LHS of the common constraints for all polyhedra
	 const arma::vec &   bcom               ///< RHS of the common constraints for all polyhedra
	 )
/**
 * @brief INTERNAL FUNCTION NOT FOR GENERAL USE.
 * @warning INTERNAL FUNCTION NOT FOR GENERAL USE.
 * @internal To generate the matrix "A" in MathOpt::convexHull using batch
 * insertion constructors. This is faster than the original line in the code:
 * A.submat(complRow, i*nC, complRow+nConsInPoly-1, (i+1)*nC-1) = *Ai->at(i);
 * Motivation behind this: Response from
 * armadillo:-https://gitlab.com/conradsnicta/armadillo-code/issues/111
 */
{
  const unsigned int nPoly{static_cast<unsigned int>(Ai->size())};
  const unsigned int nC{static_cast<unsigned int>(Ai->front()->n_cols)};
  unsigned int       N{0}; // Total number of nonzero elements in the final matrix
  const unsigned int numCommon{static_cast<unsigned int>(Acom.n_nonzero + bcom.n_rows)};
  for (unsigned int i = 0; i < nPoly; i++) {
	 N += Ai->at(i)->n_nonzero;
	 N += bi->at(i)->n_rows;
  }
  N += numCommon * nPoly; // The common constraints have to be copied for each polyhedron.

  // Now computed N which is the total number of nonzeros.
  arma::umat locations; // location of nonzeros
  arma::vec  val;       // nonzero values
  locations.zeros(2, N);
  val.zeros(N);

  unsigned int count{0}, rowCount{0}, colCount{nC};
  for (unsigned int i = 0; i < nPoly; i++) {
	 for (auto it = Ai->at(i)->begin(); it != Ai->at(i)->end(); ++it) // First constraint
	 {
		locations(0, count) = rowCount + it.row();
		locations(1, count) = colCount + it.col();
		val(count)          = *it;
		++count;
	 }
	 for (unsigned int j = 0; j < bi->at(i)->n_rows; ++j) // RHS of first constraint
	 {
		locations(0, count) = rowCount + j;
		locations(1, count) = nC + nC * nPoly + i;
		val(count)          = -bi->at(i)->at(j);
		++count;
	 }
	 rowCount += Ai->at(i)->n_rows;

	 // For common constraints
	 for (auto it = Acom.begin(); it != Acom.end(); ++it) // First constraint
	 {
		locations(0, count) = rowCount + it.row();
		locations(1, count) = colCount + it.col();
		val(count)          = *it;
		++count;
	 }
	 for (unsigned int j = 0; j < bcom.n_rows; ++j) // RHS of first constraint
	 {
		locations(0, count) = rowCount + j;
		locations(1, count) = nC + nC * nPoly + i;
		val(count)          = -bcom.at(j);
		++count;
	 }
	 rowCount += Acom.n_rows;

	 colCount += nC;
  }
  A = arma::sp_mat(locations, val, nFinCons, nFinVar);
}

arma::vec MathOpt::LPSolve(const arma::sp_mat &A, ///< The constraint matrix
									const arma::vec &   b, ///< RHS of the constraint matrix
									const arma::vec &   c, ///< If feasible, returns a std::vector that
																  ///< minimizes along this direction
									int &status,    ///< Status of the optimization problem. If optimal,
														 ///< this will be GRB_OPTIMAL
									bool positivity ///< Should @f$x\geq0@f$ be enforced?
									)
/**
 Checks if the polyhedron given by @f$ Ax\leq b@f$ is feasible.
 If yes, returns the point @f$x@f$ in the polyhedron that minimizes @f$c^Tx@f$
 positivity can be enforced on the variables easily.
*/
{
  unsigned int nR, nC;
  nR = A.n_rows;
  nC = A.n_cols;
  if (c.n_rows != nC)
	 throw ZEROException(ZEROErrorCode::Assertion, "Inconsistent number of variables");
  if (b.n_rows != nR)
	 throw ZEROException(ZEROErrorCode::Assertion, "Inconsistent number of constraints");

  arma::vec    sol = arma::vec(c.n_rows, arma::fill::zeros);
  const double lb  = positivity ? 0 : -GRB_INFINITY;

  GRBEnv    env;
  GRBModel  model = GRBModel(env);
  GRBVar    x[nC];
  GRBConstr a[nR];
  // Adding Variables
  for (unsigned int i = 0; i < nC; i++)
	 x[i] = model.addVar(lb, GRB_INFINITY, c.at(i), GRB_CONTINUOUS, "x_" + std::to_string(i));
  // Adding constraints
  for (unsigned int i = 0; i < nR; i++) {
	 GRBLinExpr lin{0};
	 for (auto j = A.begin_row(i); j != A.end_row(i); ++j)
		lin += (*j) * x[j.col()];
	 a[i] = model.addConstr(lin, GRB_LESS_EQUAL, b.at(i));
  }
  model.set(GRB_IntParam_OutputFlag, 0);
  model.set(GRB_IntParam_DualReductions, 0);
  model.optimize();
  status = model.get(GRB_IntAttr_Status);
  if (status == GRB_OPTIMAL)
	 for (unsigned int i = 0; i < nC; i++)
		sol.at(i) = x[i].get(GRB_DoubleAttr_X);
  return sol;
}

void MathOpt::print(const perps &C) noexcept {
  for (auto p : C)
	 std::cout << "<" << p.first << ", " << p.second << ">"
				  << "\t";
}