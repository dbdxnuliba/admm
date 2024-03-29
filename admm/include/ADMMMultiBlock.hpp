#ifndef ADMMBLOCKS_H
#define ADMMBLOCKS_H

#include <iostream>
#include <fstream>
#include <cmath>
#include <utility>
#include <vector>
#include <cstdio>

#include "config.h"
#include "IterativeLinearQuadraticRegulatorADMM.hpp"
#include "robot_dynamics.hpp"
#include "cost_function_admm.hpp"

#include "modern_robotics.h"
#include "differential_ik_trajectory.hpp"
#include "differential_ik_solver.hpp"
#include "KukaKinematicsScrews.hpp"

#include "curvature.hpp"
#include "cnpy.h"

#include "projection_operator.hpp"
#include "admm_public.hpp"

#include <unsupported/Eigen/CXX11/Tensor>

template<typename RobotModelOptimizer, typename RobotModel, int StateSize, int ControlSize>
class ADMMMultiBlock
{

public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  
  ADMMMultiBlock(const std::shared_ptr<RobotModelOptimizer>& kukaRobot, std::shared_ptr<CostFunctionADMM>  costFunction,
    std::shared_ptr<optimizer::IterativeLinearQuadraticRegulatorADMM>  solver, const ADMMopt& ADMM_opt, const IKTrajectory<IK_FIRST_ORDER>::IKopt& IK_opt, unsigned int Time_steps) :
    N(Time_steps), kukaRobot_(kukaRobot), ADMM_OPTS(ADMM_opt), IK_OPT(IK_opt), costFunction_(std::move(costFunction)), solver_(std::move(solver))
  {
    /* Initialize Primal and Dual variables */
    // primal parameters
    xnew.resize(StateSize, N + 1);
    qnew.resize(7, N + 1);
    cnew.resize(2, N + 1);
    unew.resize(ControlSize, N);

    xbar.resize(StateSize, N + 1);
    cbar.resize(2, N + 1);
    ubar.resize(ControlSize, N);
    qbar.resize(7, N + 1);

    // x_avg.resize(stateSize, N + 1);
    q_avg.resize(7, N + 1);
    x_lambda_avg.resize(StateSize, N + 1);
    q_lambda.resize(7, N + 1);

    // dual parameters
    x_lambda.resize(StateSize, N + 1);
    q_lambda.resize(7, N + 1);
    c_lambda.resize(2, N + 1);
    u_lambda.resize(ControlSize, N);

    x_temp.resize(StateSize, N + 1);
    q_temp.resize(7, N + 1);
    c_temp.resize(2, N + 1);
    u_temp.resize(ControlSize, N);


    xubar.resize(StateSize + ControlSize + 2, N); // for projection TODO

    // primal residual
    res_x.resize(ADMM_opt.ADMMiterMax, 0);
    res_q.resize(ADMM_opt.ADMMiterMax, 0);
    res_u.resize(ADMM_opt.ADMMiterMax, 0);
    res_c.resize(ADMM_opt.ADMMiterMax, 0);

    // dual residual
    res_xlambda.resize(ADMM_opt.ADMMiterMax, 0);
    res_qlambda.resize(ADMM_opt.ADMMiterMax, 0);
    res_ulambda.resize(ADMM_opt.ADMMiterMax, 0);
    res_clambda.resize(ADMM_opt.ADMMiterMax, 0);

    final_cost.resize(ADMM_opt.ADMMiterMax + 1, 0);

    // joint_positions_IK
    joint_positions_IK.resize(7, N + 1);  

    Eigen::VectorXd rho_init(5);
    rho_init << 0, 0, 0, 0, 0;
    IK_solve = IKTrajectory<IK_FIRST_ORDER>(IK_opt.Slist, IK_opt.M, IK_opt.joint_limits, IK_opt.eomg, IK_opt.ev, rho_init, N);

    // initialize curvature object
    curve = Curvature();

    L.resize(N + 1);
    R_c.resize(N + 1);
    k.resize(N + 1 ,3);

    X_curve.resize(3, N + 1);

    robotIK = models::KUKA();

    // initialize the tensor to save data
    #ifdef DEBUG
    data_store.resize(N + 1, 6, ADMM_opt.ADMMiterMax + 1); 
    #endif 

    // for the projection
    m_projectionOperator = ProjectionOperator(N);

    std::cout << "initilized ADMM multi block" << std::endl;
}

/* optimizer execution */
void solve(const stateVec_t& xinit, const commandVecTab_t& u_0,
  const stateVecTab_t& xtrack, const std::vector<Eigen::MatrixXd>& cartesianTrack,
   const Eigen::VectorXd& rho, const Saturation& L) 
{

    // Initial Trajectory 
    // Initialize Trajectory to get xnew with u_0 
    solver_->initializeTrajectory(xinit, u_0, xtrack, cbar, xbar, ubar, qbar, rho, R_c);

    lastTraj = solver_->getLastSolvedTrajectory();
    xnew = lastTraj.xList;
    unew = lastTraj.uList;

    final_cost[0] = lastTraj.finalCost;


    start = std::chrono::high_resolution_clock::now();
    
    /* ---------------------------------------- Initialize IK solver ---------------------------------------- */
    
    IK_solve.getTrajectory(cartesianTrack, xnew.col(0).head(7), xnew.col(0).segment(7, 7), xbar.block(0, 0, 7, N + 1), xbar.block(0, 0, 7, N + 1), 0 * rho, &joint_positions_IK);

    X_curve = IK_solve.getFKCurrentPos(); 

    Eigen::MatrixXd temp_fk(4, 4);
    temp_fk.setZero();


    /* ------------------------------------------------------------------------------------------------------- */
    double error_fk = 0.0;

    /* ----------------------------------------------- TESTING ----------------------------------------------- */
    for (int i = 0;i < cartesianTrack.size() - 1; i++) {
        temp_fk  = mr::FKinSpace(IK_OPT.M, IK_OPT.Slist, joint_positions_IK.col(i));
        error_fk = error_fk + (cartesianTrack.at(i) - mr::FKinSpace(IK_OPT.M, IK_OPT.Slist, joint_positions_IK.col(i))).norm();

        // save data
        #ifdef DEBUG
        for (int k = 0;k < 3;k++) {
            data_store(i, k, 0) = temp_fk(k, 3);

            // Force data
            Eigen::VectorXd force = xnew.col(i).tail(3);
            data_store(i, k+3, 0) = force(k);
        }

        #endif
    }

    std::cout << "error " << error_fk << std::endl; 
    end = std::chrono::high_resolution_clock::now();
    elapsed = end - start;
    std::cout << "IK compute time " << static_cast<int>(elapsed.count()) << " ms" << std::endl;
    /* ----------------------------------------------- END TESTING ----------------------------------------------- */


    /* ---------------------------------------- Initialize xbar, cbar,ubar ---------------------------------------- */
    qbar = joint_positions_IK;

    // calculates contact terms 
    contact_update(kukaRobot_, xnew, &cnew);
    cbar = cnew;
    xbar.block(0, 0, 7, N + 1) = joint_positions_IK;
    ubar.setZero();

    x_lambda.setZero();
    c_lambda.setZero();
    u_lambda.setZero();
    q_lambda.setZero();

    for (int i = 0;i < ADMM_OPTS.ADMMiterMax; i++)
    {
        res_c[i] = 0;
        res_x[i] = 0;
        res_u[i] = 0;
        res_q[i] = 0;
    }

    Eigen::MatrixXd temp(4, 4);

    double cost = 0.0;

    Eigen::VectorXd rho_ddp(5);
    rho_ddp << rho(0), rho(1), rho(2), 0, 0;

    /* ------------------------------------------------ Run ADMM ---------------------------------------------- */
    std::cout << "begin ADMM..." << std::endl;



    for (unsigned int i = 0; i < ADMM_OPTS.ADMMiterMax; i++) {

        // TODO: Stopping criterion is needed
        std::cout << "in ADMM iteration " << i + 1 << std::endl;

       /* ---------------------------------------- iLQRADMM solver block ----------------------------------------   */
        start = std::chrono::high_resolution_clock::now();
        solver_->solve(xinit, unew, xtrack, cbar - c_lambda, xbar - x_lambda, ubar - u_lambda, qbar - q_lambda, rho_ddp, R_c);
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;
        std::cout << "DDP compute time " << static_cast<int>(elapsed.count()) << " ms" << std::endl;


        lastTraj = solver_->getLastSolvedTrajectory();
        xnew     = lastTraj.xList;
        unew     = lastTraj.uList;
        qnew     = xnew.block(0, 0, 7, N + 1);

        /* ----------------------------------------------- TESTING ----------------------------------------------- */
        temp.setZero();
        error_fk = 0.0;

        for (int j = 0;j < cartesianTrack.size() ; j++) {
            temp_fk   = mr::FKinSpace(IK_OPT.M, IK_OPT.Slist, xnew.col(j).head(7));
            temp      = mr::TransInv(cartesianTrack.at(j)) * temp_fk;
            error_fk += temp.col(3).head(3).norm();

            // save data
            #ifdef DEBUG
            for (int k = 0;k < 3;k++) {
                data_store(j, k, i ) = temp_fk(k, 3);

                // Force data
                Eigen::VectorXd force   = xnew.col(j).tail(3);
                data_store(j, k + 3, i ) = force(k);
            }
            #endif
        }
        std::cout << "DDP tracking error: " << error_fk << std::endl; 

        /* --------------------------------------------- END TESTING --------------------------------------------- */

        
        start = std::chrono::high_resolution_clock::now();
        /* ----------------------------- update cnew. TODO: variable path curves -----------------------------  */
        contact_update(kukaRobot_, xnew, &cnew);
        

        /* ------------------------------------------- IK block update -----------------------------------------   */ 
        std::cout << "begin differential IK..." << std::endl;
        joint_positions_IK.setZero();
        IK_solve.getTrajectory(cartesianTrack, xnew.col(0).head(7), xnew.col(0).segment(7, 7), xbar.block(0, 0, 7, N + 1) - q_lambda, xbar.block(7, 0, 7, N + 1), rho,  &joint_positions_IK);
        std::cout << "end differential IK..." << std::endl;
        
        /* ----------------------------------------------- TESTING ----------------------------------------------- */

        error_fk = 0;
        temp.setZero();
        for (int i = 0;i < cartesianTrack.size()-1; i++) { 
            temp = mr::TransInv(cartesianTrack.at(i)) * mr::FKinSpace(IK_OPT.M, IK_OPT.Slist, joint_positions_IK.col(i));
            error_fk += temp.col(3).head(3).norm();
        }
        std::cout << "IK tracking error: " << error_fk << std::endl; 
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;
        std::cout << "IK compute time " << static_cast<int>(elapsed.count()) << " ms" << std::endl;
        /* --------------------------------------------- END TESTING --------------------------------------------- */



        /* ------------------------------------- Average States ------------------------------------   */

        q_avg = (qnew  + joint_positions_IK) / 2;
        // q_lambda = x_lambda.block(0, 0, 7, x_lambda.cols());

        x_lambda_avg.block(0, 0, 7, N + 1) = (q_lambda + x_lambda.block(0, 0, 7, x_lambda.cols())) / 2;

        /* ---------------------------------------- Projection --------------------------------------  */
        // Projection block to feasible sets (state and control contraints)
        x_temp = xnew + x_lambda;
        x_temp.block(0, 0, 7, xnew.cols()) = q_avg  + x_lambda_avg.block(0, 0, 7, N + 1);// test this line
        c_temp = cnew + c_lambda;
        u_temp = unew + u_lambda;

        m_projectionOperator.projection(x_temp, c_temp, u_temp, &xubar,  L);

        /* Dual variables update */
        for (unsigned int j = 0;j < N; j++) {

            cbar.col(j) = xubar.col(j).segment(StateSize, 2);
            xbar.col(j) = xubar.col(j).head(StateSize);
            ubar.col(j) = xubar.col(j).tail(ControlSize);

            c_lambda.col(j) += (cnew.col(j) - cbar.col(j)).eval();
            x_lambda.col(j) += (xnew.col(j) - xbar.col(j)).eval();
            u_lambda.col(j) += (unew.col(j) - ubar.col(j)).eval();
            q_lambda.col(j) += (joint_positions_IK.col(j) - xbar.col(j).head(7)).eval();

            // Save residuals for all iterations
            res_c[i] = (cnew.col(j) - cbar.col(j)).norm();
            res_x[i] = (xnew.col(j) - xbar.col(j)).norm();
            res_u[i] = (unew.col(j) - ubar.col(j)).norm();
            res_q[i] = (joint_positions_IK.col(j) - xbar.col(j).head(7)).norm();

        }

        // xbar.col(N) = xubar.col(N - 1).head(stateSize); // 
        xbar.col(N) = x_temp.col(N);
        x_lambda.col(N) += (xnew.col(N) - xbar.col(N)).eval();
        q_lambda.col(N) += (joint_positions_IK.col(N) - xbar.col(N).head(7)).eval();
        
        res_x[i] = (xnew.col(N) - xbar.col(N)).norm();
        res_c[i] = (cnew.col(N) - cbar.col(N)).norm();
        res_q[i] = (joint_positions_IK.col(N) - xbar.col(N).head(7)).norm();



        /* ------------------------------- get the cost without augmented Lagrangian terms ------------------------------- */
        cost = 0;

        for (int i = 0;i < N;i++) {
            cost = cost + costFunction_->cost_func_expre(i, xnew.col(i), unew.col(i), xtrack.col(i));
        }


        final_cost[i + 1] = cost;

    }


    solver_->initializeTrajectory(xinit, unew, xtrack, cbar, xbar, ubar, qbar, rho, R_c);

    lastTraj = solver_->getLastSolvedTrajectory();
    xnew = lastTraj.xList;

    unew = lastTraj.uList;


    #ifdef DEBUG
    cnpy::npy_save("./cartesian_trajectory_admm.npy", data_store.data(), {static_cast<unsigned long>(ADMM_OPTS.ADMMiterMax + 1), 
        6, static_cast<unsigned long>(N + 1)}, "w");
    #endif

    #ifdef PRINT
 


    std::cout << "ADMM Trajectory Generation Finished!..." << std::endl;


    for (unsigned int i = 0; i < ADMM_OPTS.ADMMiterMax; i++) {
      std::cout << "res_x[" << i << "]:" << res_x[i] << std::endl;
      std::cout << "res_u[" << i << "]:" << res_u[i] << std::endl;
      std::cout << "res_c[" << i << "]:" << res_c[i] << std::endl;
      std::cout << "final_cost[" << i << "]:" << final_cost[i] << std::endl;
    }
    #endif

    lastTraj = solver_->getLastSolvedTrajectory();

  }

  void contact_update(std::shared_ptr<RobotModelOptimizer>& kukaRobot, const stateVecTab_t& xnew, Eigen::MatrixXd* cnew)
  {
    double vel = 0.0;
    double m = 0.3; 
    double R = 0.4;

    Eigen::MatrixXd jacobian(6, 7);

    curve.curvature(X_curve.transpose(), L, R_c, k);
    R_c = Eigen::VectorXd::Constant(N+1, 1);


    for (int i = 0; i < xnew.cols(); i++) {
        kukaRobot->getSpatialJacobian(const_cast<double*>(xnew.col(i).head(7).data()), jacobian);

        vel = (jacobian * xnew.col(i).segment(6, 7)).norm();
        (*cnew)(0,i) = m * vel * vel / R_c(i);
        // std::cout << 1/R_c(i) << " " << std::endl;
        (*cnew)(1,i) = *(const_cast<double*>(xnew.col(i).tail(1).data()));
    }
  }

  optimizer::IterativeLinearQuadraticRegulatorADMM::traj getLastSolvedTrajectory()
  {
    return lastTraj;
  }

  struct optimizer::IterativeLinearQuadraticRegulatorADMM::traj lastTraj;


protected:
  models::KUKA robotIK;
  std::shared_ptr<RobotModelOptimizer> kukaRobot_;
  std::shared_ptr<CostFunctionADMM> costFunction_;
  std::shared_ptr<optimizer::IterativeLinearQuadraticRegulatorADMM> solver_;
  ProjectionOperator m_projectionOperator{};

  Curvature curve;
  Saturation projectionLimits;
  ADMMopt ADMM_OPTS;
  IKTrajectory<IK_FIRST_ORDER>::IKopt IK_OPT;

  ADMM_MPCopt ADMM_MPC_opt;

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
  std::chrono::duration<float, std::milli> elapsed{};

};

#endif



