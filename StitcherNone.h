#pragma once
#include "StitcherBase.h"
class StitcherNone :
    public StitcherBase
{

public:
    StitcherNone(const MLConfigStitcher* config)
        :StitcherBase(config)
    {}

    virtual ~StitcherNone() {}
    // Inherited via StitcherBase
    virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks) override;
    virtual int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched) override;
    virtual int warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const override;
};

