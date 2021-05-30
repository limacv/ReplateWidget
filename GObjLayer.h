#ifndef GEOBJLAYER_H
#define GEOBJLAYER_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QSizePolicy>
#include <QButtonGroup>
#include <QBitmap>
#include <string>
#include <QCheckBox>
#include "GPictureLabel.h"
#include "GTimelineEx.h"

class GObjLayer : public QWidget
{
    Q_OBJECT

public:
    explicit GObjLayer(QWidget *parent = 0);
    explicit GObjLayer(GEffectPtr &effect, const std::string &name = "", QWidget *parent = 0);

    ~GObjLayer();
    void destroy();

    //void initialize(QString name, const QPixmap *pic = 0);

    void initialize(GEffectPtr &effect, const std::string &name);

    bool isChecked() const;

    void setLabelImage(const QPixmap* pic);

    void setNumFrames(int num_frames);

    void addEffect(GEffectPtr& efx);

    void write(YAML::Emitter &out) const;

    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;

    void releaseEffectWidget(GEffectPtr& efx);
    void releaseAllEffectWidgets();

signals:
    void effectWidgetReleased(GEffectPtr& efx);
    void releaseObject(GObjLayer* obj);

public slots:
    void releaseWidget();
    void toggleShown(bool shown);

public:
    GTimelineEx *timeline() const;
    void setTimeline(GTimelineEx *timeline);
    QPushButton *deleteButton() const;

protected:
    GTimelineEx *timeline_;
    QLabel *name_label_;
    GPictureLabel *picture_;
    QCheckBox *show_checkbox_;
    QPushButton *delete_button_;
};

#endif // GEOBJLAYER_H
