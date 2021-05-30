#include "MLCacheFlow.h"
#include "MLConfigManager.h"
#include "MLDataManager.h"
#include <opencv2/imgproc.hpp>

bool MLCacheFlow::tryLoadFlows()
{
	const auto& pathcfg = MLConfigManager::get();
	const int framecount = MLDataManager::get().get_framecount();
	flows.resize(framecount);
	for (int i = 0; i < framecount; ++i)
	{
		cv::Mat load = cv::imread(pathcfg.get_stitch_optflow_path(i).toStdString(), cv::IMREAD_UNCHANGED);
		if (load.empty() || load.channels() != 4)
		{
			flows.resize(0);
			return false;
		}
		flows[i] = load;
	}
	return true;
}

bool MLCacheFlow::saveFlows() const
{
	const auto& pathcfg = MLConfigManager::get();
	// save background
	if (flows.empty() || flows.size() != MLDataManager::get().get_framecount())
		return false;

	for (int i = 0; i < flows.size(); ++i)
		cv::imwrite(pathcfg.get_stitch_optflow_path(i).toStdString(), flows[i]);
	return true;
}

bool MLCacheFlow::isprepared() const
{
	const int framecount = MLDataManager::get().get_framecount();
	return (flows.size() == framecount);
}