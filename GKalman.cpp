#include "GKalman.h"

GKalman::GKalman(int dynamParams, int measureParams, int controlParams, int type)
{
    m_KF.init(dynamParams, measureParams, controlParams, type);
    m_state = cv::Mat_<float>(dynamParams, 1);
    m_processNoise = cv::Mat(dynamParams, 1, CV_32F);
    m_measurement = cv::Mat_<float>(measureParams,1);
    m_measurement.setTo(cv::Scalar(0));

}

void GKalman::begin(float x, float y, float vx, float vy)
{
    m_KF.statePost.at<float>(0) = x;
    m_KF.statePost.at<float>(1) = y;
    m_KF.statePost.at<float>(2) = vx;
    m_KF.statePost.at<float>(3) = vy;
    m_KF.statePost.at<float>(4) = 0;
    m_KF.statePost.at<float>(5) = 0;

    m_KF.transitionMatrix = (cv::Mat_<float>(6, 6) << 1,0,1,0,0.5,0, 0,1,0,1,0,0.5, 0,0,1,0,1,0, 0,0,0,1,0,1, 0,0,0,0,1,0, 0,0,0,0,0,1);

    //cv::setIdentity(m_KF.measurementMatrix);
    //m_KF.measurementMatrix = *(cv::Mat_<float>(4, 6) << 1,0,1,0,0.5,0, 0,1,0,1,0,0.5, 0,0,1,0,1,0, 0,0,0,1,0,1);
    //m_KF.measurementMatrix.setTo(cv::Scalar(0));
    /*cv::Mat measurementNoiseDiag = (cv::Mat_<cv::Scalar>(6, 1) << cv::Scalar::all(1e-1),cv::Scalar::all(1e-1),
        cv::Scalar::all(1e-1),cv::Scalar::all(1e-1),cv::Scalar::all(1e-1),cv::Scalar::all(1e-1));
    m_KF.measurementNoiseCov = cv::Mat::diag(measurementNoiseDiag).clone();*/
    setIdentity(m_KF.measurementMatrix);
    setIdentity(m_KF.processNoiseCov, cv::Scalar::all(1e-4));
    setIdentity(m_KF.measurementNoiseCov, cv::Scalar::all(2));
    setIdentity(m_KF.errorCovPost, cv::Scalar::all(10));
}

cv::Point GKalman::update(float x, float y, float vx, float vy)
{
    cv::Mat prediction = m_KF.predict();
    cv::Point predictPt(prediction.at<float>(0),prediction.at<float>(1));

    m_measurement(0) = x;
    m_measurement(1) = y;
    m_measurement(2) = vx;
    m_measurement(3) = vy;
    //cv::Point measPt(m_measurement(0),m_measurement(1));

    cv::Mat estimated = m_KF.correct(m_measurement);
    cv::Point statePt(estimated.at<float>(0),estimated.at<float>(1));

    return statePt;
}
