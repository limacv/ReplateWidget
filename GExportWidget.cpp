#include "GExportWidget.h"
#include <QTextEdit>
#include <QFileDialog>
#include <qdebug.h>

GImageMaskWidget::GImageMaskWidget(QWidget *parent)
    : QWidget(parent)
    , width_major_(true)
{
    QSize ori_size = data_manager_->videoOriginalSize();
    width_scale_ = ori_size.width() / (qreal) data_manager_->videoWidth();
    height_scale_ = ori_size.height() / (qreal) data_manager_->videoHeight();
}

QRect GImageMaskWidget::cropRect() const
{
    QSize ori = data_manager_->videoOriginalSize();
    QRect rect(QPoint(0,0), ori);
    rect.adjust(data_manager_->crop_left_, data_manager_->crop_top_,
                -data_manager_->crop_right_, -data_manager_->crop_bottom_);
    return rect;
}

//QRectF GImageMaskWidget::cropRectF() const
//{
//    QSize ori = data_manager_->videoOriginalSize();
//    QRectF rect(QPoint(0,0), ori);
//    rect.adjust(left_, top_, -right_, -bottom_);
//    QMatrix mat;
//    mat.scale(1.0/(qreal) ori.width(), 1.0/(qreal)ori.height());
//    return mat.mapRect(rect);
//}

void GImageMaskWidget::convertExport(const std::string &filename)
{
    qDebug("GExportWidget::not implemented");
//    int input_width = cropRect().width();
//    input_width -= input_width % 4;
//    int input_height = cropRect().height();
//    input_height -= input_height % 2;
//    int output_width = data_manager_->export_width_ - data_manager_->export_width_ % 4;
//    int output_height = data_manager_->export_height_ - data_manager_->export_height_ % 4;
//    boost::format cmd_format("avconv.exe -i %5% -vf crop=%1%:%2%:%3%:%4% -r 30 -s %6%x%7% -y %8%");
////    string outputfile = ;
//    cmd_format % input_width % input_height % data_manager_->crop_left_
//            % data_manager_->crop_top_ % data_manager_->result_temp_
//            % output_width % output_height % filename;
//    qDebug() << "Command for export:" << cmd_format.str().c_str();
//    system(cmd_format.str().c_str());
}

QSize GImageMaskWidget::sizeHint() const
{
    QSize video_size = data_manager_->videoSize();
    if (1280 < video_size.width() || 720 < video_size.height()) {
        return video_size.scaled(1280, 720, Qt::KeepAspectRatio);
    }
    else
        return video_size;
}

void GImageMaskWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
//    QImage img = GUtil::mat2qimage(data_manager_->video()->getImage(frame_id_));
//    painter.drawImage(painter.viewport(), img);
    data_manager_->paintResultDisplay(painter, frame_id_);

    // paint mask
    QImage mask(painter.viewport().size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::gray);
    QImage alpha(mask.size(), QImage::Format_Indexed8);
    alpha.fill(180);

    QSize ori_size = data_manager_->videoOriginalSize();
    QMatrix mat;
    mat.scale(this->width() / (qreal)ori_size.width(), this->height() / (qreal)ori_size.height());
    QRect rect = mat.mapRect(QRectF(cropRect())).toRect();

    fillRoiRect(alpha, rect, 0);
    mask.setAlphaChannel(alpha);
    painter.drawImage(painter.viewport(), mask);

//    qDebug() << "Paint Export" << painter.viewport() << this->width() / (qreal)ori_size.width()
//             << this->height() / (qreal)ori_size.height() << cropRect() << rect << data_manager_->videoOriginalSize();
}

void GImageMaskWidget::updateExportSize()
{
    QSize size = cropRect().size();
    qreal aspect = size.width() / (qreal)size.height();

    if (width_major_)
        data_manager_->export_height_ = qRound(data_manager_->export_width_ / aspect);
    else
        data_manager_->export_width_ = qRound(data_manager_->export_height_ * aspect);
}

void GImageMaskWidget::fillRoiRect(QImage &image, QRect rect, int alpha)
{
    for (int r = rect.top(); r < rect.bottom(); ++r)
    {
        uchar *row = (uchar*)image.scanLine(r);
        for (int c = rect.left(); c < rect.right(); ++c)
        {
            row[c] = alpha;
        }
    }
}


GExportWidget::GExportWidget(GDataManager *data_manager, QWidget *parent)
    :QWidget(parent)
    , data_manager_(data_manager)
{
    image_panel_ = createImageFrame(this);
    image_panel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    control_panel_ = createControlFrame(this);
    control_panel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(image_panel_);
    layout->addWidget(control_panel_);

    connect(frame_slider_, SIGNAL(valueChanged(int)), this, SLOT(setImage(int)));
    connect(loop_slider_, SIGNAL(valueChanged(int)), this, SLOT(setLoopCount(int)));

    connect(crop_left_, SIGNAL(editingFinished()), this, SLOT(changeLeft()));
    connect(crop_right_, SIGNAL(editingFinished()), this, SLOT(changeRight()));
    connect(crop_top_, SIGNAL(editingFinished()), this, SLOT(changeTop()));
    connect(crop_bottom_, SIGNAL(editingFinished()), this, SLOT(changeBottom()));

    connect(width_edit_, SIGNAL(editingFinished()), this, SLOT(widthChanged()));
    connect(height_edit_, SIGNAL(editingFinished()), this, SLOT(heightChanged()));

    connect(export_button_, SIGNAL(clicked()), this, SLOT(exportVideo()));
    connect(width_major_, SIGNAL(toggled(bool)), this, SLOT(changeWidthMajor(bool)));

    setImage(0);
    changeLeft();
}

QSize GExportWidget::sizeHint() const
{
//    qDebug() << "GExportWidget Size:" << data_manager_->videoSize();
    return data_manager_->videoSize();
}

QFrame *GExportWidget::createImageFrame(QWidget *parent)
{
    QFrame *res = new QFrame(parent);
    image_widget_ = new GImageMaskWidget(data_manager_, res);
    image_widget_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    QGridLayout *layout = new QGridLayout(res);
    layout->setMargin(5);
    layout->addWidget(image_widget_);
    return res;
}

QFrame *GExportWidget::createControlFrame(QWidget *parent)
{
    QFrame *res = new QFrame(parent);

    int duration = data_manager_->duration();
    if (duration == 0) duration = data_manager_->videoLength();
    frame_slider_ = new QSlider(Qt::Horizontal, res);
    frame_slider_->setRange(0, duration - 1);
    QLabel *label_frame = new QLabel("Frame", res);
    label_frame->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    frame_counter_ = new QLabel("0", res);
    frame_counter_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    int max_loop_cnt = std::ceil(data_manager_->videoLength() / (qreal)duration);
    loop_slider_ = new QSlider(Qt::Horizontal, res);
    loop_slider_->setRange(1, max_loop_cnt);
    loop_slider_->setFixedWidth(50);
    loop_slider_->setValue(data_manager_->export_loop_count_);
    QLabel *label_loop = new QLabel("Loop", res);
    label_loop->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    loop_counter_ = new QLabel("1", res);
    loop_counter_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    export_button_ = new QPushButton("Export", res);
    export_button_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    QFrame *crop_frame = createCropFrame(res);

    QFrame *size_frame = createSizeFrame(res);

    QHBoxLayout *frame_layout = new QHBoxLayout;
    frame_layout->addWidget(label_frame);
    frame_layout->addWidget(frame_counter_);
    frame_layout->addWidget(frame_slider_);

    QHBoxLayout *secondline_layout = new QHBoxLayout;
    secondline_layout->setAlignment(Qt::AlignLeft);
    secondline_layout->addWidget(crop_frame);
    secondline_layout->addWidget(label_loop);
    secondline_layout->addWidget(loop_counter_);
    secondline_layout->addWidget(loop_slider_);
    secondline_layout->addWidget(size_frame);
    secondline_layout->addWidget(export_button_);

    QVBoxLayout *layout = new QVBoxLayout(res);
    layout->addLayout(frame_layout);
    layout->addLayout(secondline_layout);

    return res;
}

QFrame *GExportWidget::createCropFrame(QWidget *parent)
{
    QFrame *res = new QFrame(parent);
    res->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    QHBoxLayout *layout = new QHBoxLayout(res);
    layout->setMargin(0);
    layout->addWidget(createTextFrame(crop_left_, "Left", res));
    crop_left_->setText(QString::number(data_manager_->crop_left_));
    layout->addWidget(createTextFrame(crop_right_, "Right", res));
    crop_right_->setText(QString::number(data_manager_->crop_right_));
    layout->addWidget(createTextFrame(crop_top_, "Top", res));
    crop_top_->setText(QString::number(data_manager_->crop_top_));
    layout->addWidget(createTextFrame(crop_bottom_, "Bottom", res));
    crop_bottom_->setText(QString::number(data_manager_->crop_bottom_));

    return res;
}

QFrame *GExportWidget::createTextFrame(QLineEdit *&edit, QString text,
                                       QWidget *parent)
{
    QFrame *res = new QFrame(parent);

    QLabel* label = new QLabel(text, res);
    edit = new QLineEdit(res);
    edit->setFixedWidth(30);
    edit->setValidator(new QIntValidator(0, 1000, parent));
    edit->setText("0");

    QHBoxLayout *layout = new QHBoxLayout(res);
    layout->addWidget(label);
    layout->addWidget(edit);

    return res;
}

QFrame *GExportWidget::createSizeFrame(QWidget *parent)
{
    QFrame *res = new QFrame(parent);

    width_major_ = new QCheckBox("Width", res);
    width_major_->setChecked(true);

    QSize ori_size = data_manager_->videoOriginalSize();

    width_edit_ = new QLineEdit(res);
    width_edit_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    width_edit_->setValidator(new QIntValidator(0, ori_size.width(), parent));
    width_edit_->setText(QString::number(data_manager_->export_width_));

    height_edit_ = new QLineEdit(res);
    height_edit_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    height_edit_->setValidator(new QIntValidator(0, ori_size.height(), parent));
    height_edit_->setText(QString::number(data_manager_->export_height_));

    QLabel *label = new QLabel("x", res);

    QHBoxLayout *layout = new QHBoxLayout(res);
    layout->addWidget(width_major_);
    layout->addWidget(width_edit_);
    layout->addWidget(label);
    layout->addWidget(height_edit_);

    return res;
}

void GExportWidget::changeLeft()
{
    int value = crop_left_->text().toInt();
    image_widget_->setLeft(value);
    updateSizeWidget();
}

void GExportWidget::changeRight()
{
    int value = crop_right_->text().toInt();
    image_widget_->setRight(value);
    updateSizeWidget();
}

void GExportWidget::changeTop()
{
    int value = crop_top_->text().toInt();
    image_widget_->setTop(value);
    updateSizeWidget();
}

void GExportWidget::changeBottom()
{
    int value = crop_bottom_->text().toInt();
    image_widget_->setBottom(value);
    updateSizeWidget();
}

void GExportWidget::setImage(int frame)
{
//    image_widget_->setImage(GUtil::mat2qimage(data_manager_->video()->getImage(frame)));
    image_widget_->setFrameId(frame);
    frame_counter_->setText(QString::number(frame));
    update();
}

void GExportWidget::setLoopCount(int cnt)
{
    data_manager_->export_loop_count_ = cnt;
    loop_counter_->setText(QString::number(cnt));
}

void GExportWidget::exportVideo()
{
    std::string output_folder = MLConfigManager::get().get_raw_video_path().toStdString();
    std::string output_file = QFileDialog::getSaveFileName(
                this, tr("Save File"), output_folder.c_str()).toStdString();

    data_manager_->exportImageSequence(data_manager_->export_loop_count_);
    image_widget_->convertExport(output_file);
}

void GExportWidget::updateSizeWidget()
{
    height_edit_->setText(QString::number(data_manager_->export_height_));
    width_edit_->setText(QString::number(data_manager_->export_width_));
    update();
}

void GExportWidget::changeWidthMajor(bool checked)
{
    image_widget_->width_major_ = checked;
}

void GExportWidget::widthChanged()
{
    data_manager_->export_width_ = width_edit_->text().toInt();
    image_widget_->updateExportSize();
    updateSizeWidget();
}

void GExportWidget::heightChanged()
{
    data_manager_->export_height_ = height_edit_->text().toInt();
    image_widget_->updateExportSize();
    updateSizeWidget();
}
