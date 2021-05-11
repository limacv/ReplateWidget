#pragma once
#include "MLStepDataBase.h"
#include <opencv2/core.hpp>
#include <vector>

class MLStep2Data : public MLStepDataBase
{
public:
	MLStep2Data() {}
	virtual ~MLStep2Data() {};

public:
	cv::Mat background;
	std::vector<cv::Mat> warped_frames;
	std::vector<cv::Mat> warped_mask;
	std::vector<cv::Rect> rois;
};