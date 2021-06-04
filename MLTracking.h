#pragma once
#include "GKalman.h"
#include <opencv2/core.hpp>

class MLTracking
{
public:
    MLTracking() {};
    virtual ~MLTracking() {};
    // predict the frameidx0 +/- 1
    cv::Rect predict(const cv::Rect& rect, int frameidx0, bool backward = false) {};

    void adjust() {};

private:
    double m_centerRadius;
    GKalman* kf_;
};
