#pragma once
#include "StitcherBase.h"
#include <string>
#include <opencv2/core.hpp>
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

class StitcherCV2: public StitcherBase
{
public:
	StitcherCV2()
		:try_cuda(false),
		work_megapix(0.6),
		seam_megapix(0.2),
		compose_megapix(-1),
		ba_conf_thresh(1.),
		features_type("surf"),
		match_conf(0.65),
		matcher_type("homography"),
		estimator_type("homography"),
		ba_cost_func("ray"),
		ba_refine_mask("xxxxx"),
		do_wave_correct(false),
		wave_correct(cv::detail::WAVE_CORRECT_HORIZ),
		warp_type("cylindrical"),
		expos_comp_type(cv::detail::ExposureCompensator::GAIN_BLOCKS),
		expos_comp_nr_feeds(1),
		expos_comp_nr_filtering(2),
		expos_comp_block_size(32),
		seam_find_type("gc_color"),
		blend_type(cv::detail::Blender::MULTI_BAND),
		blend_strength(5),
		range_width(-1),

		work_scale(1),
		is_work_scale_set(false),
		seam_work_aspect(1),
		warped_image_scale(-1)
	{}

public:
	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks)
	{
		build_feature_matches(frames, masks);
		int ret = bundle_adjustment();
		ret = warp_and_composite(frames, masks);
		return 0;
	}
	
	virtual bool get_warped_frames(std::vector<cv::Mat>& frames, std::vector<cv::Rect>& windows) const ;
	virtual cv::Mat get_stitched_image() const { return stitch_result; }
	virtual bool get_warped_rects(const int frameidx, std::vector<cv::Rect>& inoutboxes) const { return false; }
private:
	void build_feature_matches(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);
	int bundle_adjustment();
	int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);

private:
	// intermediate variable
	double work_scale;
	bool is_work_scale_set;
	double seam_work_aspect;
	double warped_image_scale;

	std::vector<cv::detail::ImageFeatures> features;
	std::vector<cv::detail::MatchesInfo> pairwise_matches;
	std::vector<cv::detail::CameraParams> cameras;

	std::vector<cv::UMat> images_warped;
	std::vector<cv::UMat> masks_warped;
	std::vector<cv::Size> sizes;
	std::vector<cv::Point> corners;

	cv::Mat stitch_result;
private:
	// configures
	bool try_cuda;
	double work_megapix;
	double seam_megapix;
	double compose_megapix;

	std::string features_type;
	float match_conf;
	float ba_conf_thresh;
	std::string ba_cost_func;
	std::string ba_refine_mask;
	bool do_wave_correct;

	cv::detail::WaveCorrectKind wave_correct;
	std::string matcher_type;
	std::string estimator_type;
	std::string warp_type;

	int expos_comp_type;
	int expos_comp_nr_feeds;
	int expos_comp_nr_filtering;
	int expos_comp_block_size;
	std::string seam_find_type;

	int blend_type;
	float blend_strength;
	int range_width;
};