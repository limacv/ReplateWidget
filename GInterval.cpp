#include <QtGui>

#include "GInterval.h"
#include "GEffect.h"
//#include "GDataManager.h"
#include "MLDataManager.h"

QColor GInterval::interval_colorset_[10] = {QColor()};
QColor GInterval::interval_gradcolorset_[10][2] = {{QColor(), QColor()}};

GInterval::GInterval(GEffectPtr &efx, double step_length, QWidget *parent)
    :QWidget(parent)
    ,m_selectLeftStart(false)
    ,m_selectRightStart(false)
    ,m_moveStart(false)
    ,m_highlightLeft(false)
    ,m_highlightRight(false)
    ,step_length_(step_length)
    ,resetGradient(false)
    ,effect_(efx)
{
    duration_used = MLDataManager::get().plates_cache.replate_duration / effect()->speed();
    if (duration_used <= 0) duration_used = 1;
    presetColor();
    presetGradient(duration_used, step_length);
    setColor(interval_colorset_[efx->type()]);

    this->setMouseTracking(true);
    m_highlightPen = QPen(Qt::green, 6, Qt::SolidLine);
    m_keypointPen = QPen(Qt::darkGray, 6, Qt::SolidLine);
    m_haloPen = QPen(Qt::darkGreen, 6, Qt::SolidLine);

//    this->setStyleSheet("GeInterval{border-width: 1px; border-color: slategray; border-style: solid;}"
//                  "GeInterval:hover{border-width: 3px; border-color: black; border-style: solid;}");
//    this->setObjectName("GeInterval");
//    this->setStyleSheet("QWidget#GeInterval{background: black;}");
//    this->setAutoFillBackground(true);
}

GInterval::~GInterval() { release();}

void GInterval::release() {
    interval_gradset_.clear();
//    releaseEffect(effect());
    releaseEffect(effect_);
}

QColor GInterval::color() const
{
    return m_color;
}

QString GInterval::toolTip() const
{
    return m_toolTip;
}

//void GInterval::addKeyPoint(int x)
//{
//    if (effect()->type() == EFX_ID_MULTIPLE) {
//        G_EFX_Multiple *efx = dynamic_cast<G_EFX_Multiple*>(effect_.get());
//        x -= efx->effectStartFrame();
//        if (efx->insertKeypoint(x))
//            qDebug() << "Keypoint added" << x;
//    }
//}

//void GInterval::deleteKeyPoint(int x)
//{
//    if (effect()->type() == EFX_ID_MULTIPLE) {
//        G_EFX_Multiple *efx = dynamic_cast<G_EFX_Multiple*>(effect_.get());
//        x -= efx->effectStartFrame();
//        efx->eraseKeypoint(x);
//    }
//}

//void GInterval::addOpaque(int x)
//{
//    if (effect()->type() == EFX_ID_MULTIPLE) {
//        G_EFX_Multiple *efx = dynamic_cast<G_EFX_Multiple*>(effect_.get());
//        x -= efx->effectStartFrame();
//        if (efx->insertOpaque(x / anchor_length_used))
//            qDebug() << "Opaque added" << x;
//    }
//}

//void GInterval::deleteOpaque(int x)
//{
//    if (effect()->type() == EFX_ID_MULTIPLE) {
//        G_EFX_Multiple *efx = dynamic_cast<G_EFX_Multiple*>(effect_.get());
//        x -= efx->effectStartFrame();
//        efx->eraseOpaque(x / anchor_length_used);
//    }
//}

void GInterval::toggleShown(bool checked)
{
    effect()->toggleShown(checked);
}

void GInterval::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if(isSide(event->pos().x(), &m_selectLeftStart, &m_selectRightStart))
            m_selectStartPos = event->pos().x();
        else {
            m_moveStartPos = event->pos().x();
            m_moveStart = true;
        }
    }
//    else if (event->button() == Qt::RightButton) {
//        qDebug() << "Right click!" << event->pos() << this->pos();
//    }
}

void GInterval::mouseMoveEvent(QMouseEvent *event)
{
    //qDebug() << "Move " << event->pos();

    adjustArea(event->pos().x());
    if (!isSide(event->pos().x(), &m_highlightLeft, &m_highlightRight))
        moveArea(event->pos().x());
    update();
}

void GInterval::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    //adjustArea(event->pos().x() - m_selectStartPos);
    m_selectLeftStart = m_selectRightStart = false;
    m_moveStart = false;
}

void GInterval::paintEvent(QPaintEvent * /*event*/)
{
//    qDebug() << "Paint!!" << resetGradient;
    QPainter painter(this);
    QBrush bg(m_color);
    painter.setBrush(bg);
    painter.setPen(Qt::black);
    if (effect()->type() == EFX_ID_STILL)
        painter.setPen(Qt::NoPen);

    const auto duration = MLDataManager::get().plates_cache.replate_duration;
//    qDebug() << "Anchor_length" << GUtil::getAnchor_length() << anchor_length_used;
    if (resetGradient || 
        (duration > 0 && duration / effect()->speed() != duration_used)) 
    {
        duration_used = duration / effect()->speed();
        if (duration_used <= 0) duration_used = effect()->effectLength() - 1;
        presetGradient(duration_used, step_length_);
        resetGradient = false;
        qDebug() << "Reset Gradient" << interval_gradset_.size();
    }

//    qDebug() << "After Anchor_length" << GUtil::getAnchor_length() << anchor_length_used;
    // draw segments
//    qDebug() << effect()->type() << EFX_ID_MOTION << EFX_ID_MULTIPLE;
    if (effect()->type() == EFX_ID_MOTION || effect()->type() == EFX_ID_LOOP) {
//        GEffectMotion* efx = dynamic_cast<GEffectMotion*>(effect_.get());
        int num = std::ceil(effect_->effectLength() / (float)duration_used);
        int seg_length = step_length_ * duration_used;
        QPoint align  = QPoint(seg_length, 0);
        QPoint tl = this->rect().topLeft() + QPoint(1,1);
        QPoint br = QPoint(tl.x() + step_length_ * duration_used - IN_MARGIN,
                           this->height() - 2);
        QRect rect(tl, br);
        QPoint leftcenter = rect.center() - QPoint(rect.width() / 2, 0);
        QPainterPath triangle;
        triangle.moveTo(leftcenter);
        triangle.lineTo(rect.topRight());
        triangle.lineTo(rect.bottomRight());
        triangle.lineTo(leftcenter);
        for (int i = 0; i < num; ++i) {
//            if (efx->hasOpaque(i)) {
//                painter.setBrush(interval_colorset_[EFX_ID_MOTION]);
//                painter.setPen(Qt::blue);
//            }
//            else
            {
                painter.setBrush(interval_gradset_[i]);
                painter.setPen(QPen(Qt::blue, 1, Qt::SolidLine));
            }
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawPath(triangle);
            triangle.translate(align);
//            painter.drawRect(QRect(tl, br));
//            tl += align;
//            br += align;
        }
    }
    else
        painter.drawRect(0,0,width()-1,height()-1);

    if (m_highlightLeft)
    {
        painter.setPen(m_highlightPen);
        painter.drawLine(0,0,0,height());
    }
    if (m_highlightRight)
    {
        painter.setPen(m_highlightPen);
        painter.drawLine(width()-1,0,width()-1,height());
    }
}

bool GInterval::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        QToolTip::showText(helpEvent->globalPos(), toolTip());
        return true;
    }
    return QWidget::event(event);
}

//void GeInterval::enterEvent(QEvent *)
//{
//    m_entered = true;
//    qDebug() << "Enter";
//    update();
//}

void GInterval::leaveEvent(QEvent *)
{
    m_highlightLeft = m_highlightRight = false;
    update();
}

bool GInterval::isSide(int x, bool *left, bool *right)
{
    bool l = false;
    bool r = false;
    if ( abs(x) < 4 )
        l = true;
    if ( abs(x - this->size().width()) < 4 )
        r = true;
    if (left)
        *left = l;
    if (right)
        *right = r;
    return (l||r);
}

void GInterval::adjustArea(int pos)
{
    bool forward = (pos > m_selectStartPos);
//    int off = pos;
//    if (forward)
//        off += step_length_ / 2.0;
//    else
//        off -= step_length_ / 2.0;
//    off = std::floor(off / step_length_) * step_length_;

    int abs_off = abs(pos - m_selectStartPos) / step_length_ + 0.5;
    int off = forward ? abs_off: -abs_off;

    if (m_selectLeftStart && off != 0)
        adjustLeft(off);
    else if (m_selectRightStart && off != 0)
        adjustRight(off);
}

void GInterval::moveArea(int pos)
{
    bool forward = (pos > m_moveStartPos);
    int abs_off = abs(pos - m_moveStartPos) / step_length_ + 0.5;
    int off = forward ? abs_off: -abs_off;

    if (m_moveStart && off != 0) {
        moveStart(off);
        //int off = pos - m_moveStartPos;
//        qDebug() << "Move " << off;
//        QPoint p =  this->pos();
//        p.setX(p.x()+off);
//        this->move(p);
    }
}

void GInterval::adjustOffset(int pos, int duration)
{
    if (pos == 0) return;
    effect()->adjustAsync(pos, duration);
}

int GInterval::offset() const
{
    return effect()->async();
}

void GInterval::presetColor()
{
//    interval_colorset_[EFX_ID_MULTIPLE] = QColor(77, 166, 146);
    interval_colorset_[EFX_ID_MOTION] = QColor(0, 166, 146, 255);
    interval_colorset_[EFX_ID_MULTIPLE] = QColor(121, 179, 180, 255);
    interval_colorset_[EFX_ID_TRAIL] = QColor(105, 131, 143, 255);
    interval_colorset_[EFX_ID_STILL] = Qt::yellow;
    interval_colorset_[EFX_ID_LOOP] = QColor(154, 130, 180, 255);
    interval_colorset_[EFX_ID_INPAINT] = Qt::gray;

    for (int i = 0; i < 10; ++i) {
        interval_gradcolorset_[i][0] = interval_gradcolorset_[i][1] = interval_colorset_[i];
        interval_gradcolorset_[i][0] = Qt::black; // TODO: clean all color set and gradient
        interval_gradcolorset_[i][0].setAlpha(128);
        interval_gradcolorset_[i][1].setAlpha(255);
    }
}

void GInterval::presetGradient(int anchor_length, double frame_steps)
{
    int num = std::ceil(effect_->effectLength() / (float)anchor_length);
    interval_gradset_.resize(num);
    int length = frame_steps * anchor_length - IN_MARGIN;
    QPoint align  = QPoint(length, 0);
    QPoint tl = this->rect().topLeft();
    QPoint br = QPoint(tl.x() + length, this->height());
    for (int i = 0; i < num; ++i) {
//        qDebug() << "grad" << i << tl << br;
        interval_gradset_[i] = QLinearGradient(tl, br);
        interval_gradset_[i].setColorAt(0, interval_gradcolorset_[effect()->type()][0]);
        interval_gradset_[i].setColorAt(1, interval_gradcolorset_[effect()->type()][1]);
        tl += align;
        br += align;
    }
//    qDebug() << "B" << interval_gradset_.size();
}

void GInterval::adjustLeft(int step)
{
    if (effect_->adjustLeft(step)) {
        qDebug() << "Adjust left boundary" << step;

        int x = effect_->startFrame() * stepLength();
        int w = (effect_->effectLength()-1) * stepLength();
        this->setGeometry(x, this->y(), w, this->height());
        resetGradient = true;
    }
}

void GInterval::adjustRight(int step)
{
    if (effect_->adjustRight(step)) {
        qDebug() << "Adjust right boundary" << step;
        int x = effect_->startFrame() * stepLength();
        int w = (effect_->effectLength()-1) * stepLength();
        this->setGeometry(x, this->y(), w, this->height());
        m_selectStartPos = this->rect().right();
        resetGradient = true;
    }
}

void GInterval::moveStart(int step)
{
    if (effect_->moveStart(step)) {
        qDebug() << "Move area" << step;
        int x = effect_->startFrame() * stepLength();
        int w = (effect_->effectLength()-1) * stepLength();
        this->setGeometry(x, this->y(), w, this->height());
        resetGradient = true;
    }
}

double GInterval::stepLength() const
{
    return step_length_;
}
GEffectPtr GInterval::effect() const
{
    return effect_;
}


//void GeInterval::setRect(const QRect &rect)
//{
//    m_rect = rect;
//}

void GInterval::setColor(const QColor &color)
{
    m_color = color;
}

