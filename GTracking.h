#pragma once
#include <vector>
#include <fstream>
#include <QDebug>
#include <qrect.h>
#include <opencv2/core.hpp>
#include "GKalman.h"

struct GFrameFlowData
{
public:
    QPoint flow_center_;
    QPointF flow_velocity_;
    QPointF flow_confidence_;
    QRect active_boundbox_rect_;
    std::vector<QPoint> flow_points_;
};

class GTracking
{
public:
    GTracking();
    virtual ~GTracking();

    QPointF predict(const cv::Mat4b& flow_mat, const QRect& image_loc_rect, bool backward,
                       GFrameFlowData& out_flow_data);

    void adjust(const cv::Mat4b& flow_mat, const QRect& image_loc_rect, const QRect& predict_image_loc_rect,
                   bool backward, GFrameFlowData& out_flow_data);

    void analyzeFlow(const cv::Mat &flow_mat, bool back,
                       GFrameFlowData& flow_data);

    void getActivePoints(const cv::Mat4b &flow, bool back,
                          std::vector<QPoint> &poses,
                          std::vector<QPointF> &vecs) const;

    template<class T>
    void ransac(std::vector<T> &data,
                double desired_prob_for_no_outliers,
                size_t *num_votes, bool **out_best_votes = 0);
public:
    static qreal M_ACTIVE_LENGTH_TH;
    static qreal M_ACTIVE_RATIO_TH;

private:
    double m_centerRadius;
    GKalman *kf_;
};
