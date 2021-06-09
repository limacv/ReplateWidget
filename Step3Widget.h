#pragma once

#include <QWidget>
#include <qgroupbox.h>
#include <qdockwidget.h>
#include <qmainwindow.h>
#include <qslider.h>
#include "PathTrackerBase.h"
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
    void changePriority(int p);
    void changeSpeed(double s);

    void onCurrentEffectChanged();

    void onAutoAddPressed(bool checked);
    void onManualAddPressed(bool checked);

    void toggleModify(bool checked);

    QAction* showMainDisplayAction() const;
    QAction* showResultAction() const;
    QAction* showControlAction() const;
    QAction* showTimeLineAction() const;

private:
    void CreateConnections();
    void PrepStyleSheet();
    void ApplyStyleSheet();

    void addSingleFramePath();
public:
    int get_selection_mode() const;
    bool is_auto_selection() const;
    G_EFFECT_ID selected_efx_type() const;
    bool is_singleframe_efx() const;

public:
    //current states
    GEffectPtr cur_effect;
    GPathPtr cur_tracked_path;
    int cur_frameidx;
    void setPathRoi(const GRoiPtr& roi);
    void morphPathRoi(int dx, int dy);

    // current states (about tracking)
    PathTrackerBase* tracker;
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
};