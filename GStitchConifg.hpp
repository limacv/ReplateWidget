#pragma once

#include <vector>
#include <string>
#include <QRect>
#include <opencv2/opencv.hpp>
#include <opencv2/stitching/detail/motion_estimators.hpp>
#include <opencv2/stitching/detail/exposure_compensate.hpp>
#include <opencv2/stitching/detail/blenders.hpp>

#ifndef Q_MOC_RUN
#include <yaml-cpp/yaml.h>
#endif // Q_MOC_RU

using namespace std;
using namespace cv::detail;

static char G_SEAM_STR[][20] = {
    "no", "n",
    "voronoi", "v",
    "gc_color", "g",
    "gc_colorgrad", "gg",
    "dp_color", "d",
    "dp_colorgrad", "dg"
};

static char G_BLEND_STR[][20] = {
    "no", "n",
    "feather", "f",
    "multiband", "m"
};

static char G_WAVE_STR[][20] = {
    "horiz", "h",
    "vert", "v",
    "no", "n"
};

enum SeamType {
    NO = 0,
    VORONOI,
    GC_COLOR,
    GC_COLORGRAD,
    DP_COLOR,
    DP_COLORGRAD
};

class GStitchConfig
{
public:
    GStitchConfig()
        :conf_thresh_(1)
        , features_type_("surf")
        , warp_type_("cylindrical")
        , match_conf_(0.6f)
        , matcher_type("")
        , match_range_width(-1)
        , seam_type_(SeamType::NO)
        , blend_type_(Blender::FEATHER)
        , blend_strength_(1)
        , work_megapix_(0.7)
        , seam_megapix_(0.01)
        , compose_megapix_(-1)
        , expos_comp_type_(ExposureCompensator::NO)
        ,expos_comp_nr_feeds(1)
        ,expos_comp_nr_filtering(2)
        ,expos_comp_block_size(32)
        , try_gpu_(true)
        , preview_(false)
        , save_BA_(true)
        , load_BA_(true)
    {
        parseStitchArgs();
    }


    void write(YAML::Emitter& out) const;
    void read(const YAML::Node& doc);
    void writeBundle(YAML::Emitter& out) const;
    void readBundle(const YAML::Node& doc);
    void setWaveCorrect(const string& str);

    string features_type_;
    float match_conf_;
    string matcher_type;
    int match_range_width;
    float conf_thresh_;
    string wave_correct_;
    string warp_type_;
    SeamType seam_type_;
    vector<QRectF> logo_masks_;
    int blend_type_;
    float blend_strength_;
    double work_megapix_;
    double seam_megapix_;
    double compose_megapix_;

    int expos_comp_type_;
    int expos_comp_nr_feeds;
    int expos_comp_nr_filtering;
    int expos_comp_block_size;

    vector<CameraParams> cameras_;

    // no need to store
    bool save_BA_;
    bool load_BA_;
    int try_gpu_;
    bool preview_;
    WaveCorrectKind wave_correct_type_;

    friend YAML::Emitter& operator << (YAML::Emitter& out,
        const std::vector<QRectF>& rects);
    friend void operator >> (const YAML::Node& node, std::vector<QRectF>& rects);

    void parseStitchArgs();

private:
    WaveCorrectKind convertWaveCorrect(const string& wave_correct) const;
};


inline
void GStitchConfig::write(YAML::Emitter& out) const
{
    out << YAML::Key << "StitchConfig" << YAML::Value;

    out << YAML::BeginMap;
    out << YAML::Key << "LogoMasks" << logo_masks_;
    out << YAML::Key << "MatchConf" << match_conf_;
    out << YAML::Key << "ConfThresh" << conf_thresh_;
    out << YAML::Key << "FeatureType" << features_type_;
    out << YAML::Key << "WaveCorrect" << wave_correct_;
    out << YAML::Key << "WarpType" << warp_type_;
    out << YAML::Key << "SeamType" << seam_type_;
    out << YAML::Key << "BlendType" << blend_type_;
    out << YAML::Key << "BlendStrength" << blend_strength_;
    out << YAML::EndMap;
}

inline
void GStitchConfig::read(const YAML::Node& doc)
{
    const YAML::Node& node = doc["StitchConfig"];
    if (node) {
        node["LogoMasks"] >> logo_masks_;
        match_conf_ = node["MatchConf"].as<float>();
        conf_thresh_ = node["ConfThresh"].as<float>();
        features_type_ = node["FeatureType"].as<string>();
        wave_correct_ = node["WaveCorrect"].as<string>();
        warp_type_ = node["WarpType"].as<string>();
        seam_type_ = (SeamType)node["SeamType"].as<int>();
        blend_type_ = node["BlendType"].as<int>();
        blend_strength_ = node["BlendStrength"].as<float>();
    }
    readBundle(doc);
}

inline
void GStitchConfig::writeBundle(YAML::Emitter& out) const
{
    out << YAML::Key << "Bundle" << YAML::Value;
    out << YAML::BeginSeq;
    for (int i = 0; i < cameras_.size(); ++i) {
        out << YAML::BeginMap;
        out << YAML::Key << "Focal" << YAML::Value << cameras_[i].K().at<double>(0, 0);
        out << YAML::Key << "Principal" << YAML::Value
            << YAML::Flow << YAML::BeginSeq << cameras_[i].K().at<double>(0, 2)
            << cameras_[i].K().at<double>(1, 2) << YAML::EndSeq;
        out << YAML::Key << "Rotation" << YAML::Value;
        cv::SVD svd;
        svd(cameras_[i].R, cv::SVD::FULL_UV);
        cv::Mat R = svd.u * svd.vt;
        if (cv::determinant(R) < 0)
            R *= -1;
        cv::Mat r = R.t();
        out << YAML::Flow;
        out << YAML::BeginSeq;
        for (int j = 0; j < 9; ++j)
            out << r.at<double>(j / 3, j % 3);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
}

inline
void GStitchConfig::readBundle(const YAML::Node& doc)
{
    const YAML::Node& node = doc["Bundle"];
    if (node) {
        cameras_.resize(node.size());
        for (int i = 0; i < cameras_.size(); ++i) {
            const YAML::Node& n = node[i];
            cameras_[i].focal = n["Focal"].as<double>();
            cameras_[i].ppx = n["Principal"][0].as<double>();
            cameras_[i].ppy = n["Principal"][1].as<double>();
            double R[9];
            double t[3] = { 0,0,0 };
            for (int j = 0; j < 9; ++j)
                R[j] = n["Rotation"][j].as<double>();
            cameras_[i].R = cv::Mat(3, 3, CV_64F, R).t();
            cameras_[i].t = cv::Mat(3, 1, CV_64F, t).clone();
        }
    }
}

inline
void GStitchConfig::setWaveCorrect(const string& str)
{
    wave_correct_ = str;

    if (wave_correct_ != "no")
        wave_correct_type_ = convertWaveCorrect(str);
}

inline
void GStitchConfig::parseStitchArgs()
{
    GStitchConfig& st = *this;

    //GCONFIGVALUE2(st.match_conf_, "match_conf", float);
    //GCONFIGVALUE2(st.conf_thresh_, "conf_thresh", float);
    //GCONFIGVALUE2(st.blend_step_, "blend_step", int);
    //GCONFIGVALUE(st.load_BA_, "loadBa", bool);
    //GCONFIGVALUE(st.save_BA_, "saveBa", bool);
    //GCONFIGVALUE(st.warp_type_, "warpType", string);
    //GCONFIGVALUE(st.preview_, "preview", bool);
    //GCONFIGVALUE2(st.try_gpu_, "try_gpu", int);
    //GCONFIGVALUE2(st.work_megapix_, "work_megapix", double);
    //GCONFIGVALUE2(st.seam_megapix_, "seam_megapix", double);
    //GCONFIGVALUE2(st.compose_megapix_, "compose_megapix", double);
    //GCONFIGVALUE(st.features_type_, "features_type", string);
    //GCONFIGVALUE(st.wave_correct_, "wave_correct", string);
    if (!st.wave_correct_.empty())
    {
        if (st.wave_correct_ == "no")
            st.wave_correct_ = "no";
        else if (st.wave_correct_ == "vert") {
            st.wave_correct_type_ = WAVE_CORRECT_VERT;
        }
        else if (st.wave_correct_ == "horiz")
            st.wave_correct_type_ = WAVE_CORRECT_HORIZ;
        else
        {
            cerr << "Bad wave_correct flag value: " << st.wave_correct_type_;
        }
    }
    string expos_comp;
    //GCONFIGVALUE(expos_comp, "expos_comp", string);
    if (!expos_comp.empty())
    {
        if (expos_comp == "no")
            st.expos_comp_type_ = ExposureCompensator::NO;
        else if (expos_comp == "gain")
            st.expos_comp_type_ = ExposureCompensator::GAIN;
        else if (expos_comp == "gain_blocks")
            st.expos_comp_type_ = ExposureCompensator::GAIN_BLOCKS;
        else
        {
            cerr << "Bad exposure compensation method: " << expos_comp;
        }
    }
    string seam_type;
    //GCONFIGVALUE(seam_type, "seam", string);
    if (!seam_type.empty()) {
        if (seam_type == "no")
            st.seam_type_ = SeamType::NO;
        else if (seam_type == "voronoi")
            st.seam_type_ = SeamType::VORONOI;
        else if (seam_type == "gc_color")
            st.seam_type_ = SeamType::GC_COLOR;
        else if (seam_type == "gc_colorgrad")
            st.seam_type_ = SeamType::GC_COLORGRAD;
        else if (seam_type == "dp_color")
            st.seam_type_ = SeamType::DP_COLOR;
        else if (seam_type == "dp_colorgrad")
            st.seam_type_ = SeamType::DP_COLORGRAD;
    }

    string blend;
    //GCONFIGVALUE(blend, "blend", string);
    if (!blend.empty())
    {
        if (blend == "no")
            st.blend_type_ = Blender::NO;
        else if (blend == "feather")
            st.blend_type_ = Blender::FEATHER;
        else if (blend == "multiband")
            st.blend_type_ = Blender::MULTI_BAND;
        else
        {
            cerr << "Bad blending method: " << blend;
        }
    }
    //GCONFIGVALUE2(st.blend_strength_, "blend_strength", float);
}

inline
WaveCorrectKind GStitchConfig::convertWaveCorrect(const string& wave_correct) const
{
    if (wave_correct == "vert")
        return cv::detail::WAVE_CORRECT_VERT;
    else if (wave_correct == "horiz")
        return cv::detail::WAVE_CORRECT_HORIZ;
    else
        qDebug("Invalid wave_correct str input");
}
