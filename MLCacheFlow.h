#pragma once
#include <vector>
#include <opencv2/core.hpp>

class MLCacheFlow
{
public:
	MLCacheFlow(const std::vector<cv::Rect>& roisp, const cv::Rect& global_roip)
		:roisp(roisp), global_roip(global_roip)
	{}

	bool saveFlows() const;
	bool tryLoadFlows();
	bool isprepared() const;

	bool clear()
	{
		flows.clear();
	}

	std::vector<cv::Mat> flows;
	const std::vector<cv::Rect>& roisp;
	const cv::Rect& global_roip;
};

