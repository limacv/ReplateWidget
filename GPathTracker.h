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
    GPath* trackPath(int start_frame, QRectF start_rectF,
        int end_frame, QRectF end_rectF);
    void updateTrack(GPath* path_data);

    GPath* staticPath(int start_frame, QRectF start_rectF,
        int end_frame, QRectF end_rectF);
    void updateStatic(GPath* path_data);

private:
    void boundRectF(QRectF& src) const;
};
