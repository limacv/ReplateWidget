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

void calForegroundBorderMy(cv::Mat& fg, const cv::Mat& bg, const cv::Mat& mask)
{
	CV_Assert(fg.channels() == 4 && bg.channels() == 3 && mask.channels() == 1);
	CV_Assert(fg.size() == bg.size());

	cv::Mat diff(fg.size(), CV_32FC1);

	diff.forEach<float>([&](float& pix, const int* pos) {
			cv::Vec4b fpix = fg.at<cv::Vec4b>(pos[0], pos[1]);
			cv::Vec3b bpix = bg.at<cv::Vec3b>(pos[0], pos[1]);
			//cv::Vec3b fpix3(fpix[0], fpix[1], fpix[2]);
			//cv::Vec3b flab, blab;
			//cv::cvtColor(fpix3, flab, cv::COLOR_BGR2Lab);
			//cv::cvtColor(bpix, blab, cv::COLOR_BGR2Lab);
			//MLUtil::CIEDE2000::LAB lab1({ (float)flab[0], (float)flab[1], (float)flab[2] });
			//MLUtil::CIEDE2000::LAB lab2({ (float)flab[0], (float)flab[1], (float)flab[2] });
			//double error = MLUtil::CIEDE2000::CIEDE2000(lab1, lab2);
			int err = 0;
			for (int i = 0; i < 3; ++i)
				err += (fpix[i] > bpix[i] ? fpix[i] - bpix[i] : bpix[i] - fpix[i]);

			if (err < 20) err = 0;
			else if (err < 45) err = (err - 20) * 5;
			else if (err < 65) err = (err - 45) * 15 + 25 * 5;
			else err = 255;
			pix = err * fpix[3] / 255;
			err = MIN(255, err);
		});

	diff.convertTo(diff, CV_8UC1);
	if (diff.cols > 45 && diff.rows > 45)
	{
		cv::Mat_<uchar> kernel(3, 3); kernel << 0, 1, 0, 1, 1, 1, 0, 1, 0;
		cv::erode(diff, diff, kernel);
		cv::dilate(diff, diff, kernel, cv::Point(-1, -1), 2);
	}
	auto in_border = [&](double x, double y) -> bool
	{
		x /= fg.cols; y /= fg.rows;
		return !(x > 0.15 && x < 0.85 && y > 0.15 && y < 0.85);
	};

	do
	{
		if (!MLConfigManager::get().border_elimination)
			break;

		cv::Mat ma = diff > 150;
		cv::erode(ma, ma, cv::Mat());
		cv::Mat connected, centers;
		cv::connectedComponentsWithStats(ma, ma, connected, centers);
		for (int i = 1; i < connected.rows; ++i)
		{
			auto p = static_cast<double*>(centers.ptr<double>(i));
			//auto p1 = static_cast<int*>(connected.ptr<int>(i));
			if (in_border(p[0], p[1]))
			{
				cv::Mat in_ma = ma == i;
				cv::dilate(in_ma, in_ma, cv::Mat(), cv::Point(-1, -1), 2);
				cv::GaussianBlur(in_ma, in_ma, cv::Size(5, 5), 2, 2, cv::BORDER_REPLICATE);
				cv::subtract(diff, in_ma, diff);
			}
		}
	} while (false);
	
	MLUtil::generateFeatherBorder(diff, 0.1);
	cv::cvtColor(fg, fg, cv::COLOR_BGRA2BGR);
	cv::merge(std::vector<cv::Mat>({ fg, diff }), fg);
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
	double fourcc = cap.get(cv::CAP_PROP_FOURCC);
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

	clear();
	return true;
}

cv::Mat4b MLDataManager::getRoiofFrame(int frameidx, const QRectF& rectF) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	return getRoiofFrame(frameidx, toCropROI(rectF));
}

cv::Mat4b MLDataManager::getRoiofFrame(int frameidx, const QRect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	return getRoiofFrame(frameidx, GUtil::cvtRect(rect));
}

cv::Mat4b MLDataManager::getRoiofFrame(int frameidx, const cv::Rect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	const auto& video = stitch_cache.warped_frames;
	if (rect.area() > 0)
	{
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

cv::Mat4b MLDataManager::getRoiofForeground(int frameidx, const QRectF& rectF) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	return getRoiofForeground(frameidx, toCropROI(rectF));
}

cv::Mat4b MLDataManager::getRoiofForeground(int frameidx, const QRect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	return getRoiofForeground(frameidx, GUtil::cvtRect(rect));
}

cv::Mat4b MLDataManager::getRoiofForeground(int frameidx, const cv::Rect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat4b();
	const auto& video = stitch_cache.warped_frames;
	if (rect.area() > 0)
	{
		cv::Mat4b img(rect.size(), { 0, 0, 0, 0 });
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		cv::Rect intersection = frameroi & rect;
		if (!intersection.empty())
			stitch_cache.warped_frames[frameidx](intersection - frameroi.tl()).copyTo(img(intersection - rect.tl()));
		return img;
	}
	else
	{
		cv::Mat4b img(stitch_cache.background.size(), { 0, 0, 0, 0 });
		cv::Rect frameroi = stitch_cache.rois[frameidx] - stitch_cache.global_roi.tl();
		stitch_cache.warped_frames[frameidx].copyTo(img(frameroi));
		return img;
	}
}

cv::Mat3b MLDataManager::getRoiofBackground(const QRectF& rectF) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat3b();
	return getRoiofBackground(toCropROI(rectF));
}

cv::Mat3b MLDataManager::getRoiofBackground(const QRect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat3b();
	return getRoiofBackground(GUtil::cvtRect(rect));
}

cv::Mat3b MLDataManager::getRoiofBackground(const cv::Rect& rect) const
{
	if (!stitch_cache.isPrepared()) return cv::Mat3b();
	cv::Mat3b ret;
	if (rect.area() > 0)
	{
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
		const cv::Rect rect = toCropROI(rectF);
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

cv::Mat4b MLDataManager::getMattedRoi(int i, QRectF rectF, const cv::Mat1b mask) const
{
	cv::Mat4b fg = getRoiofFrame(i, rectF);
	cv::Mat3b bg = getRoiofBackground(rectF);
	//calForegroundBorder(fg, bg, mask);
	calForegroundBorderMy(fg, bg, mask);
	return fg;
}

cv::Mat4b MLDataManager::getMattedRoi(int i, const QPainterPath& painterpath) const
{
	QPainterPath pp = imageScale().map(painterpath);
	cv::Mat1b mask = GUtil::cvtPainterPath2Mask(pp);
	return getMattedRoi(i, painterpath.boundingRect(), mask);
}

cv::Mat4b MLDataManager::getMattedRoi(int i, const GRoiPtr& roi) const
{
	if (!roi->painter_path_.isEmpty())
		return getMattedRoi(i, roi->painter_path_);
	else
		return getMattedRoi(i, roi->rectF_);
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

cv::Rect MLDataManager::toCropROI(const QRectF& rect_norm) const
{
	return cv::Rect(rect_norm.x() * VideoWidth(),
		rect_norm.y() * VideoHeight(),
		rect_norm.width() * VideoWidth(),
		rect_norm.height() * VideoHeight());
}

QRectF MLDataManager::toNormROI(const cv::Rect& rect_crop) const
{
	float top_norm = (float)rect_crop.y / VideoHeight(),
		left_norm = (float)rect_crop.x / VideoWidth(),
		wid_norm = (float)rect_crop.width / VideoWidth(),
		hei_norm = (float)rect_crop.height / VideoHeight();

	return QRectF(left_norm, top_norm, wid_norm, hei_norm);
}

void MLDataManager::paintManualMask(QPainter& painter) const
{
	auto viewport = painter.viewport();
	const auto rects = get_processed_manual_masks(viewport.width(), viewport.height());
	for (const auto& rect : rects)
		painter.drawRect(rect);
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
	* paintVisualize boxes and names
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
	* paintVisualize boxes and names
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

void MLDataManager::clear()
{
	// clear all the preprocess cache except raw data
	manual_masks.resize(1);
	trajectories.clear();
	stitch_cache.clear();
	effect_manager_.clear();
	replate_video.clear();
	out_video_cfg.clear();
}

vector<QRectF> MLDataManager::get_processed_manual_masks(int target_wid, int target_hei) const
{
	vector<QRectF> rects;

	QMatrix scale; scale.scale(target_wid, target_hei);
	if (!manual_masks.empty())
	{
		QRectF rect = manual_masks[0];
		rects.push_back(scale.mapRect(QRectF(QPointF(0, 0), rect.bottomLeft())));
		rects.push_back(scale.mapRect(QRectF(QPointF(0, 1), rect.bottomRight())));
		rects.push_back(scale.mapRect(QRectF(QPointF(1, 1), rect.topRight())));
		rects.push_back(scale.mapRect(QRectF(QPointF(1, 0), rect.topLeft())));
		for (int i = 1; i < manual_masks.size(); ++i)
		{
			rect = manual_masks[i];
			rects.push_back(scale.mapRect(rect));
		}
	}
	return rects;
}

bool MLDataManager::is_prepared(int step) const
{
	return true;
}
