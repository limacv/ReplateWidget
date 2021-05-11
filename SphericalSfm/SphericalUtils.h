#pragma once
#include <Eigen/Dense>
#include "SfmCommon.h"

namespace Ssfm {
    void make_spherical_essential_matrix(const Eigen::Matrix3d& R, bool inward, Eigen::Matrix3d& E);
    void decompose_spherical_essential_matrix(const Eigen::Matrix3d& E, bool inward, Eigen::Vector3d& r, Eigen::Vector3d& t);
    void decompose_spherical_essential_matrix(const Eigen::Matrix3d& E, bool inward,
        RayPairList::iterator begin, RayPairList::iterator end, const std::vector<bool>& inliers,
        Eigen::Vector3d& r, Eigen::Vector3d& t);


    void visualize_fpts_match(const Features& features, const cv::Mat img,
        const Features& prevfeatures = Features(), const Matches& matches = Matches(), const vector<float>& weights = vector<float>());

    void visualize_fpts_match(const vector<cv::Point2f>& curkpts, const cv::Mat img,
        const vector<cv::Point2f>& prevkpts = {}, const Matches& matches = Matches());
}
