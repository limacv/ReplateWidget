#pragma once

#include <QWidget>
#include <qgroupbox.h>
#include <qdockwidget.h>
#include <qmainwindow.h>
#include <qslider.h>
#include "GPathTracker.h"
#include "GObjLayer.h"
namespace Ui { class Step3Widget; class GControlWidget; };

class MLCachePlatesConfig;
class MLCacheTrajectories;

class GMainDisplay;
class GResultWidget;
class GResultDisplay;
class GTimelineWidget;

class Step3Widget : public QMainWindow
{
	Q_OBJECT

public:
	Step3Widget(QWidget *parent = Q_NULLPTR);
    virtual ~Step3Widget();
    virtual void showEvent(QShowEvent* event);

    void setCurrentEffect(const GEffectPtr& efx);
    GEffectPtr createEffectFromPath(const GPathPtr& path, G_EFFECT_ID type);
    void trackPath();

signals:
    void frameChanged(int);

public slots:
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

    //void changeTrailSmooth(int s);
    void changeBlurAlpha(int s);
    void changeDuration(int s);
    void setFadeLevel(int id);
    void changeRenderOrder(bool b);
    void changeMarkerOne(bool b);
    void changeMarkerAll(bool b);
    void changePathLine(bool b);
    void setTransLevel(int id);
    void changePriority();
    void changeSpeed();

    void onCurrentEffectChanged();

    void onAutoAddPressed(bool checked);
    void onManualAddPressed(bool checked);
    void toggleInpaint(bool checked);

    void toggleModify(bool checked);

private:
    void CreateConnections();
    void PrepStyleSheet();
    void ApplyStyleSheet();

    void addSingleFramePath();
public:
    bool is_auto_selection() const;
    G_EFFECT_ID selected_efx_type() const;
    bool is_singleframe_efx() const;

public:
    //current states
    GEffectPtr cur_effect;
    GPathPtr cur_tracked_path;
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
    GResultWidget* result_widget_;
    GResultDisplay* result_widget_display_;
    GTimelineWidget* timeline_widget_;
    QWidget* control_widget_;
    Ui::GControlWidget* control_widget_ui_;
    //QGroupBox* control_widget_;

    // style sheet
    QString button_style_[9];
    QString trans_button_style_[3];

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