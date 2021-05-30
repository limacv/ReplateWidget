#ifndef GEPICTURELABEL_H
#define GEPICTURELABEL_H

#include <QLabel>
#include <QImage.h>
#include <QPixmap.h>

class GPictureLabel : public QLabel
{
private:
    QPixmap _qpSource; //preserve the original, so multiple resize events won't break the quality
    QPixmap _qpCurrent;

    void _displayImage();

public:
    GPictureLabel(QWidget *aParent) : QLabel(aParent) { }
    void setPixmap(QPixmap aPicture);
    void paintEvent(QPaintEvent *aEvent);

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;
};

#endif // GEPICTURELABEL_H
