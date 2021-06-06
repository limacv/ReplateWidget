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
		:plates_cache(&stitch_cache, &trajectories),
		flow_cache(stitch_cache.rois, stitch_cache.global_roi),
		dirty_flags({ false })
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

	// data coordinate convertion
	QRect imageRect(const QRectF& rectf) const;
	QMatrix imageScale() const;
	// convert rect from world coordinate to paint coordinate (if return normalized rect, the rect_painter is not needed)
	QRectF toPaintROI(const cv::Rect& rect_w, const QRect& rect_painter=QRect(), bool ret_norm=false) const;
	// convert rect from normalized coordinate to world coordinate
	cv::Rect toWorldROI(const QRectF& rect_norm) const;

	// get images
	cv::Mat4b getRoiofFrame(int frameidx, const QRectF& rectF) const;
	cv::Mat3b getRoiofBackground(const QRectF& rectF) const;
	cv::Mat4b getFlowImage(int i, QRectF rectF = QRect()) const;
	cv::Mat4b getForeground(int i, QRectF rectF, const cv::Mat1b mask = cv::Mat1b()) const;
	cv::Mat4b getForeground(int i, const QPainterPath& painterpath) const;
	cv::Mat4b getForeground(int i, const GRoiPtr& roi) const;
	QImage getBackgroundQImg() const;
	
	// query bounding boxes
	QVector<BBox*> queryBoxes(int frameidx, const QPointF& pt_norm, bool use_track=false) const;

	//QImage getForegroundQImg(int iFrame, QRect rect, int slow) const; // alias for loadOtherImageFg

	int VideoHeight() const { return stitch_cache.background.rows; }
	int VideoWidth() const { return stitch_cache.background.cols; }
	int VideoTop() const { return stitch_cache.global_roi.y; }
	int VideoLeft() const { return stitch_cache.global_roi.x; }

	// painting function
	// convert world coordinate to the paint coordinate
	void paintRawFrames(QPainter& painter, int frameidx) const;
	void paintWarpedFrames(QPainter& painter, int frameidx, bool paintbg = true, bool paintfg = true) const;
	void paintWorldTrackBoxes(QPainter& painter, int frameidx, bool paint_name = true, bool paint_traj = true) const;
	void paintWorldDetectBoxes(QPainter& painter, int frameidx, bool paint_name = true) const;
	void paintReplateFrame(QPainter& painter, int frameidx) const;

	// step1 <-> step2
	void initMasks();

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
	QVector<cv::Mat> replate_video;

	VideoConfig out_video_cfg;

public:
	
	enum DataIndex
	{
		RAW = 0, DETECT, TRACK, STITCH, FLOW, EFFECT, RENDER
	};
	std::array<bool, 7> dirty_flags;
	
	// the data become dirty when it's modified or dependency is modified
	// the data become clean when it's updated based on clean dependencies
	void set_dirty(DataIndex idx) { for (int i = idx; i < 7; ++i) dirty_flags[i] = true; }
	void set_clean(DataIndex idx) { dirty_flags[idx] = false; }
	bool is_prepared(DataIndex idx) { for (int i = 0; i < idx; ++i) if (dirty_flags[i]) return false; return true; }
	bool is_dirty(DataIndex idx) { return dirty_flags[idx]; }
};

