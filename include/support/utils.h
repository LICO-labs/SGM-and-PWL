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


#pragma once

/**
 * @file src/utils.h Basic utilities
 */

#include "zero.h"
#include <armadillo>
#include <fstream>
#include <set>

namespace Utils {
  arma::sp_mat resizePatch(const arma::sp_mat &mat, const unsigned int nR, const unsigned int nC);

  arma::mat resizePatch(const arma::mat &mat, const unsigned int nR, const unsigned int nC);

  arma::vec resizePatch(const arma::vec &mat, const unsigned int nR);

  bool containsRow(const arma::sp_mat &A, const arma::vec &row, const double tol = 1e-6);
  bool containsElement(const arma::vec &b, const double &element, const double tol = 1e-6);
  bool containsConstraint(const arma::sp_mat &A,
								  const arma::vec &   b,
								  const arma::vec &   lhs,
								  const double &      rhs,
								  const double        tol = 1e-6);
  bool containsConstraint(const arma::sp_mat &A,
								  const arma::vec &   b,
								  const arma::sp_mat &lhs,
								  const double &      rhs,
								  const double        tol = 1e-6);

  // Saving and retrieving an arma::vec
  void appendSave(const arma::vec & matrix,
						const std::string out,
						const std::string header = "",
						bool              erase  = false);

  long int
  appendRead(arma::vec &matrix, const std::string in, long int pos, const std::string header = "");

  // Saving and retrieving an arma::sp_mat
  void appendSave(const arma::sp_mat &matrix,
						const std::string   out,
						const std::string   header = "",
						bool                erase  = false);

  long int appendRead(arma::sp_mat &    matrix,
							 const std::string in,
							 long int          pos,
							 const std::string header = "");

  // Saving and retrieving an std::vector<double>
  void appendSave(const std::vector<double> v,
						const std::string         out,
						const std::string         header = "",
						bool                      erase  = false);

  long int appendRead(std::vector<double> &v,
							 const std::string    in,
							 long int             pos,
							 const std::string    header = "");

  // Saving std::string
  void appendSave(const std::string v, const std::string out, bool erase = false);

  long int appendRead(std::string &v, const std::string in, long int pos);

  // Saving A long int
  void appendSave(const long int    v,
						const std::string out,
						const std::string header = "",
						bool              erase  = false);

  long int
  appendRead(long int &v, const std::string in, long int pos, const std::string header = "");

  // Saving A unsigned int
  void appendSave(const unsigned int v,
						const std::string  out,
						const std::string  header = "",
						bool               erase  = false);

  long int
  appendRead(unsigned int &v, const std::string in, long int pos, const std::string header = "");

  // Binary encoding functions for the LCP class
  unsigned long int vecToNum(std::vector<short int> binary);

  std::vector<short int> numToVec(unsigned long int number, const unsigned long nCompl);

  bool isZero(arma::mat M, double tol = 1e-6) noexcept;

  bool isZero(arma::sp_mat M, double tol = 1e-6) noexcept;

} // namespace Utils

// namespace Utils
