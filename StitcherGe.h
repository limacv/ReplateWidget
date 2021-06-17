#pragma once
#include "StitcherBase.h"
#include <vector>
#include <opencv2/opencv.hpp>
#include <opencv2/opencv_modules.hpp>
#include <opencv2/core/utility.hpp>
#include "opencv2/highgui/highgui.hpp"
#include <opencv2/stitching/detail/blenders.hpp>
#include "opencv2/stitching/detail/autocalib.hpp"
#include "opencv2/stitching/detail/camera.hpp"
#include "opencv2/stitching/detail/matchers.hpp"
#include "opencv2/stitching/detail/motion_estimators.hpp"
#include "opencv2/stitching/detail/seam_finders.hpp"
#include "opencv2/stitching/detail/util.hpp"
#include "opencv2/stitching/detail/warpers.hpp"
#include "opencv2/stitching/warpers.hpp"
#include "opencv2/xfeatures2d.hpp"
#include <qwidget.h>
#include "GStitchConifg.hpp"

using namespace cv;
using namespace cv::detail;
using namespace std;

class StitcherGe :
    public StitcherBase
{
public:
	StitcherGe(GStitchConfig* config)
        :work_scale(0),
        seam_scale(0),
        compose_scale(0),
        config_(config)
	{}
	virtual ~StitcherGe() 
    {
        m_warper.release();
    }

    // user either run stitch or load camera parameter directly. The camera parameter is then used to warp and compose frames
    virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks);
    virtual bool loadCameraParams(const std::string& path) { return false; } // TODO: not implemented
    virtual bool saveCameraParams(const std::string& path) { return false; } // TODO: not implemented

    // after run stitch or laod camera parameters, run warping and composition
    virtual int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks,
        std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched);
    virtual int warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const;

private:
    void featureFinder(const vector<Mat>& fullImages, vector<ImageFeatures>& features) const;

    void pairwiseMatch(const vector<ImageFeatures>& features,
        vector<MatchesInfo>& pairwise_matches) const;

    void loadCameras(const string& filename, vector<CameraParams>& cameras) const;

    void drawMatches(const vector<Mat3b>& fullImages, vector<ImageFeatures>& features,
        vector<MatchesInfo>& pairwise_matches) const;

    void _loadCameras(const string& filename, vector<CameraParams>& cameras) const;

    void _loadRotCameras(const string& filename, vector<CameraParams>& cameras) const;

    void saveCameras(const string& filename, const vector<CameraParams>& cameras) const;

    void saveCamerasAndMatch(const string& filename, const vector<CameraParams>& cameras,
        const vector<ImageFeatures>& features, const vector<MatchesInfo>& pairwise_matches) const;

    void restrictMatchNumber(MatchesInfo& info) const;

    void initialIntrinsics(const vector<ImageFeatures>& features,
        vector<MatchesInfo>& pairwise_matches,
        vector<CameraParams>& cameras) const;

    void bundleAdjust(const vector<ImageFeatures>& features,
        const vector<MatchesInfo>& pairwise_matches,
        vector<CameraParams>& cameras) const;
    void bundleAdjustCeres(const vector<ImageFeatures>& features,
        const vector<MatchesInfo>& pairwise_matches,
        vector<CameraParams>& cameras) const;

    void estimateWorkScale(double& work_scale, double megapix_ratio, const Size& full_img_size) const;
    void estimateSeamScale(double& seam_scale, double megapix_ratio, const Size& full_img_size) const;
    void estimateWarpScale(double& warp_scale, double megapix_ratio, const Size& full_img_size) const;

    void _pairwiseMatch1(const vector<ImageFeatures>& features,
        vector<MatchesInfo>& pairwise_matches) const;

    void _pairwiseMatch2(const vector<ImageFeatures>& features,
        vector<MatchesInfo>& pairwise_matches) const;

    void _logoFilter(const vector<Rect>& logoMask, ImageFeatures& features, float scale = 1.0) const;

    void _logoFilter(const vector<Rect>& logoMask, vector<ImageFeatures>& features, float scale = 1.0) const;

    void _simpleBlend(const Mat& src, const Mat& mask, const Point& tl, Mat& dst, Mat& dst_mask);

    void seamEstimate(const vector<Mat>& images_warped_f, const std::vector<Point>& corners,
        std::vector<Mat>& masks) const;

    int warp_and_compositebyblend(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks,
        std::vector<cv::Mat>& images_warped, std::vector<cv::Mat>& masks_warped,
        std::vector<cv::Size>& sizes, std::vector<cv::Point>& corners, cv::Mat& stitch_result);

    const GStitchConfig* config() const { return config_; }
private:
    vector<ImageFeatures> m_features;
    vector<MatchesInfo> m_pairwise_matches;
    
    Ptr<RotationWarper> m_warper;

    Size full_img_size;
    double work_scale;
    double seam_scale;
    double compose_scale;

    GStitchConfig* config_;
};