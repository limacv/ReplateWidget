#pragma once
#include <opencv2/core.hpp>

class InpainterBase
{
public:
	InpainterBase() {}
	virtual ~InpainterBase() {}

	virtual void inpaint(const cv::Mat& rgba, cv::Mat& inpainted) = 0;
	virtual void inpaint(const cv::Mat& img, const cv::Mat& mask, cv::Mat& inpainted) = 0;
};

