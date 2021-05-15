#pragma once
#include <Eigen/Dense>
#include "SfmCommon.h"
#include "sfm.h"
#include <map>
#include <opencv2/stitching/detail/camera.hpp>

namespace Ssfm {

	struct SparseModel
	{
		std::vector<std::vector<cv::Point2f>> points2d;
		std::vector<std::vector<int>> pointsid;
		std::vector<cv::detail::CameraParams> poses;

		std::map<int, cv::Point3f> points3d;
	};

	class SphericalSfm 
	{
	public:
		SphericalSfm()
			:inlier_threshold(2), trackwin(8), min_rot(2.0),
			optimizeFocal(true), softspherical(true), notranslation(false), dynamicFocal(true),
			lc_bestonly(false), lc_numbegin(30), lc_numend(30), lc_mininlier(100),
			init_intrinsic(0., 0., 0.),
			verbose(false), model3d(nullptr)
		{}

		~SphericalSfm()
		{
			delete model3d; 
			model3d = nullptr;
		}
		
		void set_intrinsics(double focal, double centerx, double centery) { init_intrinsic = Intrinsics(focal, centerx, centery); }
		void set_verbose(bool v) { verbose = v; }
		void set_visualize_root(const std::string& path) { visualize_root = path; }
		void set_allow_translation(bool flag) { notranslation = !flag; }
		void set_optimize_translation(bool flag) { softspherical = flag; }
		
	public:
		void runSfM(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks)
		{
			build_feature_tracks(frames, masks);
			initialize_rotations();
			int loop_closure_count = make_loop_closures();
			refine_rotations();
			bundle_adjustment();

			if (verbose)
			{
				model3d->WritePointsOBJ(visualize_root + "/points.obj");
				model3d->WriteCameraCentersOBJ(visualize_root + "/cameras.obj");
			}
		}

		void track_all_frames(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks, SparseModel& outmodel);

	private:
		void build_feature_tracks(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks);

		void initialize_rotations();

		int make_loop_closures();

		void refine_rotations();

		void bundle_adjustment();

	private:
		// intermediate vairables
		Intrinsics init_intrinsic;

		std::vector<Eigen::Matrix3d> rotations;
		std::vector<Keyframe> keyframes;
		std::vector<ImageMatch> image_matches;
		
		SfM* model3d;

		// configuration
		double inlier_threshold;  // inlier threshold in pixels
		double min_rot;  // minimum rotation between keyframes
		int trackwin;  // window size while tracking in pixels
		bool optimizeFocal;
		bool softspherical;
		bool notranslation;
		bool dynamicFocal;

		bool lc_bestonly;
		int lc_numbegin; // Number of frames at beginning of sequence to use for loop closure
		int lc_numend; // Number of frames at end of sequence to use for loop closure
		int lc_mininlier;  // Minimum number of inliers to accept a loop closure"

		bool verbose;
		std::string visualize_root;
	};
}