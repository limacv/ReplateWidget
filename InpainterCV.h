#pragma once
#include "InpainterBase.h"
class InpainterCV :
    public InpainterBase
{
public:
    InpainterCV(int inpaintRadius = 10): inpaintRadius(inpaintRadius) {}
    virtual ~InpainterCV() {}
    // Inherited via InpainterBase
    virtual void inpaint(const cv::Mat& rgba, cv::Mat& inpainted) override;
    virtual void inpaint(const cv::Mat& img, const cv::Mat& mask, cv::Mat& inpainted) override;

private:
    double inpaintRadius;
};

