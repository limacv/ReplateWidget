#include "GPath.h"
#include "GUtil.h"
#include "MLDataManager.h"

GPath::GPath(bool backward)
    :is_backward_(backward)
    ,frame_id_start_(-1)
    ,frame_id_end_(-1)
    ,is_singleframe(true)
{}

GPath::GPath(int startframe, const QRectF & rect0, const QPainterPath & painterpath, bool backward)
    :is_backward_(backward),
    frame_id_start_(startframe),
    frame_id_end_(startframe),
    is_singleframe(true),
    painter_path_(painterpath),
    roi_rect_(1, rect0),
    roi_fg_mat_(1),
    roi_fg_qimg_(1),
    manual_adjust_(1, false),
    dirty_(1, true)
{ }

GPath::~GPath() { release(); }

bool GPath::is_draw_trajectory = true;

bool GPath::checkInside(int frame_id, QPointF pos)
{
    if (space() == 0
        || frame_id < frame_id_start_
        || frame_id > frame_id_end_)
        return false;

    int idx = worldid2thisid(frame_id);
    return roi_rect_[idx].contains(pos);
}

void GPath::translateRect(int frame_id, QPointF offset)
{
    if (space() > 0 && frame_id >= frame_id_start_ && frame_id <= frame_id_end_) 
    {
        int idx = worldid2thisid(frame_id);
        manual_adjust_[idx] = true;
        roi_rect_[idx].translate(offset);
        dirty_[idx] = true;
        GUtil::boundRectF(roi_rect_[idx]);
    }
}

void GPath::moveRectCenter(int frame_id, QPointF center, bool trycopyfromneighbor)
{
    if (space() > 0 && frame_id >= frame_id_start_ && frame_id <= frame_id_end_)
    {
        int idx = worldid2thisid(frame_id);

        if (trycopyfromneighbor && !manual_adjust_[idx])
        {
            if (idx + 1 < roi_rect_.size() 
                && manual_adjust_[idx + 1])
                roi_rect_[idx] = roi_rect_[idx + 1];
            else if (idx - 1 >= 0)
                roi_rect_[idx] = roi_rect_[idx - 1];
        }

        roi_rect_[idx].moveCenter(center);
        GUtil::boundRectF(roi_rect_[idx]);
        manual_adjust_[idx] = true;
        dirty_[idx] = true;
    }
}

void GPath::updateImage(int idx)
{
    const auto& data = MLDataManager::get();
    
    if (is_singleframe) idx = 0;

    roi_fg_mat_[idx] = data.getMattedRoi(idx, roi_rect_[idx]);

    roi_fg_qimg_[idx] = GUtil::mat2qimage(
        roi_fg_mat_[idx], QImage::Format_ARGB32_Premultiplied);
}

void GPath::forceUpdateImages()
{
    for (int i = 0; i < space(); ++i)
        dirty_[i] = true;
    updateimages();
}

void GPath::updateimages()
{
    for (int i = 0; i < space(); ++i)
    {
        if (dirty_[i])
        {
            updateImage(i);
            dirty_[i] = false;
        }
    }
}

QRectF GPath::getPlateQRect(int frame_id) const
{
    int idx = worldid2thisid(frame_id);
    
    return roi_rect_[idx];
}

QImage GPath::getPlateQImg(int frame_id) const {
    return roi_fg_qimg_[worldid2thisid(frame_id)];
}

QImage& GPath::getPlateQImg(int frame_id) {
    return roi_fg_qimg_[worldid2thisid(frame_id)];
}

cv::Mat GPath::getPlateCV(int frame_id) const
{
    return roi_fg_mat_[worldid2thisid(frame_id)];
}

cv::Mat& GPath::getPlateCV(int frame_id)
{
    return roi_fg_mat_[worldid2thisid(frame_id)];
}

cv::Mat GPath::getPlateMask(int frame_id) const
{
    const auto& data = MLDataManager::get();
    cv::Mat mask;
    if (!painter_path_.isEmpty())
    {
        QPainterPath painter_path = data.imageScale().map(painter_path_);
        mask = GUtil::cvtPainterPath2Mask(painter_path);
    }
    else
    {
        const auto& rectf = getPlateQRect(frame_id);
        cv::Rect rect = data.toCropROI(rectf);
        mask = cv::Mat(rect.size(), CV_8UC1, 255);
    }
    return mask;
}

void GPath::paintVisualize(QPainter &painter, int frame_id) const
{
    paintTrace(painter, frame_id);
    if (frame_id < startFrame() || frame_id > endFrame()) {
        return;
    }
    if (isEmpty()) return;

    QSize size = painter.viewport().size();
    QMatrix mat;
    mat.scale(size.width(), size.height());

    int idx = worldid2thisid(frame_id);

    Qt::PenStyle pen_style = (manual_adjust_[idx]? Qt::DotLine: Qt::DashLine);
    QPen rect_pen = (isBackward()?QPen(Qt::green, 1, pen_style):
                                    QPen(Qt::red, 1, pen_style));
    painter.setPen(rect_pen);

    if (painter_path_.isEmpty()) {
        QRect rect = mat.mapRect(roi_rect_[idx]).toRect();
        painter.drawRect(rect);
    }
    else {
        QPainterPath pp = mat.map(painter_path_);
        pp.closeSubpath();
        painter.drawPath(pp);
    }
}

void GPath::paintTrace(QPainter& painter, int frame_id) const
{
    // draw trajectories
    if (is_draw_trajectory
        && painter_path_.isEmpty()
        && roi_rect_.size() > 1)
    {
        QSize size = painter.viewport().size();
        QMatrix mat;
        mat.scale(size.width(), size.height());
        const auto& color_auto = QColor(143, 170, 220, 150);
        const auto& color_manual = QColor(255, 217, 102, 150);
        for (int i = startFrame(); i < endFrame(); ++i)
        {
            QPointF currpt = mat.map(roi_rect_[i].center());
            QPointF nextpt = mat.map(roi_rect_[i + 1].center());

            float width = qAbs<float>(i - frame_id) / 4;
            width = MIN(width, size.width() / 200);
            painter.setPen(QPen(manual_adjust_[i] ? color_manual : color_auto,
                width, Qt::PenStyle::SolidLine));
            painter.drawLine(currpt, nextpt);
        }
    }
}

void GPath::copyFrameState(int frame_from, int frame_to)
{
    frame_from = worldid2thisid(frame_from);
    frame_to = worldid2thisid(frame_to);
    if (frame_from < 0 || frame_to < 0 || frame_from >= roi_rect_.size() || frame_to >= roi_rect_.size())
        return;
    roi_rect_[frame_to] = roi_rect_[frame_from];
    manual_adjust_[frame_to] = manual_adjust_[frame_from];
    dirty_[frame_to] = true;
}

void GPath::setPathRoi(int frame_id, float dx, float dy)
{
    int id = worldid2thisid(frame_id);
    QRectF& rect = roi_rect_[id];
    rect.setWidth(rect.width() + dx * 2);
    rect.setHeight(rect.height() + dy * 2);
    rect.setLeft(rect.left() - dx);
    rect.setTop(rect.top() - dy);
    GUtil::boundRectF(rect);
    manual_adjust_[id] = true;
    dirty_[id] = true;
}

QImage GPath::getIconImage()
{
    updateimages();
    int idx = 0;
    for (; idx < space(); ++idx)
        if (manual_adjust_[idx]) break;
    if (idx == space()) idx = 0;
    cv::Mat icon;
    cv::cvtColor(roi_fg_mat_[idx], icon, cv::COLOR_RGBA2RGB);
    return GUtil::mat2qimage(icon, QImage::Format_RGB888).copy();
}

void GPath::resize(int n) {
    if (n != 1 || n != MLDataManager::get().get_framecount())
        qWarning("Path resize different to the total frame count");
    roi_rect_.resize(n);
    roi_fg_mat_.resize(n);
    roi_fg_qimg_.resize(n);
    manual_adjust_.resize(n, false);
    dirty_.resize(n, true);
}

void GPath::release() {
    roi_fg_mat_.clear();
    roi_fg_qimg_.clear();
    manual_adjust_.clear();
    dirty_.clear();
    qDebug() << "GPathTrackData Released";
}
