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

class MLConfigStitcher
{
public:
    MLConfigStitcher()
    {
        restore_default();
    }

    void restore_default();
    void write(YAML::Emitter& out) const;
    void read(const YAML::Node& doc);
    void setWaveCorrect(const string& str);

    string stitcher_type_;
    int stitch_skip_frame_;
    string features_type_;
    float match_conf_;
    int match_range_width;
    float match_conf_thresh_;
    string wave_correct_;
    string warp_type_;
    SeamType seam_type_;
    int blend_type_;
    float blend_strength_;
    double work_megapix_;
    double seam_megapix_;
    double compose_megapix_;

    int expos_comp_type_;
    int expos_comp_nr_feeds;
    int expos_comp_nr_filtering;
    int expos_comp_block_size;

    // this is used by StitcherSsfm
    float stitcher_detect_qualitylevel;
    int stitcher_detect_cellH;
    float stitcher_track_min_rot;
    float stitcher_track_inlier_threshold;
    int stitcher_track_win;
    bool stitcher_verbose;

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
void MLConfigStitcher::restore_default()
{
    stitcher_type_ = "Ml";
    stitch_skip_frame_ = 10;
    features_type_ = "surf";
    warp_type_ = "cylindrical";
    match_conf_thresh_ = 1;
    match_conf_ = 0.6f;
    match_range_width = 5;
    seam_type_ = SeamType::NO;
    blend_type_ = Blender::FEATHER;
    wave_correct_type_ = WAVE_CORRECT_HORIZ;
    blend_strength_ = 1;
    work_megapix_ = 0.7;
    seam_megapix_ = 0.01;
    compose_megapix_ = -1;
    expos_comp_type_ = ExposureCompensator::NO;
    expos_comp_nr_feeds = 1;
    expos_comp_nr_filtering = 2;
    expos_comp_block_size = 32;
    try_gpu_ = true;
    preview_ = false;
    save_BA_ = true;
    load_BA_ = true;

    stitcher_detect_qualitylevel = 0.1;
    stitcher_detect_cellH = 30;
    stitcher_track_min_rot = 0.5;
    stitcher_track_inlier_threshold = 1.0;
    stitcher_track_win = 8;
    stitcher_verbose = false;
}

inline
void MLConfigStitcher::write(YAML::Emitter& out) const
{
    out << YAML::Key << "StitchConfig" << YAML::Value;

    out << YAML::BeginMap;
    out << YAML::Key << "MatchConf" << match_conf_;
    out << YAML::Key << "ConfThresh" << match_conf_thresh_;
    out << YAML::Key << "FeatureType" << features_type_;
    out << YAML::Key << "SkipFrame" << stitch_skip_frame_;
    out << YAML::Key << "StitcherType" << stitcher_type_;
    out << YAML::Key << "WaveCorrect" << wave_correct_;
    out << YAML::Key << "WarpType" << warp_type_;
    out << YAML::Key << "SeamType" << seam_type_;
    out << YAML::Key << "BlendType" << blend_type_;
    out << YAML::Key << "BlendStrength" << blend_strength_;
    out << YAML::Key << "ExpoCompType" << expos_comp_type_;
    out << YAML::Key << "ExpoCompNrFeeds" << expos_comp_nr_feeds;
    out << YAML::Key << "ExpoCompNrFiltering" << expos_comp_nr_filtering;
    out << YAML::Key << "ExpoCompBlockSize" << expos_comp_block_size;
    
    out << YAML::Key << "StitcherDetectQualityLevel" << stitcher_detect_qualitylevel;
    out << YAML::Key << "StitcherDetectCellH" << stitcher_detect_cellH;
    out << YAML::Key << "StitcherTrackMinRot" << stitcher_track_min_rot;
    out << YAML::Key << "StitcherTrackInlierThreshold" << stitcher_track_inlier_threshold;
    out << YAML::Key << "StitcherTrackWin" << stitcher_track_win;
    out << YAML::Key << "StitcherVerbose" << stitcher_verbose;


    out << YAML::EndMap;
}

inline
void MLConfigStitcher::read(const YAML::Node& doc)
{
    const YAML::Node& node = doc["StitchConfig"];
    if (node) {
        if (node["MatchConf"]) match_conf_ = node["MatchConf"].as<float>();
        if (node["ConfThresh"]) match_conf_thresh_ = node["ConfThresh"].as<float>();
        if (node["FeatureType"]) features_type_ = node["FeatureType"].as<string>();
        if (node["SkipFrame"]) stitch_skip_frame_ = node["SkipFrame"].as<int>();
        if (node["StitcherType"]) stitcher_type_ = node["StitcherType"].as<string>();
        if (node["WaveCorrect"]) wave_correct_ = node["WaveCorrect"].as<string>();
        if (node["WarpType"]) warp_type_ = node["WarpType"].as<string>();
        if (node["SeamType"]) seam_type_ = (SeamType)node["SeamType"].as<int>();
        if (node["BlendType"]) blend_type_ = node["BlendType"].as<int>();
        if (node["BlendStrength"]) blend_strength_ = node["BlendStrength"].as<float>();
        if (node["ExpoCompType"]) expos_comp_type_ = node["ExpoCompType"].as<double>();
        if (node["ExpoCompNrFeeds"]) expos_comp_nr_feeds = node["ExpoCompNrFeeds"].as<double>();
        if (node["ExpoCompNrFiltering"]) expos_comp_nr_filtering = node["ExpoCompNrFiltering"].as<double>();
        if (node["ExpoCompBlockSize"]) expos_comp_block_size = node["ExpoCompBlockSize"].as<double>();

        if (node["StitcherDetectQualityLevel"]) stitcher_detect_qualitylevel = node["StitcherDetectQualityLevel"].as<float>();
        if (node["StitcherDetectCellH"]) stitcher_detect_cellH = node["StitcherDetectCellH"].as<float>();
        if (node["StitcherTrackMinRot"]) stitcher_track_min_rot = node["StitcherTrackMinRot"].as<float>();
        if (node["StitcherTrackInlierThreshold"]) stitcher_track_inlier_threshold = node["StitcherTrackInlierThreshold"].as<float>();
        if (node["StitcherTrackWin"]) stitcher_track_win = node["StitcherTrackWin"].as<float>();
        if (node["StitcherVerbose"]) stitcher_verbose = node["StitcherVerbose"].as<bool>();

    }
}

inline
void MLConfigStitcher::setWaveCorrect(const string& str)
{
    wave_correct_ = str;

    if (wave_correct_ != "no")
        wave_correct_type_ = convertWaveCorrect(str);
}

inline
void MLConfigStitcher::parseStitchArgs()
{
    MLConfigStitcher& st = *this;

    //GCONFIGVALUE2(st.match_conf_, "match_conf", float);
    //GCONFIGVALUE2(st.match_conf_thresh_, "conf_thresh", float);
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
WaveCorrectKind MLConfigStitcher::convertWaveCorrect(const string& wave_correct) const
{
    if (wave_correct == "vert")
        return cv::detail::WAVE_CORRECT_VERT;
    else if (wave_correct == "horiz")
        return cv::detail::WAVE_CORRECT_HORIZ;
    else
        qDebug("Invalid wave_correct str input");
}
