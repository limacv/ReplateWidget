#ifndef GEFFECT_H
#define GEFFECT_H

#include <QVector>
#include <QImage>
#include <QPainter>
#include <QDebug>
#include <fstream>
#include <string>
#include <set>
#include <map>
#include "GPath.h"
#include "opencv2/stitching/detail/blenders.hpp"
#include "GEffectCommon.h"

#ifndef Q_MOC_RUN
#include <yaml-cpp/yaml.h>
#include <memory>
//#include <boost/smart_ptr.hpp>
#endif // Q_MOC_RUN

class GEffect;
//typedef boost::shared_ptr<GEffect> GEffectPtr;
typedef std::shared_ptr<GEffect> GEffectPtr;

class GEffect
{
public:
    explicit GEffect(const GPathPtr &path);
    virtual ~GEffect() {this->release();}

    GPathPtr path() {return path_;}
    const GPathPtr path() const {return path_;}

    virtual void preRender(QPainter& painter, int frame_id, int duration) {};
    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const = 0;

    QRect renderLocation(int frame_id) const;
    QImage renderImage(int frame_id) const;

    void readBasic(const YAML::Node &node);
    void writeBasic(YAML::Emitter &out) const;

    virtual void write(YAML::Emitter &out) const = 0;
    virtual void read(const YAML::Node& node) { readBasic(node); };

    friend YAML::Emitter& operator<< (YAML::Emitter& out, const GEffectPtr &efx);
    friend void operator>> (const YAML::Node& out, GEffectPtr &efx);

    /*
     * Effect properties
     */
    G_EFFECT_ID type() const { return effect_type_;}
    int priority() const {return priority_level_;} // smaller value will draw first
    void setPriority(int priority) {priority_level_ = priority;}

    bool shown() const {return is_shown_;}
    void toggleShown(bool checked) { is_shown_ = checked; }

    bool adjustLeft(int step);
    bool adjustRight(int step);
    bool moveStart(int step);

    int async() const { return async_offset_; }
    void adjustAsync(int offset, int duration);

    int startFrame() const {return pathStart();}
    int endFrame() const {return pathEnd();}
    int effectLength() const {return endFrame() - startFrame() + 1;}

    virtual void setOrder(bool b) { render_order_ = b; }
    virtual bool getOrder() const { return render_order_;}

    virtual bool setAlpha(float /*a*/) { qDebug() << "Alpha invalid!"; return false;}
    virtual float getAlpha() const { return -1;}
    virtual bool setSmooth(int /*s*/) {qDebug() << "Smooth invalid!"; return false;}
    virtual int getSmooth() const {return -1;}
    virtual bool setTransLevel(int a) {qDebug() << "Trans invalid!"; return false;}
    virtual int getTransLevel() const {return -1;}
    virtual bool setFadeLevel(int a) {qDebug() << "Fade invalid!"; return false;}
    virtual int getFadeLevel() const {return -1;}
    virtual bool setMarker(int a) {qDebug() << "Marker invalid!"; return false;}
    virtual int getMarker() const {return -1;}
    virtual bool setTrailLine(bool b) {qDebug() << "Trail line invalid!"; return false;}
    virtual int getTrailLine() const {return -1;}
    virtual bool setSpeed(int a) { qDebug() << "Speed invalid!"; return false;}
    int speed() const {return speed_factor_;}
    int syncId() const {return sync_id_;}
    void setSync(int id) {
        if (id > endFrame() || id < startFrame())
            return;
        sync_id_ = id;
    }

    static void setScalar(QSize size) {
        setScalar(size.width(), size.height());
    }
    static void setScalar(int width, int height) {
        scale_mat_ = QMatrix();
        scale_mat_.scale(width, height);
    }

protected:
    void setPath(const GPathPtr &path);
    int pathLength() const {return path()->length();}
    int pathStart() const {return path()->startFrame();}
    int pathEnd() const { return path()->endFrame(); }

    virtual int getSyncLocalIndex(int frame_id, int start_id, int duration) const;
    virtual int getSyncLocalIndex2(int frame_id, int start_id, int duration,
                                   int sync_id = -1, int slow_factor = 1) const;
    virtual int getSyncLocalIndexSlow(int frame_id, int start_id, int duration,
                                   int sync_id = -1, int slow_factor = 1) const;

    int getFadeAlpha(int effect_length, int idx, int fade_level) const;
    int getFadeAlphaMotion(int src_len, int range_len, int idx, int fade_level) const;
    int convertTrans(float a);

    virtual void release();
    static QMatrix scaleMat() {return scale_mat_;}

protected:
    static QMatrix scale_mat_;

    GPathPtr path_;
    G_EFFECT_ID effect_type_;
    int priority_level_;
    int async_offset_;
    bool render_order_;
    bool is_shown_;
    int trans_level_;
    int fade_level_;
    int marker_mode_; // -1: invalid. 0: no. 1: one triangle. 2: all triangle.
    int speed_factor_;
    int sync_id_; // global
    QRgb line_color_;
    float trail_alpha_;
    int line_smooth_;
    bool draw_line_;
};

class GEffectTrash : public GEffect
{
public:
    explicit GEffectTrash(const GPathPtr &path): GEffect(path){
        effect_type_ = EFX_ID_TRASH;
        priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
    }

    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const {}

    virtual void write(YAML::Emitter &out) const;
    //virtual void read(const YAML::Node &node);
};


class GEffectStill : public GEffect
{
public:
    explicit GEffectStill(const GPathPtr &path): GEffect(path){
        effect_type_ = EFX_ID_STILL;
        priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
    }

    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter &out) const;
    //virtual void read(const YAML::Node &node);
};

class GEffectInpaint : public GEffect
{
public:
    explicit GEffectInpaint(const GPathPtr& path) : GEffect(path) {
        effect_type_ = EFX_ID_INPAINT;
        priority_level_ = G_EFX_PRIORITY[effect_type_] * G_EFX_PRIORITY_STEP;
    }

    virtual void render(QPainter& painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter& out) const;
    //virtual void read(const YAML::Node& node);
};

class GEffectBlack : public GEffect
{
public:
    explicit GEffectBlack (const GPathPtr &path);
    virtual void release() {}
    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter &out) const;
    //virtual void read(const YAML::Node &node);
};

class GEffectTrail : public GEffect
{
public:
    std::vector<QImage> d_images;
    std::vector<QImage> cached_effect_;
    std::vector<QPointF> trail_;
    cv::Rect cached_roi_;
    bool recache;
    int prev_length_;

    explicit GEffectTrail(const GPathPtr &path);
    virtual void release() {d_images.clear();}

    virtual void preRender(QPainter &painter, int frame_id, int duration);

    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter &out) const;
    //virtual void read(const YAML::Node &node);

    virtual bool setAlpha(float a) { recache = true; trail_alpha_ = a; return true; }
    virtual float getAlpha() const { return trail_alpha_;}

    virtual bool setSmooth(int s);
    virtual int getSmooth() const {return line_smooth_;}

private:
    void getImageAlphaByVec(const std::vector<QPointF> &path,
                                    std::vector<float> &out) const;
    void generateTrail(QPainter &painter);
};

class GEffectMotion : public GEffect
{
    static int max_loop_num;
    std::vector<QPointF> trail_;
    std::vector<QPointF> marker_trail_;
public:
    explicit GEffectMotion(const GPathPtr &path);

    virtual void renderSlowVideo(QPainter &painter, int frame_id, int duration) const;

    virtual void renderSlow(QPainter &painter, int frame_id, int duration, bool video = false) const;

    virtual void preRender(QPainter &painter, int frame_id,
                           int duration);

    virtual void render(QPainter &painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter &out) const;
    //virtual void read(const YAML::Node &node);

    virtual bool setSmooth(int s);
    virtual int getSmooth() const {return line_smooth_;}

    virtual bool setFadeLevel(int a) { fade_level_ = a; return true;}
    virtual int getFadeLevel() const {return fade_level_;}

    virtual bool setTransLevel(int a) {trans_level_ = a; return true;}
    virtual int getTransLevel() const {return trans_level_;}

    virtual bool setTrailLine(bool b);
    virtual int getTrailLine() const {return draw_line_;}

    virtual bool setMarker(int a);
    virtual int getMarker() const {return marker_mode_;}

    virtual bool setSpeed(int a) {speed_factor_ = a; return true;}
};


class GEffectLoop : public GEffect
{
    static int max_loop_num;
public:
    explicit GEffectLoop(const GPathPtr& path);

    virtual void renderSlow(QPainter& painter, int frame_id, int duration, bool video = false) const;

    virtual void preRender(QPainter& painter, int frame_id,
        int duration);

    virtual void render(QPainter& painter, int frame_id, int duration, bool video = false) const;

    virtual void write(YAML::Emitter& out) const;

    virtual bool setFadeLevel(int a) { fade_level_ = a; return true; }
    virtual int getFadeLevel() const { return fade_level_; }

    virtual bool setTransLevel(int a) { trans_level_ = a; return true; }
    virtual int getTransLevel() const { return trans_level_; }

    virtual bool setSpeed(int a) { speed_factor_ = a; return true; }
};

#endif // GEFFECT_H
