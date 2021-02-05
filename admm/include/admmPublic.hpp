#ifndef ADMMPUBLIC_H
#define ADMMPUBLIC_H  


  // data structure for saturation limits
  struct Saturation {
    Saturation() {}

    Eigen::Matrix<double, 2, stateSize> stateLimits;
    Eigen::Matrix<double, 2, commandSize> controlLimits;
  };


  // data structure for admm options
  struct ADMMopt {
    ADMMopt() = default;
    ADMMopt(double dt_, double tolFun_, double tolGrad_, unsigned int iterMax_, 
      int ADMMiterMax_) : dt(dt_), tolFun(tolFun_), tolGrad(tolGrad_), iterMax(iterMax_), ADMMiterMax(ADMMiterMax_) {}
    double dt;
    double tolFun; 
    double tolGrad; 
    unsigned int iterMax; // DDP iteration max
    // parameters for ADMM, penelty terms
    int ADMMiterMax;
  };


  // // data structure for admm options
  struct TrajectoryTrack 
  {
    TrajectoryTrack(double des_normal_force) {}
    const std::vector<Eigen::MatrixXd> cartesianTrajectory;
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> stateTrajectory;

  };


  #endif