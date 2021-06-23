#include "GPath.h"
#include "GUtil.h"
#include "MLDataManager.h"

GPath::GPath(bool backward)
    :is_backward_(backward)
    ,frame_id_start_(-1)
    ,frame_id_end_(-1)
    ,world_offset_(0)
{}

GPath::GPath(int startframe, int endframe, int space)
    :is_backward_(false),
    frame_id_start_(startframe),
    frame_id_end_(endframe),
    world_offset_(0)
{
    resize(space);
}


GPath::GPath(int startframe, const QRectF & rect0, const QPainterPath & painterpath, bool backward)
    :is_backward_(backward),
    frame_id_start_(startframe),
    frame_id_end_(startframe),
    world_offset_(startframe),
    painter_path_(painterpath),
    roi_rect_(1, rect0),
    flow_points_(1, 0),
    number_flow_points_(1, 0),
    roi_fg_mat_(1),
    roi_fg_qimg_(1),
    manual_adjust_(1, false),
    roi_reshape_(1, 0),
    dirty_(1, true)
{ }

GPath::~GPath() { release(); }

bool GPath::is_draw_flow = true;

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
        manual_adjust_[idx] = true;
        dirty_[idx] = true;
    }
}

void GPath::updateImage(int idx)
{
    const auto& data = MLDataManager::get();
    
    int frame_idx = idx + world_offset_;
    if (roi_reshape_[idx]) {
        roi_fg_mat_[idx] = data.getForeground(frame_idx, roi_reshape_[idx]);
    }
    else
        roi_fg_mat_[idx] = data.getForeground(frame_idx, roi_rect_[idx]);

    roi_fg_qimg_[idx] = GUtil::mat2qimage(
        roi_fg_mat_[idx], QImage::Format_ARGB32_Premultiplied);
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

QRectF GPath::frameRoiRect(int frame_id) const
{
    assert(frame_id >= frame_id_start_ && frame_id <= frame_id_end_);
    int idx = worldid2thisid(frame_id);
    if (roi_reshape_[idx])
        return roi_reshape_[idx]->rectF();
    else
        return roi_rect_[idx];
}

QImage GPath::frameRoiImage(int frame_id) const {
    return roi_fg_qimg_[worldid2thisid(frame_id)];
}

cv::Mat4b GPath::frameRoiMat4b(int frame_id) const
{
    return roi_fg_mat_[worldid2thisid(frame_id)];
}

void GPath::paint(QPainter &painter, int frame_id) const
{
    if (frame_id < startFrame() || frame_id > endFrame()) {
        qDebug() << "Out of current path range";
        return;
    }
    if (this->isEmpty()) return;

    QSize size = painter.viewport().size();
    QMatrix mat;
    mat.scale(size.width(), size.height());

    int idx = worldid2thisid(frame_id);
    if (roi_reshape_[idx]) {
        Qt::PenStyle style = Qt::SolidLine;
        QPen pen = QPen(Qt::red, 1, style);
        painter.setPen(pen);
        const GRoiPtr &roi = roi_reshape_[idx];
        if (roi->isRect()) {
            QRect rect = mat.mapRect(roi->rectF_).toRect();
            painter.drawRect(rect);
        }
        else {
            QPainterPath pp = mat.map(roi->painter_path_);
            pp.closeSubpath();
            painter.drawPath(pp);
        }
    }
    else {
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

    QRect ori_rect = mat.mapRect(roi_rect_[idx]).toRect();
    // draw flow points
    if (is_draw_flow && this->number_flow_points_[idx] > 0) {
        QPen points_pen = (this->isBackward()? QPen(QColor(255,0,0,110), 1, Qt::SolidLine) :
                                 QPen(QColor(0,255,1), 1, Qt::SolidLine));
        QMatrix orig = painter.worldMatrix();
        painter.translate(ori_rect.topLeft());
        painter.setPen(points_pen);
        std::vector<QPointF> points(number_flow_points_[idx]);
        for(int i = 0; i < points.size(); ++i)
            points[i] = mat.map(flow_points_[idx][i]);
        painter.drawPoints(points.data(),
                           number_flow_points_[idx]);
        painter.setWorldMatrix(orig);
    }
}

void GPath::setPathRoi(int frame_id, const GRoiPtr &roi)
{
    int id = worldid2thisid(frame_id);
    roi_reshape_[id] = roi;
    manual_adjust_[id] = true;
    dirty_[id] = true;
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

void GPath::resize(int n) {
    if (n != 1 || n != MLDataManager::get().get_framecount())
        qWarning("Path resize different to the total frame count");
    roi_rect_.resize(n);
    flow_points_.resize(n, 0);
    number_flow_points_.resize(n,0);
    roi_fg_mat_.resize(n);
    roi_fg_qimg_.resize(n);
    manual_adjust_.resize(n, false);
    roi_reshape_.resize(n, 0);
    dirty_.resize(n, true);
}

void GPath::release() {
    for (size_t i = 0; i < flow_points_.size(); ++i)
        delete [] flow_points_[i];
    flow_points_.clear();
    number_flow_points_.clear();
    roi_fg_mat_.clear();
    roi_fg_qimg_.clear();
    manual_adjust_.clear();
    roi_reshape_.clear();
    dirty_.clear();
    qDebug() << "GPathTrackData Released";
}
