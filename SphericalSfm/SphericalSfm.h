#pragma once
#include <Eigen/Dense>
#include "SfmCommon.h"

namespace Ssfm {

	class SphericalSfm 
	{
	public:
		SphericalSfm()
			:inlier_threshold(2), trackwin(8), min_rot(3.0),
			optimizeFocal(true), softspherical(true), notranslation(false),
			lc_bestonly(false), lc_numbegin(30), lc_numend(30), lc_mininlier(100),
			intrinsic(0., 0., 0.),
			verbose(false)
		{}
		
		void set_intrinsics(double focal, double centerx, double centery) { intrinsic = Intrinsics(focal, centerx, centery); }
		void set_verbose(bool v) { verbose = v; }
		
	public:
		void run(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks)
		{
			build_feature_tracks(frames, masks);
			initialize_rotations();
			int loop_closure_count = make_loop_closures();
			refine_rotations();
			bundle_adjustment();
		}

	private:
		void build_feature_tracks(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks);

		void initialize_rotations();

		int make_loop_closures();

		void refine_rotations();

		void bundle_adjustment();

	private:
		// intermediate vairables
		Intrinsics intrinsic;

		vector<Eigen::Matrix3d> rotations;
		vector<Keyframe> keyframes;
		vector<ImageMatch> image_matches;

		// configuration
		double inlier_threshold;  // inlier threshold in pixels
		double min_rot;  // minimum rotation between keyframes
		int trackwin;  // window size while tracking in pixels
		bool optimizeFocal;
		bool softspherical;
		bool notranslation;

		bool lc_bestonly;
		int lc_numbegin; // Number of frames at beginning of sequence to use for loop closure
		int lc_numend; // Number of frames at end of sequence to use for loop closure
		int lc_mininlier;  // Minimum number of inliers to accept a loop closure"

		bool verbose;
	};
}