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
#include <exception>
#include <gurobi_c++.h>
#include <string>

enum class ZEROStatus {
  /**
	* Set of Status in which the solution Status for a game
	*/
  NashEqNotFound, ///< Instance proved to be infeasible.
  NashEqFound,    ///< Solution found for the instance.
  Solved,         ///< When no nash equilibrium is available, Solved replace NashEqFound
  NotSolved,      ///< When no nash equilibrium is available, NotSolved replaces
						///< NashEqNotFound
  TimeLimit,      ///< Time limit reached, nash equilibrium not found.
  Numerical,      ///< Numerical issues
  Uninitialized   ///< Not started to solve the problem.
};

template <typename T> class Attr {
private:
  T Object;

public:
  Attr(T value) : Object{value} {};
  T    get() const { return Object; }
  void set(const T &value) { Object = value; }
  // T &operator=(const T &a) { std::cerr << "Operation not allowed. Use set()";
  // } operator T() { std::cerr << "Operation not allowed. Use get()"; }
};

class ZEROAlgorithmData {
public:
  Attr<double> DeviationTolerance{
		51e-4}; ///< The numerical tolerance to check for existing deviations
  Attr<bool> IndicatorConstraints{
		true};                  ///< If this is set to false, any MIP that can be reformulated with
										///< indicator constraints will be reformulated with bigM-s
  Attr<double> TimeLimit{-1}; ///< The timelimit for the solving procedure.
  Attr<int>    Threads{0};    ///< The number of threads for the solving process
  Attr<bool>   PureNashEquilibrium{false}; ///< If true, the algorithm will specifically
														 ///< seek pure equilibria (if any)
  Attr<unsigned long int> RandomSeed{42};  ///< Random seed for randomic operations
};

template <typename DataObjectType> struct ZEROStatistics {
  ZEROStatistics(DataObjectType t) : AlgorithmData{t} {};
  Attr<ZEROStatus> Status         = ZEROStatus::Uninitialized;
  Attr<int>        NumVar         = {0};  ///< Number of variables in the last solved model
  Attr<int>        NumConstraints = {0};  ///< Number of constraints in the last solved model
  Attr<int>        NumIterations  = {0};  ///< Number of iteration of the Algorithm, if available
  Attr<int>        NumNonZero     = {-1}; ///< Number of non-zero coefficients in the constraint
														///< matrix of the last model, if available
  Attr<bool> NumericalIssues = {false};   ///< True if there have been some Numerical
														///< issues during any iteration
  ///< leader (country)
  Attr<double>   WallClockTime = {0};        ///< The time required to solve the problem
  Attr<bool>     PureNashEquilibrium{false}; ///< True if the equilibrium is a pure NE.
  DataObjectType AlgorithmData;              ///< Stores the configuration and results
															///< related to the specific algorithm
};

enum class ZEROErrorCode {
  /**
	* This enum class contains the error codes
	**/
  MemoryError  = 100, ///< Memory error
  InvalidQuery = 101, ///< The attribute/data is not available
  InvalidData  = 102, ///< The data in input is not valid!
  SolverError  = 103, ///< A third-party solver has thrown an error. Use .more()
  ///< on the exception to get some more info
  OutOfRange = 104, ///< An index or parameter is out-of-range.
  Numeric    = 105, ///< Numeric error
  IOError    = 106, ///< An error involving the IO interface
  Assertion  = 107, ///< An assertion failed
  Unknown    = 0    ///< Unknown error
};

namespace std {
  std::string to_string(const ZEROErrorCode &code);
  std::string to_string(ZEROStatus st);
} // namespace std

class ZEROException : virtual public std::exception {
  /**
	* @brief This class manages the errors thrown by ZERO.
	*/

protected:
  ZEROErrorCode error_code;           ///< Error code for the thrown exception
  std::string error_additional = "-"; ///< Additional information about the error. This may be empty

public:
  explicit ZEROException(ZEROErrorCode code) : error_code(code){};
  explicit ZEROException(ZEROErrorCode code, const std::string &more)
		: error_code(code), error_additional(more){};
  ZEROException(GRBException &e)
		: error_code(ZEROErrorCode::SolverError),
		  error_additional(std::to_string(e.getErrorCode()) + e.getMessage()){};
  ~ZEROException() noexcept override = default;
  const char *what() const noexcept override { return std::to_string(error_code).c_str(); };
  virtual ZEROErrorCode which() const noexcept { return error_code; };
  const char *          more() const noexcept { return error_additional.c_str(); };
};