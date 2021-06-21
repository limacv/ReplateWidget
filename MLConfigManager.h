#pragma once
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <yaml-cpp/yaml.h>
#include <string>
#include "MLConfigStitcher.hpp"
#include "MLConfigDetectTrack.hpp"
#include <qwidget.h>
#include <qdockwidget.h>

namespace Ui { class MLConfigWidget; };

class MLConfigManager
{
public:
	MLConfigManager()
		:cache_root("./.cache"),
		config_widget_ui(nullptr),
		config_widget(nullptr)
	{}

	~MLConfigManager() {}
	static MLConfigManager& get()
	{
		static MLConfigManager manager;
		return manager;
	}

	void setup_ui(QWidget* parent = Q_NULLPTR);
	QAction* config_widget_view_action() { return config_widget->toggleViewAction(); }
	
	void update_ui();
	void update_videopath(const QString& video_path);
	void readFromFile(const QString& cfgpath = "./config.yaml");
	void writeToFile(const QString& cfgpapth) const;
	void restore_to_default();

public:
	// path config
	QString get_cache_path() const { return QDir(cache_root).filePath(basename); }
	QString get_localconfig_path() const { return QDir(get_cache_path()).filePath("config.yaml"); }
	QString get_replatecfg_path() const { return QDir(get_cache_path()).filePath("replate.yaml"); }

	QString get_yolov5_path() const { return detectrack_cfg.detect_yolov5_path; }
	QString get_yolov5_weight() const { return detectrack_cfg.detect_yolov5_weight; }
	QString get_python_path() const { return detectrack_cfg.detect_python_path; }
	QString get_raw_video_path() const { return raw_video_path; }
	QString get_raw_video_base() const { return basename; }

	QString get_detect_result_cache(int frameidx) const { return QDir(get_cache_path()).filePath(QString("%1_%2.txt").arg(get_raw_video_base(), QString::number(frameidx))); }
	QString get_track_result_cache() const { return QDir(get_cache_path()).filePath("tracking.txt"); }

	QString get_stitch_cache() const { return QDir(get_cache_path()).filePath("stitch"); }
	QString get_stitch_warpedimg_path(int frameidx) const { return QDir(get_stitch_cache()).filePath(QString("frame_%1.png").arg(QString::number(frameidx))); }
	QString get_stitch_cameraparams_path() const { return QDir(get_stitch_cache()).filePath("cameras.yaml"); }
	QString get_stitch_optflow_path(int frameidx) const { return QDir(get_stitch_cache()).filePath(QString("flow_%1.png").arg(QString::number(frameidx))); }
	QString get_stitch_background_path() const { return QDir(get_stitch_cache()).filePath("background.png"); }
	QString get_stitch_rois_path() const { return QDir(get_stitch_cache()).filePath("rois.txt"); }
	QString get_stitch_track_bboxes_path() const { return QDir(get_stitch_cache()).filePath("track_bboxes.txt"); }
	QString get_stitch_detect_bboxes_path() const { return QDir(get_stitch_cache()).filePath("detect_bboxes.txt"); }

private:
	QString cache_root;

	QString raw_video_path;
	QString basename;
	
	Ui::MLConfigWidget* config_widget_ui;
	QDockWidget* config_widget;
public:
	// variable config
	MLConfigDetectTrack detectrack_cfg;
	MLConfigStitcher stitcher_cfg;
};