#include <memory>
#include <Eigen/Dense>


#include "admm.hpp"
#include "config.h"

void generateCartesianTrajectory(stateVec_t& xinit, stateVec_t& xgoal, stateVecTab_t& xtrack, std::vector<Eigen::MatrixXd> &cartesianPoses);
void admm_mpc();
void admm();


/* Generate cartesian trajectory */
void generateCartesianTrajectory(stateVec_t& xinit, stateVec_t& xgoal, stateVecTab_t& xtrack, std::vector<Eigen::MatrixXd> &cartesianPoses) {
    Eigen::MatrixXd joint_lims(2,7);
    double eomg = 0.00001;
    double ev   = 0.00001;
    unsigned int N = NumberofKnotPt;

    /* Cartesian Tracking. IKopt */
    IKTrajectory<IK_FIRST_ORDER>::IKopt IK_OPT(7);
    models::KUKA robotIK = models::KUKA();
    Eigen::MatrixXd Slist(6,7);
    Eigen::MatrixXd M(4,4);
    robotIK.getSlist(&Slist); 
    robotIK.getM(&M);

    IK_OPT.joint_limits = joint_lims;
    IK_OPT.ev = ev;
    IK_OPT.eomg = eomg;
    IK_OPT.Slist = Slist;
    IK_OPT.M = M;

    // this is for admm parameters. For normal ddp, set it to zero.
    Eigen::VectorXd rho_init(5);
    rho_init << 0, 0, 0, 0, 0;

    IKTrajectory<IK_FIRST_ORDER> IK_traj = IKTrajectory<IK_FIRST_ORDER>(Slist, M, joint_lims, eomg, ev, rho_init, N);

    Eigen::MatrixXd R(3,3);
    R << 1, 0, 0, 0, 1, 0, 0, 0, 1;
    double Tf = 2 * M_PI;

    cartesianPoses = IK_traj.generateLissajousTrajectories(R, 0.8, 1, 3, 0.08, 0.08, N, Tf);


    /* initialize xinit, xgoal, xtrack - for the hozizon*/
    Eigen::MatrixXd joint_trajectory(7, N + 1);
    Eigen::VectorXd thetalist0(7), thetalistd0(7), thetalist_ret(7);
    thetalist0  << 0.1, 0.2, 0.1, 0.2, 0.1, 0.1, 0.1;
    thetalistd0 << 0, 0, 0, 0, 0, 0, 0;


    bool initial = true;
    IK_FIRST_ORDER IK = IK_FIRST_ORDER(IK_OPT.Slist,  IK_OPT.M, IK_OPT.joint_limits, IK_OPT.eomg, IK_OPT.ev, rho_init);

    IK.getIK(cartesianPoses.at(0), thetalist0, thetalistd0, Eigen::VectorXd::Zero(7), Eigen::VectorXd::Zero(7), initial, rho_init, &thetalist_ret);
    xinit.head(7) = thetalist_ret;

    // IK trajectory initialization
    // IKTrajectory<IK_FIRST_ORDER> IK_solve = IKTrajectory<IK_FIRST_ORDER>(IK_OPT.Slist, IK_OPT.M, 
    // IK_OPT.joint_limits, IK_OPT.eomg, IK_OPT.ev, rho_init, N);

    // IK_solve.getTrajectory(cartesianPoses, xinit.col(0).head(7), xinit.col(0).segment(7, 7), 
    // Eigen::MatrixXd::Zero(7, N + 1), Eigen::MatrixXd::Zero(7, N + 1), rho_init, &joint_trajectory);


    // xtrack.block(0, 0, 7, N + 1) = joint_trajectory;
    // xgoal.head(7) = joint_trajectory.col(N).head(7);

}

/* input robot model */
void admm_mpc()
{

    MPC_ADMM optimizerADMM;
    stateVec_t xinit, xgoal;
    stateVecTab_t xtrack;
    xtrack.resize(stateSize, NumberofKnotPt + 1);

    std::vector<Eigen::MatrixXd> cartesianPoses;
    generateCartesianTrajectory(xinit, xgoal, xtrack, cartesianPoses);

    // xtrack.row(16) = 5 * Eigen::VectorXd::Ones(NumberofKnotPt + 1); 

    KUKAModelKDLInternalData robotParams;
    robotParams.numJoints = 7;
    robotParams.Kv = Eigen::MatrixXd(7,7);
    robotParams.Kp = Eigen::MatrixXd(7,7);
    
    // optimizerADMM.run(xinit, xgoal, xtrack, cartesianPoses);
    optimizerADMM.run();
}

/* input robot model */
void admm(std::shared_ptr<KUKAModelKDL>& kukaRobot)
{

  unsigned int N = NumberofKnotPt;
  int ADMMiterMax = 5;
  double dt = TimeStep;

  ADMM::ADMMopt ADMM_OPTS(dt, 1e-7, 1e-7, 15, ADMMiterMax);

  Eigen::MatrixXd joint_lims(2,7);
  double eomg = 0.00001;
  double ev   = 0.00001;

  /* Cartesian Tracking. IKopt */
  IKTrajectory<IK_FIRST_ORDER>::IKopt IK_OPT(7);
  models::KUKA robotIK = models::KUKA();
  Eigen::MatrixXd Slist(6,7);
  Eigen::MatrixXd M(4,4);
  robotIK.getSlist(&Slist); 
  robotIK.getM(&M);

  IK_OPT.joint_limits = joint_lims;
  IK_OPT.ev = ev;
  IK_OPT.eomg = eomg;
  IK_OPT.Slist = Slist;
  IK_OPT.M = M;

  unsigned int iterMax = 10; // DDP iteration max

  /*------------------initialize control input-----------------------*/


  /* -------------------- Optimizer Params ------------------------ */
  optimizer::ILQRSolverADMM::OptSet solverOptions;
  solverOptions.n_hor    = N;
  solverOptions.tolFun   = ADMM_OPTS.tolFun;
  solverOptions.tolGrad  = ADMM_OPTS.tolGrad;
  solverOptions.max_iter = iterMax;


  FULL_ADMM admm = FULL_ADMM(N, TimeStep);
  admm.run(kukaRobot, solverOptions, ADMM_OPTS, IK_OPT);



}

// For testing 
int main(int argc, char *argv[]) {

    /* -------------------- orocos kdl robot initialization-------------------------*/
  KUKAModelKDLInternalData robotParams;
  robotParams.numJoints = NDOF;
  robotParams.Kv = Eigen::MatrixXd(7,7);
  robotParams.Kp = Eigen::MatrixXd(7,7);

  /* ---------------------------------- Define the robot and contact model ---------------------------------- */
  KDL::Chain robot = KDL::KukaDHKdl();
  std::shared_ptr<KUKAModelKDL> kukaRobot = std::shared_ptr<KUKAModelKDL>(new KUKAModelKDL(robot, robotParams));

  admm(kukaRobot);

}