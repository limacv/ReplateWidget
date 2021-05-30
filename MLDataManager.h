#pragma once
#include <qimage.h>
#include <qvector.h>
#include <qstring.h>
#include <opencv2/core.hpp>
#include <array>
#include <qrect.h>
#include "MLCacheTrajectories.h"
#include "MLCacheStitching.h"
#include "MLCachePlatesConfig.h"
#include "MLCacheFlow.h"
#include "GEffectManager.h"

class MLDataManager
{
private:
	MLDataManager()
		:flag_israwok(false),
		flag_isstep1ok(false),
		flag_isstep2ok(false),
		flag_isstep3ok(false),
		plates_cache(&stitch_cache, &trajectories),
		flow_cache(stitch_cache.rois, stitch_cache.global_roi)
	{}
	MLDataManager(const MLDataManager& dm) = delete;

public:

	~MLDataManager() {};
	static MLDataManager& get()
	{
		static MLDataManager global_manager;
		return global_manager;
	}

	// raw data
	bool load_raw_video(const QString& path);
	int get_framecount() const { return raw_frames.size(); }

	// getImage stitched data coordinate convertion
	QRect imageRect(const QRectF& rectf) const;
	QMatrix imageScale() const;

	// get images
	cv::Mat4b getRoiofFrame(int frameidx, const QRectF& rectF) const;
	cv::Mat3b getRoiofBackground(const QRectF& rectF) const;
	cv::Mat4b getFlowImage(int i, QRectF rectF = QRect()) const;
	cv::Mat4b getForeground(int i, QRectF rectF, const cv::Mat1b mask = cv::Mat1b()) const;
	cv::Mat4b getForeground(int i, const QPainterPath& painterpath) const;
	cv::Mat4b getForeground(int i, const GRoiPtr& roi) const;
	QImage getBackgroundQImg() const;
	
	//QImage getForegroundQImg(int iFrame, QRect rect, int slow) const; // alias for loadOtherImageFg

	int VideoHeight() const { return stitch_cache.background.rows; }
	int VideoWidth() const { return stitch_cache.background.cols; }

	// painting function
	QRectF toPaintROI(const cv::Rect& rect_w, const QRect& rect_painter) const;
	void paintWarpedFrames(QPainter& painter, int frameidx, bool paintbg = true, bool paintfg = true) const;

	// step1 <-> step2
	void reinitMasks();

	bool is_prepared(int step) const;

public:
	VideoConfig raw_video_cfg;
	QVector<cv::Mat> raw_frames;

	MLCacheTrajectories trajectories;
	QVector<cv::Mat> masks;
	MLCacheStitching stitch_cache;
	MLCacheFlow flow_cache;

	GEffectManager effect_manager_;
	MLCachePlatesConfig plates_cache;
	VideoConfig out_video_cfg;
	QVector<cv::Mat> replate_video;

private:
	bool flag_israwok;
	bool flag_isstep1ok;
	bool flag_isstep2ok;
	bool flag_isstep3ok;
};

