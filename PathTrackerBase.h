#pragma once
#include <vector>

#include "GPath.h"

class PathTrackerBase
{
public:
    PathTrackerBase() {}
    virtual ~PathTrackerBase() {}
    virtual GPathPtr trackPath(int start_frame, const QRectF& start_rectF, int end_frame, const QRectF& end_rectF) { return nullptr; }
    virtual void updateTrack(GPath* path_data) { }
};

// small utils

inline float compute_IoU(const cv::Rect& r1, const cv::Rect& r2)
{
    return static_cast<float>((r1 & r2).area()) / (r1 | r2).area();
}

inline float compute_Euli(const cv::Rect& r1, const cv::Rect& r2)
{
    cv::Point2f d = cv::Point2f(r1.tl() + r1.br() - r2.tl() - r2.br()) / 2;
    return d.x * d.x + d.y * d.y;
}

inline void boundRectF(QRectF& src)
{
    QPointF off(0, 0);
    if (src.x() < 0)
        off.setX(-src.x());
    if (src.right() >= 1)
        off.setX(1 - src.right());
    if (src.y() < 0)
        off.setY(-src.y());
    if (src.bottom() >= 1)
        off.setY(1 - src.bottom());
    src.translate(off);
}