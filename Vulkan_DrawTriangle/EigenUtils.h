#pragma once

#include<eigen3/Eigen/Dense>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace EigenUtils
{
	inline Eigen::Vector3f GLMV3ToEigen(glm::vec3 in)
	{
		return Eigen::Vector3f(in.x, in.y, in.z);
	}
}