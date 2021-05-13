#include "MLStep2Data.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <qmessagebox.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>

bool MLStep2Data::tryLoadStitchingFromFiles()
{
	const auto& pathcfg = MLConfigManager::get();
	const int framecount = MLDataManager::get().get_framecount();

	warped_frames.resize(framecount);

	background = cv::imread(pathcfg.get_stitch_background_path().toStdString(), cv::IMREAD_UNCHANGED);
	if (background.empty())
		return false;

	std::ifstream file(pathcfg.get_stitch_meta_path().toStdString());
	if (!file.is_open())
	{
		qWarning("MLStep2Data::failed to open stitch meta file");
		return false;
	}
	for (int i = 0; i < framecount; ++i)
	{
		cv::Mat load = cv::imread(pathcfg.get_stitch_aligned_path(i).toStdString(), cv::IMREAD_UNCHANGED);
		if (load.empty() || load.channels() != 4)
			return false;
		warped_frames[i] = load;
		if (file.eof())
		{
			qWarning("MLStep2Data::meta file end early");
			return false;
		}
		int left, top, wid, hei;
		file >> left >> top >> wid >> hei;
		rois.emplace_back(left, top, wid, hei);
		if (wid != load.cols || hei != load.rows)
		{
			qWarning("MLStep2Data::meta file doesn't agree with the cached warped_frame");
			return false;
		}
	}
	file.close();
	update_global_roi();
}

bool MLStep2Data::saveToFiles() const
{
    const auto& pathcfg = MLConfigManager::get();
	// save background
	if (background.empty()
		|| warped_frames.empty()
		|| rois.empty()
		|| warped_frames.size() != rois.size())
		return false;

	cv::imwrite(pathcfg.get_stitch_background_path().toStdString(), background);
	for (int i = 0; i < warped_frames.size(); ++i)
	{
		cv::imwrite(pathcfg.get_stitch_aligned_path(i).toStdString(), warped_frames[i]);
	}

	std::ofstream file(pathcfg.get_stitch_meta_path().toStdString());
	for (const auto& roi : rois)
		file << roi.tl().x << " " << roi.tl().y << " " << roi.width << " " << roi.height << std::endl;
	file.close();
    return true;
}

bool MLStep2Data::isprepared() const
{
	const int framecount = MLDataManager::get().get_framecount();
	return (!background.empty()
		&& warped_frames.size() == framecount
		&& rois.size() == framecount);
}
