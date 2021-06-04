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
	raw_frames.resize(0);
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

QVector<BBox*> MLDataManager::queryBoxes(int frameidx, const QPointF& pt_norm, bool use_track) const
{
	QVector<BBox*> outboxes; outboxes.reserve(4);
	cv::Point worldpt(pt_norm.x() * VideoWidth() + stitch_cache.global_roi.x, pt_norm.y() * VideoHeight() + stitch_cache.global_roi.y);
	if (use_track)
	{
		const auto& boxes = trajectories.frameidx2trackboxes[frameidx];
		for (const auto& pbox : boxes)
			if (pbox->rect_global.contains(worldpt))
				outboxes.push_back(pbox);
	}
	else
	{
		const auto& boxes = trajectories.frameidx2detectboxes[frameidx];
		for (const auto& pbox : boxes)
			if (pbox->rect_global.contains(worldpt))
				outboxes.push_back(pbox);
	}
	return outboxes;
}

QRect MLDataManager::imageRect(const QRectF& rectf) const
{
	return imageScale().mapRect(rectf).toRect();
}

QMatrix MLDataManager::imageScale() const
{
	return QMatrix().scale(stitch_cache.background.cols, stitch_cache.background.rows);
}

QRectF MLDataManager::toPaintROI(const cv::Rect& rect_w, const QRect& viewport, bool ret_norm) const
{
	float top_norm = ((float)rect_w.y - VideoTop()) / VideoHeight(),
		left_norm = ((float)rect_w.x - VideoLeft()) / VideoWidth(),
		wid_norm = (float)rect_w.width / VideoWidth(),
		hei_norm = (float)rect_w.height / VideoHeight();
	if (ret_norm)
		return QRectF(left_norm, top_norm, wid_norm, hei_norm);
	else
		return QRectF(left_norm * viewport.width(), top_norm * viewport.height(), wid_norm * viewport.width(), hei_norm * viewport.height());
}

cv::Rect MLDataManager::toWorldROI(const QRectF& rect_norm) const
{
	return cv::Rect(rect_norm.x() * VideoWidth() + VideoLeft(), 
		rect_norm.y() * VideoHeight() + VideoTop(),
		rect_norm.width() * VideoWidth(),
		rect_norm.height() * VideoHeight());
}

void MLDataManager::paintRawFrames(QPainter& painter, int frameidx) const
{
	painter.drawImage(painter.viewport(),
		MLUtil::mat2qimage(raw_frames[frameidx], QImage::Format_RGB888));
}

void MLDataManager::paintWarpedFrames(QPainter& painter, int frameidx, bool paintbg, bool paintfg) const
{
	auto viewport = painter.viewport();
	if (paintbg)
		painter.drawImage(viewport, getBackgroundQImg());

	if (paintfg)
		painter.drawImage(
			toPaintROI(stitch_cache.rois[frameidx], viewport), 
			MLUtil::mat2qimage(stitch_cache.warped_frames[frameidx], QImage::Format_ARGB32_Premultiplied));
	return;
}

void MLDataManager::paintWorldTrackBoxes(QPainter& painter, int frameidx, bool paint_name, bool paint_traj) const
{
	const auto& boxes = trajectories.frameidx2trackboxes[frameidx];
	const auto viewport = painter.viewport();
	const float scalex = (float)viewport.width() / VideoWidth();
	const float scaley = (float)viewport.height() / VideoHeight();

	/******************
	* paint boxes and names
	******************/
	for (auto it = boxes.constKeyValueBegin(); it != boxes.constKeyValueEnd(); ++it)
	{
		const auto& color = trajectories.getColor(it->first);
		const auto& box = it->second;
		const auto& paint_roi = toPaintROI(box->rect_global, viewport);

		// draw rectangles
		painter.setPen(QPen(color, 2));
		painter.drawRect(paint_roi);

		// draw names
		if (paint_name)
		{
			QString text;
			text = QString("%1_%2").arg(MLCacheTrajectories::classid2name[box->classid], QString::number(box->instanceid));

			painter.fillRect(
				(int)paint_roi.x() - 1,
				(int)paint_roi.y() - 10,
				text.size() * 7,
				10, color);
			painter.setPen(QColor(255, 255, 255));
			painter.drawText(
				(int)paint_roi.x(),
				(int)paint_roi.y() - 1, text);
		}
	}
	if (paint_traj)
	{
		const auto& trace = trajectories.objid2trajectories;
		const auto& framecount = get_framecount();
		for (auto it = trace.constKeyValueBegin(); it != trace.constKeyValueEnd(); ++it)
		{
			const auto& color = trajectories.getColor(it->first);
			const auto& boxes = it->second.boxes;
			QPointF lastpt(-999, -999);
			bool skip_flag = true;
			for (const auto& pbox : boxes)
			{
				if (pbox == nullptr || pbox->empty())
				{
					skip_flag = true;
					continue;
				}

				QPointF currpt = toPaintROI(pbox->rect_global, viewport).center();

				if (lastpt.x() < 0)
				{
					lastpt = currpt;
					skip_flag = false;
					continue;
				}
				float width = 4 - qAbs<float>(pbox->frameidx - frameidx) / 5;
				width = MAX(width, 0.2);
				auto style = skip_flag ? Qt::PenStyle::DotLine : Qt::PenStyle::SolidLine;
				painter.setPen(QPen(color, width, style));
				painter.drawLine(lastpt, currpt);
				lastpt = currpt;
				skip_flag = false;
			}
		}
	}
}

void MLDataManager::paintWorldDetectBoxes(QPainter& painter, int frameidx, bool paint_name) const
{
	const auto& boxes = trajectories.frameidx2detectboxes[frameidx];
	const auto viewport = painter.viewport();
	const float scalex = (float)viewport.width() / VideoWidth();
	const float scaley = (float)viewport.height() / VideoHeight();

	/******************
	* paint boxes and names
	******************/
	for (auto it = boxes.constBegin(); it != boxes.constEnd(); ++it)
	{
		const auto& box = *it;
		const auto& color = trajectories.getColor(ObjID(box->classid, box->instanceid));
		const auto& paint_roi = toPaintROI(box->rect_global, viewport);

		// draw rectangles
		painter.setPen(QPen(color, 2));
		painter.drawRect(paint_roi);

		// draw names
		if (paint_name)
		{
			QString text;
			text = MLCacheTrajectories::classid2name[box->classid];

			painter.fillRect(
				(int)paint_roi.x() - 1,
				(int)paint_roi.y() - 10,
				text.size() * 7,
				10, color);
			painter.setPen(QColor(255, 255, 255));
			painter.drawText(
				(int)paint_roi.x(),
				(int)paint_roi.y() - 1, text);
		}
	}
}

void MLDataManager::paintReplateFrame(QPainter& painter, int frameidx) const
{
	painter.drawImage(painter.viewport(), getBackgroundQImg());
	effect_manager_.drawEffects(painter, frameidx);
}

void MLDataManager::initMasks()
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
