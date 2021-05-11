#include "SphericalUtils.h"

#include <Eigen/Jacobi>
#include <Eigen/SVD>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "so3.hpp"
#include "SphericalUtils.h"

namespace Ssfm {
    void make_spherical_essential_matrix(const Eigen::Matrix3d& R, bool inward, Eigen::Matrix3d& E)
    {
        Eigen::Vector3d t(R(0, 2), R(1, 2), R(2, 2) - 1);
        if (inward) t = -t;
        E = skew3(t) * R;
    }

    void decompose_spherical_essential_matrix(const Eigen::Matrix3d& E, bool inward, Eigen::Vector3d& r, Eigen::Vector3d& t)
    {
        Eigen::JacobiSVD<Eigen::Matrix3d> svdE(E, Eigen::ComputeFullU | Eigen::ComputeFullV);

        Eigen::Matrix3d U = svdE.matrixU();
        Eigen::Matrix3d V = svdE.matrixV();

        // from theia sfm
        if (U.determinant() < 0) {
            U.col(2) *= -1.0;
        }

        if (V.determinant() < 0) {
            V.col(2) *= -1.0;
        }

        Eigen::Matrix3d D;
        D <<
            0, 1, 0,
            -1, 0, 0,
            0, 0, 1;

        Eigen::Matrix3d DT;
        DT <<
            0, -1, 0,
            1, 0, 0,
            0, 0, 1;

        Eigen::Matrix3d VT = V.transpose().eval();

        Eigen::Vector3d tu = U.col(2);

        Eigen::Matrix3d R1 = U * D * VT;
        Eigen::Matrix3d R2 = U * DT * VT;

        Eigen::Vector3d t1(R1(0, 2), R1(1, 2), R1(2, 2) - 1);
        Eigen::Vector3d t2(R2(0, 2), R2(1, 2), R2(2, 2) - 1);

        if (inward) { t1 = -t1; t2 = -t2; }

        Eigen::Vector3d myt1 = t1 / t1.norm();
        Eigen::Vector3d myt2 = t2 / t2.norm();

        Eigen::Vector3d r1 = so3ln(R1);
        Eigen::Vector3d r2 = so3ln(R2);

        double score1 = fabs(myt1.dot(tu));
        double score2 = fabs(myt2.dot(tu));

        if (score1 > score2) { r = r1; t = t1; }
        else { r = r2; t = t2; }
    }

    static Eigen::Vector3d triangulateMidpoint(const Eigen::Matrix4d& rel_pose, const Eigen::Vector3d& u, const Eigen::Vector3d& v)
    {
        Eigen::Vector3d cu(0, 0, 0);
        Eigen::Vector3d cv(-rel_pose.block<3, 3>(0, 0).transpose() * rel_pose.block<3, 1>(0, 3));

        Eigen::Matrix3d A;
        A <<
            u(0), -v(0), cu(0) - cv(0),
            u(1), -v(1), cu(1) - cv(1),
            u(2), -v(2), cu(2) - cv(2);

        const Eigen::Vector3d soln = A.jacobiSvd(Eigen::ComputeFullV).matrixV().col(2);
        const double du = soln(0) / soln(2);
        const double dv = soln(1) / soln(2);

        const Eigen::Vector3d Xu = cu + u * du;
        const Eigen::Vector3d Xv = cv + v * dv;

        return (Xu + Xv) * 0.5;
    }

    void decompose_spherical_essential_matrix(const Eigen::Matrix3d& E, bool inward,
        RayPairList::iterator begin, RayPairList::iterator end, const std::vector<bool>& inliers,
        Eigen::Vector3d& r, Eigen::Vector3d& t)
    {
        Eigen::JacobiSVD<Eigen::Matrix3d> svdE(E, Eigen::ComputeFullU | Eigen::ComputeFullV);

        Eigen::Matrix3d U = svdE.matrixU();
        Eigen::Matrix3d V = svdE.matrixV();

        // from theia sfm
        if (U.determinant() < 0) {
            U.col(2) *= -1.0;
        }

        if (V.determinant() < 0) {
            V.col(2) *= -1.0;
        }

        Eigen::Matrix3d D;
        D <<
            0, 1, 0,
            -1, 0, 0,
            0, 0, 1;

        Eigen::Matrix3d DT;
        DT <<
            0, -1, 0,
            1, 0, 0,
            0, 0, 1;

        Eigen::Matrix3d VT = V.transpose().eval();

        Eigen::Matrix3d R1 = U * D * VT;
        Eigen::Matrix3d R2 = U * DT * VT;

        Eigen::Vector3d t1(R1(0, 2), R1(1, 2), (inward) ? R1(2, 2) + 1 : R1(2, 2) - 1);
        Eigen::Vector3d t2(R2(0, 2), R2(1, 2), (inward) ? R2(2, 2) + 1 : R2(2, 2) - 1);

        Eigen::Vector3d r1 = so3ln(R1);
        Eigen::Vector3d r2 = so3ln(R2);

        double r1test = r1.norm();
        double r2test = r2.norm();

        if (r2test > M_PI / 2 && r1test < M_PI / 2) { r = r1; t = t1; return; }
        if (r1test > M_PI / 2 && r2test < M_PI / 2) { r = r2; t = t2; return; }

        Eigen::Matrix4d P1(Eigen::Matrix4d::Identity());
        Eigen::Matrix4d P2(Eigen::Matrix4d::Identity());

        P1.block(0, 0, 3, 3) = R1;
        P1.block(0, 3, 3, 1) = t1;

        P2.block(0, 0, 3, 3) = R2;
        P2.block(0, 3, 3, 1) = t2;

        int ninfront1 = 0;
        int ninfront2 = 0;

        int i = 0;
        for (RayPairList::iterator it = begin; it != end; it++, i++)
        {
            if (!inliers[i]) continue;

            Eigen::Vector3d u = it->first.head(3);
            Eigen::Vector3d v = it->second.head(3);

            Eigen::Vector3d X1 = triangulateMidpoint(P1, u, v);
            Eigen::Vector3d X2 = triangulateMidpoint(P2, u, v);

            if (X1(2) > 0) ninfront1++;
            if (X2(2) > 0) ninfront2++;
        }

        if (ninfront1 > ninfront2)
        {
            r = so3ln(R1);
            t = t1;
        }
        else
        {
            r = so3ln(R2);
            t = t2;
        }
    }


    void visualize_fpts_match(const Features& features, const cv::Mat img,
        const Features& prevfeatures, const Matches& matches, const vector<float>& weights)
    {
        cv::Mat display = img.clone();
        if (display.channels() == 1)
            cv::cvtColor(display, display, cv::COLOR_GRAY2BGR);

        if (weights.empty())
        {
            for (auto it = features.points.begin(); it != features.points.end(); ++it)
                cv::circle(display, *it, 3, cv::Scalar(0, 100, 0));
        }
        else
        {
            for (auto it = features.points.begin(); it != features.points.end(); ++it)
            {
                float w_ = weights[distance(features.points.begin(), it)];
                cv::circle(display, *it, w_ * 2, cv::Scalar(0, 100, 0));
            }
        }
        if (!matches.empty() && !prevfeatures.empty())
        {
            for (auto it = prevfeatures.points.begin(); it != prevfeatures.points.end(); ++it)
                cv::circle(display, *it, 3, cv::Scalar(0, 0, 100));
            for (auto it = matches.begin(); it != matches.end(); ++it)
            {
                const auto& pt1 = prevfeatures.points[it->first],
                    & pt2 = features.points[it->second];
                cv::circle(display, pt1, 3, cv::Scalar(0, 255, 0));
                cv::circle(display, pt2, 3, cv::Scalar(0, 0, 255));
                cv::line(display, pt1, pt2, cv::Scalar(255, 0, 0), 1);
            }
        }
        cv::imshow("feature points", display);
        cv::waitKey(1);
    }

    void visualize_fpts_match(const vector<cv::Point2f>& curkpts, const cv::Mat img,
        const vector<cv::Point2f>& prevkpts, const Matches& matches)
    {
        cv::Mat display = img.clone();
        if (display.channels() == 1)
            cv::cvtColor(display, display, cv::COLOR_GRAY2BGR);
        for (auto it = curkpts.begin(); it != curkpts.end(); ++it)
            cv::circle(display, *it, 3, cv::Scalar(0, 100, 0));
        if (!matches.empty() && !prevkpts.empty())
        {
            for (auto it = prevkpts.begin(); it != prevkpts.end(); ++it)
                cv::circle(display, *it, 3, cv::Scalar(0, 0, 100));
            for (auto it = matches.begin(); it != matches.end(); ++it)
            {
                const auto& pt1 = prevkpts[it->first],
                    & pt2 = curkpts[it->second];
                cv::circle(display, pt1, 3, cv::Scalar(0, 255, 0));
                cv::circle(display, pt2, 3, cv::Scalar(0, 0, 255));
                cv::line(display, pt1, pt2, cv::Scalar(255, 0, 0), 1);
            }
        }
        cv::imshow("feature points", display);
        cv::waitKey(1);
    }
}
