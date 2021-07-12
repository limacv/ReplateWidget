#pragma once
#include "PathTrackerBase.h"

const float RECT_MARGIN = 0.17;

class MLPathTracker :
    public PathTrackerBase
{
public:
    MLPathTracker() {};
    virtual GPathPtr trackPath(int start_frame, const QRectF& start_rectF,
        int end_frame, const QRectF& end_rectF);
    virtual void updateTrack(GPath* path_data);
};

