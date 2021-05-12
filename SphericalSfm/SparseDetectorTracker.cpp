#include "SparseDetectorTracker.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/optflow.hpp>

using namespace Ssfm;

void SparseDetectorTracker::detect(const cv::Mat& image, vector<cv::Point2f>& points, cv::Mat& descs, const cv::Mat& mask)
{
    assert(image.type() == CV_8UC1);

    cv::Mat mask_ = mask.empty() ? cv::Mat(image.size(), CV_8UC1, cv::Scalar(255)) : mask.clone();
    if (!points.empty())
        for (auto& pt : points)
            cv::circle(mask, pt, min_dist, cv::Scalar(0), -1);

    vector<cv::Point2f> allcorners;
    cv::Mat eig;
    //cv::cornerMinEigenVal(imageg, eig, 5);
    cv::cornerHarris(image, eig, 5, 3, 0.04, cv::BORDER_REPLICATE);
    // divide eig into different grids
    for (int beginx = 1; beginx < eig.cols - 1; beginx += cellW)
    {
        for (int beginy = 1; beginy < eig.rows - 1; beginy += cellH)
        {
            cv::Rect roi(beginx, beginy, std::min(cellW, eig.cols - beginx), std::min(cellH, eig.rows - beginy));
            cv::Mat eigroi = eig(roi), eigfinal;
            cv::Mat maskroi = mask(roi);
            cv::Mat tmp;
            double maxVal = 0;
            cv::minMaxLoc(eigroi, 0, &maxVal);
            maxVal = std::max(maxVal, maxEigminThr);
            cv::threshold(eigroi, eigfinal, maxVal * qualitylevel, 0, cv::THRESH_TOZERO);
            if (cv::countNonZero(eigfinal) < 1)
                cv::threshold(eigroi, eigfinal, maxVal * qualitylevel * 0.1, 0, cv::THRESH_TOZERO);
            cv::dilate(eigfinal, tmp, cv::Mat());

            vector<const float*> tmpCorners;
            for (int y = 0; y < roi.height; y++)
            {
                const float* eig_data = (const float*)eigfinal.ptr(y);
                const float* tmp_data = (const float*)tmp.ptr(y);
                const uchar* mask_data = maskroi.ptr(y);

                for (int x = 0; x < roi.width; x++)
                {
                    float val = eig_data[x];
                    if (val != 0 && val == tmp_data[x] && (!mask_data || mask_data[x]))
                        tmpCorners.push_back(eig_data + x);
                }
            }

            vector<cv::Point2f> cornerspercell;
            const size_t total = tmpCorners.size();
            if (total == 0)
                continue;
            // sort based on the eig value
            auto gtPtr = [](const float*& a, const float*& b)->bool { return (*a) > (*b); };
            std::sort(tmpCorners.begin(), tmpCorners.end(), gtPtr);

            size_t ncorners = 0;
            for (size_t i = 0; i < total; i++)
            {
                int ofs = (int)((const uchar*)tmpCorners[i] - eigfinal.ptr());
                int y = (int)(ofs / eigfinal.step);
                int x = (int)((ofs - y * eigfinal.step) / sizeof(float));
                cv::Point2f posinCell(x, y);
                cv::Point2f posinImg(x + beginx, y + beginy);
                if (maskroi.at<uchar>(posinCell))
                {
                    cornerspercell.push_back(posinImg);
                    ++ncorners;

                    cv::circle(mask, posinImg, min_dist, cv::Scalar(0), -1);
                }
                if (ncorners >= maxPointPerCell)
                    continue;
            }
            allcorners.insert(allcorners.end(), cornerspercell.begin(), cornerspercell.end());
        }
    }

    /*cv::Mat display = imageg.clone();
    for (auto& pt : allcorners) cv::circle(display, pt, 4, cv::Scalar(0), 2);*/

    cv::TermCriteria termcrit(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 20, 0.03);
    cv::cornerSubPix(image, allcorners, cv::Size(3, 3), cv::Size(-1, -1), termcrit);

    cv::Mat descs_;
    vector<cv::KeyPoint> keypoints(allcorners.size());
    for (int i = 0; i < allcorners.size(); ++i)
        keypoints[i].pt = allcorners[i];

    //orb->detectAndCompute( imageg, cv::noArray(), keypoints, descs, true);
    orb->compute(image, keypoints, descs_);

    points.reserve(points.size() + keypoints.size());
    for (auto& kpt : keypoints)
        points.push_back(kpt.pt);
    descs.push_back(descs_);
}

void SparseDetectorTracker::track(const cv::Mat& image0, const cv::Mat& image1, const Features& features0,
    vector<uchar>& status_mask, Features& features1, Matches& m01,
    const cv::Mat& mask1)
{
    const size_t npts0 = features0.size();

    const vector<cv::Point2f>& prevPts = features0.points;
    vector<cv::Point2f> nextPts;
    vector<uchar> status;
    vector<float> err;

    cv::calcOpticalFlowPyrLK(image0, image1, prevPts, nextPts, status, err, cv::Size(xradius * 2 + 1, yradius * 2 + 1));

    features1.points.insert(features1.points.begin(), nextPts.begin(), nextPts.end());
    features1.descs = features0.descs.clone();
    int outofboundarycount = 0;
    for (int i = 0; i < npts0; i++)
    {
        if (nextPts[i].x < xradius 
            || nextPts[i].x > image0.cols - xradius 
            || nextPts[i].y < yradius
            || nextPts[i].y > image0.rows - yradius
            || (!mask1.empty() && !mask1.at<uchar>(nextPts[i])))
        {
            outofboundarycount++;
            status_mask[i] = 5; // arbitrary number
        }
        // if status_mask is unsucess, than just keep all the same
        if (status_mask[i] != 1) continue;

        // if status_mask is success, and current status is unsucess, update status_mask
        if (status[i] != 1)
        {
            status_mask[i] = status[i];
        }
        else
        {
            m01[i] = i; // features1.size() - 1;
        }
    }
}

void SparseDetectorTracker::track(const cv::Mat& image0, const cv::Mat& image1, const vector<cv::Point2f>& keypoints0, 
    vector<uchar>& status_mask, vector<cv::Point2f>& keypoints1)
{
    assert(status_mask.size() == keypoints0.size());
    if (keypoints0.empty()) return;
    const size_t npts0 = keypoints0.size();

    vector<uchar> status;
    vector<float> err;

    cv::calcOpticalFlowPyrLK(image0, image1, keypoints0, keypoints1, status, err, cv::Size(xradius * 2 + 1, yradius * 2 + 1));

    int outofboundarycount = 0;
    for (int i = 0; i < npts0; i++)
    {
        if (keypoints1[i].x < xradius || keypoints1[i].y < yradius
            || keypoints1[i].x > image0.cols - xradius || keypoints1[i].y > image0.rows - yradius)
        {
            outofboundarycount++;
            status_mask[i] = 5; // arbitrary number
        }
        // if current status is unsucess, update status_mask
        if (status[i] != 1)
        {
            status_mask[i] = status[i];
        }
    }
}