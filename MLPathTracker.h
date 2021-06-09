#pragma once
#include "PathTrackerBase.h"

const int RECT_MARGIN = 15;

class MLPathTracker :
    public PathTrackerBase
{
public:
    MLPathTracker() {};
    virtual GPathPtr trackPath(int start_frame, const QRectF& start_rectF,
        int end_frame, const QRectF& end_rectF);
    virtual void updateTrack(GPath* path_data);
};

