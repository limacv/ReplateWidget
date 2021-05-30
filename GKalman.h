#ifndef GKALMAN_H
#define GKALMAN_H

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>


class GKalman
{
public:
    GKalman(int dynamParams, int measureParams, int controlParams = 0, int type = CV_32F);

    void begin(float x, float y, float vx, float vy);

    cv::Point update(float x, float y, float vx, float vy);

private:
    cv::KalmanFilter m_KF;
    cv::Mat_<float> m_state;
    cv::Mat m_processNoise;
    cv::Mat_<float> m_measurement;

};

#endif // GKALMAN_H
