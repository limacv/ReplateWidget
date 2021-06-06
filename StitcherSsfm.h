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
		blend_scheme("blend"), // seam, blend
		seam_scale(0.1),
		expos_comp_type(cv::detail::ExposureCompensator::GAIN_BLOCKS),
		expos_comp_nr_feeds(1),
		expos_comp_nr_filtering(2),
		expos_comp_block_size(32),
		blend_type(cv::detail::Blender::MULTI_BAND),
		blend_strength(5)
	{}
	virtual ~StitcherSsfm() {}

	// user either run stitch or load camera parameter directly. The camera parameter is then used to warp and compose frames
	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);
	virtual bool loadCameraParams(const std::string& path) { return false; } // TODO: not implemented
	virtual bool saveCameraParams(const std::string& path) { return false; } // TODO: not implemented

	// after run stitch or laod camera parameters, run warping and composition
	virtual int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks,
		std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched);
	virtual int warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const;
private:
	int warp_and_compositebyseam(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, 
		std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped, 
		std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result);
	int warp_and_compositebyblend(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks,
		std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped,
		std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result);

private:  // intermediate variables
	std::vector<cv::detail::CameraParams> cameras;
	cv::Ptr<cv::detail::RotationWarper> warper;

private:  // configures
	std::string warp_type;
	std::string seam_find_type;
	float seam_scale;
	int expos_comp_type;
	int expos_comp_nr_feeds;
	int expos_comp_nr_filtering;
	int expos_comp_block_size;
	int blend_type;
	std::string blend_scheme;
	float blend_strength;
	// float compose_scale == 1;
	// float work_scale == 1;
	
};

