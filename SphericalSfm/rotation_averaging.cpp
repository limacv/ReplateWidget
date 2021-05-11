
#include <Eigen/Dense>
#include "so3.hpp"
#include "rotation_averaging.h"

#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <ceres/loss_function.h>
#include <ceres/autodiff_cost_function.h>

namespace Ssfm {

    struct RotationError
    {
        RotationError( const Eigen::Matrix3d &_meas ) : meas(_meas) { }
        
        template <typename T>
        bool operator()(const T* const r0,
                        const T* const r1,
                        T* residuals) const
        {
            Eigen::Matrix<T,3,3> R, R0, R1;
            for ( int i = 0; i < 3; i++ )
                for ( int j = 0; j < 3; j++ )
                    R(i,j) = T(meas(i,j));
            ceres::AngleAxisToRotationMatrix( r0, R0.data() );
            ceres::AngleAxisToRotationMatrix( r1, R1.data() );
            
            Eigen::Matrix<T,3,3> cycle = (R1 * R0.transpose()) * R.transpose();
            ceres::RotationMatrixToAngleAxis( cycle.data(), residuals );

            return true;
        }
        
        Eigen::Matrix3d meas;
    };

    void optimize_rotations( std::vector<Eigen::Matrix3d> &rotations, const std::vector<RelativeRotation> &relative_rotations )
    {
        std::vector<Eigen::Vector3d> data(rotations.size());
        for ( int i = 0; i < rotations.size(); i++ ) data[i] = so3ln(rotations[i]);
        
        ceres::Problem problem;
        ceres::LossFunction* loss_function = new ceres::SoftLOneLoss(0.03);
        for ( int i = 0; i < relative_rotations.size(); i++ )
        {
            //std::cout << relative_rotations[i].index1 << "\n";
            //std::cout << relative_rotations[i].index0 << "\n";
            //std::cout << relative_rotations[i].R << "\n";
            RotationError *error = new RotationError(relative_rotations[i].R);
            ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<RotationError, 3, 3, 3>(error);
            problem.AddResidualBlock(cost_function,
                loss_function,
                data[relative_rotations[i].index0].data(),
                data[relative_rotations[i].index1].data()
            );
            
        }
     
        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.minimizer_progress_to_stdout = true;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        //std::cout << summary.FullReport() << "\n";
        if ( summary.termination_type == ceres::FAILURE )
        {
            std::cout << "error: ceres failed.\n";
            exit(1);
        }
        
        for ( int i = 0; i < rotations.size(); i++ ) rotations[i] = so3exp(data[i]);
    }

    /// <summary>
    /// return averaged rotations
    /// i.e. after return, rotations[0] = absrot0, rotations[-1] = absrot1, other rotations are 
    /// interpolated based on minimize rotation error
    /// </summary>
    /// <param name="rotations">inout vector, input the relative rotation on rotations[0] and rotations[0] = [0, 0, 0]\
            return absrotation</param>
    /// <param name="absrot0">rotations[0]</param>
    /// <param name="absrot1">rotations[-1]</param>
    void optimize_rotations_my(std::vector<Eigen::Vector3d>& rotations, 
        const Eigen::Vector3d& absrot0, const Eigen::Vector3d& absrot1)
    {
        if (rotations.size() <= 2) return;
        ceres::Problem problem;
        ceres::LossFunction* loss_function = new ceres::SoftLOneLoss(0.03);
        for (int i = 1; i < rotations.size(); i++)
        {
            auto rot0 = so3exp(rotations[i - 1]);
            auto rot1 = so3exp(rotations[i]);
            RotationError* error = new RotationError(rot1 * rot0.transpose());
            ceres::CostFunction* cost_function = new ceres::AutoDiffCostFunction<RotationError, 3, 3, 3>(error);
            problem.AddResidualBlock(cost_function,
                loss_function,
                rotations[i - 1].data(),
                rotations[i].data()
            );
        }
        
        // MA Li: is this correct?
        rotations[0] = absrot0;
        rotations[rotations.size() - 1] = absrot1;
        problem.SetParameterBlockConstant(rotations[0].data());
        problem.SetParameterBlockConstant(rotations[rotations.size() - 1].data());

        ceres::Solver::Options options;
        options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
        options.minimizer_progress_to_stdout = true;
        ceres::Solver::Summary summary;
        ceres::Solve(options, &problem, &summary);
        if (summary.termination_type == ceres::FAILURE)
        {
            std::cout << "error: ceres failed.\n";
            exit(1);
        }
    }

}
