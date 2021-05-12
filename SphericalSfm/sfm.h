#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "SfmCommon.h"

#include <opencv2/core.hpp>

#include <ceres/problem.h>
#include <ceres/solver.h>
#include <ceres/rotation.h>

namespace Ssfm {
    struct ReprojectionError
    {
        ReprojectionError(double _focal, double _x, double _y, double _w)
            : focal(_focal), x(_x), y(_y), w(_w)
        {

        }

        template <typename T>
        bool operator()(const T* const camera_t,
            const T* const camera_r,
            const T* const point,
            T* residuals) const
        {
            // transform from world to camera
            T point_nh[3];
            point_nh[0] = point[0] / w;
            point_nh[1] = point[1] / w;
            point_nh[2] = point[2] / w;
            T p[3];
            ceres::AngleAxisRotatePoint(camera_r, point_nh, p);
            p[0] += camera_t[0]; p[1] += camera_t[1]; p[2] += camera_t[2];

            // projection
            T xp = p[0] / p[2];
            T yp = p[1] / p[2];

            // intrinsics
            T fxp = T(focal) * xp;
            T fyp = T(focal) * yp;

            // residuals
            residuals[0] = fxp - T(x);
            residuals[1] = fyp - T(y);

            return true;
        }

        double focal, x, y, w;
    };


    struct ReprojectionErrorhomo
    {
        ReprojectionErrorhomo(double _focal, double _x, double _y)
            : focal(_focal), x(_x), y(_y)
        { }

        template <typename T>
        bool operator()(const T* const camera_t,
            const T* const camera_r,
            const T* const point,
            T* residuals) const
        {
            // transform from world to camera
            T p[4];
            // homo coordinate don't affect rotation
            ceres::AngleAxisRotatePoint(camera_r, point, p);
            const auto& w = point[3];
            p[0] += camera_t[0] * w; p[1] += camera_t[1] * w; p[2] += camera_t[2] * w;
            /*p[3] = w;*/

            // projection, the project can ignore w
            T xp = p[0] / p[2];
            T yp = p[1] / p[2];

            // intrinsics
            T fxp = T(focal) * xp;
            T fyp = T(focal) * yp;

            // residuals
            residuals[0] = fxp - T(x);
            residuals[1] = fyp - T(y);

            // the newly added
            // when p[2] == w, the point is in the image project rectangle, so we want to push as far as possible
            //residuals[2] = (p[2] - w * T(10)); // dangerous shot

            return true;
        }

        double focal, x, y;
    };

    struct ReprojErrhomowithWeight
    {
        ReprojErrhomowithWeight(double _focal, double _x, double _y, double _w)
            : focal(_focal), x(_x), y(_y), weight(_w)
        { }

        template <typename T>
        bool operator()(const T* const camera_t,
            const T* const camera_r,
            const T* const point,
            T* residuals) const
        {
            // transform from world to camera
            T p[4];
            // homo coordinate don't affect rotation
            ceres::AngleAxisRotatePoint(camera_r, point, p);
            const auto& w = point[3];
            p[0] += camera_t[0] * w; p[1] += camera_t[1] * w; p[2] += camera_t[2] * w;
            /*p[3] = w;*/

            // projection, the project can ignore w
            T xp = p[0] / p[2];
            T yp = p[1] / p[2];

            // intrinsics
            T fxp = T(focal) * xp;
            T fyp = T(focal) * yp;

            // residuals
            residuals[0] = T(weight) * (fxp - T(x));
            residuals[1] = T(weight) * (fyp - T(y));

            // the newly added
            // when p[2] == w, the point is in the image project rectangle, so we want to push as far as possible
            //residuals[2] = (p[2] - w * T(10)); // dangerous shot

            return true;
        }

        double focal, x, y, weight;
    };

    struct ReprojErrorhomo_optfocal
    {
        ReprojErrorhomo_optfocal(double _x, double _y)
            : x(_x), y(_y)
        { }

        template <typename T>
        bool operator()(const T* const camera_t,
            const T* const camera_r,
            const T* const point,
            const T* const focal,
            T* residuals) const
        {
            // transform from world to camera
            T p[4];
            // homo coordinate don't affect rotation
            ceres::AngleAxisRotatePoint(camera_r, point, p);
            const auto& w = point[3];
            p[0] += camera_t[0] * w; p[1] += camera_t[1] * w; p[2] += camera_t[2] * w;
            /*p[3] = w;*/

            // projection, the project can ignore w
            T xp = p[0] / p[2];
            T yp = p[1] / p[2];

            // intrinsics
            T fxp = focal[0] * xp;
            T fyp = focal[0] * yp;

            // residuals
            residuals[0] = fxp - T(x);
            residuals[1] = fyp - T(y);

            // the newly added
            // when p[2] == w, the point is in the image project rectangle, so we want to push as far as possible
            //residuals[2] = (p[2] - w * T(10)); // dangerous shot

            return true;
        }

        double x, y;
    };

    class SfM
    {
    public:
        bool optimizeFocal;
        bool notranslation;
    protected:
        friend class ParallelTriangulator;
        Intrinsics intrinsics;
        
        SparseVector<Camera> cameras;          // m cameras
        SparseVector<Pointhomo> points;            // n points
        
        SparseVector<int> frameIdx2camIdx;       // indexed by camera index
        SparseMatrix<Observation> observations;  // m x n x 2
        //SparseMatrix<Pose> measurements;  // m x m
        
        SparseVector<cv::Mat> descriptors;     // indexed by point index
        
        SparseVector<bool> rotationFixed;
        SparseVector<bool> translationFixed;
        SparseVector<bool> pointFixed;
        SparseVector<size_t> pointObsnum;
        
        int numCameras;
        int numPoints;
        int nextCamera;
        int nextPoint;
        
        double * GetCameraPtr( int camera );
        double * GetPointPtr( int point );
        
        ceres::LossFunction *loss_function;

        void PreOptimize();
        void ConfigureSolverOptions( ceres::Solver::Options &options );
        void AddResidual(ceres::Problem& problem, int camera, int point);
        void AddResidualWithFocal( ceres::Problem &problem, int camera, int point );
        //void AddPoseGraphResidual( ceres::Problem &problem, int i, int j );
        void PostOptimize();
    public:
        int GetCameraIdxfromFrameIdx(int camera);
        bool IsKeyframe(int frameidx);
        bool GetAllObservationofCam(const int camera, std::vector<Observation>& obses, std::vector<int>& ptidx);
        bool GetAllKpointsofCam(const int camera, std::vector<cv::Point2f>& obses, std::vector<int>& ptidx);
        void GetAllPoints(std::vector<Eigen::Vector3d>& allpoints, std::vector<int>& allpointids);
        void GetAllPointsHomo(std::vector<Eigen::Vector4d>& allpoints, std::vector<int>& allpointids);
    public:
        SfM()
            : intrinsics(0, 0, 0),
            numCameras(0),
            numPoints(0),
            nextCamera(-1),
            nextPoint(0),
            optimizeFocal(false),
            notranslation(false)
        {}

        SfM(const Intrinsics& _intrinsics, bool optimizeFocal, bool notranslation)
            : intrinsics(_intrinsics),
            numCameras(0),
            numPoints(0),
            nextCamera(-1),
            nextPoint(0),
            optimizeFocal(optimizeFocal),
            notranslation(notranslation)
        {}
        
        Intrinsics GetIntrinsics() const { return intrinsics; }
        
        int AddCamera( const Pose &initial_pose, const int videoIdx);
        int AddPoint( const Pointhomo &initial_position, const cv::Mat &descriptor = cv::Mat() );
        void AddObservation( int camera, int point, const Observation &observation );
        //void AddMeasurement( int i, int j, const Pose &measurement );

        void RemoveCamera( int camera );
        void RemovePoint( int point );
        
        void MergePoint( int point1, int point2 ); // point2 will be removed
        
        int GetNumCameras() { return numCameras; }
        int GetNumPoints() { return numPoints; }
        bool GetObservation( int camera, int point, Observation &observation );
        size_t GetNumObservationOfPoint(int point);
        //bool GetMeasurement( int i, int j, Pose &measurement );
        cv::Mat GetDescriptor( int point ) { return descriptors(point); }
        
        std::vector<int> Retriangulate();
        
        bool Optimize();
        
        void Apply( const Pose &pose );
        void Apply( double scale );
        void Unapply( const Pose &pose );
        
        Pose GetPose( int camera );
        void SetPose( int camera, const Pose &pose );
        Pointhomo GetPoint(int point);
        void SetPoint( int point, const Pointhomo &position );
        
        void SetRotationFixed( int camera, bool fixed ) { rotationFixed[camera] = fixed; }
        void SetTranslationFixed( int camera, bool fixed ) { translationFixed[camera] = fixed; }
        void SetPointFixed( int point, bool fixed ) { pointFixed[point] = fixed; }
        
        void WritePoses( const std::string &path, const std::vector<int> &indices );
        void WritePointsOBJ( const std::string &path );
        void WriteCameraCentersOBJ( const std::string &path );
    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
}

