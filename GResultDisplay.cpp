#include "GResultDisplay.h"
#include "MLDataManager.h"
#include <qdialog.h>
#include "ui_GResultWidget.h"
#include "MLUtil.h"

GResultWidget::GResultWidget(QWidget* parent)
    :QWidget(parent),
    current_frame_id_(0),
    duration(&MLDataManager::get().plates_cache.replate_duration),
    ui(new Ui::GResultWidget()),
    display_widget(nullptr)
{
    ui->setupUi(this);
    display_widget = ui->widget;
    ui->button->setFlat(true);
    ui->button->setIcon(MLUtil::getIcon(MLUtil::ICON_ID::STOP));
    ui->widget->setparent(this);

    timer.setInterval(33);
    timer.setSingleShot(false);

    connect(&timer, &QTimer::timeout, this, &GResultWidget::updateFrame);
    connect(ui->button, &QPushButton::clicked, this, [this](bool checked) {
        if (checked)
        {
            ui->button->setIcon(MLUtil::getIcon(MLUtil::ICON_ID::STOP));
            play();
        }
        else
        {
            ui->button->setIcon(MLUtil::getIcon(MLUtil::ICON_ID::PLAY));
            stop();
        }
        });
    connect(ui->slider, &QSlider::valueChanged, this, [this](int value) { current_frame_id_ = value; update(); });
}
    
void GResultWidget::setPathSelectModel(bool b) { ui->widget->setPathSelectionMode(b); }

void GResultWidget::clearMouseSelection() { ui->widget->clearMouseSelection(); }

void GResultWidget::setInpaintState(bool b)
{
    ui->widget->clearMouseSelection();
    ui->widget->setPathSelectionMode(b);
}

void GResultWidget::play()
{
    timer.start();
}

void GResultWidget::stop()
{
    timer.stop();
}

void GResultWidget::updateFrame()
{
    if (*duration > 0)
    {
        current_frame_id_ = (current_frame_id_++) % (*duration * 5);
        ui->slider->setRange(0, (*duration) * 5 - 1);
        ui->slider->setValue(current_frame_id_);
        update();
    }
}

void GResultWidget::editFrameRate()
{
    QDialog *res = new QDialog(this);
    res->setWindowTitle("Frame Rate");

    frame_rate_edit_ = new QLineEdit;
    frame_rate_edit_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    frame_rate_edit_->setValidator(new QIntValidator(1, 100, res));
    frame_rate_edit_->setText(QString::number(timer.interval()));

    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

    QHBoxLayout *layout = new QHBoxLayout(res);
    layout->addWidget(frame_rate_edit_);
    layout->addWidget(button_box);

    connect(frame_rate_edit_, &QLineEdit::editingFinished, this, &GResultWidget::changeFrameRate);
    connect(button_box, &QDialogButtonBox::accepted, res, &QDialog::accept);

    res->exec();
}

void GResultWidget::changeFrameRate()
{
    int rate = frame_rate_edit_->text().toInt();
    timer.setInterval(rate);
}

GResultDisplay::GResultDisplay(QWidget *parent)
    : GBaseWidget(parent)
{
}

QSize GResultDisplay::sizeHint() const
{
    //const auto& size = MLDataManager::get().raw_video_cfg.size;
    //return QSize(size.width, size.height);
    return QWidget::sizeHint();
}

void GResultDisplay::paintEvent(QPaintEvent *event)
{
    if (NULL == event) return;
    const auto& global_data = MLDataManager::get();
    QPainter painter(this);
    global_data.paintReplateFrame(painter, parent_widget->current_frame_id_);

    paintMouseSelection(painter);
}
