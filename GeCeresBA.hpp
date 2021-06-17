#pragma once

#include <cmath>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <ceres/iteration_callback.h>

struct RayBundleError {
    RayBundleError(const double* p2d0, const double* ppxy0, const double* p2d1,
        const double* ppxy1)
    {
        const double* p2d[2] = { p2d0, p2d1 };
        const double* ppxy[2] = { ppxy0, ppxy1 };
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                _p2d[i][j] = p2d[i][j];
                _ppxy[i][j] = ppxy[i][j];
            }
        }
    }
    template <typename T>
    bool operator()(const T* const cam0, const T* const cam1, T* residuals) const
    {
        const T* const cam[2] = { cam0, cam1 };
        const T* const r[2] = { cam0 + 1, cam1 + 1 };
        T foc[2];
        T ppxy[2][2];
        T p3d[2][3];
        T p3d_r[2][3];
        for (int i = 0; i < 2; ++i)
        {
            foc[i] = cam[i][0];
            for (int j = 0; j < 2; ++j)
                ppxy[i][j] = T(_ppxy[i][j]);

            typedef Eigen::Matrix<T, 3, 3> MatrixType;
            MatrixType k = MatrixType::Identity();
            k(0, 0) = foc[i]; k(2, 0) = ppxy[i][0];
            k(1, 1) = foc[i]; k(2, 1) = ppxy[i][1];

            MatrixType kI = k.inverse();
            p3d[i][0] = kI(0, 0) * _p2d[i][0] + kI(1, 0) * _p2d[i][1] + kI(2, 0);
            p3d[i][1] = kI(0, 1) * _p2d[i][0] + kI(1, 1) * _p2d[i][1] + kI(2, 1);
            p3d[i][2] = kI(0, 2) * _p2d[i][0] + kI(1, 2) * _p2d[i][1] + kI(2, 2);

            ceres::AngleAxisRotatePoint(r[i], p3d[i], p3d_r[i]);

            // normalize vector
            T len = T(0);
            for (int j = 0; j < 3; ++j)
                len += p3d_r[i][j] * p3d_r[i][j];
            len = sqrt(len);
            for (int j = 0; j < 3; ++j)
                p3d_r[i][j] = p3d_r[i][j] / len;/**/
        }
        T _mult = sqrt(foc[0] * foc[1]);
        if (_mult < T(1000))
            _mult = T(1000);
        for (int i = 0; i < 3; ++i)
            residuals[i] = _mult * (p3d_r[0][i] - p3d_r[1][i]);
        return true;
    }

    static ceres::CostFunction* Create(const double* p2d0, const double* ppxy0,
        const double* p2d1, const double* ppxy1) {
        return (new ceres::AutoDiffCostFunction<RayBundleError, 3, 4, 4>(
            new RayBundleError(p2d0, ppxy0, p2d1, ppxy1)));
    }

    double _p2d[2][2];
    double _ppxy[2][2];
};