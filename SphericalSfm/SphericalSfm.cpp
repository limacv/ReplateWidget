#include "SphericalSfm.h"
#include "EstimatorSpherical.h"
#include "SfmUtils.hpp"
#include "SphericalUtils.h"
#include "SparseDetectorTracker.h"
#include "so3.hpp"
#include "rotation_averaging.h"
#include "sfm.h"
#include <string>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>


using namespace Ssfm;
using namespace cv;
using namespace cv::detail;

/// <summary>
/// detect, track and compute feature point and ORB features
/// domain required:
///		intrinsic
/// domain written:
///		keyframes, image match
/// </summary>
void SphericalSfm::build_feature_tracks(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks)
{
	bool inward = false;

	const int totalframecount = frames.size();
	Eigen::Matrix3d Kinv = intrinsic.getKinv();

	/*************
	*  image0  => ...  => last_img => image1
	*   ^ last keyframe                 ^ current keyframe
	***************/
	cv::Mat image0, mask0;
	if (frames[0].channels() == 3) cv::cvtColor(frames[0], image0, cv::COLOR_BGR2GRAY);
	else image0 = frames[0].clone();

	mask0 = masks.empty() ? cv::Mat::ones(frames[0].size(), CV_8UC1) : masks[0].clone();

	SparseDetectorTracker detectortracker(8, trackwin);

	Features features0, last_features;
	detectortracker.detect(image0, features0, mask0);
	last_features = features0;

	if (verbose)
		visualize_fpts_match(features0, image0);

	keyframes.emplace_back(0, features0, image0);

	PreemptiveRANSAC<RayPairList, Estimator> ransac(100);
	//MSAC<RayPairList,Estimator> ransac;
	ransac.inlier_threshold = inlier_threshold * Kinv(0);///intrinsic.focal;

	vector<Estimator*> estimators(500);
	for (size_t i = 0; i < estimators.size(); i++)
		estimators[i] = new SphericalEstimator();

	int kf_index = 1;
	cv::Mat image1, mask1;
	cv::Mat last_image = image0;
	std::vector<uchar> status(features0.size(), 1); // initialize all status to successfully
	for (int frameidx = 1; frameidx < totalframecount; ++frameidx)
	{
		if (frames[frameidx].channels() == 3) cv::cvtColor(frames[frameidx], image1, cv::COLOR_BGR2GRAY);
		else image1 = frames[frameidx].clone();

		mask1 = masks.empty() ? cv::Mat::ones(frames[frameidx].size(), CV_8UC1) : masks[frameidx].clone();

		Features features1;
		Matches m01;

		// status -> KLTtrack return -> status ->     ...        -> status
		//                   |                                        |
		//                   +--> m01 -> epipolar RANSAC return -> inliers -> m01inliers
		detectortracker.track(last_image, image1, last_features, status, features1, m01, mask1);

		vector<bool> inliers; // inliers has same order as m01
		int ninliers;

		// find inlier matches using relative pose
		RayPairList ray_pair_list;
		ray_pair_list.reserve(m01.size());
		for (Matches::const_iterator it = m01.begin(); it != m01.end(); it++)
		{
			size_t index0 = it->first;
			size_t index1 = it->second;

			//Eigen::Vector3d loc0 = Eigen::Vector3d( features0[index0].x, features0[index0].y, 1 );
			//Eigen::Vector3d loc1 = Eigen::Vector3d( features1[index1].x, features1[index1].y, 1 );
			Eigen::Vector3d loc0 = Eigen::Vector3d(features0.points[index0].x, features0.points[index0].y, 1);
			Eigen::Vector3d loc1 = Eigen::Vector3d(features1.points[index1].x, features1.points[index1].y, 1);
			loc0 = Kinv * loc0;
			loc1 = Kinv * loc1;

			Ray u;
			u.head(3) = loc0;
			Ray v;
			v.head(3) = loc1;

			ray_pair_list.push_back(make_pair(u, v));
		}

		SphericalEstimator* best_estimator = NULL;
		cout << "running ransac on " << ray_pair_list.size() << " matches\n";

		ninliers = ransac.compute(ray_pair_list.begin(), ray_pair_list.end(), estimators, (Estimator**)&best_estimator, inliers);

		fprintf(stdout, "%d: %lu matches and %d inliers (%0.2f%%)\n", frameidx, m01.size(), ninliers, (double)ninliers / (double)m01.size() * 100);

		Eigen::Vector3d best_estimator_t;
		Eigen::Vector3d best_estimator_r;
		decompose_spherical_essential_matrix(best_estimator->E, false, best_estimator_r, best_estimator_t);
		cout << best_estimator_r.transpose() << "\n";

		Matches m01inliers;
		size_t i = 0;
		for (Matches::const_iterator it = m01.begin(); it != m01.end(); it++)
		{
			assert(it->first == it->second);
			if (inliers[i++])
				m01inliers[it->first] = it->second;
			else
				// update status -- is this good?
				status[it->first] = 0;

		}
		// test
		int inlierfromstatus = 0;
		for (auto& s : status)
			if (s == 1)
				inlierfromstatus++;
		assert(inlierfromstatus == ninliers && ninliers == m01inliers.size());

		if (verbose)
			visualize_fpts_match(features1, image1, features0, m01inliers);

		// check for minimum rotation OR if it's the last frame, force it to be a keyframe
		// Not a key frame, update tmp information
		if (best_estimator_r.norm() < min_rot * M_PI / 180. && (frameidx < totalframecount - 1))
		{
			last_features = features1;
		}
		// add a new key frame
		else
		{
			image0 = image1.clone();
			mask0 = mask1.clone();

			// only propagate inlier points
			Matches new_m01;
			features0 = Features();
			for (Matches::const_iterator it = m01inliers.begin(); it != m01inliers.end(); it++)
			{
				new_m01[it->first] = features0.size();
				//Feature &feature1(features1[it->second]);
				//features0.push_back(Feature(feature1.x,feature1.y,feature1.descriptor));
				features0.points.push_back(features1.points[it->second]);
				features0.descs.push_back(features1.descs.row(it->second));
			}

			// also detect new points that are some distance away from matched points
			// PLEASE don't detect new feature points if it's the last frame (new points will only have one observation!!!
			if (frameidx < totalframecount - 1)
				detectortracker.detect(image0, features0, mask0);
			cout << "features0 now has " << features0.size() << " features\n";

			keyframes.push_back(Keyframe(frameidx, features0));
			image0.copyTo(keyframes[keyframes.size() - 1].image);
			image_matches.push_back(ImageMatch(kf_index - 1, kf_index, new_m01, so3exp(best_estimator_r)));

			last_features = features0;
			status.resize(0);
			status.resize(features0.size(), 1);
			kf_index++;
		}
		last_image = image1.clone();
	}

	// collect garbage
	for (auto& ep : estimators)
		delete ep;
}


/// <summary>
/// domain required:
///		image_matches
/// domain written:
///		rotations
/// </summary>
void SphericalSfm::initialize_rotations()
{
	const int num_cameras = keyframes.size();
	rotations.resize(num_cameras);
	for (int i = 0; i < num_cameras; i++) rotations[i] = Eigen::Matrix3d::Identity();

	Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
	rotations[0] = R;
	for (int index = 1; index < num_cameras; index++)
	{
		for (int i = 0; i < image_matches.size(); i++)
		{
			if (image_matches[i].index0 == index - 1 && image_matches[i].index1 == index)
			{
				R = image_matches[i].R * R;
				rotations[index] = R;
				break;
			}
		}
	}
}


void match(const Features& features0, const Features& features1, Matches& m01, double ratio = 0.75);
/// <summary>
/// detect the loop closure
/// domain required:
///		intrinsics, keyframes
/// domain written:
///		image_matches
/// </summary>
/// <returns></returns>
int SphericalSfm::make_loop_closures()
{
	Eigen::Matrix3d Kinv = intrinsic.getKinv();
	//const double fov = 2 * atan2(intrinsics.centerx, intrinsics.centery);
	PreemptiveRANSAC<RayPairList, Estimator> ransac;//( 100 );
	//MSAC<RayPairList,Estimator> ransac;
	ransac.inlier_threshold = inlier_threshold * Kinv(0);///intrinsics.focal;

	vector<Estimator*> estimators(2000);
	for (size_t i = 0; i < estimators.size(); i++)
	{
		estimators[i] = new SphericalEstimator;
	}

	ImageMatch best_image_match(-1, -1, Matches(), Eigen::Matrix3d::Identity());
	int loop_closure_count = 0;

	int count0 = 0;
	for (int index0 = 0; index0 < keyframes.size(); index0++)
	{
		if (++count0 > lc_numbegin) break;

		const Features& features0 = keyframes[index0].features;
		//Features features0;
		//detectortracker.detect( keyframes[index0].imageg, features0 );

		int count1 = 0;
		for (int index1 = keyframes.size() - 1; index1 >= index0 + 1; index1--)
		{
			if (++count1 > lc_numend) break;

			//Eigen::Matrix3d relativerot = absrotations[index1] * absrotations[index0].inverse();
			//double theta = acos((relativerot.trace() - 1) / 2);
			//if (theta < acos(intrinsics.focal)) continue;
			if (index1 - index0 < 50) continue;

			const Features& features1 = keyframes[index1].features;
			//Features features1;
			//detectortracker.detect( keyframes[index1].imageg, features1 );

			Matches m01;

			match(features0, features1, m01);
			if (m01.size() < lc_mininlier) continue;

			//cv::Mat matchesim;
			//drawmatches( keyframes[index0].imageg, keyframes[index1].imageg, features0, features1, m01, matchesim );
			//cv::imwrite( "matches" + to_string(keyframes[index0].index) + "-" + to_string(keyframes[index1].index) + ".png", matchesim );

			vector<bool> inliers;
			int ninliers;

			// find inlier matches using relative pose
			RayPairList ray_pair_list;
			ray_pair_list.reserve(m01.size());
			for (Matches::const_iterator it = m01.begin(); it != m01.end(); it++)
			{
				cv::Point2f pt0 = features0.points[it->first];
				cv::Point2f pt1 = features1.points[it->second];

				Eigen::Vector3d loc0 = Eigen::Vector3d(pt0.x, pt0.y, 1);
				Eigen::Vector3d loc1 = Eigen::Vector3d(pt1.x, pt1.y, 1);

				loc0 = Kinv * loc0;
				loc1 = Kinv * loc1;

				Ray u;
				u.head(3) = loc0;
				Ray v;
				v.head(3) = loc1;

				ray_pair_list.push_back(make_pair(u, v));
			}

			SphericalEstimator* best_estimator = NULL;
			//SphericalFastEstimator *best_estimator = NULL;

			if (m01.empty()) ninliers = 0;
			else ninliers = ransac.compute(ray_pair_list.begin(), ray_pair_list.end(), estimators, (Estimator**)&best_estimator, inliers);
			fprintf(stdout, "%d %d: %lu matches and %d inliers (%0.2f%%)\n",
				keyframes[index0].index, keyframes[index1].index, m01.size(), ninliers, (double)ninliers / (double)m01.size() * 100);

			Matches m01inliers;
			size_t i = 0;
			for (Matches::const_iterator it = m01.begin(); it != m01.end(); it++)
			{
				if (inliers[i++]) m01inliers[it->first] = it->second;
			}

			//cv::Mat inliersim;
			//drawmatches( keyframes[index0].imageg, keyframes[index1].imageg, features0, features1, m01inliers, inliersim );
			//cv::imwrite( "inliers" + to_string(keyframes[index0].index) + "-" + to_string(keyframes[index1].index) + ".png", inliersim );

			if (ninliers > lc_mininlier)
			{
				Eigen::Vector3d best_estimator_t;
				Eigen::Vector3d best_estimator_r;
				decompose_spherical_essential_matrix(best_estimator->E, false, best_estimator_r, best_estimator_t);
				cout << best_estimator_r.transpose() << "\n";


				ImageMatch image_match(index0, index1, m01inliers, so3exp(best_estimator_r));
				if (ninliers > best_image_match.matches.size()) best_image_match = image_match;
				if (!lc_bestonly)
				{
					image_matches.push_back(image_match);
					loop_closure_count++;
				}
			}
		}
	}
	if (lc_bestonly)
	{
		if (best_image_match.matches.empty()) return 0;
		image_matches.push_back(best_image_match);
		return 1;
	}

	// collect garbage
	for (auto& ep : estimators)
		delete ep;
	return loop_closure_count;
}

/// <summary>
/// apply rotation average
///	domain required:
///		image_matches, rotations
/// domain written:
///		rotations
/// </summary>
void SphericalSfm::refine_rotations()
{
	vector<RelativeRotation> relative_rotations;
	for (int i = 0; i < image_matches.size(); i++)
	{
		relative_rotations.push_back(RelativeRotation(image_matches[i].index0, image_matches[i].index1, image_matches[i].R));
	}

	optimize_rotations(rotations, relative_rotations);
}

/// <summary>
/// domain required:
///		intrinsic, image_matches, rotations
/// 
/// </summary>
void SphericalSfm::bundle_adjustment()
{
	SfM sfm(intrinsic, optimizeFocal, notranslation);
	// buliding tracks
	vector< vector<int> > tracks(keyframes.size());
	for (int i = 0; i < keyframes.size(); i++)
	{
		tracks[i].resize(keyframes[i].features.size());
		for (int j = 0; j < keyframes[i].features.size(); j++)
		{
			tracks[i][j] = -1;
		}
	}

	// add cameras
	cout << "adding cameras\n";
	for (int index = 0; index < keyframes.size(); index++)
	{
		// camera extrinsic
		Pose curpose;
		if (notranslation)
			curpose = Pose(Eigen::Vector3d(0, 0, 0), so3ln(rotations[index]));
		else
			curpose = Pose(Eigen::Vector3d(0, 0, -1), so3ln(rotations[index]));
		int camera = sfm.AddCamera(curpose, keyframes[index].index);
		sfm.SetRotationFixed(camera, (index == 0));
		if (softspherical)
			sfm.SetTranslationFixed(camera, (index == 0)/*true*/); // Soft spherical constrain 
		else
			sfm.SetTranslationFixed(camera, true);
	}

	cout << "adding tracks\n";
	cout << "number of keyframes is " << keyframes.size() << "\n";

	// ===========================================
	// Keep track of all the point, merge the point across multi-view
	// Specifically, pack all the feature points and match relationships to sfm class
	//   and get all the point and observation
	// ===========================================
	for (int i = 0; i < image_matches.size(); i++)
	{
		const Matches& m01 = image_matches[i].matches;

		const int index0 = image_matches[i].index0;
		const int index1 = image_matches[i].index1;
		//cout << "match between keyframes " << index0 << " and " << index1 << " has " << m01.size() << " matches\n";

		const Features& features0 = keyframes[index0].features;
		const Features& features1 = keyframes[index1].features;

		// arrange all the points
		for (Matches::const_iterator it = m01.begin(); it != m01.end(); it++)
		{
			//const Feature &feature0 = features0[it->first];
			//const Feature &feature1 = features1[it->second];
			const cv::Point2f pt0 = features0.points[it->first];
			const cv::Point2f pt1 = features1.points[it->second];

			int& track0 = tracks[index0][it->first];
			int& track1 = tracks[index1][it->second];

			//Observation obs0( feature0.x-sfm.GetIntrinsics().centerx, feature0.y-sfm.GetIntrinsics().centery );
			//Observation obs1( feature1.x-sfm.GetIntrinsics().centerx, feature1.y-sfm.GetIntrinsics().centery );
			Observation obs0(pt0.x - sfm.GetIntrinsics().centerx, pt0.y - sfm.GetIntrinsics().centery);
			Observation obs1(pt1.x - sfm.GetIntrinsics().centerx, pt1.y - sfm.GetIntrinsics().centery);

			// this case means the previous keypoints is tracked (have an id),
			// so add the current observation and copy the id
			if (track0 != -1 && track1 == -1)
			{
				track1 = track0;
				sfm.AddObservation(index1, track0, obs1);
			}
			// this case means the previous keypoints is tracked
			// usually happened when there is a loop
			else if (track0 == -1 && track1 != -1)
			{
				track0 = track1;
				sfm.AddObservation(index0, track1, obs0);
			}
			// this case, the point is newly discovered, so should first assign a global ID
			// and then add two observations
			else if (track0 == -1 && track1 == -1)
			{
				//track0 = track1 = sfm.AddPoint( Eigen::Vector3d::Zero(), feature0.descriptor );
				track0 = track1 = sfm.AddPoint(Eigen::Vector4d::Zero(), features0.descs.row(it->first));

				sfm.SetPointFixed(track0, false);

				sfm.AddObservation(index0, track0, obs0);
				sfm.AddObservation(index1, track1, obs1);
			}
			// this case, the two point is all ready discovered, but was recognized as two points
			// this usually happened when there is a loop
			else if (track0 != -1 && track1 != -1 && (track0 != track1))
			{
				// track1 will be removed
				sfm.MergePoint(track0, track1);

				// update all features with track1 and set to track0
				for (int i = 0; i < tracks.size(); i++)
				{
					for (int j = 0; j < tracks[i].size(); j++)
					{
						if (tracks[i][j] == track1) tracks[i][j] = track0;
					}
				}
			}
		}
	}

	cout << "retriangulating...\n";
	auto wrongpoints = sfm.Retriangulate();

	// Bundle Adjustment
	sfm.Optimize();
	
}

void match(const Features& features0, const Features& features1, Matches& m01, double ratio)
{
	cv::BFMatcher matcher(cv::NORM_HAMMING, false);

	vector< vector<cv::DMatch> > matches;

	// p1 = query, p0 = train
	matcher.knnMatch(features1.descs, features0.descs, matches, 2);
	for (int i = 0; i < matches.size(); i++)
	{
		if (matches[i].empty()) continue;
		if (matches[i][0].distance < ratio * matches[i][1].distance)
		{
			//m01.insert(pair<size_t, size_t>(matches[i][0].trainIdx, matches[i][0].queryIdx));
			m01[matches[i][0].trainIdx] = matches[i][0].queryIdx;
		}
	}
}