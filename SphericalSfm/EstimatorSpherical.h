
#pragma once

#include <Eigen/Core>
#include "EstimatorBase.h"

namespace Ssfm {
    struct SphericalEstimator : public Estimator
    {
        Eigen::Matrix3d Esolns[4];
        
        Eigen::Matrix3d E;
        
        int sampleSize();
        double score( RayPairList::iterator it );
        void chooseSolution( int soln );
        int compute( RayPairList::iterator begin, RayPairList::iterator end );
        bool canRefine();
        
        void decomposeE( bool inward, Eigen::Vector3d &r, Eigen::Vector3d &t );
    };
}
