#ifndef GEFFECTMANAGER_H
#define GEFFECTMANAGER_H

#include "GEffect.h"
//#include "GVideoContent.h"
#include "opencv2/stitching/detail/blenders.hpp"
#include <vector>
#include <fstream>
#include <map>
#include <list>

#ifndef Q_MOC_RUN
#include <yaml-cpp/yaml.h>
#endif // Q_MOC_RUN

typedef std::multimap<int, GEffectPtr> OrderedEffectMap;
typedef std::map<int, std::list<GEffectPtr>> PriorityEffectMap;

class GEffectManager
{
public:
//    static qreal M_STREAK_LENGTH; // change to class DisplayResult?
    static qreal M_BLEND_ALPHA;
    static int M_TRAIL_ALPHA;
    static int M_TRAIL_STEP;
    static int M_LOOP_COUNT;
    static bool M_BLUR_ORDER;

public:
    QImage m_background;
    double window_image_scale_;

public:
    GEffectManager();

    void applyInpaint(const GPathPtr& path);
    void applyTrash(const GPathPtr &path);
    void applyStill(const GPathPtr &path);
    void applyBlack(const GPathPtr &path);
    void applyLoop(const GPathPtr& path);

    void refreshAllPathImage();

//    void cvtPathData(GPathPtr &path) const;
    GEffectPtr addEffect(const GPathPtr &path, G_EFFECT_ID type);

    GEffectPtr addPathEffect(GPathPtr &path, G_EFFECT_ID type, const YAML::Node &node);

    void reset();
    bool undo();

    void read(const QString& file);
    void readOld(const QString& file);
    void write(const QString& file);

    void loadOldPathData(const YAML::Node &node, GPathPtr &path);

    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const GPathPtr &path);
    friend void operator >> (const YAML::Node &node, GPathPtr &path);

    friend YAML::Emitter &operator << (YAML::Emitter& out,
                                       const QPainterPath &painter_path);
    friend void operator >> (const YAML::Node &node, QPainterPath &painter_path);

    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const std::vector<GRoiPtr> &rois);
    friend void operator >> (const YAML::Node& node, std::vector<GRoiPtr> &rois);

    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const std::vector<QRect> &rects);
    friend void operator >> (const YAML::Node& node, std::vector<QRect> &rects);
    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const std::vector<QRectF> &rects);
    friend void operator >> (const YAML::Node& node, std::vector<QRectF> &rects);

    friend YAML::Emitter& operator << (YAML::Emitter& out,
                                       const std::vector<bool> &arr);
    friend void operator >> (const YAML::Node& node, std::vector<bool> &arr);

    friend YAML::Emitter& operator << (YAML::Emitter& out, const GRoiPtr &roi);
    friend void operator >> (const YAML::Node& node, GRoiPtr &roi);
    friend YAML::Emitter& operator << (YAML::Emitter& out, const QRect &rect);
    friend void operator >> (const YAML::Node& node, QRect &rect);
    friend YAML::Emitter& operator << (YAML::Emitter& out, const QRectF &rectF);
    friend void operator >> (const YAML::Node& node, QRectF &rectF);

    friend YAML::Emitter& operator << (YAML::Emitter& out, const QSize &size);
    friend void operator >> (const YAML::Node &node, QSize &size);

//    void restoreForegroundImage(GPathPtr &path, GVideoContent *video_);

//    int pathId(std::vector<const GPathTrackData *> &paths, G_EFX *efx) const;

//    void blendEffects(cv::Mat3b &background, cv::Mat1b &background_mask,
//                      cv::Mat1b &background_mask2, int frame_id, GVideoContent *video) const;

    void drawEffects(QPainter &painter, int frame_id) const;
    void renderEffects(QPainter &painter, int frame_id) const;
    void printDrawingOrder() const;

protected:
    void preDrawEffects(QPainter &painter, int frame_id) const;

public:
//    int anchor_length_;
    bool show_text;

    QSize video_size_;
    std::vector<QPoint> m_lines_;

    static std::map<GEffectPtr, int> effect_map_write_tmp;
    static std::vector<GEffectPtr> effect_map_read_tmp;

//    typedef std::multimap<int, G_EFX*> DrawOrderMap;
//    DrawOrderMap draw_order_map_;

//    void addEffects(G_EFX *efx);
//    void eraseEffect(G_EFX *efx);

    bool empty() const { return prioritied_effects_.empty(); }
    void pushEffect(const GEffectPtr &efx);
    bool popEffect(const GEffectPtr &efx);

    void clear()
    {
        prioritied_effects_.clear();
        still_trash_effects_.clear();
        recent_effects_.clear();
    }

    const PriorityEffectMap& getEffects() const {
        return prioritied_effects_;
    }

private:
//    OrderedEffectMap ordered_effects_;
    PriorityEffectMap prioritied_effects_;
    std::vector<GEffectPtr> still_trash_effects_;
    std::vector<GEffectPtr> recent_effects_;

    GEffectPtr createEffect(const GPathPtr &path, G_EFFECT_ID type);
};

#endif // GEFFECTMANAGER_H
