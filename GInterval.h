#ifndef GEINTERVAL_H
#define GEINTERVAL_H

#include <QPainter>
#include <QDebug>
#include <QToolTip>
#include "GEffect.h"

class GInterval: public QWidget
{
    Q_OBJECT

signals:
    void changeCurrentEffect(GEffectPtr &efx);
    void releaseEffect(GEffectPtr &efx);

public:
    GInterval(QWidget *parent = 0) : QWidget(parent){}
    GInterval(GEffectPtr &efx, double step_length,
              QWidget *parent = 0);

    virtual ~GInterval();
    void release();

    void setToolTip(const QString &toolTip);
    void setColor(const QColor &color);

    QColor color() const;
    QString toolTip() const;

//    void addKeyPoint(int x);
//    void deleteKeyPoint(int x);

//    void addOpaque(int x);
//    void deleteOpaque(int x);

    void adjustOffset(int pos, int duration);
    int offset() const;

    void toggleShown(bool checked);

    friend YAML::Emitter& operator <<(YAML::Emitter& out, const GInterval* interval);
//    friend const YAML::Node& operator >> (const YAML::Node& node, GInterval* interval);

    GEffectPtr effect() const;

protected:
    virtual void paintEvent(QPaintEvent * /*event*/);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent * /*event*/);
    virtual bool event(QEvent *event);
    virtual void leaveEvent(QEvent *);

    bool isSide(int x, bool *left = 0, bool *right = 0);
    void adjustArea(int pos);
    void moveArea(int pos);

private:
    static void presetColor();
    static QColor interval_colorset_[10];
    static QColor interval_gradcolorset_[10][2];
    void presetGradient(int anchor_length, double frame_steps);
    std::vector<QLinearGradient> interval_gradset_;
    void adjustLeft(int step);
    void adjustRight(int step);
    void moveStart(int step);
    double stepLength() const;

private:
    //QRect m_rect;
    double step_length_;
    QColor m_color;
    QString m_toolTip;
    int duration_used;
    bool resetGradient;
    //IntervalType m_type;
    //std::set<int> keypoints;
    bool m_selectLeftStart;
    bool m_selectRightStart;
    bool m_highlightLeft;
    bool m_highlightRight;
    int m_selectStartPos;

    bool m_moveStart;
    int m_moveStartPos;

    QPen m_highlightPen;
    QPen m_keypointPen;
    QPen m_haloPen;

    GEffectPtr effect_;

    static const int IN_MARGIN = 5;
};

#endif // GEINTERVAL_H
