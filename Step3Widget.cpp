#include "arthurstyle.h"
#include "Step3Widget.h"
#include "MLDataManager.h"
#include "ui_Step3Widget.h"
#include "GResultDisplay.h"
#include "GTimelineWidget.h"
#include "GMainDisplay.h"

Step3Widget::Step3Widget(QWidget *parent)
	: QWidget(parent)
{
	ui = new Ui::Step3Widget();
	ui->setupUi(this);
}

Step3Widget::~Step3Widget()
{
	delete ui;
}

void Step3Widget::initState()
{
    PrepStyleSheet();

    display_widget_ = new GMainDisplay(this, this);
    result_widget_ = new GResultDisplay(this);
    control_widget_ = CreateControlGroup(this);
    timeline_widget_ = new GTimelineWidget(this, this);
    ui->dockMain->setWidget(display_widget_);
	ui->dockResult->setWidget(result_widget_);
    ui->dockControl->setWidget(control_widget_);
    ui->dockTimeline->setWidget(timeline_widget_);
    CreateConnections();

	auto& globaldata = MLDataManager::get();
	trajp = &globaldata.trajectories;
	platesp = &globaldata.plates_cache;

    UpdateDisplay(0);
    result_widget_->play();
    control_play_slider_->setRange(0, globaldata.get_framecount() - 1);
    control_play_slider_->setFocus();

    trail_smooth_slider_->setValue(3);
    blur_alpha_slider_->setValue(1 * blur_alpha_slider_->maximum());
    loop_slider_->setValue(0);
    updateEffectWidget();
}

void Step3Widget::onWidgetShowup()
{
	platesp->initialize_cut(45);
}



void Step3Widget::CreateConnections()
{
    // play slider
    connect(control_play_slider_, &QSlider::valueChanged, this, &Step3Widget::UpdateDisplay);
    connect(this, &Step3Widget::frameChanged, control_play_slider_, &QSlider::setValue);

    connect(video_play_button_, &QPushButton::clicked, result_widget_, &GResultDisplay::play);
    connect(video_stop_button_, &QPushButton::clicked, result_widget_, &GResultDisplay::play);

    connect(trail_smooth_slider_, &QSlider::valueChanged, this, &Step3Widget::changeTrailSmooth);
    connect(blur_alpha_slider_, &QSlider::valueChanged, this, &Step3Widget::changeBlurAlpha);
    connect(anchor_slider_, &QSlider::valueChanged, this, &Step3Widget::changeDuration);
    connect(render_order_button_, &QPushButton::toggled, this, &Step3Widget::changeRenderOrder);
    connect(marker_one_button_, &QPushButton::toggled, this, &Step3Widget::changeMarkerOne);
    connect(marker_all_button_, &QPushButton::toggled, this, &Step3Widget::changeMarkerAll);
    connect(path_line_button_, &QPushButton::toggled, this, &Step3Widget::changePathLine);
    connect(trans_level_button_[0], &QPushButton::clicked, this, &Step3Widget::changeTransLevel0);
    connect(trans_level_button_[1], &QPushButton::clicked, this, &Step3Widget::changeTransLevel1);
    connect(trans_level_button_[2], &QPushButton::clicked, this, &Step3Widget::changeTransLevel2);
    connect(fade_level_button_[0], &QPushButton::clicked, this, &Step3Widget::changeFadeLevel0);
    connect(fade_level_button_[1], &QPushButton::clicked, this, &Step3Widget::changeFadeLevel1);
    connect(fade_level_button_[2], &QPushButton::clicked, this, &Step3Widget::changeFadeLevel2);
    connect(control_edit_priority_, &QLineEdit::editingFinished, this, &Step3Widget::changePriority);
    connect(control_edit_speed_, &QLineEdit::editingFinished, this, &Step3Widget::changeSpeed);

    connect(control_add_still_button_, &QPushButton::toggled, this, &Step3Widget::toggleStill);
    connect(control_add_inpaint_button_, &QPushButton::toggled, this, &Step3Widget::toggleInpaint);
    connect(control_add_black_button_, &QPushButton::toggled, this, &Step3Widget::toggleBlack);
    connect(control_add_multiple_button_, &QPushButton::toggled, this, &Step3Widget::toggleMotion);
    connect(control_add_trail_button_, &QPushButton::toggled, this, &Step3Widget::toggleTrail);
    connect(control_modify_path_button_, &QPushButton::toggled, this, &Step3Widget::toggleModify);

    connect(timeline_widget_, &GTimelineWidget::currentEffectChanged, this, &Step3Widget::updateEffectWidget);
    connect(this, &Step3Widget::frameChanged, timeline_widget_, &GTimelineWidget::updateFrameId);

    //connect(dock_export_, SIGNAL(visibilityChanged(bool)), this, SLOT(ExportFinished(bool)));
}

void Step3Widget::setPathRoi(const GRoiPtr& roi)
{
    cur_tracked_path->setPathRoi(cur_frameidx, roi);
    cur_tracked_path->updateimages();
}

QGroupBox* Step3Widget::CreateControlGroup(QWidget* parent)
{
    QGroupBox* res = new QGroupBox(parent);
    res->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    res->setFlat(true);

    QVBoxLayout* layout = new QVBoxLayout(res);

    control_play_slider_ = new QSlider(Qt::Horizontal, res);
    control_play_slider_->setStyle(new ArthurStyle);
    control_play_slider_->setSizePolicy(QSizePolicy::Expanding,
        QSizePolicy::Minimum);
    //    control_play_slider_->setTickPosition(QSlider::TicksBelow);
    layout->addWidget(control_play_slider_);

    QHBoxLayout* hLayout = new QHBoxLayout(res);
    layout->addLayout(hLayout);

    hLayout->addWidget(CreateControlFrame(res));
    hLayout->addWidget(CreateEffectPropertyFrame(res));

    return res;
}

GEffectPtr Step3Widget::createEffectFromPath(const GPathTrackDataPtr& path, G_EFFECT_ID type)
{
    if (!path) return 0;
    auto& effect_manager = MLDataManager::get().effect_manager_;
    GEffectPtr efx = effect_manager.addEffect(path, type);
    if (type != EFX_ID_BLACK && type != EFX_ID_TRASH)
        setCurrentEffect(efx);
    return efx;
}

void Step3Widget::setCurrentEffect(const GEffectPtr& efx)
{
    if (efx != cur_effect) {
        cur_effect = efx;
    }
    if (cur_tracked_path != efx->path())
        cur_tracked_path = efx->path();
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

QGroupBox* Step3Widget::CreateControlPathGroup(QWidget* parent)
{
    QGroupBox* gb = new QGroupBox("Track Path", parent);
    gb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    gb->setAlignment(Qt::AlignLeft);
    gb->setFlat(true);

    QHBoxLayout* hLayout = new QHBoxLayout(gb);
    hLayout->setAlignment(Qt::AlignLeft);
    hLayout->setMargin(0);

    hLayout->addSpacing(10);

    QFrame* track_frame = CreatePathTrackFrame(gb);
    hLayout->addWidget(track_frame);

    return gb;
}

QGroupBox* Step3Widget::CreateControlEffectGroup(QWidget* parent)
{
    QGroupBox* res = new QGroupBox("Modes", parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    res->setAlignment(Qt::AlignLeft);
    res->setFlat(true);

    QHBoxLayout* hLayout = new QHBoxLayout(res);
    hLayout->setAlignment(Qt::AlignLeft);
    hLayout->setMargin(0);

    hLayout->addSpacing(5);

    hLayout->addWidget(CreateBackgroundFrame(res));

    hLayout->addSpacing(10);

    hLayout->addWidget(CreatePathEffectFrame(res));

    return res;
}

QGroupBox* Step3Widget::CreateControlResultGroup(QWidget* parent)
{
    QGroupBox* res = new QGroupBox("Result", parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    res->setAlignment(Qt::AlignLeft);
    res->setFlat(true);

    QHBoxLayout* hLayout = new QHBoxLayout(res);
    hLayout->setAlignment(Qt::AlignLeft);
    hLayout->setMargin(0);

    hLayout->addSpacing(5);

    hLayout->addWidget(CreateResultControlFrame(res));

    return res;
}

QFrame* Step3Widget::CreateControlFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    //    res->setStyleSheet("background-color:#F2F2F2;");

    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setMargin(0);
    layout->setAlignment(Qt::AlignLeft);

    layout->addSpacing(10);
    
    QFrame* anchor_frame = CreateSliderFrame(
        res, "Duration", anchor_slider_, anchor_label_,
        1, MLDataManager::get().get_framecount());
    layout->addWidget(anchor_frame);
    layout->addSpacing(10);

    QGroupBox* track_group = CreateControlPathGroup(res);
    layout->addWidget(track_group);
    track_group->hide();
    //layout->addSpacing(10);

    layout->addWidget(CreateControlEffectGroup(res));
    layout->addSpacing(20);

    layout->addWidget(CreateControlResultGroup(res));
    return res;
}

QFrame* Step3Widget::CreateBackgroundFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setMargin(0);

    control_add_still_button_ = new QPushButton("Still", res);
    control_add_still_button_->setStyleSheet(button_style_[6]);
    control_add_still_button_->setCheckable(true);
    control_add_still_button_->setChecked(false);
    control_add_still_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addWidget(control_add_still_button_);

    control_add_inpaint_button_ = new QPushButton("Inpaint", res);
    control_add_inpaint_button_->setCheckable(true);
    control_add_inpaint_button_->setChecked(false);
    control_add_inpaint_button_->setStyleSheet(button_style_[6]);
    control_add_inpaint_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addWidget(control_add_inpaint_button_);

    control_add_black_button_ = new QPushButton("Black", res);
    control_add_black_button_->setCheckable(true);
    control_add_black_button_->setChecked(false);
    control_add_black_button_->setStyleSheet(button_style_[6]);
    control_add_black_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    layout->addWidget(control_add_black_button_);

    //    control_trash_checkbox_ = new QCheckBox("Black", res);
    //    layout->addWidget(control_trash_checkbox_);
    //    control_trash_checkbox_->hide();

    //    control_add_border_button_ = new QPushButton("Border", res);
    //    control_add_border_button_->setCheckable(true);
    //    control_add_border_button_->setChecked(false);
    //    control_add_border_button_->setStyleSheet(button_style_[6]);
    //    control_add_border_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    //    control_add_border_button_->hide();
    //    layout->addWidget(control_add_border_button_);

    return res;
}

void Step3Widget::trackPath()
{
    if (cur_tracked_path) {
        tracker->updateTrack(cur_tracked_path.get());
        cur_tracked_path->updateimages();
    }
    else {
        GPath* path = tracker->trackPath(tracker_frame_A_, tracker_rect_A_, tracker_frame_B_, tracker_rect_B_);
        path->updateimages();
        cur_tracked_path = GPathTrackDataPtr(path);
    }
}

QFrame* Step3Widget::CreatePathTrackFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    //    res->setFlat(true);

    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setMargin(0);

    control_add_object_button_ = new QPushButton(tr("Add"), res);
    control_add_object_button_->setStyleSheet(button_style_[1]);
    control_add_object_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(control_add_object_button_);

    layout->addSpacing(5);

    control_set_A_button_ = new QPushButton("A");
    control_set_A_button_->setStyleSheet(button_style_[3]);
    control_set_A_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    control_set_B_button_ = new QPushButton("B");
    control_set_B_button_->setStyleSheet(button_style_[3]);
    control_set_B_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(control_set_A_button_);
    layout->addWidget(control_set_B_button_);
    layout->addSpacing(5);

    control_track_AB_button_ = new QPushButton("Track");
    control_track_AB_button_->setStyleSheet(button_style_[0]);
    control_track_AB_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(control_track_AB_button_);

    control_track_stable_button_ = new QPushButton("Static");
    control_track_stable_button_->setStyleSheet(button_style_[0]);
    control_track_stable_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(control_track_stable_button_);

    return res;
}

QFrame* Step3Widget::CreatePathEffectFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setMargin(0);

    control_add_multiple_button_ = new QPushButton("Motion", parent);
    control_add_multiple_button_->setStyleSheet(button_style_[2]);
    control_add_multiple_button_->setCheckable(true);
    control_add_multiple_button_->setChecked(false);
    control_add_multiple_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    control_add_trail_button_ = new QPushButton("Trail", parent);
    control_add_trail_button_->setStyleSheet(button_style_[2]);
    control_add_trail_button_->setCheckable(true);
    control_add_trail_button_->setChecked(false);
    control_add_trail_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    control_modify_path_button_ = new QPushButton("Modify");
    control_modify_path_button_->setStyleSheet(button_style_[0]);
    control_modify_path_button_->setCheckable(true);
    control_modify_path_button_->setChecked(false);
    control_modify_path_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    layout->addWidget(control_add_multiple_button_);
    layout->addWidget(control_add_trail_button_);
    layout->addSpacing(10);
    layout->addWidget(control_modify_path_button_);
    return res;
}

QFrame* Step3Widget::CreateEffectPropertyFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    res->setStyleSheet("background-color:#E8E8E8;");

    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setAlignment(Qt::AlignLeft);
    layout->setMargin(0);

    QFrame* line2 = new QFrame(res);
    line2->setFrameStyle(QFrame::VLine | QFrame::Sunken);
    layout->addWidget(line2);

    //    layout->addSpacing(10);

    layout->addWidget(CreatePriorityFrame(res, "Priority", control_edit_priority_));

    layout->addSpacing(10);

    layout->addWidget(CreateSlowMotionFrame(res, "Speed", control_edit_speed_));

    layout->addSpacing(10);

    layout->addWidget(CreateFadeFrame(res));

    layout->addSpacing(10);

    layout->addWidget(CreateTransFrame(res));

    layout->addSpacing(10);

    render_order_button_ = new QPushButton("Order", res);
    render_order_button_->setStyleSheet(button_style_[7]);
    render_order_button_->setCheckable(true);
    render_order_button_->setChecked(false);
    layout->addWidget(render_order_button_);
    render_order_button_->hide();

    marker_one_button_ = new QPushButton("Mark one", res);
    marker_one_button_->setStyleSheet(button_style_[7]);
    marker_one_button_->setCheckable(true);
    marker_one_button_->setChecked(false);
    layout->addWidget(marker_one_button_);

    marker_all_button_ = new QPushButton("Mark all", res);
    marker_all_button_->setStyleSheet(button_style_[7]);
    marker_all_button_->setCheckable(true);
    marker_all_button_->setChecked(false);
    layout->addWidget(marker_all_button_);

    path_line_button_ = new QPushButton("Path", res);
    path_line_button_->setStyleSheet(button_style_[7]);
    path_line_button_->setCheckable(true);
    path_line_button_->setChecked(false);
    layout->addWidget(path_line_button_);

    layout->addSpacing(10);
    smooth_slider_frame_ = CreateSliderFrame(
        res, "Smooth", trail_smooth_slider_, trail_smooth_label_, 0, 50);
    layout->addWidget(smooth_slider_frame_);
    smooth_slider_frame_->hide();

    alpha_slider_frame_ = CreateSliderFrame(
        res, "Blur alpha", blur_alpha_slider_, blur_alpha_label_, 0, 100);
    layout->addWidget(alpha_slider_frame_);
    //        alpha_slider_frame_->hide();


    loop_slider_frame_ = CreateSliderFrame(
        res, "Loop", loop_slider_, loop_label_, 0, MLDataManager::get().get_framecount());
    layout->addWidget(loop_slider_frame_);
    loop_slider_frame_->hide();

    return res;
}

QFrame* Step3Widget::CreateResultControlFrame(QWidget* parent)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QHBoxLayout* layout = new QHBoxLayout(res);
    layout->setMargin(0);

    video_play_button_ = new QPushButton(tr("Play"), res);
    video_play_button_->setStyleSheet(button_style_[8]);
    video_play_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    video_stop_button_ = new QPushButton(tr("Stop"), res);
    video_stop_button_->setStyleSheet(button_style_[8]);
    video_stop_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    control_clear_show_button_ = new QPushButton("Clear");
    control_clear_show_button_->setStyleSheet(button_style_[8]);
    control_clear_show_button_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    control_clear_show_button_->hide();

    layout->addWidget(video_play_button_);
    layout->addWidget(video_stop_button_);
    //    layout->addSpacing(20);
    layout->addWidget(control_clear_show_button_);
    return res;
}

QFrame* Step3Widget::CreateButtonGroupFrame(QWidget* parent, const char name[], int num, QPushButton* button[],
    QButtonGroup*& buttonGroup, int style)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    res->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    QVBoxLayout* vLayout = new QVBoxLayout(res);
    vLayout->setMargin(0);

    QLabel* label = new QLabel(name, res);
    label->setAlignment(Qt::AlignCenter);
    vLayout->addWidget(label);

    buttonGroup = new QButtonGroup(res);
    buttonGroup->setExclusive(true);
    char c[] = "LMH";
    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setMargin(0);
    for (int i = 0; i < num; ++i) {
        button[i] = new QPushButton(QString(c[i]), res);
        button[i]->setStyleSheet(trans_button_style_[style]);
        button[i]->setCheckable(true);
        button[i]->setChecked(false);
        buttonGroup->addButton(button[i]);
        buttonGroup->setId(button[i], i);
        hLayout->addWidget(button[i]);
    }
    vLayout->addLayout(hLayout);
    return res;
}

QFrame* Step3Widget::CreatePriorityFrame(QWidget* parent, const char name[],
    QLineEdit*& lineedit, int type)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QVBoxLayout* vLayout = new QVBoxLayout(res);
    vLayout->setMargin(2);

    QLabel* label = new QLabel(name, res);
    label->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
    vLayout->addWidget(label);

    lineedit = new QLineEdit(res);
    lineedit->setFixedWidth(40);
    lineedit->setValidator(new QIntValidator(0, 80, lineedit));
    lineedit->setPlaceholderText("0~80");
    vLayout->addWidget(lineedit);
    return res;
}

QFrame* Step3Widget::CreateSlowMotionFrame(QWidget* parent, const char name[],
    QLineEdit*& lineedit, int type)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QVBoxLayout* vLayout = new QVBoxLayout(res);
    vLayout->setMargin(2);

    QLabel* label = new QLabel(name, res);
    label->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
    vLayout->addWidget(label);

    lineedit = new QLineEdit(res);
    lineedit->setFixedWidth(30);
    lineedit->setValidator(new QIntValidator(1, 3, lineedit));
    lineedit->setPlaceholderText("1~3");
    vLayout->addWidget(lineedit);
    return res;
}

QFrame* Step3Widget::CreateTransFrame(QWidget* parent)
{
    return CreateButtonGroupFrame(parent, "Trans", 3, trans_level_button_,
        trans_button_group_, 0);
}

QFrame* Step3Widget::CreateFadeFrame(QWidget* parent)
{
    return CreateButtonGroupFrame(parent, "Fade", 3, fade_level_button_,
        fade_button_group_, 0);
}

QFrame* Step3Widget::CreateSliderFrame(QWidget* parent, const char name[],
    QSlider*& slider, QLabel*& show_label, int min, int max)
{
    QFrame* res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QVBoxLayout* layout = new QVBoxLayout(res);
    layout->setMargin(2);

    QHBoxLayout* hLayout = new QHBoxLayout;
    QLabel* label = new QLabel(name, res);
    label->setAlignment(Qt::AlignLeft);
    hLayout->addWidget(label);
    show_label = new QLabel("", res);
    hLayout->addWidget(show_label);
    layout->addLayout(hLayout);
    slider = new QSlider(Qt::Horizontal, res);
    slider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    slider->setFixedWidth(100);
    slider->setRange(min, max);
    layout->addWidget(slider);
    return res;
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
            if (type == EFX_ID_MOTION || type == EFX_ID_TRAIL || type == EFX_ID_STILL) {
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
    anchor_slider_->setValue(MLDataManager::get().plates_cache.replate_duration);
}

void Step3Widget::changeTrailSmooth(int s)
{
    if (cur_effect)
        cur_effect->setSmooth(s);
    trail_smooth_label_->setText(QString::number(s));
}

void Step3Widget::changeBlurAlpha(int s)
{
    qreal a = s / (qreal)blur_alpha_slider_->maximum();
    if (cur_effect)
        cur_effect->setAlpha(a);
    blur_alpha_label_->setText(QString::number(a));
}

void Step3Widget::changeDuration(int s)
{
    MLDataManager::get().plates_cache.replate_duration = s;
    anchor_label_->setText(QString::number(s));
    timeline_widget_->updateDuration();
}

void Step3Widget::changeFadeLevel0()
{
    setFadeLevel(1);
}

void Step3Widget::changeFadeLevel1()
{
    setFadeLevel(2);
}

void Step3Widget::changeFadeLevel2()
{
    setFadeLevel(3);
}

void Step3Widget::setFadeLevel(int id)
{
    if (!cur_effect) return;
    if (cur_effect->getFadeLevel() == id) {
        fade_button_group_->setExclusive(false);
        fade_button_group_->checkedButton()->setChecked(false);
        fade_button_group_->setExclusive(true);
        cur_effect->setFadeLevel(0);
    }
    else
        cur_effect->setFadeLevel(id);
}

void Step3Widget::changeTransLevel0()
{
    setTransLevel(0);
}

void Step3Widget::changeTransLevel1()
{
    setTransLevel(1);
}

void Step3Widget::changeTransLevel2()
{
    setTransLevel(2);
}

void Step3Widget::setTransLevel(int id)
{
    if (!cur_effect) return;
    if (cur_effect->getTransLevel() == id) {
        trans_button_group_->setExclusive(false);
        trans_button_group_->checkedButton()->setChecked(false);
        trans_button_group_->setExclusive(true);
        cur_effect->setTransLevel(3);
    }
    else
        cur_effect->setTransLevel(id);
    //data_manager_->setTransLevel(id, trans_button_group_);
}

void Step3Widget::changeRenderOrder(bool backward)
{
    if (cur_effect)
        cur_effect->setOrder(backward);
    //data_manager_->setRenderOrder(backward);
}

void Step3Widget::changeMarkerOne(bool b)
{
    if (!cur_effect) return;
    if (b) {
        marker_all_button_->setChecked(false);
        cur_effect->setMarker(1);
    }
    else cur_effect->setMarker(0);
    //data_manager_->setMarkerOne(b, marker_all_button_);
}

void Step3Widget::changeMarkerAll(bool b)
{
    if (!cur_effect) return;
    if (b) {
        marker_one_button_->setChecked(false);
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

    int priority = control_edit_priority_->text().toInt();
    auto& effect_manager = MLDataManager::get().effect_manager_;
    effect_manager.popEffect(cur_effect);
    cur_effect->setPriority(priority);
    effect_manager.pushEffect(cur_effect);
}

void Step3Widget::changeSpeed()
{
    int speed = control_edit_speed_->text().toInt();
    if (!cur_effect) return;
    cur_effect->setSpeed(speed);
}

//void Step3Widget::changeCurrentEffect(GEffectPtr &efx)
//{
//    // TODO: update Effect widget
////    setCurrentPath(efx->path());
////    setCurrentEffect(efx);
////    updateEffectWidget();
////    update();
//}

void Step3Widget::updateEffectWidget()
{
    display_widget_->update();
    if (!cur_effect || cur_effect->type() == EFX_ID_TRASH || cur_effect->type() == EFX_ID_BLACK) {
        qDebug() << "Current effect not selected";
        for (int i = 0; i < 3; ++i)
            trans_level_button_[i]->setEnabled(false);
        for (int i = 0; i < 3; ++i)
            fade_level_button_[i]->setEnabled(false);
        control_edit_priority_->setEnabled(false);
        control_edit_speed_->setEnabled(false);
        marker_one_button_->setEnabled(false);
        marker_all_button_->setEnabled(false);
        path_line_button_->setEnabled(false);
        render_order_button_->setEnabled(false);

        smooth_slider_frame_->setEnabled(false);
        loop_slider_frame_->setEnabled(false);
        alpha_slider_frame_->setEnabled(false);
        return;
    }

    qDebug() << "Update Effect Widget";
    if (cur_effect->getAlpha() < 0)
        alpha_slider_frame_->setEnabled(false);
    else {
        alpha_slider_frame_->setEnabled(true);
        blur_alpha_slider_->setValue(cur_effect->getAlpha()
            * blur_alpha_slider_->maximum());
    }

    if (cur_effect->getSmooth() == -1)
        smooth_slider_frame_->setEnabled(false);
    else {
        smooth_slider_frame_->setEnabled(true);
        trail_smooth_slider_->setValue(cur_effect->getSmooth());
    }

    //    if (efx->getLoopCount() == -1)
    //        loop_slider_frame_->setEnabled(false);
    //    else {
    //        loop_slider_frame_->setEnabled(true);
    //        loop_slider_->setValue(efx->getLoopCount());
    //    }

    if (cur_effect->getTransLevel() == -1) {
        for (int i = 0; i < 3; ++i)
            trans_level_button_[i]->setEnabled(false);
    }
    else {
        for (int i = 0; i < 3; ++i)
            trans_level_button_[i]->setEnabled(true);
        //        qDebug() << currentEffect()->getTransLevel() << trans_button_group_->checkedButton();
        if (cur_effect->getTransLevel() != 3)
            trans_level_button_[cur_effect->getTransLevel()]->setChecked(true);
        else if (trans_button_group_->checkedButton()) {
            trans_button_group_->setExclusive(false);
            trans_button_group_->checkedButton()->setChecked(false);
            trans_button_group_->setExclusive(true);
        }
        //        qDebug() << currentEffect()->getTransLevel() << trans_button_group_->checkedButton();
    }

    if (cur_effect->getTransLevel() == -1) {
        for (int i = 0; i < 3; ++i)
            fade_level_button_[i]->setEnabled(false);
    }
    else {
        for (int i = 0; i < 3; ++i)
            fade_level_button_[i]->setEnabled(true);
        if (cur_effect->getFadeLevel() != 0)
            fade_level_button_[cur_effect->getFadeLevel() - 1]->setChecked(true);
        else if (fade_button_group_->checkedButton()) {
            fade_button_group_->setExclusive(false);
            fade_button_group_->checkedButton()->setChecked(false);
            fade_button_group_->setExclusive(true);
        }
    }

    render_order_button_->setEnabled(true);
    render_order_button_->setChecked(cur_effect->getOrder());

    if (cur_effect->getMarker() == -1) {
        marker_one_button_->setEnabled(false);
        marker_all_button_->setEnabled(false);
    }
    else {
        marker_one_button_->setEnabled(true);
        marker_one_button_->setChecked(cur_effect->getMarker() == 1);
        marker_all_button_->setEnabled(true);
        marker_all_button_->setChecked(cur_effect->getMarker() == 2);
    }

    if (cur_effect->getTrailLine() == -1) {
        path_line_button_->setEnabled(false);
    }
    else {
        path_line_button_->setEnabled(true);
        path_line_button_->setChecked(cur_effect->getTrailLine());
    }

    control_edit_priority_->setEnabled(true);
    control_edit_priority_->setText(
        QString::number(cur_effect->priority()));
    control_edit_speed_->setEnabled(true);
    control_edit_speed_->setText(
        QString::number(cur_effect->speed()));
}

bool Step3Widget::setA()
{
    const QRectF& rectf = display_widget_->curSelectRectF();
    if (rectf.isEmpty())
        return false;

    tracker_frame_A_ = cur_frameidx;
    tracker_rect_A_ = rectf;
    tracker_is_A_set_ = true;
    return true;
}

bool Step3Widget::setB()
{
    if (!tracker_is_A_set_ || tracker_frame_A_ >= cur_frameidx) return false;

    const QRectF& rectf = display_widget_->curSelectRectF();
    tracker_frame_B_ = cur_frameidx;
    tracker_rect_B_ = rectf;

    // track path
    if (cur_tracked_path) 
    {
        tracker->updateTrack(cur_tracked_path.get());
        cur_tracked_path->updateimages();
    }
    else 
    {
        GPath* path = tracker->trackPath(tracker_frame_A_, tracker_rect_A_, tracker_frame_B_, tracker_rect_B_);
        path->updateimages();
        cur_tracked_path = GPathTrackDataPtr(path);
    }
    return true;
}

void Step3Widget::toggleStill(bool checked)
{
    if (checked) 
    {
        display_widget_->clearCurrentSelection();
        display_widget_->setPathMode(true);
        result_widget_->setStillState(true);
        control_add_still_button_->setText("Apply");
        return;
    }

    QRectF select_rect;
    QPainterPath select_path;
    if (!display_widget_->curSelectRectF().isEmpty()) {
        select_rect = display_widget_->curSelectRectF();
        select_path = display_widget_->curSelectPath();
    }
    else if (!result_widget_->curSelectRectF().isEmpty()) {
        select_rect = result_widget_->curSelectRectF();
        select_path = result_widget_->curSelectPath();
    }
    else {
        control_add_still_button_->setChecked(true);
        return;
    }
    GPathTrackDataPtr path = GPathTrackDataPtr(new GPath(cur_frameidx, select_rect, select_path));
    GEffectPtr efx = createEffectFromPath(path, EFX_ID_STILL);

    result_widget_->update();
    // Create UI for effect
    qDebug() << "Add UI" << G_EFFECT_STR[efx->type()];
    timeline_widget_->addTimeline(efx);

    control_add_still_button_->setText("Still");
    display_widget_->setPathMode(false);
    result_widget_->setStillState(false);
    display_widget_->clearMouseSelection();
    updateEffectWidget();
}

void Step3Widget::toggleInpaint(bool checked)
{
    if (checked) {
        result_widget_->setInpaintState(true);
        control_add_inpaint_button_->setText("Apply");
        return;
    }

    QRectF select_rect;
    QPainterPath select_path;
    if (!result_widget_->curSelectRectF().isEmpty()) {
        select_rect = result_widget_->curSelectRectF();
        select_path = result_widget_->curSelectPath();
    }
    else {
        control_add_inpaint_button_->setChecked(true);
        return;
    }
    GPathTrackDataPtr path = GPathTrackDataPtr(new GPath(0, select_rect, select_path));
    GEffectPtr efx = createEffectFromPath(path, EFX_ID_TRASH);

    result_widget_->update();
    control_add_inpaint_button_->setText("Inpaint");
    result_widget_->setInpaintState(false);
    display_widget_->clearMouseSelection();
    updateEffectWidget();
}

void Step3Widget::toggleBlack(bool checked)
{
    qDebug() << "toggleBlack" << checked;
    if (checked) {
        result_widget_->setInpaintState(true);
        control_add_black_button_->setText("Apply");
        return;
    }
    
    QRectF select_rect;
    QPainterPath select_path;
    if (!result_widget_->curSelectRectF().isEmpty()) {
        select_rect = result_widget_->curSelectRectF();
        select_path = result_widget_->curSelectPath();
    }
    else {
        control_add_inpaint_button_->setChecked(true);
        return;
    }
    GPathTrackDataPtr path = GPathTrackDataPtr(new GPath(0, select_rect, select_path));
    GEffectPtr efx = createEffectFromPath(path, EFX_ID_BLACK);

    result_widget_->update();
    control_add_black_button_->setText("Black");
    result_widget_->setInpaintState(false);
    display_widget_->clearMouseSelection();
    updateEffectWidget();
}

void Step3Widget::toggleTrail(bool checked)
{
    if (checked) {
        if (!setA()) {
            control_add_trail_button_->setChecked(false);
            return;
        }

        display_widget_->clearCurrentSelection();
        control_add_trail_button_->setText("Add");
    }
    else {
        control_add_trail_button_->setText("Trail");

        if (!setB()) return;

        GEffectPtr efx = createEffectFromPath(cur_tracked_path, EFX_ID_TRAIL);
        // Create UI for effect
        qDebug() << "Add UI" << G_EFFECT_STR[efx->type()];
        timeline_widget_->addTimeline(efx);

        display_widget_->clearMouseSelection();
        result_widget_->update();
        updateEffectWidget();
    }
}

void Step3Widget::toggleMotion(bool checked)
{
    if (checked) {
        if (!setA()) {
            control_add_multiple_button_->setChecked(false);
            return;
        }

        control_add_multiple_button_->setText("Add");
        display_widget_->clearCurrentSelection();
    }
    else {
        control_add_multiple_button_->setText("Motion");
        if (!setB()) return;

        GEffectPtr efx = createEffectFromPath(cur_tracked_path, EFX_ID_MOTION);
        // Create UI for effect
        qDebug() << "Add UI" << G_EFFECT_STR[efx->type()];
        timeline_widget_->addTimeline(efx);

        result_widget_->update();
        display_widget_->clearMouseSelection();
        updateEffectWidget();
    }
}

void Step3Widget::toggleModify(bool checked)
{
    //    qDebug() << "ToggleModify" << checked << data_manager_->currentPath();
    if (checked) {
        if (!cur_tracked_path) {
            control_modify_path_button_->setChecked(false);
            return;
        }
        display_widget_->toggleModifyMode();
        control_modify_path_button_->setText("Apply");
    }
    else {
        control_modify_path_button_->setText("Modify");
        if (!cur_tracked_path) return;

        display_widget_->toggleModifyMode();
        display_widget_->update();
    }
}
