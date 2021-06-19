#include "MLCacheStitching.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <qmessagebox.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <qvector.h>
#include <qmap.h>
#include <qdebug.h>
#include <qprogressdialog.h>
#include "SphericalSfm/MLProgressObserverBase.h"

bool MLCacheStitching::tryLoadBackground()
{
	const auto& pathcfg = MLConfigManager::get();
	const int framecount = MLDataManager::get().get_framecount();
	background = cv::imread(pathcfg.get_stitch_background_path().toStdString(), cv::IMREAD_UNCHANGED);
	if (background.empty())
		return false;
	return true;
}

bool MLCacheStitching::tryLoadWarppedFrames(MLProgressObserverBase* observer)
{
	const auto& pathcfg = MLConfigManager::get();
	const int framecount = MLDataManager::get().get_framecount();
	warped_frames.resize(framecount);
	if (observer) observer->beginStage("Load Stitching");

	for (int i = 0; i < framecount; ++i)
	{
		cv::Mat load = cv::imread(pathcfg.get_stitch_warpedimg_path(i).toStdString(), cv::IMREAD_UNCHANGED);
		if (load.empty() || load.channels() != 4)
		{
			warped_frames.resize(0);
			return false;
		}
		warped_frames[i] = load;
		if (observer) observer->setValue((float)i / framecount);
	}
	if (observer) observer->setValue(1.f);
	return true;
}

bool MLCacheStitching::tryLoadRois()
{
	const auto& pathcfg = MLConfigManager::get();
	const int framecount = MLDataManager::get().get_framecount();
	std::ifstream file(pathcfg.get_stitch_rois_path().toStdString());
	if (!file.is_open())
	{
		qWarning("MLCacheStitching::failed to open stitch meta file");
		return false;
	}
	file.ignore(1024, '\n');
	for (int i = 0; i < framecount; ++i)
	{
		if (!file)
		{
			qWarning("MLCacheStitching::meta file end early");
			return false;
		}
		int left, top, wid, hei;
		file >> left >> top >> wid >> hei;
		rois.emplace_back(left, top, wid, hei);
		if (!warped_frames.empty() && rois[i].size() != warped_frames[i].size())
		{
			qWarning("MLCacheStitching::meta file doesn't agree with the cached warped_frame");
			rois.resize(0);
			return false;
		}
	}
	file.close();
	update_global_roi();
	return true;
}

bool MLCacheStitching::saveBackground() const
{
	const auto& pathcfg = MLConfigManager::get();
	QString path = pathcfg.get_stitch_cache();
	if (!QDir().mkpath(path))
		qWarning("MLConfigManager::failed to create directory %s", qPrintable(path));
	// save background
	if (background.empty())
		return false;
	cv::imwrite(pathcfg.get_stitch_background_path().toStdString(), background);
	return true;
}

bool MLCacheStitching::saveWarppedFrames(MLProgressObserverBase* observer) const
{
	const auto& pathcfg = MLConfigManager::get();
	// save background
	if (warped_frames.empty() || warped_frames.size() != MLDataManager::get().get_framecount())
		return false;

	QString path = pathcfg.get_stitch_cache();
	if (!QDir().mkpath(path))
		qWarning("MLConfigManager::failed to create directory %s", qPrintable(path));

	if (observer) observer->beginStage("Save Stitching");
	for (int i = 0; i < warped_frames.size(); ++i)
	{
		cv::imwrite(pathcfg.get_stitch_warpedimg_path(i).toStdString(), warped_frames[i]);
		if (observer) observer->setValue((float)i / warped_frames.size());
	}
	if (observer) observer->setValue(1.f);
	return true;
}

bool MLCacheStitching::saveRois() const
{
	const auto& pathcfg = MLConfigManager::get();
	
	if (rois.empty() || rois.size() != MLDataManager::get().get_framecount())
		return false;

	QString path = pathcfg.get_stitch_cache();
	if (!QDir().mkpath(path))
		qWarning("MLConfigManager::failed to create directory %s", qPrintable(path));

	std::ofstream file(pathcfg.get_stitch_rois_path().toStdString());
	if (!file.is_open()) return false;

	file << "# roi.x, roi.y, roi.width, roi.height" << std::endl;
	for (const auto& roi : rois)
		file << roi.tl().x << " " << roi.tl().y << " " << roi.width << " " << roi.height << std::endl;
	file.close();
}

bool MLCacheStitching::isPrepared() const
{
	const int framecount = MLDataManager::get().get_framecount();
	return (!background.empty()
		&& warped_frames.size() == framecount
		&& rois.size() == framecount);
}
