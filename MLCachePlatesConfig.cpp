#include "MLCachePlatesConfig.h"
#include "MLCacheStitching.h"
#include "MLCacheTrajectories.h"
#include "MLDataManager.h"

void MLCachePlatesConfig::initialize_cut(int outframecount)
{
	const int totalframecount = MLDataManager::get().get_framecount();
	framecount = outframecount;
	frameoffset = 0;
	for (auto traj = trajs_datap->objid2trajectories.constKeyValueBegin();
		traj != trajs_datap->objid2trajectories.constKeyValueEnd(); ++traj)
	{
		cut[traj->first] = QVector<int>();
		int a = traj->second.beginidx / outframecount,
			b = traj->second.endidx / outframecount + 1;
		for (int i = a; i <= b; ++i)
			cut[traj->first].push_back(i * framecount);
	}
}

void MLCachePlatesConfig::collectPlate(const float framepct, const ObjID& objid, QVector<BBox*>& boxes)
{
	const auto& onecut = cut[objid];
	const auto& traj = MLDataManager::get().trajectories.objid2trajectories[objid];
	for (int i = 0; i < onecut.size() - 1; ++i)
	{
		int clipcount = onecut[i + 1] - onecut[i];
		float clippct = framepct * clipcount + onecut[i];

		if (abs(clippct - roundf(clippct)) < 0.05)  // round to the nearest boxes
		{
			int idx = roundf(clippct);
			if (idx >= traj.beginidx && idx <= traj.endidx && traj[idx] != nullptr)
				boxes.push_back(traj[idx]);
		}
		else
		{
			// TODO: interpolate plates
			int idx = roundf(clippct);
			if (idx >= traj.beginidx && idx <= traj.endidx)
				boxes.push_back(traj[idx]);
		}
	}
}

void MLCachePlatesConfig::collectPlate(const float framepct, QVector<BBox*>& boxes)
{
	boxes.reserve(1000);
	for (auto it = cut.keyBegin(); it != cut.keyEnd(); ++it)
		collectPlate(framepct, *it, boxes);
}
