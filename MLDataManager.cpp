#include "MLDataManager.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>
#include "MLUtil.h"
#include "MLConfigManager.h"
#include "GUtil.h"

void calForegroundBorder(cv::Mat& fg, const cv::Mat& bg, const cv::Mat& mask)
{
	CV_Assert(fg.channels() == 4 && bg.channels() == 3 && mask.channels() == 1);
	CV_Assert(fg.size() == bg.size());
	if (!mask.empty())
		CV_Assert(fg.size() == mask.size());

	for (int y = 0; y < fg.rows; ++y) {
		cv::Vec4b* v0 = fg.ptr<cv::Vec4b>(y);
		const cv::Vec3b* v1 = bg.ptr<cv::Vec3b>(y);
		const unsigned char* m = mask.empty() ? 0 : mask.ptr<uchar>(y);
		for (int x = 0; x < fg.cols; ++x) {
			if (m && m[x] == 0) {
				v0[x][3] = 0;
				continue;
			}

			// calculate distance
			const auto& a = v0[x];
			const auto& b = v1[x];
			int dis = abs(a[0] - b[0]) + abs(a[1] - b[1]) + abs(a[2] - b[2]);
			// dis filter
			int alpha = 0;
			if (dis < 10) alpha = 0;
			else if (dis < 25) alpha = (dis - 10) * 4;
			else if (dis < 35) alpha = (dis - 25) * 20 + 60;
			else alpha = 255;
			alpha = MIN(255, alpha);
			
			int w = qMax(qMin(20, fg.size().width / 10), 1);
			int h = qMax(qMin(20, fg.size().height / 10), 1);

			float border = 1.0;
			// add border filter
			float y_dis = qMin(qMin(y, fg.size().height - y - 1) / (float)h, 1.0f);
			float x_dis = qMin(qMin(x, fg.size().width - x - 1) / (float)w, 1.0f);
			if (x_dis < 1 || y_dis < 1) {
				border = qMin(x_dis, y_dis);
			}

			v0[x][3] = alpha * border;
		}
	}
}

bool MLDataManager::load_raw_video(const QString& path)
{
	auto cap = cv::VideoCapture(path.toStdString());
	if (!cap.isOpened())
	{
		qCritical("opencv failed to open the video %s", qPrintable(path));
		return false;
	}
	
	int framenum = cap.get(cv::CAP_PROP_FRAME_COUNT);
	raw_frames.reserve(framenum);
	cv::Mat frame;
	while (true)
	{
		if (!cap.read(frame))
			break;
		cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
		raw_frames.push_back(frame.clone());
	}
	qDebug("successfully load %d frames of raw video", raw_frames.size());

	raw_video_cfg.filepath = path;
	raw_video_cfg.framecount = raw_frames.size();
	raw_video_cfg.fourcc = cap.get(cv::CAP_PROP_FOURCC);
	raw_video_cfg.fps = cap.get(cv::CAP_PROP_FPS);
	raw_video_cfg.size = raw_frames[0].size();
	out_video_cfg = raw_video_cfg;
	// update globalconfig
	MLConfigManager::get().update_videopath(path);
	return true;
}

cv::Mat4b MLDataManager::getRoiofFrame(int frameidx, const QRectF& rectF) const
{
	if (!stitch_cache.isprepared()) return cv::Mat4b();
	const auto& video = stitch_cache.warped_frames;
	if (rectF.isValid())
	{
		const cv::Rect rect = GUtil::cvtRect(imageRect(rectF));
		cv::Mat4b img = stitch_cache.background(rect).clone();
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		cv::Rect intersection = frameroi & rect;
		if (!intersection.empty())
			stitch_cache.warped_frames[frameidx](intersection - frameroi.tl()).copyTo(img(intersection - rect.tl()));
		return img;
	}
	else
	{
		cv::Mat4b img = stitch_cache.background.clone();
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		stitch_cache.warped_frames[frameidx].copyTo(img(frameroi));
		return img;
	}
}

cv::Mat3b MLDataManager::getRoiofBackground(const QRectF& rectF) const
{
	if (!stitch_cache.isprepared()) return cv::Mat3b();
	cv::Mat3b ret;
	if (rectF.isValid())
	{
		const cv::Rect rect = GUtil::cvtRect(imageRect(rectF));
		cv::cvtColor(stitch_cache.background(rect), ret, cv::COLOR_RGBA2RGB);
	}
	else
		cv::cvtColor(stitch_cache.background, ret, cv::COLOR_RGBA2RGB);
	return ret;
}


cv::Mat4b MLDataManager::getFlowImage(int frameidx, QRectF rectF) const
{
	if (!flow_cache.isprepared()) return cv::Mat4b();
	const auto& video = flow_cache.flows;
	if (rectF.isValid())
	{
		const cv::Rect rect = GUtil::cvtRect(imageRect(rectF));
		cv::Mat img(rect.size(), CV_8UC4, cv::Scalar(128, 128, 128, 128));
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		cv::Rect intersection = frameroi & rect;
		if (!intersection.empty())
			video[frameidx](intersection - frameroi.tl()).copyTo(img(intersection - rect.tl()));
		return img;
	}
	else
	{
		cv::Mat img(stitch_cache.background.size(), CV_8UC4, cv::Scalar(128, 128, 128, 128));
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		video[frameidx].copyTo(img(frameroi));
		return img;
	}
}

cv::Mat4b MLDataManager::getForeground(int i, QRectF rectF, const cv::Mat1b mask) const
{
	cv::Mat4b fg = getRoiofFrame(i, rectF);
	cv::Mat3b bg = getRoiofBackground(rectF);
	calForegroundBorder(fg, bg, mask);
	return fg;
}

cv::Mat4b MLDataManager::getForeground(int i, const QPainterPath& painterpath) const
{
	QPainterPath pp = imageScale().map(painterpath);
	cv::Mat1b mask = GUtil::cvtPainterPath2Mask(pp);
	return getForeground(i, painterpath.boundingRect(), mask);
}

cv::Mat4b MLDataManager::getForeground(int i, const GRoiPtr& roi) const
{
	if (!roi->painter_path_.isEmpty())
		return getForeground(i, roi->painter_path_);
	else
		return getForeground(i, roi->rectF_);
}

QImage MLDataManager::getBackgroundQImg() const
{
	return MLUtil::mat2qimage(stitch_cache.background, QImage::Format_ARGB32_Premultiplied);
}

QRect MLDataManager::imageRect(const QRectF& rectf) const
{
	return imageScale().mapRect(rectf).toRect();
}

QMatrix MLDataManager::imageScale() const
{
	return QMatrix().scale(stitch_cache.background.cols, stitch_cache.background.rows);
}

QRectF MLDataManager::toPaintROI(const cv::Rect& rect_w, const QRect& viewport) const
{
	float top_norm = ((float)rect_w.y - stitch_cache.global_roi.y) / stitch_cache.global_roi.height,
		left_norm = ((float)rect_w.x - stitch_cache.global_roi.x) / stitch_cache.global_roi.width,
		wid_norm = (float)rect_w.width / stitch_cache.global_roi.width,
		hei_norm = (float)rect_w.height / stitch_cache.global_roi.height;
	return QRectF(left_norm * viewport.width(), top_norm * viewport.height(), wid_norm * viewport.width(), hei_norm * viewport.height());
}

void MLDataManager::paintWarpedFrames(QPainter& painter, int frameidx, bool paintbg, bool paintfg) const
{
	auto viewport = painter.viewport();
	if (paintbg)
		painter.drawImage(viewport, MLUtil::mat2qimage(stitch_cache.background, QImage::Format_ARGB32_Premultiplied));

	if (paintfg)
		painter.drawImage(
			toPaintROI(stitch_cache.rois[frameidx], viewport), 
			MLUtil::mat2qimage(stitch_cache.warped_frames[frameidx], QImage::Format_ARGB32_Premultiplied));
	return;
}

void MLDataManager::reinitMasks()
{
	const int framecount = get_framecount();
	masks.resize(framecount);
	for (int i = 0; i < framecount; ++i)
	{
		masks[i].create(raw_video_cfg.size, CV_8UC1);
		masks[i].setTo(255);
	}
}

bool MLDataManager::is_prepared(int step) const
{
	return true;
}
