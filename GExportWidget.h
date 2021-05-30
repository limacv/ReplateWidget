#ifndef GEXPORTWIDGET_H
#define GEXPORTWIDGET_H

#include <QWidget>
#include <QSlider>
#include <QFrame>
#include <QBoxLayout>
#include <QLineEdit>
#include <QValidator>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include "MLConfigManager.h"

class GImageMaskWidget : public QWidget
{
    Q_OBJECT
public:
    GImageMaskWidget(QWidget *parent);

    void setFrameId(int id) {
        frame_id_ = id;
        update();
    }

    void setLeft(int left) {data_manager_->crop_left_ = left; updateExportSize(); update();}
    void setRight(int right) {data_manager_->crop_right_ = right; updateExportSize(); update();}
    void setTop(int top) {data_manager_->crop_top_ = top; updateExportSize(); update();}
    void setBottom(int bottom) {data_manager_->crop_bottom_ = bottom; updateExportSize(); update();}

    void updateExportSize();
    void convertExport(const std::string &filename);

    bool width_major_;

protected:
    virtual QSize sizeHint() const;
    virtual void paintEvent(QPaintEvent *event);

    QRect cropRect() const;
//    QRectF cropRectF() const;
private:
    void fillRoiRect(QImage &image, QRect rect, int alpha);

    int frame_id_;
    qreal width_scale_, height_scale_;
};

class GExportWidget : public QWidget
{
    Q_OBJECT

public:
    GExportWidget(QWidget *parent);

protected:
    virtual QSize sizeHint() const;
//    virtual void paintEvent(QPaintEvent *event);

private:
    QFrame *createImageFrame(QWidget *parent);
    QFrame *createControlFrame(QWidget *parent);
    QFrame *createCropFrame(QWidget *parent);
    QFrame *createTextFrame(QLineEdit *&edit, QString text, QWidget *parent);

    QFrame *createSizeFrame(QWidget *parent);

protected slots:
    void changeLeft();
    void changeRight();
    void changeTop();
    void changeBottom();
    void setImage(int frame);
    void setLoopCount(int cnt);
    void exportVideo();

    void updateSizeWidget();

    void changeWidthMajor(bool checked);

    void widthChanged();
    void heightChanged();
protected:
    QFrame *image_panel_;
    QFrame *control_panel_;

    GImageMaskWidget *image_widget_;

    QSlider *frame_slider_;
    QLabel *frame_counter_;
    QSlider *loop_slider_;
    QLabel *loop_counter_;

    QLineEdit *crop_left_;
    QLineEdit *crop_right_;
    QLineEdit *crop_top_;
    QLineEdit *crop_bottom_;

    QPushButton *export_button_;

    QCheckBox *width_major_;
    QLineEdit *width_edit_;
    QLineEdit *height_edit_;
};

#endif // GEXPORTWIDGET_H
