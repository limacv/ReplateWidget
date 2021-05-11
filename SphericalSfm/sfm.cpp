#include "sfm.h"
#include "so3.hpp"

#include <ceres/ceres.h>
#include <ceres/loss_function.h>
#include <ceres/autodiff_cost_function.h>

#include <Eigen/Jacobi>
#include <Eigen/SVD>
#include <Eigen/LU>

#include <iostream>
#include <fstream>
#include <mutex>
#include <opencv2/core/utility.hpp>


namespace Ssfm {

	class ParallelTriangulator : public cv::ParallelLoopBody
	{
	public:
		ParallelTriangulator(SfM& _sfm) :sfm(_sfm) { }
		virtual void operator()(const cv::Range& range) const CV_OVERRIDE
		{
			for (int j = range.start; j < range.end; j++)
			{
				if (!sfm.points.exists(j)) continue;

				int firstcam = -1;
				int lastcam = -1;

				int nobs = 0;
				for (int i = 0; i < sfm.numCameras; i++)
				{
					if (!sfm.cameras.exists(i)) continue;
					if (!(sfm.observations.exists(i, j))) continue;
					if (firstcam == -1) firstcam = i;
					lastcam = i;
					nobs++;
				}

				sfm.SetPoint(j, Eigen::Vector4d(0, 0, 0, 1)); // MA Li: pointhomo
				if (nobs < 3) continue;

				Eigen::MatrixXd A(nobs * 2, 4);

				int n = 0;
				for (int i = 0; i < sfm.numCameras; i++)
				{
					if (!sfm.cameras.exists(i)) continue;
					if (!(sfm.observations.exists(i, j))) continue;

					Observation vec = sfm.observations(i, j);

					Eigen::Vector2d point(vec(0) / sfm.intrinsics.focal, vec(1) / sfm.intrinsics.focal);
					Eigen::Matrix4d P = sfm.GetPose(i).P;

					A.row(2 * n + 0) = P.row(2) * point[0] - P.row(0);
					A.row(2 * n + 1) = P.row(2) * point[1] - P.row(1);
					n++;
				}

				Eigen::JacobiSVD<Eigen::MatrixXd> svdA(A, Eigen::ComputeFullV);
				Eigen::Vector4d Xh = svdA.matrixV().col(3);
				Eigen::Vector3d X = Xh.head(3) / Xh(3);

				sfm.SetPoint(j, Xh); // MA Li: pointhomo
			}
		}
		ParallelTriangulator& operator=(const ParallelTriangulator&) {
			return *this;
		}
	private:
		SfM& sfm;
	};

	double* SfM::GetCameraPtr(int camera)
	{
		return (double*)&cameras(camera);
	}

	int SfM::GetCameraIdxfromFrameIdx(int camera)
	{
		return frameIdx2camIdx(camera);
	}
	bool SfM::IsKeyframe(int frameidx)
	{
		return frameIdx2camIdx.exists(frameidx);
	}

	double* SfM::GetPointPtr(int point)
	{
		return (double*)&points(point);
	}

	bool SfM::GetAllObservationofCam(const int camera, std::vector<Observation>& obses, std::vector<int>& ptidx)
	{
		if (!cameras.exists(camera)) return false;
		SparseVector<Observation> obsofcamera = observations.at(camera);
		obses.reserve(numPoints * 3 / numCameras);
		ptidx.reserve(numPoints * 3 / numCameras);
		for (int i = 0; i < numPoints; ++i)
		{
			if (obsofcamera.exists(i))
			{
				obses.push_back(obsofcamera(i));
				ptidx.push_back(i);
			}
		}
		return true;
	}

	bool SfM::GetAllKpointsofCam(const int camera, std::vector<cv::Point2f>& obses, std::vector<int>& ptidx)
	{
		if (!cameras.exists(camera)) return false;
		SparseVector<Observation> obsofcamera = observations.at(camera);
		obses.reserve(numPoints * 3 / numCameras);
		ptidx.reserve(numPoints * 3 / numCameras);
		for (int i = 0; i < numPoints; ++i)
		{
			if (obsofcamera.exists(i))
			{
				const auto& obs = obsofcamera(i);
				cv::Point2f pt(obs.x() + intrinsics.centerx, obs.y() + intrinsics.centery);
				obses.push_back(pt);
				ptidx.push_back(i);
			}
		}
		return true;
	}

	int SfM::AddCamera(const Pose& initial_pose, const int videoIdx)
	{
		nextCamera++;
		numCameras++;

		cameras(nextCamera).head(3) = initial_pose.t;
		cameras(nextCamera).tail(3) = initial_pose.r;
		frameIdx2camIdx(videoIdx) = nextCamera;
		rotationFixed(nextCamera) = false;
		translationFixed(nextCamera) = false;

		return nextCamera;
	}

	int SfM::AddPoint(const Pointhomo& initial_position, const cv::Mat& descriptor)
	{
		numPoints++;

		points(nextPoint) = initial_position;
		pointFixed(nextPoint) = false;

		pointObsnum(nextPoint) = 0;

		cv::Mat descriptor_copy;
		descriptor.copyTo(descriptor_copy);
		descriptors(nextPoint) = descriptor_copy;

		return nextPoint++;
	}

	void SfM::MergePoint(int point1, int point2)
	{
		for (int i = 0; i < numCameras; i++)
		{
			if (!cameras.exists(i)) continue;
			if (!(observations.exists(i, point2))) continue;

			observations(i, point1) = observations(i, point2);
		}
		pointObsnum(point1) += pointObsnum(point2);

		RemovePoint(point2);
	}

	void SfM::AddObservation(int camera, int point, const Observation& observation)
	{
		observations(camera, point) = observation;
		pointObsnum(point) += 1;
	}

	//void SfM::AddMeasurement( int i, int j, const Pose &measurement )
	//{
	//    measurements(i,j) = measurement;
	//}

	//bool SfM::GetMeasurement( int i, int j, Pose &measurement )
	//{
	//    if ( !measurements.exists(i,j) ) return false;

	//    measurement = measurements(i,j);
	//    
	//    return true;
	//}

	bool SfM::GetObservation(int camera, int point, Observation& observation)
	{
		if (!observations.exists(camera, point)) return false;

		observation = observations(camera, point);

		return true;
	}

	size_t SfM::GetNumObservationOfPoint(int point)
	{
		return pointObsnum(point);
	}

	std::vector<int> SfM::Retriangulate()
	{
		std::vector<int> wrongpoints;
		std::mutex wrongptmutex;
		cv::parallel_for_(cv::Range(0, numPoints), [&](const cv::Range& range) {
			//for ( int j = 0; j < numPoints; j++ )
			for (int j = range.start; j < range.end; j++)
			{
				if (!points.exists(j)) continue;

				int firstcam = -1;
				int lastcam = -1;

				int nobs = 0;
				for (int i = 0; i < numCameras; i++)
				{
					if (!cameras.exists(i)) continue;
					if (!(observations.exists(i, j))) continue;
					if (firstcam == -1) firstcam = i;
					lastcam = i;
					nobs++;
				}

				SetPoint(j, Eigen::Vector4d(0, 0, 0, 1));
				if (nobs < 3) continue;

				Eigen::MatrixXd A(nobs * 2, 4);

				// Theory:
				// point in world: X
				// camera extrinsic matrix: P = [p1, p2, p3, p4]^T
				// observed position in camera (normalized space): point = [x, y, 1]^T
				// we have 
				//      1. p1*X / p3*X = x
				//      2. p2*X / p3*X = y
				// so we have use DLT to solve the X
				int n = 0;
				std::vector<size_t> cameraspt;
				for (int i = 0; i < numCameras; i++)
				{
					if (!cameras.exists(i)) continue;
					if (!(observations.exists(i, j))) continue;

					Observation vec = observations(i, j);

					Eigen::Vector2d point(vec(0) / intrinsics.focal, vec(1) / intrinsics.focal); // corresponding 3d point(ray): (point.x, point.y, 1) * lambda
					Eigen::Matrix4d P = GetPose(i).P;

					A.row(2 * n + 0) = P.row(2) * point[0] - P.row(0);
					A.row(2 * n + 1) = P.row(2) * point[1] - P.row(1);
					n++;
					cameraspt.push_back(i);
				}

				Eigen::Vector4d Xh;
				if (!notranslation) // normal triangulation
				{
					Eigen::JacobiSVD<Eigen::MatrixXd> svdA(A, Eigen::ComputeFullV);
					Xh = svdA.matrixV().col(3);
				}
				else
				{
					// if no translation, then P.col(3) == 1, so A.col(3) == 0
					// we no more care about homo cooridnate w, so just use the first 3 row
					Eigen::JacobiSVD<Eigen::MatrixXd> svdA(A.leftCols(3), Eigen::ComputeFullV);
					Xh.head(3) = svdA.matrixV().col(2);
					Xh[3] = 0.1;
				}

				// reproject
				/*for (int i : cameraspt)
				{
					Eigen::Vector4d proj = GetPose(i).P * Xh;
					double depth = proj[2] / proj[3];
					if (depth < -1)
					{
						Xh[3] = -Xh[3];
						std::lock_guard<std::mutex> lockprint(wrongptmutex);
						wrongpoints.push_back(j);
						break;
					}
				}*/

				// MA Li: no use, please delete afterwards
				Eigen::Vector3d X = Xh.head(3) / Xh(3);
				SetPoint(j, Xh);
			}
			});
		return wrongpoints;
	}

	void SfM::PreOptimize()
	{
		loss_function = new ceres::CauchyLoss(2.0);
	}

	void SfM::ConfigureSolverOptions(ceres::Solver::Options& options)
	{
		options.minimizer_type = ceres::TRUST_REGION;
		options.linear_solver_type = ceres::SPARSE_SCHUR;
		options.max_num_iterations = 1000;
		options.max_num_consecutive_invalid_steps = 100;
		options.minimizer_progress_to_stdout = true;
		options.num_threads = 12;
	}

	void SfM::AddResidual(ceres::Problem& problem, int camera, int point)
	{
		Observation vec = observations(camera, point);

		//ReprojectionError* reproj_error = new ReprojectionError(intrinsics.focal, vec(0), vec(1), GetPoint(point)[3]);
		//ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<ReprojectionError, 2, 3, 3, 3>(reproj_error);

		/*ReprojectionErrorhomo *reproj_error = new ReprojectionErrorhomo(intrinsics.focal,vec(0),vec(1));
		ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<ReprojectionErrorhomo, 2, 3, 3, 4>(reproj_error);
		problem.AddResidualBlock(cost_function, loss_function, GetCameraPtr(camera), GetCameraPtr(camera)+3, GetPointPtr(point) );*/

		double weight = std::min(10. / pointObsnum(point), 3.);
		ReprojErrhomowithWeight* reproj_err = new ReprojErrhomowithWeight(intrinsics.focal, vec(0), vec(1), weight);
		ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<ReprojErrhomowithWeight, 2, 3, 3, 4>(reproj_err);
		problem.AddResidualBlock(cost_function, loss_function, GetCameraPtr(camera), GetCameraPtr(camera) + 3, GetPointPtr(point));

		if (translationFixed(camera))
			problem.SetParameterBlockConstant(GetCameraPtr(camera));
		if (rotationFixed(camera))
			problem.SetParameterBlockConstant(GetCameraPtr(camera) + 3);
		if (pointFixed(point))
			problem.SetParameterBlockConstant(GetPointPtr(point));
	}

	void SfM::AddResidualWithFocal(ceres::Problem& problem, int camera, int point)
	{
		Observation vec = observations(camera, point);

		ReprojErrorhomo_optfocal* reproj_error = new ReprojErrorhomo_optfocal(vec(0), vec(1));
		ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<ReprojErrorhomo_optfocal, 2, 3, 3, 4, 1>(reproj_error);
		problem.AddResidualBlock(cost_function, loss_function, GetCameraPtr(camera), GetCameraPtr(camera) + 3, GetPointPtr(point), &intrinsics.focal);

		if (translationFixed(camera))
			problem.SetParameterBlockConstant(GetCameraPtr(camera));
		if (rotationFixed(camera))
			problem.SetParameterBlockConstant(GetCameraPtr(camera) + 3);
		if (pointFixed(point))
			problem.SetParameterBlockConstant(GetPointPtr(point));
	}

	bool SfM::Optimize()
	{
		if (numCameras == 0 || numPoints == 0) return false;

		ceres::Problem problem;

		PreOptimize();

		bool added_one_camera = false;

		std::cout << "\tBuilding BA problem...\n";

		for (int j = 0; j < numPoints; j++)
		{
			if (!points.exists(j)) continue;
			if (points(j).norm() == 0) continue;
			/*int nobs = 0;
			for ( int i = 0; i < numCameras; i++ )
			{
				if ( !cameras.exists(i) ) continue;
				if ( !( observations.exists(i,j) ) ) continue;

				nobs++;
			}*/
			int nobs = GetNumObservationOfPoint(j);
			if (nobs < 3) continue;
			for (int i = 0; i < numCameras; i++)
			{
				if (!cameras.exists(i)) continue;
				if (!(observations.exists(i, j))) continue;

				if (optimizeFocal)
					AddResidualWithFocal(problem, i, j);
				else
					AddResidual(problem, i, j);
				added_one_camera = true;
			}
		}

		if (!added_one_camera) {
			std::cout << "didn't add any cameras\n";
			return false;
		}

		std::cout << "Running optimizer...\n";
		std::cout << "\t" << problem.NumResiduals() << " residuals\n";

		ceres::Solver::Options options;
		ConfigureSolverOptions(options);
		ceres::Solver::Summary summary;
		ceres::Solve(options, &problem, &summary);
		std::cout << summary.FullReport() << "\n";
		if (summary.termination_type == ceres::FAILURE)
		{
			std::cout << "error: ceres failed.\n";
			exit(1);
		}

		PostOptimize();

		return (summary.termination_type == ceres::CONVERGENCE);
	}

	void SfM::PostOptimize()
	{

	}

	void SfM::Apply(const Pose& pose)
	{
		// x = PX
		// X -> pose*X
		// x = P' * (pose*X)
		// x = (P*poseinv) * (pose*X)
		Pose poseinv = pose.inverse();
		for (int i = 0; i < numCameras; i++)
		{
			Pose campose = GetPose(i);
			campose.postMultiply(poseinv);
			SetPose(i, campose);
		}
		for (int j = 0; j < numPoints; j++)
		{
			Pointhomo Xh = GetPoint(j);
			Xh = pose.apply(Xh);
			SetPoint(j, Xh);
		}
	}

	void SfM::Apply(double scale)
	{
		for (int i = 0; i < numCameras; i++)
		{
			Pose campose = GetPose(i);
			campose.t *= scale;
			campose.P.block<3, 1>(0, 3) = campose.t;
			SetPose(i, campose);
		}
		for (int j = 0; j < numPoints; j++)
		{
			Pointhomo Xh = GetPoint(j);
			Xh[3] /= scale;
			SetPoint(j, Xh);
		}
	}

	void SfM::Unapply(const Pose& pose)
	{
		// x = PX
		// X -> poseinv*X
		// x = P' * (poseinv*X)
		// x = (P*pose) * (poseinv*X)
		for (int i = 0; i < numCameras; i++)
		{
			Pose campose = GetPose(i);
			campose.postMultiply(pose);
			SetPose(i, campose);
		}
		Pose poseinv = pose.inverse();
		for (int j = 0; j < numPoints; j++)
		{
			Pointhomo Xh = GetPoint(j);
			Xh = poseinv.apply(Xh);
			SetPoint(j, Xh);
		}
	}

	Pose SfM::GetPose(int camera)
	{
		if (!cameras.exists(camera)) return Pose();

		return Pose(cameras(camera).head(3), cameras(camera).tail(3));
	}

	void SfM::SetPose(int camera, const Pose& pose)
	{
		cameras(camera).head(3) = pose.t;
		cameras(camera).tail(3) = pose.r;
	}

	Pointhomo SfM::GetPoint(int point)
	{
		if (!points.exists(point)) return Pointhomo(0, 0, 0, 0);
		return points(point);
	}

	void SfM::SetPoint(int point, const Pointhomo& position)
	{
		points(point) = position;
	}

	void SfM::RemovePoint(int point)
	{
		for (int i = 0; i < numCameras; i++)
		{
			observations.erase(i, point);
		}
		points.erase(point);
		descriptors.erase(point);
		pointObsnum.erase(point);
	}

	void SfM::RemoveCamera(int camera)
	{
		cameras.erase(camera);
		observations.erase(camera);

		for (int j = 0; j < numPoints; j++)
		{
			int i = 0;
			for (; i < numCameras; i++)
			{
				if (observations.exists(i, j)) break;
			}
			if (i == numCameras) points.erase(j);
		}
	}

	void SfM::WritePoses(const std::string& path, const std::vector<int>& indices)
	{
		assert(indices.size() == numCameras);

		//FILE* f = fopen_s(path.c_str(), "w");
		auto f = std::ofstream(path);
		for (int i = 0; i < numCameras; i++)
		{
			f << indices[i];
			Camera camera = cameras(i);
			for (int j = 0; j < 6; j++)
			{
				f << camera(j);
			}
			f << endl;
		}
	}

	void SfM::GetAllPoints(std::vector<Eigen::Vector3d>& allpoints, std::vector<int>& allpointids)
	{
		allpoints.reserve(numPoints);
		for (int i = 0; i < numPoints; i++)
		{
			if (!points.exists(i)) continue;

			Pointhomo Xh = GetPoint(i);
			Point X = Xh.head(3) / Xh[3]; // MA Li: Pointhomo
			double distance = X.norm();
			if (distance > 100)
				X /= (distance / 100.);

			if (X == Point(0, 0, 0)) continue;
			allpoints.push_back(X);
			allpointids.push_back(i);
		}
	}

	void SfM::GetAllPointsHomo(std::vector<Eigen::Vector4d>& allpoints, std::vector<int>& allpointids)
	{
		allpoints.reserve(numPoints); allpointids.reserve(numPoints);
		for (int i = 0; i < numPoints; i++)
		{
			if (!points.exists(i)) continue;
			Pointhomo Xh = GetPoint(i);
			if (Xh == Pointhomo(0, 0, 0, 1) || Xh == Pointhomo(0, 0, 0, 0)) continue;
			allpoints.push_back(Xh);
			allpointids.push_back(i);
		}
	}

	void SfM::WritePointsOBJ(const std::string& path)
	{
		//FILE* f = fopen(path.c_str(), "w");
		auto f = ofstream(path);
		for (int i = 0; i < numPoints; i++)
		{
			if (!points.exists(i)) continue;

			Pointhomo Xh = GetPoint(i);
			Point X = Xh.head(3) / Xh[3]; // MA Li: Pointhomo
			double distance = X.norm();
			if (distance == 0) continue;
			else if (distance > 100)
				X /= (distance / 100.);
			f << "v " << X(0) << X(1) << X(2) << endl;
		}
	}

	void SfM::WriteCameraCentersOBJ(const std::string& path)
	{
		//FILE* f = fopen(path.c_str(), "w");
		auto f = ofstream(path);
		for (int i = 0; i < numCameras; i++)
		{
			Pose pose = GetPose(i);
			Eigen::Vector3d center = pose.getCenter();
			f << "v " << center(0) << center(1) << center(2) << endl;
			//fprintf(f, "v %0.15lf %0.15lf %0.15lf\n", center(0), center(1), center(2));
		}
	}
}
