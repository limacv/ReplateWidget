#pragma once
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <opencv2/core.hpp>
#include <string>

class MLConfigManager
{
public:
	MLConfigManager()
		:cache_root("./.cache"),
		raw_video_path(),
		yolov5_path("./YOLOv5"),
		yolov5_weight("yolov5m.pt"),
		python_path("python.exe")
	{}

	~MLConfigManager() {}
	static MLConfigManager& get()
	{
		static MLConfigManager manager;
		return manager;
	}
	void update_videopath(const QString& video_path);
	void initFromFile(const QString& cfgpath = "./config.yaml");

public:
	QString get_cache_path() const { return QDir(cache_root).filePath(basename); }
	QString get_detect_result_cache(int frameidx) const 
	{ 
		return QString("%1%2%3_%4.txt").arg(
			get_cache_path(), 
			QDir::separator(), 
			get_raw_video_base(), 
			QString::number(frameidx)); 
	}
	QString get_track_result_cache() const
	{
		return QString("%1%2tracking.txt").arg(
			get_cache_path(),
			QDir::separator()
		);
	}
	QString get_yolov5_path() const { return yolov5_path; }
	QString get_yolov5_weight() const { return yolov5_weight; }
	QString get_python_path() const { return python_path; }
	QString get_raw_video_path() const { return raw_video_path; }
	QString get_raw_video_base() const { return basename; }
private:
	QString cache_root;
	QString yolov5_path;
	QString yolov5_weight;
	QString python_path;

	QString raw_video_path;
	QString basename;
};

inline
void MLConfigManager::update_videopath(const QString& video_path)
{
	auto ff = QFileInfo(video_path);
	raw_video_path = ff.absoluteFilePath();
	basename = ff.baseName();
	
	if (!QDir().mkpath(get_cache_path()))
		qWarning("MLConfigManager::failed to create directory %s", qPrintable(get_cache_path()));
}

inline
void MLConfigManager::initFromFile(const QString& cfgpath)
{
	cv::FileStorage fs;
	try
	{
		fs.open(cfgpath.toStdString(), cv::FileStorage::READ);
	}
	catch (const std::exception&)
	{
		qWarning("ConfigManager::cannot parser config file %s, use default settings", qPrintable(cfgpath));
	}
	if (!fs.isOpened())
		qWarning("ConfigManager::cannot open config file %s, use default settings", qPrintable(cfgpath));

	if (!fs["cache_root"].empty())
	{
		cache_root = QString::fromStdString((std::string)fs["cache_root"]);
		cache_root = QFileInfo(cache_root).absoluteFilePath();
	}
	if (!fs["yolov5_path"].empty())
	{
		yolov5_path = QString::fromStdString((std::string)fs["yolov5_path"]);
		yolov5_path = QFileInfo(yolov5_path).absoluteFilePath();
	}
	if (!fs["python_path"].empty()) python_path = QString::fromStdString((std::string)fs["python_path"]);

	if (!fs["detector"].empty())
	{
		if (!fs["detector"]["yolov5weights"].empty())
			yolov5_weight = QString::fromStdString((std::string)fs["detector"]["yolov5weights"]);
	}

	fs.release();
}

