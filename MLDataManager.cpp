#include "MLDataManager.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>
#include "MLUtil.h"
#include "MLConfigManager.h"

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
