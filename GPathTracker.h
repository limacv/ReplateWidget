#pragma once
#include "GUtil.h"
#include <vector>
#include <fstream>
#include <QDebug>

#include "GPath.h"
#include "PathTrackerBase.h"

class GPathTracker: public PathTrackerBase
{
public:
    GPathTracker() {};
    virtual GPathPtr trackPath(int start_frame, const QRectF& start_rectF,
        int end_frame, const QRectF& end_rectF);
    virtual void updateTrack(GPath* path_data);

private:
    GPathPtr autoTrackPath(int start_frame, const QRectF& start_rectF);
};
