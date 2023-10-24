#pragma once
#include<eigen3/Eigen/Dense>
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

	//ref:http://eecs.qmul.ac.uk/~gslabaugh/publications/euler.pdf
	inline Eigen::Vector3f GetEulerAngleFromRotationMatrix(Eigen::Matrix3f mat)
	{
		Eigen::Vector3f ret;

		float theta0 = 0.0f, theta1 = 0.0f;
		float psi0 = 0.0f, psi1 = 0.0f;
		float phi0 = 0.0f, phi1 = 0.0f;

		if (!IsNearEqual(std::abs(mat(2, 0)), 1.0f))
		{
			theta0 = -asin(mat(2, 0));
			theta1 = EIGEN_PI - theta0;

			psi0 = atan2(mat(2, 1) / cos(theta0), mat(2, 2) / cos(theta0));
			psi1 = atan2(mat(2, 1) / cos(theta1), mat(2, 2) / cos(theta1));

			phi0 = atan2(mat(1, 0) / cos(theta0), mat(0, 0) / cos(theta0));
			phi1 = atan2(mat(1, 0) / cos(theta1), mat(0, 0) / cos(theta1));
		}
		else
		{
			phi0 = 0.0f;
			if (IsNearEqual(mat(2, 0), -1.0f))
			{
				theta0 = EIGEN_PI / 2.0f;
				psi0 = atan2(mat(0, 1), mat(0, 2));
			}
			else
			{
				theta0 = -EIGEN_PI / 2.0f;
				psi0 = atan2(-mat(0, 1), -mat(0, 2));
			}
		}

		ret.x() = fmodf(Radius2Degree(psi0), 180.0f);
		ret.y() = fmodf(Radius2Degree(theta0), 180.0f);
		ret.z() = fmodf(Radius2Degree(phi0), 180.0f);

		return ret;
	}

	inline Eigen::Vector3f GetEulerAngleFromRotationMatrix(glm::mat4 mat)
	{
		Eigen::Matrix3f tmp;

		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				tmp(i, j) = mat[j][i];

		// view to world matrix
		tmp.transposeInPlace();

		return GetEulerAngleFromRotationMatrix(tmp);
	}
	
}


