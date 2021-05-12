#pragma once
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/stitching/detail/matchers.hpp>
#include "SfmCommon.h"


namespace Ssfm {
    using namespace std;

    class SparseDetectorTracker
    {
    public:
        SparseDetectorTracker(double _min_dist=8, double _radius=8, double _qualitylevel=0.4, double _maxEigminThr=1e-5,
            int _maxPointPerCell=30, int _cellH=50, int _cellW=50)
            :min_dist(_min_dist), xradius(_radius), yradius(_radius), 
            qualitylevel(_qualitylevel), maxEigminThr(_maxEigminThr), maxPointPerCell(_maxPointPerCell),
            cellW(_cellW), cellH(_cellH),
            orb(cv::ORB::create(2000)) 
        { }

        ~SparseDetectorTracker() {}

        void detect(const cv::Mat& image, vector<cv::Point2f>& points, cv::Mat& descs, const cv::Mat& mask = cv::Mat());

        /// <summary>
        /// detect corner points and compute the ORB features. 
        /// if points is not empty, then the program assume that the points is the already detected points, 
        /// so will not detect points that is min_dist apart from the points
        /// </summary>
        void detect(const cv::Mat& image, Features& features, const cv::Mat& mask = cv::Mat())
        {
            detect(image, features.points, features.descs, mask);
        }

        /// <summary>
        /// only track the points in features0, the status_mask and matches will be updated accordingly
        /// </summary>
        void track(const cv::Mat& image0, const cv::Mat& image1, const Features &features0, 
            vector<uchar>& status_mask, Features& features1, Matches& m01,
            const cv::Mat& mask1=cv::Mat());

        void track(const cv::Mat& image0, const cv::Mat& image1, const vector<cv::Point2f>& keypoints0,
            vector<uchar>& status_mask, vector<cv::Point2f>& keypoints1);

    private:
        const double min_dist; // minimum distance between existing points and detecting points
        const int xradius; // horizontal tracking radius
        const int yradius; // vertical tracking radius
        const double qualitylevel;
        const double maxEigminThr;
        const int maxPointPerCell;
        const int cellW, cellH;
        //cv::Ptr<cv::xfeatures2d::SIFT> sift;
        cv::Ptr<cv::Feature2D> orb;
    };
}

