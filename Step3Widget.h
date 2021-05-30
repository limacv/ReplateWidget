#pragma once

#include <QWidget>
#include <qgroupbox.h>
#include <qdockwidget.h>
#include <qslider.h>
#include "GPathTracker.h"
#include "GObjLayer.h"
namespace Ui { class Step3Widget; };

class MLCachePlatesConfig;
class MLCacheTrajectories;

class GMainDisplay;
class GResultDisplay;
class GTimelineWidget;

class Step3Widget : public QWidget
{
	Q_OBJECT

public:
	Step3Widget(QWidget *parent = Q_NULLPTR);
	~Step3Widget();
	void initState();


    void setCurrentEffect(const GEffectPtr& efx);
    GEffectPtr createEffectFromPath(const GPathTrackDataPtr& path, G_EFFECT_ID type);
    void trackPath();

signals:
    void frameChanged(int);

public slots:
	void onWidgetShowup();
    void UpdateDisplay(int i);
    //void OpenVideo(const QString& path = QString());
    //void ExportStarted();
    //void ExportFinished(bool visible);
    //void SaveProject(const QString& path = QString());
    //void LoadProject(const QString& path = QString());
    //void LoadProjectOld(const QString& path = QString());

    void saveUI(YAML::Emitter& out) const;
    //void loadUI(YAML::Node& node);
    void loadUI(const GEffectManager* manager);
    void updateAnchorLength();

    void changeTrailSmooth(int s);
    void changeBlurAlpha(int s);
    void changeDuration(int s);
    void changeFadeLevel0();
    void changeFadeLevel1();
    void changeFadeLevel2();
    void setFadeLevel(int id);
    void changeTransLevel0();
    void changeTransLevel1();
    void changeTransLevel2();
    void changeRenderOrder(bool b);
    void changeMarkerOne(bool b);
    void changeMarkerAll(bool b);
    void changePathLine(bool b);
    void setTransLevel(int id);
    void changePriority();
    void changeSpeed();

    void updateEffectWidget();

    bool setA();
    bool setB();
    //    void addMotion();
    //    void addTrail();
    //    void addEffect(G_EFFECT_ID type);

    void toggleStill(bool checked);
    void toggleInpaint(bool checked);
    void toggleBlack(bool checked);
    void toggleTrail(bool checked);
    void toggleMotion(bool checked);

    void toggleModify(bool checked);

private:
    void CreateConnections();
    void PrepStyleSheet();
    QGroupBox* CreateControlGroup(QWidget* parent);
    QGroupBox* CreateControlPathGroup(QWidget* parent);
    QGroupBox* CreateControlEffectGroup(QWidget* parent);
    QGroupBox* CreateControlResultGroup(QWidget* parent);
    QFrame* CreateControlFrame(QWidget* parent);
    QFrame* CreateBackgroundFrame(QWidget* parent);
    QFrame* CreatePathTrackFrame(QWidget* parent);
    QFrame* CreatePathEffectFrame(QWidget* parent);
    QFrame* CreateEffectPropertyFrame(QWidget* parent);
    QFrame* CreateResultControlFrame(QWidget* parent);

    QFrame* CreateButtonGroupFrame(QWidget* parent, const char name[], int num, QPushButton* button[],
        QButtonGroup*& buttonGroup, int style);

    QFrame* CreatePriorityFrame(QWidget* parent, const char name[],
        QLineEdit*& lineedit, int type = 0);

    QFrame* CreateSlowMotionFrame(QWidget* parent, const char name[],
        QLineEdit*& lineedit, int type = 0);

    QFrame* CreateTransFrame(QWidget* parent);
    QFrame* CreateFadeFrame(QWidget* parent);

    QFrame* CreateSliderFrame(QWidget* parent, const char name[], QSlider*& slider, QLabel*& show_label, int min, int max);

public:
    //current states
    GEffectPtr cur_effect;
    GPathTrackDataPtr cur_tracked_path;
    int cur_frameidx;
    void setPathRoi(const GRoiPtr& roi);

    // current states (about tracking)
    GPathTracker* tracker;
    bool tracker_is_A_set_;
    int tracker_frame_A_;
    QRectF tracker_rect_A_;
    int tracker_frame_B_;
    QRectF tracker_rect_B_;
	
private:
	MLCacheTrajectories* trajp;
	MLCachePlatesConfig* platesp;
	Ui::Step3Widget *ui;

private:
    GMainDisplay* display_widget_;
    GResultDisplay* result_widget_;
    GTimelineWidget* timeline_widget_;
    QGroupBox* control_widget_;

    QSlider* control_play_slider_;

    // background widget
    QPushButton* control_add_still_button_;
    QPushButton* control_add_inpaint_button_;
    QPushButton* control_add_black_button_;
    //    QCheckBox *control_trash_checkbox_;
    //    QPushButton *control_add_border_button_;

        // PathTrack widget
    QPushButton* control_add_object_button_;
    QPushButton* control_set_A_button_;
    QPushButton* control_set_B_button_;
    QPushButton* control_track_AB_button_;
    QPushButton* control_track_stable_button_;

    // PathEffect widget
    QPushButton* control_add_multiple_button_;
    QPushButton* control_add_trail_button_;
    QPushButton* control_modify_path_button_;

    // Result
    QPushButton* control_clear_show_button_;
    QPushButton* video_play_button_;
    QPushButton* video_stop_button_;

    // style sheet
    QString button_style_[9];
    QString trans_button_style_[3];

    // effect property
    QSlider* anchor_slider_;
    QLabel* anchor_label_;
    QSlider* blur_alpha_slider_;
    QLabel* blur_alpha_label_;
    QSlider* trail_smooth_slider_;
    QLabel* trail_smooth_label_;
    QSlider* loop_slider_;
    QLabel* loop_label_;
    QPushButton* render_order_button_;
    QPushButton* marker_one_button_;
    QPushButton* marker_all_button_;
    QPushButton* path_line_button_;
    QPushButton* trans_level_button_[3];
    QButtonGroup* trans_button_group_;
    QPushButton* fade_level_button_[3];
    QButtonGroup* fade_button_group_;
    QLineEdit* control_edit_priority_;
    QLineEdit* control_edit_speed_;
    QFrame* loop_slider_frame_;
    QFrame* smooth_slider_frame_;
    QFrame* alpha_slider_frame_;

    /*
     * Export Widget
     */
    QDockWidget* dock_export_;
    //GExportWidget* export_widget_;

    /*
     * Timeline Widget
     */
    QDockWidget* dock_timeline_;
    QVBoxLayout* dock_timeline_layout_vert_;
    QButtonGroup* dock_timeline_button_group_;
    QList<GObjLayer*> m_objects;

    /*
     * Menu and Acts
     */
    QAction* act_open_video_;
    //    QAction* act_save_project_;
    //    QAction* act_load_project_;
    QAction* act_save_project2_;
    QAction* act_load_project2_;
    QAction* act_export_;
    QAction* act_exit_;
    QMenu* menu_file_;

    QAction* act_show_timeline_;
    QAction* act_show_control_;
    QAction* act_show_result_;
    QAction* act_show_export_;
    QMenu* menu_window_;

    QAction* act_set_fps_;
    QMenu* menu_setting_;
};