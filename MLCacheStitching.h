#pragma once
#include "MLDataStructure.h"
#include <opencv2/core.hpp>
#include <vector>

class MLProgressObserverBase;

class MLCacheStitching
{
public:
	MLCacheStitching()
	{}
	virtual ~MLCacheStitching() {};

	bool tryLoadBackground();
	bool tryLoadWarppedFrames(MLProgressObserverBase* observer = nullptr);
	bool tryLoadRois();
	bool tryLoadAllFromFiles(MLProgressObserverBase* observer = nullptr)
	{
		if (!tryLoadBackground())
			return false;
		if (!tryLoadWarppedFrames(observer))
			return false;
		if (!tryLoadRois())
			return false;
		return true;
	}

	bool saveBackground() const;
	bool saveWarppedFrames(MLProgressObserverBase* observer = nullptr) const;
	bool saveRois() const;
	bool saveAllToFiles(MLProgressObserverBase* observer = nullptr) const
	{
		if (!saveBackground())
			return false;
		if (!saveWarppedFrames(observer))
			return false;
		if (!saveRois())
			return false;
		return true;
	}

	bool isprepared() const;

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