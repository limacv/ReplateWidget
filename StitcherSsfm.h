#pragma once
#include "StitcherBase.h"
#include <opencv2/core.hpp>
#include <string>
#include <iostream>
#include "opencv2/stitching/detail/autocalib.hpp"
#include "opencv2/stitching/detail/blenders.hpp"
#include "opencv2/stitching/detail/timelapsers.hpp"
#include "opencv2/stitching/detail/camera.hpp"
#include "opencv2/stitching/detail/exposure_compensate.hpp"
#include "opencv2/stitching/detail/matchers.hpp"
#include "opencv2/stitching/detail/motion_estimators.hpp"
#include "opencv2/stitching/detail/seam_finders.hpp"
#include "opencv2/stitching/detail/warpers.hpp"
#include "opencv2/stitching/warpers.hpp"

class StitcherSsfm : public StitcherBase
{
public:
	StitcherSsfm()
		:warp_type("cylindrical"),
		seam_find_type("gc_color"),
		seam_scale(0.2),
		expos_comp_type(cv::detail::ExposureCompensator::GAIN_BLOCKS),
		expos_comp_nr_feeds(1),
		expos_comp_nr_filtering(2),
		expos_comp_block_size(32),
		blend_type(cv::detail::Blender::MULTI_BAND),
		blend_strength(5)
	{}
	virtual ~StitcherSsfm() {}

	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
	{
		int ret = align(frames, masks);
		ret = warp_and_composite(frames, masks);
		return 0;
	}

	virtual bool get_warped_frames(std::vector<cv::Mat>& frames, std::vector<cv::Mat>& masks, std::vector<cv::Rect>& windows)
	{
		frames = images_warped;
		masks = masks_warped;
		const int num_img = images_warped.size();
		windows.resize(num_img);
		for (int i = 0; i < images_warped.size(); ++i)
			windows[i] = cv::Rect(corners[i], sizes[i]);
	}
	virtual cv::Mat get_stitched_image() { return stitch_result; }

private:
	int align(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);
	int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);

private:  // intermediate variables
	std::vector<cv::Mat> images_warped;
	std::vector<cv::Mat> masks_warped;
	std::vector<cv::Size> sizes;
	std::vector<cv::Point> corners;

	std::vector<cv::detail::CameraParams> cameras;
	cv::Mat stitch_result;

private:  // configures
	std::string warp_type;
	std::string seam_find_type;
	float seam_scale;
	int expos_comp_type;
	int expos_comp_nr_feeds;
	int expos_comp_nr_filtering;
	int expos_comp_block_size;
	int blend_type;
	float blend_strength;
	// float compose_scale == 1;
	// float work_scale == 1;
};

