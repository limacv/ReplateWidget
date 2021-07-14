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
    GPath(int startframe, const QRectF& rect0, const QPainterPath& painterpath, bool backward = false);
    ~GPath();

public:
    bool checkInside(int frame_id, QPointF pos);

    // modifier
    void translateRect(int frame_id, QPointF offset);
    void moveRectCenter(int frame_id, QPointF center, bool trycopyfromneighbor=false);
    void setPathRoi(int frame_id, float dx, float dy);
    void copyFrameState(int frame_from, int frame_to);
    //void expandSingleFrame2Multiframe(int frame_count);

    // get image
    QRectF getPlateQRect(int frame_id = -1) const;
    QImage getPlateQImg(int frame_id = -1) const;
    QImage& getPlateQImg(int frame_id = -1);
    cv::Mat getPlateCV(int frame_id = -1) const;
    cv::Mat& getPlateCV(int frame_id = -1);
    QPainterPath getPlatePainterPath() const { return painter_path_; }
    const std::vector<cv::Mat>& getAllPlatesCV() const { return roi_fg_mat_; }

    // used for GObjLayer to show a 'preview' of the timeline
    QImage getIconImage();
    cv::Mat getPlateMask(int frame_id = -1) const;

    void updateImage(int idx);
    void updateimages();
    void forceUpdateImages();

    // common accessor
    int startFrame() const {return frame_id_start_;}
    int endFrame() const {return frame_id_end_;}
    void setStartFrame(int id) { frame_id_start_ = id;}
    void setEndFrame(int id) { frame_id_end_ = id; }

    bool isBackward() const {return is_backward_;}
    void setBackward(bool b) {is_backward_ = b;}
    
    bool isEmpty() const { return length() <= 0 || !space(); }
    int length() const {return frame_id_end_ - frame_id_start_ + 1;}
    size_t space() const { return roi_rect_.size(); }
    void resize(int n);
    void release();

    static bool is_draw_trajectory;
    void paintVisualize(QPainter &painter, int frame_id) const;

private:
    int worldid2thisid(int frameidx) const { return is_singleframe ? 0 : frameidx; }
    void paintTrace(QPainter& painter, int frame_id) const;

public:
    std::vector<QRectF> roi_rect_;
    std::vector<cv::Mat> roi_fg_mat_;
    std::vector<QImage> roi_fg_qimg_;// generated on adding to the effects

    // modification
    std::vector<bool> manual_adjust_;
    std::vector<bool> dirty_;

    // rendering
    QPainterPath painter_path_;
    
protected:
    bool is_singleframe;
    int frame_id_start_;
    int frame_id_end_;
    bool is_backward_;

    friend YAML::Emitter& operator <<(YAML::Emitter& out, const GPathPtr& path);
    friend void operator >> (const YAML::Node& node, GPathPtr& path);
};

#endif // GPATH_H
