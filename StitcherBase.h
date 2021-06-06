#pragma once
#include <opencv2/core.hpp>
#include <vector>

class MLProgressObserverBase;

class StitcherBase
{
public:
	StitcherBase() :progress_reporter(nullptr)
	{}
	virtual ~StitcherBase() {}

public:

	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks) = 0;
	virtual bool loadCameraParams(const std::string& path) = 0;
	virtual bool saveCameraParams(const std::string& path) = 0;
	virtual int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, 
		std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched) = 0;
	virtual int warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const = 0;

	void set_progress_observer(MLProgressObserverBase* observer) { progress_reporter = observer; }

protected:
	MLProgressObserverBase* progress_reporter;
};