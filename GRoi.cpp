#include "GRoi.h"


bool GRoi::isRect() const
{
    return painter_path_.isEmpty();
}

QRectF GRoi::rectF() const
{
    if (isRect())
        return rectF_;
    else
        return painter_path_.boundingRect();
}
