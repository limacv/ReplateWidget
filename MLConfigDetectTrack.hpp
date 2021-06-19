#pragma once
#include <qstring.h>

class MLConfigDetectTrack
{
public:
	MLConfigDetectTrack() { restore_default(); }

	void restore_default();
	void write(YAML::Emitter& out) const;
	void read(const YAML::Node& doc);

public:
    QString detect_yolov5_path;
    QString detect_python_path;
	QString detect_yolov5_weight;
	int detect_image_size;
	float detect_conf_threshold;
	float detect_iou_threshold;
	QString detect_device;
	bool detect_save_visualization;
};

inline
void MLConfigDetectTrack::restore_default()
{
    detect_python_path = "pythonw.exe";
    detect_yolov5_path = "./YOLOv5";
	detect_yolov5_weight = "yolov5m.pt";
	detect_image_size = 1280;
	detect_conf_threshold = 0.3;
	detect_iou_threshold = 0.7;
	detect_device = "0";
	detect_save_visualization = false;
}

inline 
void MLConfigDetectTrack::write(YAML::Emitter& out) const
{
    out << YAML::Key << "Detectrack" << YAML::Value;

    out << YAML::BeginMap;
    out << YAML::Key << "Yolov5Path" << detect_yolov5_path.toStdString();
    out << YAML::Key << "PythonPath" << detect_python_path.toStdString();
    out << YAML::Key << "Yolov5Weight" << detect_yolov5_weight.toStdString();
    out << YAML::Key << "ImageSize" << detect_image_size;
    out << YAML::Key << "ConfidenceThreshold" << detect_conf_threshold;
    out << YAML::Key << "IouThreshold" << detect_iou_threshold;
    out << YAML::Key << "Device" << detect_device.toStdString();
    out << YAML::Key << "SaveVisualization" << detect_save_visualization;

    out << YAML::EndMap;
}

inline 
void MLConfigDetectTrack::read(const YAML::Node& doc)
{
    const YAML::Node& node = doc["Detectrack"];
    if (node) {
        if (node["Yolov5Path"]) detect_yolov5_path = QString::fromStdString(node["Yolov5Path"].as<string>());
        if (node["PythonPath"]) detect_python_path = QString::fromStdString(node["PythonPath"].as<string>());
        if (node["Yolov5Weight"]) detect_yolov5_weight = QString::fromStdString(node["Yolov5Weight"].as<string>());
        if (node["ImageSize"]) detect_image_size = node["ImageSize"].as<int>();
        if (node["ConfidenceThreshold"]) detect_conf_threshold = node["ConfidenceThreshold"].as<float>();
        if (node["IouThreshold"]) detect_iou_threshold = node["IouThreshold"].as<float>();
        if (node["Device"]) detect_device = QString::fromStdString(node["Device"].as<string>());
        if (node["SaveVisualization"]) detect_save_visualization = node["SaveVisualization"].as<bool>();
    }

    detect_yolov5_path = QFileInfo(detect_yolov5_path).absoluteFilePath();
}