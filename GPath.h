#ifndef GPATH_H
#define GPATH_H

#include <QWidget>
#include <opencv2/core.hpp>
#include "GRoi.h"

#ifndef Q_MOC_RUN
#include <memory>
//#include <boost/smart_ptr.hpp>
#endif // Q_MOC_RUN

class GPath;
typedef std::shared_ptr<GPath> GPathPtr;

class GPath
{
public:
    explicit GPath(bool backward = false);
    GPath(int startframe, const QRectF& rect0, const QPainterPath& painterpath, bool backward = false);
    ~GPath();

public:
    bool checkInside(int frame_id, QPointF pos);

    void translateRect(int frame_id, QPointF offset);
    void moveRectCenter(int frame_id, QPointF center);
    void setPathRoi(int frame_id, const GRoiPtr &roi);

    QRectF frameRoiRect(int frame_id) const;
    QImage frameRoiImage(int frame_id) const;
    cv::Mat4b frameRoiMat4b(int frame_id) const;

    void updateImage(int frame_id);

    void updateimages();

    int startFrame() const {return frame_id_start_;}
    int endFrame() const {return frame_id_start_ + length() - 1;}
    void setStartFrame(int id) { frame_id_start_ = id;}
    bool isBackward() const {return is_backward_;}
    void setBackward(bool b) {is_backward_ = b;}

    bool isEmpty() const { return !length(); }
    size_t length() const {return roi_rect_.size();}
    void resize(int n);
    void release();

    void paint(QPainter &painter, int frame_id) const;

    static bool is_draw_flow;

public: // to be changed to protected
    std::vector<QRectF> roi_rect_;
    std::vector<QPointF*> flow_points_;
    std::vector<int> number_flow_points_;
    std::vector<cv::Mat4b> roi_fg_mat_;

    // rendering
    QPainterPath painter_path_;
    std::vector<QImage> roi_fg_qimg_;// generated on adding to the effects

    // modification
    std::vector<bool> manual_adjust_;
    std::vector<GRoiPtr> roi_reshape_;
    std::vector<bool> dirty_;

protected:
    int frame_id_start_;
    bool is_backward_;
};

#endif // GPATH_H
