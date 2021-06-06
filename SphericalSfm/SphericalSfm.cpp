#include "SphericalSfm.h"
#include "SphericalSfm.h"
#include "EstimatorSpherical.h"
#include "SfmUtils.hpp"
#include "SphericalUtils.h"
#include "SparseDetectorTracker.h"
#include "so3.hpp"
#include "rotation_averaging.h"
#include <string>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/eigen.hpp>
#include <ceres/ceres.h>


using namespace Ssfm;
using namespace cv;
using namespace cv::detail;

CameraParams pose2camparam(const Pose& pose, const Intrinsics& intrin)
{
	CameraParams cam;
	cv::eigen2cv(pose.t, cam.t);
	cv::eigen2cv(static_cast<Eigen::Matrix3d>(so3exp(pose.r).transpose()), cam.R);
	cam.t.convertTo(cam.t, CV_32FC1);
	cam.R.convertTo(cam.R, CV_32FC1);
	cam.aspect = 1;
	cam.focal = intrin.focal;
	cam.ppx = intrin.centerx;
	cam.ppy = intrin.centery;
	return cam;
}

void Ssfm::SphericalSfm::track_all_frames(const vector<cv::Mat>& frames, const vector<cv::Mat>& masks, SparseModel& outmodel)
{
	const int totalframecount = frames.size();
	if (progress_observer) progress_observer->beginStage("Reconstruct remaining frames");

	outmodel.points2d.resize(totalframecount);
	outmodel.pointsid.resize(totalframecount);
	outmodel.poses.resize(totalframecount);

	cv::Mat image = frames[0].clone();
	if (image.channels() == 3) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);

	int xradius, yradius; yradius = xradius = trackwin;
	SparseDetectorTracker detectortracker;

	int camera0 = model3d->GetCameraIdxfromFrameIdx(0);
	vector<cv::Point2f> keypoints0;
	vector<int> ptids0;
	model3d->GetAllKpointsofCam(camera0, keypoints0, ptids0);

	if (verbose)
		visualize_fpts_andid(keypoints0, ptids0, image);

	cout << keypoints0.size() << " features in image 0\n";

	int bufferBeginIdx = 0;
	vector<cv::Mat> framesBuffer; // store images from image0 to image1
	vector<vector<cv::Point2f>> kpointsBuffer; // store keypoints list from image0 to image1
	vector<vector<int>> kpidsBuffer; // store point id from image0 to image1

	framesBuffer.push_back(image.clone());
	kpointsBuffer.push_back(keypoints0);
	kpidsBuffer.push_back(ptids0);
	
	for (int frame_idx = 1; frame_idx < totalframecount; ++frame_idx)
	{
		if (progress_observer) progress_observer->setValue(frame_idx / totalframecount);
		image = frames[frame_idx].clone();
		if (image.channels() == 3) cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);

		vector<cv::Point2f> keypoints1;
		auto& lastframe = framesBuffer[framesBuffer.size() - 1];
		auto& lastkeypoint = kpointsBuffer[kpointsBuffer.size() - 1];
		auto& lastkptid = kpidsBuffer[kpidsBuffer.size() - 1];
		vector<uchar> cur_status(lastkeypoint.size(), 1);
		detectortracker.track(lastframe, image, lastkeypoint, cur_status, keypoints1);

		// the status should all be successful
		int ninliers = 0;
		vector<cv::Point2f> tracked_kpoints; tracked_kpoints.reserve(keypoints1.size());
		vector<int> tracked_kpointsid; tracked_kpointsid.reserve(keypoints1.size());
		for (int i = 0; i < cur_status.size(); ++i)
		{
			if (cur_status[i] == 1)
			{
				ninliers++;
				tracked_kpoints.push_back(keypoints1[i]);
				tracked_kpointsid.push_back(lastkptid[i]); // same idx as ptids0
			}
		}
		cout << "tracked " << ninliers << " features in image " << frame_idx << "\n";
		assert(keypoints1.size() == tracked_kpoints.size());
		if (verbose)
			visualize_fpts_andid(tracked_kpoints, tracked_kpointsid, image);

		framesBuffer.push_back(image.clone());
		kpointsBuffer.push_back(tracked_kpoints);
		kpidsBuffer.push_back(tracked_kpointsid);
		if (model3d->IsKeyframe(frame_idx))
		{
			// features1 is tracked kpoints while newfeatures1 is the previously track+detect kpoints
			vector<cv::Point2f> newkeypoints;
			vector<int> newkpointsid;
			const int buffersize = framesBuffer.size();

			const int camera1 = model3d->GetCameraIdxfromFrameIdx(frame_idx);
			model3d->GetAllKpointsofCam(camera1, newkeypoints, newkpointsid);
			kpointsBuffer[buffersize - 1] = newkeypoints;
			kpidsBuffer[buffersize - 1] = newkpointsid;

			//find for extra keypoints and test whether the points matches
			vector<cv::Point2f> extrakeypoints;
			vector<int> extraptids;
			{
				std::unordered_map<int, cv::Point2f> kptid2pos;
				for (int i = 0; i < tracked_kpoints.size(); ++i)
					kptid2pos[tracked_kpointsid[i]] = tracked_kpoints[i];
				for (int i = 0; i < newkeypoints.size(); ++i)
				{
					auto posit = kptid2pos.find(newkpointsid[i]);
					if (posit == kptid2pos.end()) // not find, means it's new
					{
						extrakeypoints.push_back(newkeypoints[i]);
						extraptids.push_back(newkpointsid[i]);
					}
					else
					{
						auto errordis = cv::norm(posit->second - newkeypoints[i]);
						if (errordis > 0.0001) // tracking failed
							cout << "tracking failed for "
							<< "id " << posit->first << ", pos " << posit->second
							<< "newid " << newkpointsid[i] << ", pos " << newkeypoints[i]
							<< "err " << errordis << endl;
					}
				}
			}

			// track backwards
			std::vector<uchar> backwards_status(extrakeypoints.size(), 1);
			for (int bufferidx = buffersize - 1; bufferidx > 0; --bufferidx)
			{
				std::vector<cv::Point2f> curkpts;
				detectortracker.track(framesBuffer[bufferidx], framesBuffer[bufferidx - 1], extrakeypoints, backwards_status, curkpts);
				for (int i = 0; i < curkpts.size(); ++i)
				{
					if (backwards_status[i] == 1)
					{
						kpointsBuffer[bufferidx - 1].push_back(curkpts[i]);
						kpidsBuffer[bufferidx - 1].push_back(extraptids[i]);
					}
				}
				if (verbose)
					visualize_fpts_andid(kpointsBuffer[bufferidx - 1], kpidsBuffer[bufferidx - 1], framesBuffer[bufferidx - 1], "backwards");
				extrakeypoints = curkpts;
			}
			//cv::destroyWindow("backwards");
			// apply pose refine (rotation averaging)
			vector<Intrinsics> intrinBuffer; intrinBuffer.reserve(buffersize);
			vector<Eigen::Vector3d> rotsBuffer; rotsBuffer.reserve(buffersize);
			vector<Eigen::Vector3d> transBuffer; transBuffer.reserve(buffersize);
			vector<double> ave_reproject_errors; ave_reproject_errors.reserve(buffersize);
			{
				cout << "computing pose for non-keyframe" << endl;
				/********
				* Scheme1: interpolate the rotation using rotation_averaging algorithm + linear interpolate translation
				******/
				auto pose0 = model3d->GetPose(camera0),
					pose1 = model3d->GetPose(camera1);
				const auto intrin0 = model3d->GetIntrinsics(camera0),
					intrin1 = model3d->GetIntrinsics(camera1);
				//rotsBuffer.insert(rotsBuffer.begin(), cache.allrotation.begin() + bufferBeginIdx, cache.allrotation.begin() + bufferBeginIdx + buffersize)
				// Todo: this function has some bug
				//optimize_rotations_my(rotsBuffer, pose0.r, pose1.r);

				/********
				* Scheme2: linear interpolate the rotation and translation
				******/
				//Eigen::Vector3d stepr = (pose1.r - pose0.r) / (buffersize - 1);
				//for (int i = 0; i < buffersize; ++i)
				//    rotsBuffer[i] = (pose0.r + i * stepr);

				// linear interpolate the translation
				//Eigen::Vector3d step = (pose1.t - pose0.t) / (buffersize - 1);
				//for (int i = 0; i < buffersize; ++i)
				//    transBuffer.push_back(pose0.t + i * step);

				/********
				* Scheme3: minimize the reprojection error
				******/
				// initialize with first pose
				rotsBuffer.resize(buffersize / 2, pose0.r);
				transBuffer.resize(buffersize / 2, pose0.t);
				intrinBuffer.resize(buffersize / 2, intrin0);
				rotsBuffer.resize(buffersize, pose1.r);
				transBuffer.resize(buffersize, pose1.t);
				intrinBuffer.resize(buffersize, intrin1);
				for (int bufferi = 0; bufferi < buffersize - 1; ++bufferi)
				{
					const auto& pt2ds = kpointsBuffer[bufferi];
					const auto& pt3dids = kpidsBuffer[bufferi];
					
					std::vector<Pointhomo> pt3dpos; pt3dpos.reserve(pt3dids.size());
					for (auto& id : pt3dids)
						pt3dpos.push_back(model3d->GetPoint(id));

					ceres::Problem problem;
					ceres::LossFunction* lossfunc = new ceres::SoftLOneLoss(2.);
					for (int pti = 0; pti < pt2ds.size(); ++pti)
					{
						if (pt3dpos[pti] == Pointhomo(0, 0, 0, 1) || pt3dpos[pti] == Pointhomo(0, 0, 0, 0))
							continue;
						//ReprojectionErrorhomo* reproj_error = new ReprojectionErrorhomo(focal, pt2ds[pti].x - centerx, pt2ds[pti].y - centery);
						//ceres::CostFunction* costfunc = new ceres::AutoDiffCostFunction<ReprojectionErrorhomo, 2, 3, 3, 4>(reproj_error);
						//problem.AddResidualBlock(costfunc, lossfunc, transBuffer[bufferi].data(), rotsBuffer[bufferi].data(), pt3dpos[pti].data());
						//problem.SetParameterBlockConstant(pt3dpos[pti].data());
						ReprojErrorhomo_optfocal* reproj_error = new ReprojErrorhomo_optfocal(pt2ds[pti].x - intrinBuffer[bufferi].centerx, pt2ds[pti].y - intrinBuffer[bufferi].centery);
						ceres::CostFunction* costfunc = new ceres::AutoDiffCostFunction<ReprojErrorhomo_optfocal, 2, 3, 3, 4, 1>(reproj_error);
						problem.AddResidualBlock(costfunc, lossfunc, transBuffer[bufferi].data(), rotsBuffer[bufferi].data(), pt3dpos[pti].data(), &intrinBuffer[bufferi].focal);
						problem.SetParameterBlockConstant(pt3dpos[pti].data());
						if (notranslation)
							problem.SetParameterBlockConstant(transBuffer[bufferi].data());
						if (!optimizeFocal || !dynamicFocal)
							problem.SetParameterBlockConstant(&intrinBuffer[bufferi].focal);
					}

					ceres::Solver::Options solveropt;
					solveropt.max_num_iterations = 500;
					solveropt.minimizer_type = ceres::TRUST_REGION;
					solveropt.logging_type = ceres::LoggingType::SILENT;
					//solveropt.minimizer_progress_to_stdout = true;
					solveropt.num_threads = 12;
					ceres::Solver::Summary summary;
					ceres::Solve(solveropt, &problem, &summary);
					//std::cout << summary.FullReport() << std::endl;
					// get reproject loss after optimization
					double finalcost = summary.final_cost * 2;
					finalcost /= pt2ds.size();
					finalcost = ceres::sqrt(finalcost);
					ave_reproject_errors.push_back(finalcost);
					if (summary.termination_type == ceres::FAILURE)
					{
						std::cout << "error: ceres failed when solve pose for frame " << bufferi + bufferBeginIdx;
						throw runtime_error("ceres error");
					}
				}
			}
			camera0 = camera1;

			// save the buffer to the file 
			const cv::Scalar red(0, 0, 255), darkred(0, 0, 100),
				blue(255, 0, 0), darkblue(100, 0, 0),
				green(0, 255, 0), darkgreen(0, 255, 0);
			for (int i = 0; i < buffersize - 1; ++i)
			{
				// key points position and global ids
				auto& pt2ds = kpointsBuffer[i];
				auto& pt3dids = kpidsBuffer[i];
				Pose curpose(transBuffer[i], rotsBuffer[i]);
				Intrinsics curintrin = intrinBuffer[i];
				int frameindex = bufferBeginIdx + i;

				std::vector<bool> inlier(pt2ds.size(), true); int inliernum = pt2ds.size();
				// detect inlier based on reproject error and visualize
				{
					const auto& curK = curintrin.getK();
					cv::Mat display = framesBuffer[i].clone();
					cv::cvtColor(display, display, cv::COLOR_GRAY2BGR);
					auto it1 = pt2ds.begin(); auto it2 = pt3dids.begin();
					for (; it1 != pt2ds.end(); ++it1, ++it2)
					{
						const int index = std::distance(pt2ds.begin(), it1);

						auto numobs = model3d->GetNumObservationOfPoint(*it2);
						// compute reproject error
						Pointhomo thispt3d = model3d->GetPoint(*it2);
						if (thispt3d == Pointhomo(0, 0, 0, 1) || thispt3d == Pointhomo(0, 0, 0, 0)) // if not exist or not optimized
						{
							inlier[index] = false; inliernum--;
							continue;
						}

						thispt3d = curpose.P * thispt3d;
						auto depth = thispt3d[2] / thispt3d[3];
						Point thispt2d_proj = curK * thispt3d.head(3);
						cv::Point2f pt2d_proj(thispt2d_proj[0] / thispt2d_proj[2], thispt2d_proj[1] / thispt2d_proj[2]);

						auto reprojecterror = cv::norm(*it1 - pt2d_proj);

						// when depth < 1. the point invisiable
						// when projecterror > threshold, the point is invalid
						cv::circle(display, *it1, 2 /*numobs*/, green);
						cv::line(display, *it1, pt2d_proj, blue);
						if (reprojecterror > ave_reproject_errors[i] + 5 || depth < 1.)
						{
							inlier[index] = false;
							inliernum--;
							cv::circle(display, pt2d_proj, 6, darkred, 2);
						}
						else
							cv::circle(display, pt2d_proj, 2, red);
					}
				}
				std::vector<cv::Point2f> pt2ds_inlier; pt2ds_inlier.reserve(inliernum);
				std::vector<int> pt3dids_inlier; pt3dids_inlier.reserve(inliernum);
				for (int pti = 0; pti < inlier.size(); ++pti)
				{
					if (inlier[pti])
					{
						pt2ds_inlier.push_back(pt2ds[pti]);
						pt3dids_inlier.push_back(pt3dids[pti]);
					}
				}
				std::cout << "frame " << frameindex << " finally got " << inliernum << " inlier points" << std::endl;

				outmodel.points2d[frameindex] = pt2ds_inlier;
				outmodel.pointsid[frameindex] = pt3dids_inlier;
				outmodel.poses[frameindex] = pose2camparam(curpose, curintrin);
			}

			// update all the information for next frame
			kpointsBuffer[0] = kpointsBuffer[buffersize - 1]; kpointsBuffer.resize(1);
			kpidsBuffer[0] = kpidsBuffer[buffersize - 1]; kpidsBuffer.resize(1);
			framesBuffer[0] = framesBuffer[buffersize - 1]; framesBuffer.resize(1);
			rotsBuffer[0] = rotsBuffer[buffersize - 1]; rotsBuffer.resize(1);
			transBuffer[0] = transBuffer[buffersize - 1]; transBuffer.resize(1);
			intrinBuffer[0] = intrinBuffer[buffersize - 1]; intrinBuffer.resize(1);
			bufferBeginIdx = frame_idx;
			//last_status.resize(0);
			//last_status.resize(kpointsBuffer[0].size(), 1);

			if (frame_idx == totalframecount - 1) // lastframe
			{
				outmodel.points2d[frame_idx] = kpointsBuffer[0];
				outmodel.pointsid[frame_idx] = kpidsBuffer[0];
				//outmodel.poses[frame_idx] = pose2camparam(model3d->GetPose(camera1), model3d->GetIntrinsics());
				outmodel.poses[frame_idx] = pose2camparam(Pose(transBuffer[0], rotsBuffer[0]), intrinBuffer[0]);
			}
		}
	}

	// store global information
	vector<Pointhomo> allpt3dpos; vector<int> allpt3did;
	model3d->GetAllPointsHomo(allpt3dpos, allpt3did);
	const int allptnum = allpt3dpos.size();

	for (int i = 0; i < allptnum; ++i)
	{
		auto& pt3d = allpt3dpos[i];
		outmodel.points3d[allpt3did[i]] = Point3f(pt3d[0] / pt3d[3], pt3d[1] / pt3d[3], pt3d[2] / pt3d[3]);
	}
	if (progress_observer) progress_observer->setValue(1.f);
}

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
	if (progress_observer) progress_observer->beginStage("Analyzing Frames");
	const int totalframecount = frames.size();
	Eigen::Matrix3d Kinv = init_intrinsic.getKinv();

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
		if (progress_observer) progress_observer->setValue((float)frameidx / totalframecount);
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
	if (progress_observer) progress_observer->setValue(1.f);
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
	Eigen::Matrix3d Kinv = init_intrinsic.getKinv();
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
	if (progress_observer) progress_observer->beginStage("Reconstruct Key Frames");

	model3d = new SfM(init_intrinsic, optimizeFocal, notranslation, !dynamicFocal);
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
		int camera = model3d->AddCamera(curpose, keyframes[index].index);
		model3d->SetRotationFixed(camera, (index == 0));
		if (softspherical)
			model3d->SetTranslationFixed(camera, (index == 0)/*true*/); // Soft spherical constrain 
		else
			model3d->SetTranslationFixed(camera, true);
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
		if (progress_observer) progress_observer->setValue(0.2 * i / image_matches.size());
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
			Observation obs0(pt0.x - model3d->GetGlobalIntrinsics().centerx, pt0.y - model3d->GetGlobalIntrinsics().centery);
			Observation obs1(pt1.x - model3d->GetGlobalIntrinsics().centerx, pt1.y - model3d->GetGlobalIntrinsics().centery);

			// this case means the previous keypoints is tracked (have an id),
			// so add the current observation and copy the id
			if (track0 != -1 && track1 == -1)
			{
				track1 = track0;
				model3d->AddObservation(index1, track0, obs1);
			}
			// this case means the previous keypoints is tracked
			// usually happened when there is a loop
			else if (track0 == -1 && track1 != -1)
			{
				track0 = track1;
				model3d->AddObservation(index0, track1, obs0);
			}
			// this case, the point is newly discovered, so should first assign a global ID
			// and then add two observations
			else if (track0 == -1 && track1 == -1)
			{
				//track0 = track1 = sfm.AddPoint( Eigen::Vector3d::Zero(), feature0.descriptor );
				track0 = track1 = model3d->AddPoint(Eigen::Vector4d::Zero(), features0.descs.row(it->first));

				model3d->SetPointFixed(track0, false);

				model3d->AddObservation(index0, track0, obs0);
				model3d->AddObservation(index1, track1, obs1);
			}
			// this case, the two point is all ready discovered, but was recognized as two points
			// this usually happened when there is a loop
			else if (track0 != -1 && track1 != -1 && (track0 != track1))
			{
				// track1 will be removed
				model3d->MergePoint(track0, track1);

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
	auto wrongpoints = model3d->Retriangulate();
	if (progress_observer) progress_observer->setValue(0.3);

	// Bundle Adjustment
	model3d->Optimize();
	if (progress_observer) progress_observer->setValue(1.f);
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