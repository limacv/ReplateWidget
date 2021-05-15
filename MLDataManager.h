#pragma once
#include <qimage.h>
#include <qvector.h>
#include <qstring.h>
#include <opencv2/core.hpp>
#include <array>
#include "MLCacheTrajectories.h"
#include "MLCacheStitching.h"

class MLDataManager
{
public:
	MLDataManager()
	{}
	~MLDataManager() {};
	static MLDataManager& get()
	{
		static MLDataManager global_manager;
		return global_manager;
	}

	// process raw
	bool load_raw_video(const QString& path);
	int get_framecount() const { return raw_frames.size(); }

	// step1 <-> step2
	void reinitMasks();

	bool is_prepared(int step) const;

public:
	cv::Size raw_frame_size;
	QString video_filename;
	QVector<cv::Mat> raw_frames;

	MLCacheTrajectories trajectories;
	QVector<cv::Mat> masks;
	MLCacheStitching stitch_cache;
};

