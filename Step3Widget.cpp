#include "arthurstyle.h"
#include "Step3Widget.h"
#include "MLDataManager.h"
#include "ui_Step3Widget.h"
#include "GResultDisplay.h"
#include "GTimelineWidget.h"
#include "GMainDisplay.h"
#include "ui_GControlWidget.h"
#include "GPathTracker.h"
#include "MLPathTracker.h"
#include <qevent.h>
#include <qcombobox.h>

Step3Widget::Step3Widget(QWidget *parent)
	: QMainWindow(parent),
    tracker(new MLPathTracker)
{
	ui = new Ui::Step3Widget();
	ui->setupUi(this);
    
    PrepStyleSheet();
    display_widget_ = new GMainDisplay(this, this);
    result_widget_ = new GResultWidget(this);
    result_widget_display_ = result_widget_->display_widget;
    control_widget_ = new QWidget(this);
    control_widget_ui_ = new Ui::GControlWidget();
    control_widget_ui_->setupUi(control_widget_);
    //control_widget_ = CreateControlGroup(this);
    timeline_widget_ = new GTimelineWidget(this, this);

    ui->dockMain->setWidget(display_widget_);
	ui->dockResult->setWidget(result_widget_);
    ui->dockControl->setWidget(control_widget_);
    //ui->dockTimeline->setWidget(timeline_widget_);
    ui->scrollArea->setWidgetResizable(true);
    ui->scrollArea->setWidget(timeline_widget_);

    ApplyStyleSheet();
    CreateConnections();
}

Step3Widget::~Step3Widget()
{
	delete ui;
}

void Step3Widget::ApplyStyleSheet()
{
    control_widget_ui_->control_play_slider_->setStyle(new ArthurStyle);
    control_widget_ui_->control_modify_path_button_->setStyleSheet(button_style_[0]);
    control_widget_ui_->marker_one_button_->setStyleSheet(button_style_[7]);
    control_widget_ui_->marker_all_button_->setStyleSheet(button_style_[7]);
    //control_widget_ui_->control_clear_show_button_->setStyleSheet(button_style_[8]);
    control_widget_ui_->path_line_button_->setStyleSheet(button_style_[7]);
    //render_order_button_->setStyleSheet(button_style_[7]);
    control_widget_ui_->control_edit_priority_->setValidator(new QIntValidator(0, 80, control_widget_ui_->control_edit_priority_));
    control_widget_ui_->control_edit_speed_->setValidator(new QIntValidator(1, 3, control_widget_ui_->control_edit_speed_));
    display_widget_->setMouseTracking(true);
}

void Step3Widget::showEvent(QShowEvent* event)
{
    if (event->spontaneous()) return;
    auto& globaldata = MLDataManager::get();
    trajp = &globaldata.trajectories;
    platesp = &globaldata.plates_cache;

    UpdateDisplay(0);
    result_widget_->play();
    control_widget_ui_->anchor_slider_->setRange(0, globaldata.get_framecount() - 1);
    control_widget_ui_->control_play_slider_->setRange(0, globaldata.get_framecount() - 1);
    control_widget_ui_->control_play_slider_->setFocus();
    control_widget_ui_->blur_alpha_slider_->setValue(1 * control_widget_ui_->blur_alpha_slider_->maximum());
    onCurrentEffectChanged();

	platesp->initialize_cut(45);
}

void Step3Widget::CreateConnections()
{
    // play slider
    connect(control_widget_ui_->control_play_slider_, &QSlider::valueChanged, this, &Step3Widget::UpdateDisplay);
    connect(this, &Step3Widget::frameChanged, control_widget_ui_->control_play_slider_, &QSlider::setValue);

    //connect(trail_smooth_slider_, &QSlider::valueChanged, this, &Step3Widget::changeTrailSmooth);
    connect(control_widget_ui_->blur_alpha_slider_, &QSlider::valueChanged, this, &Step3Widget::changeBlurAlpha);
    connect(control_widget_ui_->anchor_slider_, &QSlider::valueChanged, this, &Step3Widget::changeDuration);
    //connect(render_order_button_, &QPushButton::toggled, this, &Step3Widget::changeRenderOrder);
    connect(control_widget_ui_->marker_one_button_, &QPushButton::toggled, this, &Step3Widget::changeMarkerOne);
    connect(control_widget_ui_->marker_all_button_, &QPushButton::toggled, this, &Step3Widget::changeMarkerAll);
    connect(control_widget_ui_->path_line_button_, &QPushButton::toggled, this, &Step3Widget::changePathLine);
    connect(control_widget_ui_->trans_spinbox, 
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int i) 
        {this->setTransLevel(i); });
    connect(control_widget_ui_->fade_spinbox, 
        static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int i) 
        {this->setFadeLevel(i); });
    connect(control_widget_ui_->control_edit_priority_, &QLineEdit::editingFinished, this, &Step3Widget::changePriority);
    connect(control_widget_ui_->control_edit_speed_, &QLineEdit::editingFinished, this, &Step3Widget::changeSpeed);

    connect(control_widget_ui_->control_modify_path_button_, &QPushButton::toggled, this, &Step3Widget::toggleModify);
    
    connect(this, &Step3Widget::frameChanged, timeline_widget_, &GTimelineWidget::updateFrameId);
    
    //connect(dock_export_, SIGNAL(visibilityChanged(bool)), this, SLOT(ExportFinished(bool)));

    connect(control_widget_ui_->combo_autoselection, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
        this, [this](int state) {
        display_widget_->setMouseTracking(state == 0);
        display_widget_->setPathSelectionMode(state == 2);
        result_widget_->setPathSelectModel(state == 2);
        });
    connect(control_widget_ui_->button_manual_add, &QPushButton::clicked, this, &Step3Widget::onManualAddPressed);
    connect(control_widget_ui_->button_auto_add, &QPushButton::clicked, this, &Step3Widget::onAutoAddPressed);
    //connect(ui->dockMain, &QDockWidget::destroyed)
}

int Step3Widget::get_selection_mode() const { return control_widget_ui_->combo_autoselection->currentIndex(); }
bool Step3Widget::is_auto_selection() const { return control_widget_ui_->combo_autoselection->currentIndex() == 0; }

inline G_EFFECT_ID Step3Widget::selected_efx_type() const
{
    G_EFFECT_ID idx[4] = { EFX_ID_MOTION, EFX_ID_TRAIL, EFX_ID_STILL, EFX_ID_INPAINT };
    return idx[control_widget_ui_->comboMode->currentIndex()];
}

inline bool Step3Widget::is_singleframe_efx() const { return selected_efx_type() == EFX_ID_STILL || selected_efx_type() == EFX_ID_INPAINT; }

void Step3Widget::setPathRoi(const GRoiPtr& roi)
{
    cur_tracked_path->setPathRoi(cur_frameidx, roi);
    cur_tracked_path->updateimages();
}

GEffectPtr Step3Widget::createEffectFromPath(const GPathPtr& path, G_EFFECT_ID type)
{
    if (!path) return 0;
    auto& effect_manager = MLDataManager::get().effect_manager_;
    GEffectPtr efx = effect_manager.addEffect(path, type);
    if (type != EFX_ID_TRASH)
        setCurrentEffect(efx);
    return efx;
}

void Step3Widget::setCurrentEffect(const GEffectPtr& efx)
{
    cur_effect = efx;
    if (efx)
        cur_tracked_path = efx->path();
    else
        cur_tracked_path = nullptr;

    onCurrentEffectChanged();
}

void Step3Widget::PrepStyleSheet() {

    button_style_[0] = QString("* { border-width: 1px;"
        "background-color: lightsteelblue; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 6ex; min-height: 2.5ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #FFA54F;; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}");

    button_style_[1] = QString("* { border-width: 2px;"
        "background-color: indianred; border-color: darkslategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}");

    button_style_[2] = QString("* { border-width: 2px;"
        "background-color: #e5e67f; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #FFA54F;; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}");

    button_style_[3] = QString("* { border-width: 1px;"
        "background-color: steelblue; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 6ex; min-height: 2.5ex;}");

    button_style_[4] = QString("* { border-width: 2px;"
        "background-color: #6AAEFC; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #2A77DC;; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}");

    button_style_[5] = QString("* { border-width: 2px;"
        "background-color: #98FB98; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #2A77DC;; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 2.5ex;}");

    button_style_[6] = QString("* { border-width: 2px;"
        "background-color: #98FB98; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 3ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #FFA54F;; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 9ex; min-height: 3ex;}");

    button_style_[7] = QString("* { "
        "border-width: 1px; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 6ex; min-height: 2.5ex;}"
        "*:checked{border-width: 1px;"
        "background-color: #6AAEFC; border-color: slategray;"
        "border-style: groove; border-radius: 5;padding: 3px; min-width: 6ex; min-height: 2.5ex;}");

    button_style_[8] = QString("* { border-width: 1px;"
        "background-color: Azure; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 6ex; min-height: 2.5ex;}"
        "*:checked{}"
        "*:hover{}");

    trans_button_style_[0] =
        QString("* { border-width: 1px;"
            "background-color: #f0fff0; border-color: slategray;"
            "border-style: solid; border-radius: 3;padding: 3px; min-width: 3ex; min-height: 3ex;}"
            "*:checked{border-width: 1px;"
            "background-color: #778899;; border-color: slategray;"
            "border-style: solid; border-radius: 3;padding: 3px; min-width: 3ex; min-height: 3ex;}");

    trans_button_style_[1] = QString("* { border-width: 1px;"
        "background-color: #cdc0b0; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 4ex; min-height: 2.5ex;}");

    trans_button_style_[2] = QString("* { border-width: 1px;"
        "background-color: #8b8378; border-color: slategray;"
        "border-style: solid; border-radius: 5;padding: 3px; min-width: 4ex; min-height: 2.5ex;}");
}

void Step3Widget::trackPath()
{
    if (cur_tracked_path) {
        tracker->updateTrack(cur_tracked_path.get());
        cur_tracked_path->updateimages();
    }
    else {
        cur_tracked_path = tracker->trackPath(tracker_frame_A_, tracker_rect_A_, tracker_frame_B_, tracker_rect_B_);
        if (cur_tracked_path != nullptr)
            cur_tracked_path->updateimages();
    }
}
///////////////////////
// SLOTS
////////////////////////////

void Step3Widget::UpdateDisplay(int i)
{
    if (i != cur_frameidx)
    {
        qDebug() << "#Frame id:" << i;
        cur_frameidx = i;
        display_widget_->update();

        frameChanged(i);
    }
}

void Step3Widget::saveUI(YAML::Emitter& out) const
{
    out << YAML::Key << "UI" << YAML::Value;
    out << YAML::BeginSeq;
    for (int i = 0; i < m_objects.size(); ++i) {
        if (!m_objects[i]->isHidden())
            m_objects[i]->write(out);
    }
    out << YAML::EndSeq;
}

void Step3Widget::loadUI(const GEffectManager* manager)
{
    // TODO: need implementation
    const PriorityEffectMap& effects = manager->getEffects();
    for (PriorityEffectMap::const_iterator it = effects.begin();
        it != effects.end(); ++it) {
        std::list<GEffectPtr> list = it->second;
        for (std::list<GEffectPtr>::iterator iter = list.begin();
            iter != list.end(); ++iter) {
            GEffectPtr& effect = (*iter);
            G_EFFECT_ID type = effect->type();
            if (type == EFX_ID_MOTION || type == EFX_ID_TRAIL || type == EFX_ID_STILL || type == EFX_ID_INPAINT) {
                qDebug() << "Load" << G_EFFECT_STR[type];
                timeline_widget_->addTimeline(effect);
                //                QString name;// = QString("%1").arg(m_objects.size() + 1);
                //                GObjLayer *obj = addObject(name.toStdString(), effect.get(), video_content_,
                //                                           display_->size(), dock_timeline_button_group_);
                //                Q_ASSERT(effect->type() == type);
                //                obj->addEffect(effect);
            }
        }
    }
}

void Step3Widget::updateAnchorLength()
{
    control_widget_ui_->anchor_slider_->setValue(MLDataManager::get().plates_cache.replate_duration);
}

void Step3Widget::changeBlurAlpha(int s)
{
    qreal a = s / (qreal)control_widget_ui_->blur_alpha_slider_->maximum();
    if (cur_effect)
        cur_effect->setAlpha(a);
    control_widget_ui_->blur_alpha_label_->setText(QString::number(a));
}

void Step3Widget::changeDuration(int s)
{
    MLDataManager::get().plates_cache.replate_duration = s;
    control_widget_ui_->anchor_label_->setText(QString::number(s));
    timeline_widget_->updateDuration();
}

void Step3Widget::setFadeLevel(int id)
{
    if (!cur_effect) return;
    cur_effect->setFadeLevel(id);
}

void Step3Widget::setTransLevel(int id)
{
    if (!cur_effect) return;

    cur_effect->setTransLevel(id);
}

void Step3Widget::changeRenderOrder(bool backward)
{
    if (cur_effect)
        cur_effect->setOrder(backward);
}

void Step3Widget::changeMarkerOne(bool b)
{
    if (!cur_effect) return;
    if (b) {
        control_widget_ui_->marker_all_button_->setChecked(false);
        cur_effect->setMarker(1);
    }
    else cur_effect->setMarker(0);
    //data_manager_->setMarkerOne(b, marker_all_button_);
}

void Step3Widget::changeMarkerAll(bool b)
{
    if (!cur_effect) return;
    if (b) {
        control_widget_ui_->marker_one_button_->setChecked(false);
        cur_effect->setMarker(2);
    }
    else cur_effect->setMarker(0);
    //data_manager_->setMarkerAll(b, marker_one_button_);
}

void Step3Widget::changePathLine(bool b)
{
    if (!cur_effect) return;
    cur_effect->setTrailLine(b);
}

void Step3Widget::changePriority()
{
    if (!cur_effect) return;

    int priority = control_widget_ui_->control_edit_priority_->text().toInt();
    auto& effect_manager = MLDataManager::get().effect_manager_;
    effect_manager.popEffect(cur_effect);
    cur_effect->setPriority(priority);
    effect_manager.pushEffect(cur_effect);
}

void Step3Widget::changeSpeed()
{
    int speed = control_widget_ui_->control_edit_speed_->text().toInt();
    if (!cur_effect) return;
    cur_effect->setSpeed(speed);
}

//void Step3Widget::changeCurrentEffect(GEffectPtr &efx)
//{
//    // TODO: update Effect widget
////    setCurrentPath(efx->path());
////    setCurrentEffect(efx);
////    onCurrentEffectChanged();
////    update();
//}

void Step3Widget::onCurrentEffectChanged()
{
    display_widget_->update();
    
    if (!cur_effect || cur_effect->type() == EFX_ID_TRASH || cur_effect->type() == EFX_ID_BLACK) {
        qDebug() << "Current effect not selected";
        control_widget_ui_->trans_spinbox->setEnabled(false);
        control_widget_ui_->fade_spinbox->setEnabled(false);
        control_widget_ui_->control_edit_priority_->setEnabled(false);
        control_widget_ui_->control_edit_speed_->setEnabled(false);
        control_widget_ui_->marker_one_button_->setEnabled(false);
        control_widget_ui_->marker_all_button_->setEnabled(false);
        control_widget_ui_->path_line_button_->setEnabled(false);
        //render_order_button_->setEnabled(false);

        //control_widget_ui_->smooth_slider_frame_->setEnabled(false);
        //control_widget_ui_->loop_slider_frame_->setEnabled(false);
        control_widget_ui_->alpha_slider_frame_->setEnabled(false);
        return;
    }

    qDebug() << "Update Effect Widget";
    if (cur_effect->getAlpha() < 0)
        control_widget_ui_->alpha_slider_frame_->setEnabled(false);
    else {
        control_widget_ui_->alpha_slider_frame_->setEnabled(true);
        control_widget_ui_->blur_alpha_slider_->setValue(cur_effect->getAlpha()
            * control_widget_ui_->blur_alpha_slider_->maximum());
    }

    if (cur_effect->getTransLevel() == -1) {
        control_widget_ui_->trans_spinbox->setEnabled(false);
    }
    else {
        control_widget_ui_->trans_spinbox->setEnabled(true);
        control_widget_ui_->trans_spinbox->setValue(cur_effect->getTransLevel());
    }

    if (cur_effect->getFadeLevel() == -1) {
        control_widget_ui_->fade_spinbox->setEnabled(false);
    }
    else {
        control_widget_ui_->fade_spinbox->setEnabled(true);
        control_widget_ui_->fade_spinbox->setValue(cur_effect->getTransLevel());
    }

    if (cur_effect->getMarker() == -1) {
        control_widget_ui_->marker_one_button_->setEnabled(false);
        control_widget_ui_->marker_all_button_->setEnabled(false);
    }
    else {
        control_widget_ui_->marker_one_button_->setEnabled(true);
        control_widget_ui_->marker_one_button_->setChecked(cur_effect->getMarker() == 1);
        control_widget_ui_->marker_all_button_->setEnabled(true);
        control_widget_ui_->marker_all_button_->setChecked(cur_effect->getMarker() == 2);
    }

    if (cur_effect->getTrailLine() == -1) {
        control_widget_ui_->path_line_button_->setEnabled(false);
    }
    else {
        control_widget_ui_->path_line_button_->setEnabled(true);
        control_widget_ui_->path_line_button_->setChecked(cur_effect->getTrailLine());
    }

    control_widget_ui_->control_edit_priority_->setEnabled(true);
    control_widget_ui_->control_edit_priority_->setText(
        QString::number(cur_effect->priority()));

    if (cur_effect->type() == EFX_ID_MOTION)
    {
        control_widget_ui_->control_edit_speed_->setEnabled(true);
        control_widget_ui_->control_edit_speed_->setText(QString::number(cur_effect->speed()));
    }
}

void Step3Widget::addSingleFramePath()
{
    QRectF select_rect;
    QPainterPath select_path;
    if (!display_widget_->curSelectRectF().isEmpty()) {
        select_rect = display_widget_->curSelectRectF();
        select_path = display_widget_->curSelectPath();
    }
    else if (!result_widget_display_->curSelectRectF().isEmpty()) {
        select_rect = result_widget_display_->curSelectRectF();
        select_path = result_widget_display_->curSelectPath();
    }
    else {
        cur_tracked_path = nullptr;
        return;
    }
    cur_tracked_path = GPathPtr(new GPath(cur_frameidx, select_rect, select_path));
}

void Step3Widget::onAutoAddPressed(bool checked)
{
    if (is_singleframe_efx())
    {
        addSingleFramePath();
    }
    else
    {
        const QRectF& rectf = display_widget_->curSelectRectF();
        if (rectf.isEmpty())
        {
            result_widget_->clearMouseSelection();
            return;
        }

        tracker_frame_A_ = cur_frameidx;
        tracker_rect_A_ = rectf;
        tracker_frame_B_ = -1;
        tracker_rect_B_ = rectf;
        cur_tracked_path = nullptr;

        // track path
        trackPath();
    }

    if (cur_tracked_path != nullptr)
    {
        GEffectPtr efx = createEffectFromPath(cur_tracked_path, selected_efx_type());
        timeline_widget_->addTimeline(efx);
    }

    display_widget_->clearMouseSelection();
    result_widget_->clearMouseSelection();
    result_widget_->update();
    onCurrentEffectChanged();
}

void Step3Widget::onManualAddPressed(bool checked)
{
    // single frame effect
    if (is_singleframe_efx())
    {
        addSingleFramePath();
        control_widget_ui_->button_manual_add->setChecked(false);
    }
    // not single frame effect
    else if (checked) {  // first stage
        const QRectF& rectf = display_widget_->curSelectRectF();
        if (rectf.isEmpty())
        {
            control_widget_ui_->button_manual_add->setChecked(false);
            result_widget_->clearMouseSelection();
            return;
        }
        tracker_frame_A_ = cur_frameidx;
        tracker_rect_A_ = rectf;
        tracker_is_A_set_ = true;

        control_widget_ui_->button_manual_add->setText("Add");
        control_widget_ui_->button_auto_add->setEnabled(false);
        display_widget_->clearMouseSelection();
        cur_tracked_path = nullptr;
        return;
    }
    else {
        if (!tracker_is_A_set_ || tracker_frame_A_ >= cur_frameidx)
        {
            return;
        }

        const QRectF& rectf = display_widget_->curSelectRectF();
        tracker_frame_B_ = cur_frameidx;
        tracker_rect_B_ = rectf;
        // track path
        trackPath();

        control_widget_ui_->button_manual_add->setText("Start");
        control_widget_ui_->button_auto_add->setEnabled(false);
    }

    GEffectPtr efx = createEffectFromPath(cur_tracked_path, selected_efx_type());
    if (efx) timeline_widget_->addTimeline(efx);

    display_widget_->clearMouseSelection();
    result_widget_->clearMouseSelection();
    result_widget_->update();
    onCurrentEffectChanged();
}

void Step3Widget::toggleModify(bool checked)
{
    //    qDebug() << "ToggleModify" << checked << data_manager_->currentPath();
    if (checked) {
        if (!cur_tracked_path) {
            control_widget_ui_->control_modify_path_button_->setChecked(false);
            return;
        }
        display_widget_->toggleModifyMode();
        
        // set button
        control_widget_ui_->combo_autoselection->setEnabled(false);
        control_widget_ui_->comboMode->setEnabled(false);
        control_widget_ui_->button_auto_add->setEnabled(false);
        control_widget_ui_->button_manual_add->setEnabled(false);
        control_widget_ui_->control_modify_path_button_->setText("Apply");
    }
    else {
        control_widget_ui_->combo_autoselection->setEnabled(true);
        control_widget_ui_->comboMode->setEnabled(true);
        control_widget_ui_->button_auto_add->setEnabled(true);
        control_widget_ui_->button_manual_add->setEnabled(true);
        control_widget_ui_->control_modify_path_button_->setText("Modify");
        if (!cur_tracked_path) return;

        display_widget_->toggleModifyMode();
        display_widget_->update();
    }
}

QAction* Step3Widget::showMainDisplayAction() const { return ui->dockMain->toggleViewAction(); }
QAction* Step3Widget::showResultAction() const { return ui->dockResult->toggleViewAction();}
QAction* Step3Widget::showControlAction() const { return ui->dockControl->toggleViewAction(); }
QAction* Step3Widget::showTimeLineAction() const { return ui->dockTimeline->toggleViewAction(); }