#ifndef GPATH_H
#define GPATH_H

#include <QWidget>
#include <opencv2/core.hpp>
#include "GRoi.h"
#include "yaml-cpp/yaml.h"

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
    GPath::GPath(int startframe, int endframe, int space);
    GPath(int startframe, const QRectF& rect0, const QPainterPath& painterpath, bool backward = false);
    ~GPath();

public:
    bool checkInside(int frame_id, QPointF pos);

    // modifier
    void translateRect(int frame_id, QPointF offset);
    void moveRectCenter(int frame_id, QPointF center, bool trycopyfromneighbor=false);
    void setPathRoi(int frame_id, const GRoiPtr &roi);
    void setPathRoi(int frame_id, float dx, float dy);
    void copyFrameState(int frame_from, int frame_to);
    void expandSingleFrame2Multiframe(int frame_count);

    QRectF frameRoiRect(int frame_id) const;
    QImage frameRoiImage(int frame_id) const;
    cv::Mat4b frameRoiMat4b(int frame_id) const;
    
    void forceUpdateImages();
    void updateimages();

    int startFrame() const {return frame_id_start_;}
    int endFrame() const {return frame_id_end_;}
    void setStartFrame(int id) { frame_id_start_ = id;}
    void setEndFrame(int id) { frame_id_end_ = id; }

    bool isBackward() const {return is_backward_;}
    void setBackward(bool b) {is_backward_ = b;}

    QImage getIconImage(); // used for GObjLayer to show a 'preview' of the timeline

    bool isEmpty() const { return length() <= 0 || !space(); }
    int length() const {return frame_id_end_ - frame_id_start_ + 1;}
    size_t space() const { return roi_rect_.size(); }
    void resize(int n);
    void release();

    void paint(QPainter &painter, int frame_id) const;

    static bool is_draw_trajectory;

private:
    int worldid2thisid(int frameidx) const { return frameidx - world_offset_; }
    void updateImage(int idx);
    void paintTrace(QPainter& painter, int frame_id) const;

public:
    std::vector<QRectF> roi_rect_;
    std::vector<QPointF*> flow_points_;
    std::vector<int> number_flow_points_;
    std::vector<cv::Mat> roi_fg_mat_;

    // rendering
    QPainterPath painter_path_;
    std::vector<QImage> roi_fg_qimg_;// generated on adding to the effects

    // modification
    std::vector<bool> manual_adjust_;
    std::vector<GRoiPtr> roi_reshape_;
    std::vector<bool> dirty_;

protected:
    int world_offset_;  // usually 0, or start frame, used alongside
    int frame_id_start_;
    int frame_id_end_;
    bool is_backward_;

    friend YAML::Emitter& operator <<(YAML::Emitter& out, const GPathPtr& path);
    friend void operator >> (const YAML::Node& node, GPathPtr& path);
};

#endif // GPATH_H
