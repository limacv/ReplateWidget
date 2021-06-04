#pragma once
#include "GUtil.h"
#include <vector>
#include <fstream>
#include <QDebug>

#include "GPath.h"

class GPathTracker
{
public:
    GPathTracker() {};
    GPathPtr autoTrackPath(int start_frame, const QRectF& start_rectF);
    GPathPtr trackPath(int start_frame, const QRectF& start_rectF,
        int end_frame, const QRectF& end_rectF);
    void updateTrack(GPath* path_data);

private:
    void boundRectF(QRectF& src) const;
};
