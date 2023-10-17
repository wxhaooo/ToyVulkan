#pragma once
#include<eigen3/Eigen/Dense>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace MathUtils
{
	inline bool IsNearEqual(float a, float b, float eps = 1e-4)
	{
		return std::abs(a - b) < eps;
	}

	inline float Radius2Degree(float radius)
	{
		return radius * (180.0f / EIGEN_PI);
	}

	inline float Degree2Radius(float degree)
	{
		return degree * (EIGEN_PI / 180.0f);
	}

	inline Eigen::Vector3f GetEulerAngleFromRotationMatrix(Eigen::Matrix3f mat)
	{
		Eigen::Vector3f ret;

		if (!IsNearEqual(std::abs(mat(2, 0)), 1.0f))
		{
			float theta0 = -asin(mat(2, 0));
			float theta1 = EIGEN_PI - theta0;

			float psi0 = atan2(mat(2, 1) / cos(theta0), mat(2, 2) / cos(theta0));
			float psi1 = atan2(mat(2, 1) / cos(theta1), mat(2, 2) / cos(theta1));

			float phi0 = atan2(mat(1, 0) / cos(theta0), mat(0, 0) / cos(theta0));
			float phi1 = atan2(mat(1, 0) / cos(theta1), mat(0, 0) / cos(theta1));

			ret.x() = theta0;
			ret.y() = psi0;
			ret.z() = phi0;
		}
		else
		{
			ret.z() = 0.0f;
			if (IsNearEqual(mat(2, 0), -1.0f))
			{
				ret.x() = EIGEN_PI / 2.0f;
				ret.y() = atan2(mat(0, 1), mat(0, 2));
			}
			else
			{
				ret.x() = -EIGEN_PI / 2.0f;
				ret.y() = atan2(-mat(0, 1), -mat(0, 2));
			}
		}

		ret.x() = Radius2Degree(ret.x());
		ret.y() = Radius2Degree(ret.y());
		ret.z() = Radius2Degree(ret.z());

		return ret;
	}

	inline Eigen::Vector3f GetEulerAngleFromRotationMatrix(glm::mat4 mat)
	{
		Eigen::Matrix3f tmp;

		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				tmp(i, j) = mat[i][j];

		return GetEulerAngleFromRotationMatrix(tmp);
	}

	
}


