#ifndef GTIMELINEEX_H
#define GTIMELINEEX_H

#include <QWidget>
#include <QAction>
#include <QMenu>
#include <QToolButton>
#include <QToolTip>
#include <QRadioButton>
#include <QPushButton>
#include <QStyleOption>
#include <QMouseEvent>
#include "GInterval.h"
#include "GEffect.h"
#include "GEffectManager.h"

class GTimelineEx : public QPushButton
{
    Q_OBJECT

public:
    GTimelineEx(QWidget *parent = 0);
    ~GTimelineEx();

    void release();

    void init(int num_frames);

    size_t IntervalCount() const;
    bool releaseEffect(GEffectPtr &efx);

    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const GTimelineEx* timeline);
    friend YAML::Emitter& operator <<(YAML::Emitter& out,
                                      const std::vector<GInterval*> &intervals);
    friend const YAML::Node& operator >>(const YAML::Node &node, GTimelineEx* timeline);
//    friend const YAML::Node& operator >>(const YAML::Node &node,
//                                   std::vector<GInterval*> &intervals);

//    const std::vector<GInterval *> &getIntervals() const;
    GInterval *intervals(int i) const;
protected:
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;
    virtual void paintEvent(QPaintEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void resizeEvent(QResizeEvent *);

signals:
    void frameChanged(int iFrame);
    void intervalAdded(GInterval* interval);
    void onPressEffect(GEffectPtr efx);

public slots:
//    void setCurFrame(int iFrame);
//    void addEffect(GInterval::IntervalType type, G_EFX *efx);
    void addEffect(GEffectPtr &efx);
    void deleteInterval();
    void setSync();
//    void addKeypoint();
//    void deleteKeypoint();
//    void addOpaque();
//    void deleteOpaque();
    bool updateFrameChange(int iFrame);
    void toggleShown(bool checked);

private:
    void createActions();
    int getFrameId(int pos_x, bool round = true);
    int intervalAt(const QPoint &pos);
    bool getClickedInterval(GInterval*& in, int &frame_id, bool round = true);

    void addIntervalWidget(GEffectPtr &efx, int start_pos, int end_pos);
    void addIntervalEx(GEffectPtr &efx, int start_frame, int end_frame);
    void addIntervalEx(GEffectPtr &efx, int start_frame);

//    void addIntervalWidget(GInterval::IntervalType type,
//                           G_EFX *efx, int start_pos, int end_pos);
//    void addIntervalEx(GInterval::IntervalType type,
//                       G_EFX *efx, int start_frame, int end_frame);
//    void addIntervalEx(GInterval::IntervalType type,
//                       G_EFX *efx, int start_frame);
    void clearCurrent();
    int convertFrameToPosition(int iFrame);
    double frameStep() const;

    int num_frames_;
    int cur_frame_id_;

    float second_hand_step_length_;
    int mouse_select_start_frame_;
    int mouse_select_end_frame_;

    QPoint right_click_pos_;
    bool select_started_;
    //bool align_started_;
    //int align_start_pos_;
    bool move_stated_;

    std::vector<GInterval*> intervals_;

//    QAction *act_loop_;
//    QAction *act_still_;
//    QAction *act_blur_;
//    QAction *act_motion_;

//    QAction *act_delete_;
//    QAction *act_add_keyPt_;
//    QAction *act_delete_keyPt_;
//    QAction *act_set_opaque;
//    QAction *act_delete_opaque;
    QAction *act_set_syncPt_;
};

#endif // GTIMELINEEX_H
