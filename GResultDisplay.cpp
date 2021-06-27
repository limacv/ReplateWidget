#include "GResultDisplay.h"
#include "MLDataManager.h"
#include <qdialog.h>
#include "ui_GResultWidget.h"
#include "MLUtil.h"
#include "Step3Widget.h"

GResultWidget::GResultWidget(QWidget* parent, Step3Widget* step3widget)
    :QWidget(parent),
    step3widget(step3widget),
    current_frame_id_(0),
    duration(&MLDataManager::get().plates_cache.replate_duration),
    ui(new Ui::GResultWidget()),
    display_widget(nullptr)
{
    ui->setupUi(this);
    display_widget = ui->widget;
    
    player_manager = new MLPlayerHandler(
        [&]()->int {return (*duration) * 5; },
        ui->slider,
        display_widget,
        &timer,
        ui->button
    );
    display_widget->setparent(this);
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
    player_manager->play();
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

    if (!rect_select_.isEmpty() || !path_select_.isEmpty())
    {
        parent_widget->step3widget->setCurrentEffect(nullptr);
    }

    QPainter painter(this);
    global_data.paintReplateFrame(painter, parent_widget->player_manager->display_frameidx());

    paintMouseSelection(painter);
}
