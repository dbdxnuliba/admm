#ifndef IK_SOLVER_HPP
#define IK_SOLVER_HPP

#include <iostream>
#include <Eigen/Dense>
#include "modern_robotics.h"



class InverseKinematics {
public:
	InverseKinematics() = default;
	~InverseKinematics() = default;

	/* Get IK solution given the Td and initial thetalist */
	virtual void getIK(const Eigen::MatrixXd& Td, const Eigen::VectorXd& thetalist0, const Eigen::VectorXd& thetalistd0, const Eigen::VectorXd& q_bar, const Eigen::VectorXd& qd_bar,
	 bool initial, const Eigen::VectorXd& rho, Eigen::VectorXd& thetalist) {};

private:
	/* Null space projection */
	virtual void getRedundancyResolution(const Eigen::VectorXd& thetalist, Eigen::VectorXd* q_grad_ret) {};

	/* Compute jacobian dot, given the jacobian */
	virtual void getJacobianDot(const Eigen::MatrixXd& jacobian, const Eigen::VectorXd& thetalist, Eigen::MatrixXd* jacobianDot) {};


protected:
	Eigen::MatrixXd Slist;
	Eigen::MatrixXd M;
	double ev{};
	double eomg{};
};

/* First order IK solution */
class IK_FIRST_ORDER : public InverseKinematics {

public:
	IK_FIRST_ORDER() = default;
	~IK_FIRST_ORDER() = default;
	IK_FIRST_ORDER(const Eigen::MatrixXd& Slist, const Eigen::MatrixXd& M, const Eigen::MatrixXd& jointLimits, const double& eomg, const double& ev, const Eigen::VectorXd& rho);

	void getIK(const Eigen::MatrixXd& Td, const Eigen::VectorXd& thetalist0, const Eigen::VectorXd& thetalistd0, const Eigen::VectorXd& q_bar, const Eigen::VectorXd& qd_bar, 
		bool initial, const Eigen::VectorXd& rho, Eigen::VectorXd& thetalist) override;


	void getIK_random_initial(const Eigen::MatrixXd& Td, const Eigen::VectorXd& q_bar,
		const Eigen::VectorXd& rho, Eigen::VectorXd& thetalist_ret); 

	static void getRandomState(Eigen::VectorXd& randomState, int elbow);
	
	void getRedundancyResolution(const Eigen::VectorXd& thetalist, Eigen::VectorXd* q_grad_ret) override;
	
	int maxIterations{};
	Eigen::Matrix<double, 4, 4> Tsb;
	Eigen::Matrix<double, 6, 1> Vs;
	Eigen::VectorXd rho;
	Eigen::Matrix<double, 2, 7> joint_limits;
	Eigen::Matrix<double, 7, 1> q_range;
	Eigen::Matrix<double, 7, 1> q_mid;

	// Eigen::MatrixX<double, 6, 7> J;

	Eigen::CompleteOrthogonalDecomposition<Eigen::Matrix<double, 6, 7> > cod;

};


/* Second order IK solution */
class IK_SECOND_ORDER : public InverseKinematics {

public:
	IK_SECOND_ORDER();

	IK_SECOND_ORDER(const Eigen::MatrixXd& Slist_, const Eigen::MatrixXd& M_, const Eigen::MatrixXd& joint_limits_, const double& eomg_, const double& ev_, const Eigen::VectorXd& rho_);

	void getIK(const Eigen::MatrixXd& Td, const Eigen::VectorXd& thetalist0, const Eigen::VectorXd& thetalistd0, const Eigen::VectorXd& q_bar, const Eigen::VectorXd& qd_bar, 
		bool initial, const Eigen::VectorXd& rho, Eigen::VectorXd* thetalist);
	
	void getRedundancyResolution(const Eigen::VectorXd& thetalist, Eigen::VectorXd* q_grad_ret);
	
	int maxIterations;
	Eigen::Matrix<double, 4, 4> Tsb;
	Eigen::Matrix<double, 6, 1> Vs;
	Eigen::VectorXd rho;
	Eigen::Matrix<double, 2, 7> joint_limits;
	Eigen::Matrix<double, 7, 1> q_range;
	Eigen::Matrix<double, 7, 1> q_mid;

	Eigen::CompleteOrthogonalDecomposition<Eigen::Matrix<double, 6, 7> > cod;
};

#endif //IK_SOLVER_HPP

