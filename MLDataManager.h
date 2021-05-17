#pragma once
#include <qimage.h>
#include <qvector.h>
#include <qstring.h>
#include <opencv2/core.hpp>
#include <array>
#include "MLCacheTrajectories.h"
#include "MLCacheStitching.h"
#include "MLCachePlatesConfig.h"

class MLDataManager
{
public:
	MLDataManager()
		:flag_israwok(false),
		flag_isstep1ok(false),
		flag_isstep2ok(false),
		flag_isstep3ok(false),
		plates_cache(&stitch_cache, &trajectories)
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
	VideoConfig raw_video_cfg;
	QVector<cv::Mat> raw_frames;

	MLCacheTrajectories trajectories;
	QVector<cv::Mat> masks;
	MLCacheStitching stitch_cache;

	MLCachePlatesConfig plates_cache;
	VideoConfig out_video_cfg;
	QVector<cv::Mat> replate_video;

private:
	bool flag_israwok;
	bool flag_isstep1ok;
	bool flag_isstep2ok;
	bool flag_isstep3ok;
};

