#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include "MLConfigStitcher.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>

class MLProgressObserverBase;

class StitcherBase
{
public:
	StitcherBase(const MLConfigStitcher* config) 
		:progress_reporter(nullptr),
		config_(config)
	{}
	virtual ~StitcherBase() {}

public:

	virtual int stitch(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks) = 0;
	virtual bool loadCameraParams(const std::string& path);
	virtual bool saveCameraParams(const std::string& path) const;
	virtual int warp_and_composite(const std::vector<cv::Mat>& frames, const std::vector<cv::Mat>& masks, 
		std::vector<cv::Mat>& warped, std::vector<cv::Rect>& windows, cv::Mat& stitched) = 0;
	virtual int warp_points(const int frameidx, std::vector<cv::Point>& inoutpoints) const = 0;

	void set_progress_observer(MLProgressObserverBase* observer) { progress_reporter = observer; }

protected:
	std::vector<cv::detail::CameraParams> cameras_;
	const MLConfigStitcher* config_;
	MLProgressObserverBase* progress_reporter;
};


inline
bool StitcherBase::loadCameraParams(const std::string& path)
{
	//cv::FileStorage outfile(path, cv::FileStorage::WRITE);
	//if (!outfile.isOpened()) return false;
	//if (cameras_.empty()) return false;

	//outfile << "Cameras" << cameras_;
	//outfile.release();
	//return true;
    
    YAML::Node doc;
    try
    {
        doc = YAML::LoadFile(path);
    }
    catch (YAML::BadFile& e)
    {
        qDebug(e.what());
        return false;
    }
    catch (YAML::ParserException& e)
    {
        qWarning("Failed to parse the camera parameter file");
        return false;
    }

	const YAML::Node& node = doc["Bundle"];
    if (!node) return false;

    cameras_.resize(node.size());
    for (int i = 0; i < cameras_.size(); ++i) {
        const YAML::Node& n = node[i];
        cameras_[i].focal = n["Focal"].as<double>();
        cameras_[i].ppx = n["Principal"][0].as<double>();
        cameras_[i].ppy = n["Principal"][1].as<double>();
        float R[9];
        float t[3] = { 0,0,0 };
        for (int j = 0; j < 9; ++j)
            R[j] = n["Rotation"][j].as<float>();
        cameras_[i].R = cv::Mat(3, 3, CV_32F, R).clone();
        cameras_[i].t = cv::Mat(3, 1, CV_32F, t).clone();
    }
    return true;
}

inline
bool StitcherBase::saveCameraParams(const std::string& path) const
{
	YAML::Emitter out;
    out << YAML::BeginMap;
	out << YAML::Key << "Bundle" << YAML::Value;
    out << YAML::BeginSeq;
    for (int i = 0; i < cameras_.size(); ++i) {
        out << YAML::BeginMap;
        out << YAML::Key << "Focal" << YAML::Value << cameras_[i].focal;
        out << YAML::Key << "Principal" << YAML::Value
            << YAML::Flow << YAML::BeginSeq << cameras_[i].ppx << cameras_[i].ppy << YAML::EndSeq;

        out << YAML::Key << "Rotation" << YAML::Value;
        out << YAML::Flow;
        out << YAML::BeginSeq;
        cv::Mat_<float> Rf;
        cameras_[i].R.convertTo(Rf, Rf.type());
        for (int j = 0; j < 9; ++j)
            out << *((float*)Rf.data + j);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
	ofstream f(path);
	if (!f.is_open()) return false;

	f << out.c_str();
    f.close();
    return true;
}