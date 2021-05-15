#pragma once
#include <vector>
#include <map>
#include <algorithm>
#include <opencv2/core.hpp>
#include <Eigen/Dense>
#include "so3.hpp"

namespace Ssfm {
	using namespace std;

    /*************************************************
    * Feature Detection
    **********************************************************/
	using Match = pair<int, int>;
	using Matches = map<int, int>;

	struct Features
	{
		std::vector<cv::Point2f> points;
		cv::Mat descs;

		Features() {}
		~Features() {}
		int size() const { return points.size(); }
		bool empty() const { return points.empty(); }
	};

	struct Keyframe
	{
		int index;
		Features features;
		cv::Mat image;
		Keyframe(int _index, const Features& _features) 
            :index(_index), features(_features) 
        { }
        Keyframe(int _index, const Features& _features, cv::Mat image)
            :index(_index), features(_features), image(image)
        {}
	};

	struct ImageMatch
	{
		int index0, index1;
		Matches matches;
		Eigen::Matrix3d R;
		ImageMatch(const int _index0, const int _index1, const Matches& _matches, const Eigen::Matrix3d& _R) :
			index0(_index0), index1(_index1), matches(_matches), R(_R) { }
	
    public:
		EIGEN_MAKE_ALIGNED_OPERATOR_NEW
	};

    /*************************************************
    * Camera Modeling
    **********************************************************/
    typedef Eigen::Vector4d Pointhomo;
    typedef Eigen::Vector3d Point;
    typedef Eigen::Matrix<double, 6, 1> Camera;

    // camera center should be removed
    typedef Eigen::Vector2d Observation;

    // The pose represent the extrinsic matrix
    struct Pose
    {
        Eigen::Vector3d t;
        Eigen::Vector3d r;
        Eigen::Matrix4d P;

        Pose()
            : t(Eigen::Vector3d::Zero()), r(Eigen::Vector3d::Zero()), P(Eigen::Matrix4d::Identity())
        {};
        Pose(const Eigen::Vector3d& _t, const Eigen::Vector3d& _r)
            : t(_t), r(_r), P(Eigen::Matrix4d::Identity())
        {
            P.block<3, 3>(0, 0) = so3exp(r);
            P.block<3, 1>(0, 3) = t;
        }
        inline Pose inverse() const
        {
            Pose poseinv;
            poseinv.P.block<3, 3>(0, 0) = P.block<3, 3>(0, 0).transpose();
            poseinv.t = -P.block<3, 3>(0, 0).transpose() * t;
            poseinv.r = -r;
            poseinv.P.block<3, 1>(0, 3) = poseinv.t;
            return poseinv;
        }
        inline void postMultiply(const Pose& pose)
        {
            P = P * pose.P;
            t = P.block<3, 1>(0, 3);
            r = so3ln(P.block<3, 3>(0, 0));
        }
        inline Point apply(const Point& point) const
        {
            return P.block<3, 3>(0, 0) * point + t;
        }
        inline Pointhomo apply(const Pointhomo& point) const
        {
            return P * point;
        }
        inline Point applyInverse(const Point& point) const
        {
            return P.block<3, 3>(0, 0).transpose() * (point - t);
        }
        inline Eigen::Vector3d getCenter() const
        {
            return P.block<3, 3>(0, 0).transpose() * (-t);
        }

    public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };

    struct Intrinsics
    {
        double focal, centerx, centery;
        Intrinsics() :
            focal(0), centerx(0), centery(0) { }

        Intrinsics(double _focal, double _centerx, double _centery) :
            focal(_focal), centerx(_centerx), centery(_centery) { }

        Eigen::Matrix3d getK() const {
            Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
            K(0, 0) = focal;
            K(1, 1) = focal;
            K(0, 2) = centerx;
            K(1, 2) = centery;
            return K;
        }
        Eigen::Matrix3d getKinv() const {
            return getK().lu().inverse();
        }
    };

    /*************************************************
    * 3D Modeling
    **********************************************************/
    typedef Eigen::Matrix<double, 6, 1> Ray;
    typedef std::pair<Ray, Ray> RayPair;
    typedef std::vector<RayPair> RayPairList;

    /*************************************************
    * Sparse
    **********************************************************/
    template<typename T>
    class SparseVector : public std::map<int, T>
    {
    public:
        typedef typename std::map<int, T>::iterator Iterator;
        typedef typename std::map<int, T>::const_iterator ConstIterator;

        T& operator()(int r) { return (*this)[r]; }
        const T& operator()(int r) const { return (*this)[r]; }

        bool exists(int r) const
        {
            return (this->find(r) != this->end());
        }
    };

    template<typename T>
    class SparseMatrix : public SparseVector< SparseVector<T> >
    {
    public:
        typedef typename SparseVector< SparseVector<T> >::Iterator RowIterator;
        typedef typename SparseVector< SparseVector<T> >::ConstIterator ConstRowIterator;

        typedef typename SparseVector<T>::Iterator ColumnIterator;
        typedef typename SparseVector<T>::ConstIterator ConstColumnIterator;

        T& operator()(int r, int c) { return (*this)[r][c]; }
        const T& operator()(int r, int c) const { return (*this)[r][c]; }

        bool exists(int r, int c) const
        {
            ConstRowIterator rowit = this->find(r);
            if (rowit == this->end()) return false;
            ConstColumnIterator colit = rowit->second.find(c);
            return (colit != rowit->second.end());
        }

        // not sure why this is necessary
        void erase(int r)
        {
            SparseVector< SparseVector<T> >::erase(r);
        }

        void erase(int r, int c)
        {
            RowIterator rowit = this->find(r);
            if (rowit == this->end()) return;
            ColumnIterator colit = rowit->second.find(c);
            if (colit == rowit->second.end()) return;
            rowit->second.erase(colit);
        }
    };
}