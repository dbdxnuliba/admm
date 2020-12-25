#ifndef ADMMBLOCKS_H
#define ADMMBLOCKS_H

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <stdio.h>

#include "config.h"
#include "ilqrsolver_admm.hpp"
#include "RobotDynamics.hpp"
#include "SoftContactModel.h"
#include "cost_function_admm.h"

#include "modern_robotics.h"
#include "DiffIKTrajectory.hpp"
#include "DiffIKSolver.hpp"
#include "KukaKinematicsScrews.hpp"

#include "curvature.hpp"
#include "cnpy.h"

#include "ProjectionOperator.hpp"
#include "admmPublic.hpp"

#include <unsupported/Eigen/CXX11/Tensor>


class ADMMMultiBlock {
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  
  
  ADMMMultiBlock(std::shared_ptr<RobotAbstract>& kukaModel, const CostFunctionADMM& costFunction, 
    const optimizer::ILQRSolverADMM& solverDDP, const ADMMopt& ADMM_opt, const IKTrajectory<IK_FIRST_ORDER>::IKopt& IK_opt, unsigned int Time_steps);

  void solve(const stateVec_t& xinit, const commandVecTab_t& u_0, const stateVecTab_t& xtrack, 
    const std::vector<Eigen::MatrixXd>& cartesianTrack, const Eigen::VectorXd& rho, const Saturation& L);

  void contact_update(std::shared_ptr<RobotAbstract>& kukaRobot, const stateVecTab_t& xnew, Eigen::MatrixXd* cnew);

  optimizer::ILQRSolverADMM::traj getLastSolvedTrajectory();

  struct optimizer::ILQRSolverADMM::traj lastTraj;


protected:
  models::KUKA robotIK;
  std::shared_ptr<RobotAbstract> kukaRobot_;
  CostFunctionADMM costFunction_;
  optimizer::ILQRSolverADMM solver_;
  ProjectionOperator m_projectionOperator;

  Curvature curve;
  Saturation projectionLimits;
  ADMMopt ADMM_OPTS;
  IKTrajectory<IK_FIRST_ORDER>::IKopt IK_OPT;

  stateVecTab_t joint_state_traj;

  unsigned int N;

  /* Initalize Primal and Dual variables */
  // primal parameters
  stateVecTab_t xnew;
  Eigen::MatrixXd qnew, cnew;
  commandVecTab_t unew;

  // stateVecTab_t x_avg;
  Eigen::MatrixXd q_avg;
  stateVecTab_t x_lambda_avg;

  stateVecTab_t xbar;
  Eigen::MatrixXd cbar;
  commandVecTab_t ubar;
  Eigen::MatrixXd qbar;

  stateVecTab_t xbar_old; // "old" for last ADMM iteration 
  Eigen::MatrixXd cbar_old;
  commandVecTab_t ubar_old; 
  
  // dual parameters
  stateVecTab_t x_lambda;
  Eigen::MatrixXd c_lambda;
  commandVecTab_t u_lambda;
  Eigen::MatrixXd q_lambda;

  stateVecTab_t x_temp;
  Eigen::MatrixXd c_temp;
  commandVecTab_t u_temp;
  Eigen::MatrixXd q_temp;

  commandVecTab_t u_0;

  Eigen::MatrixXd xubar; // for projection

  // primal residual
  std::vector<double> res_x, res_q, res_u, res_c;

  // dual residual
  std::vector<double> res_xlambda, res_qlambda, res_ulambda, res_clambda;

  std::vector<double> final_cost;

  // joint_positions_IK
  Eigen::MatrixXd joint_positions_IK;

  Eigen::VectorXd L;
  Eigen::VectorXd R_c;
  Eigen::MatrixXd k;

  Eigen::MatrixXd X_curve;

  IKTrajectory<IK_FIRST_ORDER> IK_solve;

  Eigen::Tensor<double, 3> data_store;

  std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
  std::chrono::duration<float, std::milli> elapsed;

};

#endif


