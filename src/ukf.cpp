#include "ukf.h"
#include "Eigen/Dense"

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  n_x_ = 5;
  // initial state vector
  x_ = VectorXd(n_x_);
  // x_.fill(0);

  // initial covariance matrix
  P_ = MatrixXd(n_x_, n_x_);
  // P_.fill(0);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 3;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  

  is_initialized_ = false;
  time_us_ = 0;
  n_aug_ = 7;
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  lambda_ = 3 - n_aug_;
  weights_ = VectorXd(2 * n_aug_ + 1);
  
  weights_(0) = lambda_ / (lambda_ + n_aug_);
  for (int i = 1; i < 2 * n_aug_ + 1; i++){
    weights_(i) = 0.5 / (lambda_ + n_aug_);
  }




}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
  
  // // initialize the state vector and covariance matrix
  if (!is_initialized_)
  {
    time_us_ = meas_package.timestamp_;

    if(meas_package.sensor_type_ == MeasurementPackage::LASER)
    {
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);

      P_ << std_laspx_ * std_laspx_, 0, 0, 0, 0,
            0, std_laspy_ * std_laspy_, 0, 0, 0,
            0, 0, 1, 0, 0, 
            0, 0, 0, 1, 0,
            0, 0, 0, 0, 1;

    }
    else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
    {
      double rho = meas_package.raw_measurements_(0);
      double phi = meas_package.raw_measurements_(1);
      double rho_dot = meas_package.raw_measurements_(2);

      x_(0) = rho * cos(phi);
      x_(1) = rho * sin(phi);
      x_(2) = rho_dot;

      P_ << std_radr_ * std_radr_, 0, 0, 0, 0,
            0, std_radphi_ * std_radphi_, 0, 0, 0,
            0, 0, std_radrd_ * std_radrd_, 0, 0, 
            0, 0, 0, 1, 0,
            0, 0, 0, 0, 1;

    }

    is_initialized_ = true; 
    return;
  }

  // // after initialization, we can predict and update using the other functions
  double delta_t = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  Prediction(delta_t);

  if(meas_package.sensor_type_ == MeasurementPackage::LASER)
  {
    UpdateLidar(meas_package);
  }
  else if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
  {
    UpdateRadar(meas_package);
  }


}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

  //create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

  x_aug.head(n_x_) = x_;  // just copy the first 5 elements of x_ to x_aug(n_x_ = 5)
  x_aug(5) = 0; // mean of acceleration noise
  x_aug(6) = 0; // mean of yaw acceleration noise

  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(n_x_,n_x_) = std_a_ * std_a_; // variance of acceleration noise
  P_aug(n_x_+1,n_x_+1) = std_yawdd_ * std_yawdd_; // variance of yaw acceleration noise

  MatrixXd L = P_aug.llt().matrixL(); // square root of P_aug

  Xsig_aug.fill(0.0);
  Xsig_aug.col(0) = x_aug;

  // create augmented sigma points method 1
  // for (int i = 0; i < n_aug_; i++) {
  //   Xsig_aug.col(i+1) = x_aug + sqrt(lambda_ + n_aug_) * L.col(i);
  //   Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_ + n_aug_) * L.col(i);
  // }

  // create augmented sigma points method 2
  // Xsig_aug.block(0,1,n_aug_,n_aug_) = x_aug.replicate(1,n_aug_) + sqrt(lambda_ + n_aug_) * L;
  // Xsig_aug.block(0,n_aug_+1,n_aug_,n_aug_) = x_aug.replicate(1,n_aug_) - sqrt(lambda_ + n_aug_) * L;

  // create augmented sigma points method 3
  Xsig_aug.block(0,1,n_aug_,n_aug_) = (sqrt(lambda_ + n_aug_) * L).colwise() + x_aug;
  Xsig_aug.block(0,n_aug_+1,n_aug_,n_aug_) =(-sqrt(lambda_ + n_aug_) * L).colwise() + x_aug;

  // sigma point prediction
  for (int i = 0; i < 2 * n_aug_ + 1; i++){

    double px = Xsig_aug(0,i);
    double py = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    double px_p, py_p;

    if (fabs(yawd) > 0.001) {
      px_p = px + v/yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
      py_p = py + v/yawd * (cos(yaw) - cos(yaw + yawd * delta_t));

    } else {
      px_p = px + v * delta_t * cos(yaw);
      py_p = py + v * delta_t * sin(yaw);

    }

    double v_p = v;
    double yaw_p = yaw + yawd * delta_t;
    double yawd_p = yawd;

    px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
    py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
    v_p = v_p + nu_a * delta_t;

    yaw_p = yaw_p + 0.5 * nu_yawdd * delta_t * delta_t;
    yawd_p = yawd_p + nu_yawdd * delta_t;

    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }

  // mean and covariance prediction
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }


  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){

    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    while (x_diff(3) > M_PI) x_diff(3) -= 2. * M_PI;
    while (x_diff(3) < -M_PI) x_diff(3) += 2. * M_PI;

    P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
  }

}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */

  VectorXd z = meas_package.raw_measurements_;
  int n_z = 2; // lidar measurement dimension

  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  VectorXd z_pred = VectorXd(n_z);

  MatrixXd S = MatrixXd(n_z, n_z);

  MatrixXd Tc = MatrixXd(n_x_, n_z);

  // transform sigma points into measurement space
  Zsig.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){

    double px = Xsig_pred_(0,i);
    double py = Xsig_pred_(1,i);

    Zsig(0,i) = px;
    Zsig(1,i) = py;
  }

  z_pred.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++){
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

  S.fill(0.0);
  Tc.fill(0.0);

  for (int i = 0; i < 2 * n_aug_ + 1; i++)
  {

      VectorXd z_diff = Zsig.col(i) - z_pred;

      S = S + weights_(i) * z_diff * z_diff.transpose();

      VectorXd x_diff = Xsig_pred_.col(i) - x_;

      while (x_diff(3) > M_PI) x_diff(3) -= 2. * M_PI;
      while (x_diff(3) < -M_PI) x_diff(3) += 2. * M_PI;

      Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

    MatrixXd R = MatrixXd(n_z, n_z);
    R << std_laspx_ * std_laspx_, 0,
         0, std_laspy_ * std_laspy_;

    S = S + R;

    MatrixXd K = Tc * S.inverse();
    VectorXd z_diff = z - z_pred;

    x_ = x_ + K * z_diff;
    P_ = P_ - K * S * K.transpose();


}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */

  
  VectorXd z = meas_package.raw_measurements_;
  int n_z = 3; // lidar measurement dimension

  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  
  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);

  /**
   * Student part begin
   */

  // transform sigma points into measurement space
  Zsig.fill(0.0);
  for (int i=0; i<2*n_aug_ +1; i++) {  
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v   = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double v1  = cos(yaw)*v;
    double v2  = sin(yaw)*v;
    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }
  
  // calculate mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i<2*n_aug_+1; i++) {  
    z_pred = z_pred + weights_(i)*Zsig.col(i);
  }
  
  // calculate innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2*n_aug_+1; i++) {  

    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization(phi)
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI; 

  

    S = S + weights_(i)*z_diff*z_diff.transpose();
  }

  MatrixXd R_radar_ = MatrixXd(n_z,n_z);
  R_radar_ <<    std_radr_*std_radr_, 0, 0,
                 0, std_radphi_*std_radphi_, 0,
                 0, 0,std_radrd_*std_radrd_;

  S = S + R_radar_;

  MatrixXd Tc = MatrixXd(n_x_, n_z);

   // calculate cross correlation matrix
  Tc.fill(0.0);

  for (int i = 0; i < 2*n_aug_+1; i++) {  
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization(phi)
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI; 

    // angle normalization(phi)
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI; 

    Tc = Tc + weights_(i)*x_diff*z_diff.transpose();
  }
  
  // calculate Kalman gain K;
  MatrixXd K = Tc*S.inverse();

  // update state mean and covariance matrix
  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization(phi)
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  x_ = x_ + K*z_diff;

  P_ = P_ - K*S*K.transpose();


}