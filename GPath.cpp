#include "GPath.h"
#include "GUtil.h"
#include "MLDataManager.h"

GPath::GPath(bool backward)
    :is_backward_(backward)
    ,frame_id_start_(-1)
{}

GPath::GPath(int startframe, const QRectF & rect0, const QPainterPath & painterpath, bool backward)
    :is_backward_(backward),
    frame_id_start_(startframe),
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
    int idx = frame_id - startFrame();
    if (idx < 0 || idx >= this->length())
        return false;
    if (roi_rect_[idx].contains(pos))
        return true;
    return false;
}

void GPath::translateRect(int frame_id, QPointF offset)
{
    int idx = frame_id - startFrame();
    if (idx >= 0 && idx < this->length()) {
        manual_adjust_[idx] = true;
        roi_rect_[idx].translate(offset);
        GUtil::boundRectF(roi_rect_[idx]);
    }
    dirty_[idx] = true;
}

void GPath::moveRectCenter(int frame_id, QPointF center)
{
    int idx = frame_id - startFrame();
    if (idx >= 0 && idx < this->length()) {
        manual_adjust_[idx] = true;
        roi_rect_[idx].moveCenter(center);
    }
    dirty_[idx] = true;
}

void GPath::updateImage(int frame_id)
{
    const auto& data = MLDataManager::get();
    int idx = frame_id - startFrame();
    if (idx >= 0 && idx < this->length()) {
        if (roi_reshape_[idx]) {
            roi_fg_mat_[idx] = data.getForeground(frame_id, roi_reshape_[idx]);
        }
        else
            roi_fg_mat_[idx] = data.getForeground(frame_id, roi_rect_[idx]);
        roi_fg_qimg_[idx] = GUtil::mat2qimage(
                    roi_fg_mat_[idx], QImage::Format_ARGB32_Premultiplied);
    }
}

void GPath::updateimages()
{
    for (int i = 0; i < length(); ++i) {
        if (dirty_[i]) {
            updateImage(i + startFrame());
            dirty_[i] = false;
        }
    }
}

QRectF GPath::frameRoiRect(int frame_id) const
{
    int idx = frame_id - startFrame();
    if (roi_reshape_[idx])
        return roi_reshape_[idx]->rectF();
    else
        return roi_rect_[idx];
}

QImage GPath::frameRoiImage(int frame_id) const {
    return roi_fg_qimg_[frame_id - startFrame()];
}

cv::Mat4b GPath::frameRoiMat4b(int frame_id) const
{
    return roi_fg_mat_[frame_id - startFrame()];
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

    int idx = frame_id - this->startFrame();
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
        Qt::PenStyle pen_style = (manual_adjust_[idx]? Qt::SolidLine: Qt::DashLine);
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
    int id = frame_id - startFrame();
    roi_reshape_[id] = roi;
    manual_adjust_[id] = true;
    dirty_[id] = true;
}

void GPath::resize(int n) {
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
