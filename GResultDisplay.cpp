#include "GResultDisplay.h"
#include "MLDataManager.h"
#include <qdialog.h>

GResultDisplay::GResultDisplay(QWidget *parent)
    : GBaseWidget(parent)
{
    video_player_ = new GVideoPlayer(this);
    const int framecount = MLDataManager::get().get_framecount();
    video_player_->initialize(0, framecount, 30);
    connect(video_player_, SIGNAL(playFrame(int)), this, SLOT(updateFrame(int)));
}

void GResultDisplay::setStillState(bool b)
{
    setPathMode(b);
//    show_mirror = b;
}

void GResultDisplay::setInpaintState(bool b)
{
    clearMouseSelection();
    setPathMode(b);
}

void GResultDisplay::clearCurrentSelection()
{
    clearMouseSelection();
}

void GResultDisplay::play()
{
    video_player_->Play();
}

void GResultDisplay::stop()
{
    video_player_->Stop();
}

void GResultDisplay::updateFrame(int id)
{
//    int duration = data_manager_->duration();
    ++current_frame_id_;
//    if (duration != 0)
//        current_frame_id_ %= duration;
    update();
}

void GResultDisplay::editFrameRate()
{
    QDialog *res = new QDialog(this);
    res->setWindowTitle("Frame Rate");

    frame_rate_edit_ = new QLineEdit;
    frame_rate_edit_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    frame_rate_edit_->setValidator(new QIntValidator(1, 100, res));
    frame_rate_edit_->setText(QString::number(video_player_->frameRate()));

    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok);

    QHBoxLayout *layout = new QHBoxLayout(res);
    layout->addWidget(frame_rate_edit_);
    layout->addWidget(button_box);

    connect(frame_rate_edit_, SIGNAL(editingFinished()), this, SLOT(changeFrameRate()));
    connect(button_box, SIGNAL(accepted()), res, SLOT(accept()));

    res->exec();
}

void GResultDisplay::changeFrameRate()
{
    int rate = frame_rate_edit_->text().toInt();
    video_player_->setFrameRate(rate);
}

QSize GResultDisplay::sizeHint() const
{
    const auto& size = MLDataManager::get().raw_video_cfg.size;
    return QSize(size.width, size.height);
}

void GResultDisplay::paintEvent(QPaintEvent *event)
{
    if (NULL == event) return;
    const auto& global_data = MLDataManager::get();
    QPainter painter(this);
    painter.drawImage(painter.viewport(), global_data.getBackgroundQImg());
    global_data.effect_manager_.drawEffects(painter, current_frame_id_);
    //dataManager()->paintResultDisplay(painter, current_frame_id_);
    paintMouseSelection(painter);
}
