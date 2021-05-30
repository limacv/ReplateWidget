#include "GPictureLabel.h"
#include <QPainter.h>
#include <QDebug>

void GPictureLabel::paintEvent(QPaintEvent *aEvent)
{
    QLabel::paintEvent(aEvent);
    _displayImage();
}

QSize GPictureLabel::sizeHint() const
{
    return QSize(50, 50);
}

QSize GPictureLabel::minimumSizeHint() const
{
    return QSize(50, 50);
}

void GPictureLabel::setPixmap(QPixmap aPicture)
{
    _qpSource = _qpCurrent = aPicture;
    repaint();
}

void GPictureLabel::_displayImage()
{
    if (_qpSource.isNull()) //no image was set, don't draw anything
        return;

    //qDebug() << "W: " << width() << "  H: " << height();
    float cw = width(), ch = height();
    float pw = _qpCurrent.width(), ph = _qpCurrent.height();

    if (pw > cw && ph > ch && pw/cw > ph/ch || //both width and high are bigger, ratio at high is bigger or
        pw > cw && ph <= ch || //only the width is bigger or
        pw < cw && ph < ch && cw/pw < ch/ph //both width and height is smaller, ratio at width is smaller
        )
        _qpCurrent = _qpSource.scaledToWidth(cw, Qt::TransformationMode::FastTransformation);
    else if (pw > cw && ph > ch && pw/cw <= ph/ch || //both width and high are bigger, ratio at width is bigger or
        ph > ch && pw <= cw || //only the height is bigger or
        pw < cw && ph < ch && cw/pw > ch/ph //both width and height is smaller, ratio at height is smaller
        )
        _qpCurrent = _qpSource.scaledToHeight(ch, Qt::TransformationMode::FastTransformation);

    int x = (cw - _qpCurrent.width())/2, y = (ch - _qpCurrent.height())/2;

    QPainter paint(this);
    paint.drawPixmap(x, y, _qpCurrent);
}
