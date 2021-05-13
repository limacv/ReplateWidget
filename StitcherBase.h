#pragma once
#include <opencv2/core.hpp>
#include <vector>

class StitcherBase
{
public:
	StitcherBase() {}
	virtual ~StitcherBase() {}

public:
	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks) = 0;
	virtual bool get_warped_frames(
		std::vector<cv::Mat>& frames_warpped,
		std::vector<cv::Rect>& windows
		) = 0;
	virtual cv::Mat get_stitched_image() = 0;
};