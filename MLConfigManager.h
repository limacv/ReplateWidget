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
		python_path("pythonw.exe"),
		stitcher_detect_qualitylevel(0.1),
		stitcher_detect_cellH(30),
		stitcher_track_min_rot(0.5),
		stitcher_track_inlier_threshold(1.0),
		stitcher_track_win(8),
		stitcher_verbose(false)
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
	// Step1
	QString get_cache_path() const { return QDir(cache_root).filePath(basename); }

	QString get_yolov5_path() const { return yolov5_path; }
	QString get_yolov5_weight() const { return yolov5_weight; }
	QString get_python_path() const { return python_path; }
	QString get_raw_video_path() const { return raw_video_path; }
	QString get_raw_video_base() const { return basename; }

	QString get_detect_result_cache(int frameidx) const { return QDir(get_cache_path()).filePath(QString("%1_%2.txt").arg(get_raw_video_base(), QString::number(frameidx))); }
	QString get_track_result_cache() const { return QDir(get_cache_path()).filePath("tracking.txt"); }

	QString get_stitch_cache() const { return QDir(get_cache_path()).filePath("stitch"); }
	QString get_stitch_warpedimg_path(int frameidx) const { return QDir(get_stitch_cache()).filePath(QString("frame_%1.png").arg(QString::number(frameidx))); }
	QString get_stitch_optflow_path(int frameidx) const { return QDir(get_stitch_cache()).filePath(QString("flow_%1.png").arg(QString::number(frameidx))); }
	QString get_stitch_background_path() const { return QDir(get_stitch_cache()).filePath("background.png"); }
	QString get_stitch_rois_path() const { return QDir(get_stitch_cache()).filePath("rois.txt"); }
	QString get_stitch_track_bboxes_path() const { return QDir(get_stitch_cache()).filePath("track_bboxes.txt"); }
	QString get_stitch_detect_bboxes_path() const { return QDir(get_stitch_cache()).filePath("detect_bboxes.txt"); }

private:
	QString cache_root;
	QString yolov5_path;
	QString yolov5_weight;
	QString python_path;

	QString raw_video_path;
	QString basename;

public:
	float stitcher_detect_qualitylevel;
	int stitcher_detect_cellH;
	float stitcher_track_min_rot;
	float stitcher_track_inlier_threshold;
	int stitcher_track_win;
	bool stitcher_verbose;
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

	if (!fs["stitcher"].empty())
	{
		if (!fs["stitcher"]["detect_qualitylevel"].empty())
			stitcher_detect_qualitylevel = (float)fs["stitcher"]["detect_qualitylevel"];
		if (!fs["stitcher"]["detect_cellH"].empty())
			stitcher_detect_cellH = (int)fs["stitcher"]["detect_cellH"];
		if (!fs["stitcher"]["track_min_rot"].empty())
			stitcher_track_min_rot = (float)fs["stitcher"]["track_min_rot"];
		if (!fs["stitcher"]["track_inlier_threshold"].empty())
			stitcher_track_inlier_threshold = (float)fs["stitcher"]["track_inlier_threshold"];
		if (!fs["stitcher"]["track_win"].empty())
			stitcher_track_win = (int)fs["stitcher"]["track_win"];
		if (!fs["stitcher"]["verbose"].empty())
			stitcher_verbose = (int)fs["stitcher"]["verbose"];
	}

	fs.release();
}

