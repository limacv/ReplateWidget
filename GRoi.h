#ifndef GROI_H
#define GROI_H

#include <QWidget>
#include <QPainterPath>

#ifndef Q_MOC_RUN
#include <memory>
//#include <boost/smart_ptr.hpp>
#endif // Q_MOC_RUN

class GRoi {
public:
    GRoi(){}
    GRoi(const QRectF &rectF): rectF_(rectF) {}
    GRoi(const QPainterPath &path): painter_path_(path) {}

    bool isRect() const;
    QRectF rectF() const;

    QRectF rectF_;
    QPainterPath painter_path_;

};

typedef std::shared_ptr<GRoi> GRoiPtr;

#endif // GROI_H
