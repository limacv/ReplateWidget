#pragma once
#include "MLStepDataBase.h"
#include <opencv2/core.hpp>
#include <vector>

class MLStep2Data : public MLStepDataBase
{
public:
	MLStep2Data() {}
	virtual ~MLStep2Data() {};

	bool tryLoadStitchingFromFiles();
	bool saveToFiles() const;
	bool isprepared() const;

private:
	void update_global_roi()
	{
		if (rois.empty()) return;
		global_roi = rois[0];
		for (const auto& roi : rois) global_roi |= roi;
	}

public:
	cv::Mat background;  // RGBA images
	std::vector<cv::Mat> warped_frames;  // RGBA images
	std::vector<cv::Rect> rois;
	cv::Rect global_roi;
};