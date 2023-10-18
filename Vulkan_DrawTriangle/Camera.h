#pragma once
#include<eigen3/Eigen/Dense>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "EigenUtils.h"
#include "MathUtils.h"

class Camera
{
private:
	float fov;
	float znear, zfar;

	void updateViewMatrix()
	{
		glm::mat4 rotM = glm::mat4(1.0f);
		glm::mat4 transM;

		rotM = glm::rotate(rotM, glm::radians(rotation.x() * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.y()), glm::vec3(0.0f, 1.0f, 0.0f));
		rotM = glm::rotate(rotM, glm::radians(rotation.z()), glm::vec3(0.0f, 0.0f, 1.0f));

		glm::vec3 translation = glm::vec3(position.x(), position.y(), position.z());
		if (flipY) {
			translation.y *= -1.0f;
		}
		transM = glm::translate(glm::mat4(1.0f), translation);

		if (type == CameraType::firstperson)
		{
			matrices.view = rotM * transM;
		}
		else
		{
			matrices.view = transM * rotM;
		}

		viewPos = Eigen::Vector4f(-position.x(), position.y(), -position.z(), 1.0f);

		updated = true;
	}

public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	Eigen::Vector3f rotation = Eigen::Vector3f::Zero();
	Eigen::Vector3f position = Eigen::Vector3f::Zero();
	Eigen::Vector4f viewPos = Eigen::Vector4f::Zero();

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	bool updated = false;
	bool flipY = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	float getNearClip() {
		return znear;
	}

	float getFarClip() {
		return zfar;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		if (flipY) {
			matrices.perspective[1][1] *= -1.0f;
		}
	}

	void updateAspectRatio(float aspect)
	{
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
		if (flipY) {
			matrices.perspective[1][1] *= -1.0f;
		}
	}

	void setPosition(glm::vec3 position)
	{
		this->position = Eigen::Vector3f(position.x, position.y, position.z);
		updateViewMatrix();
	}

	void setLookAt(glm::vec3 pos, glm::vec3 target)
	{
		glm::vec3 defaultUp(0.0f, 0.0f, 1.0f);
		matrices.view = glm::lookAt(pos, target, defaultUp);

		this->position = Eigen::Vector3f(matrices.view[3][0], matrices.view[3][1], matrices.view[3][2]);
		// ref: Introduction to 3D Game PROGRAMMING With DirectX 12,section 5.6.2
		this->rotation = MathUtils::GetEulerAngleFromRotationMatrix(matrices.view);
		updateViewMatrix();
	}

	void setRotation(glm::vec3 rotation)
	{
		this->rotation = Eigen::Vector3f(rotation.x, rotation.y, rotation.z);
		updateViewMatrix();
	}

	void rotate(glm::vec3 delta)
	{
		this->rotation += Eigen::Vector3f(delta.x, delta.y, delta.z);
		updateViewMatrix();
	}

	void setTranslation(glm::vec3 translation)
	{
		this->position = Eigen::Vector3f(translation.x, translation.y, translation.z);
		updateViewMatrix();
	}

	void translate(glm::vec3 delta)
	{
		this->position += Eigen::Vector3f(delta.x, delta.y, delta.z);
		updateViewMatrix();
	}

	void setRotationSpeed(float rotationSpeed)
	{
		this->rotationSpeed = rotationSpeed;
	}

	void setMovementSpeed(float movementSpeed)
	{
		this->movementSpeed = movementSpeed;
	}

	glm::vec3 Front()
	{
		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(rotation.x())) * sin(glm::radians(rotation.y()));
		camFront.y = sin(glm::radians(rotation.x()));
		camFront.z = cos(glm::radians(rotation.x())) * cos(glm::radians(rotation.y()));
		camFront = glm::normalize(camFront);
		return camFront;
	}

	void update(float deltaTime)
	{
		updated = false;
		if (type == CameraType::firstperson)
		{
			if (moving())
			{
				glm::vec3 camFront;
				camFront.x = -cos(glm::radians(rotation.x())) * sin(glm::radians(rotation.y()));
				camFront.y = sin(glm::radians(rotation.x()));
				camFront.z = cos(glm::radians(rotation.x())) * cos(glm::radians(rotation.y()));
				camFront = glm::normalize(camFront);

				float moveSpeed = deltaTime * movementSpeed;

				if (keys.up)
					position += EigenUtils::GLMV3ToEigen(camFront * moveSpeed);
				if (keys.down)
					position -= EigenUtils::GLMV3ToEigen(camFront * moveSpeed);
				if (keys.left)
					position -= EigenUtils::GLMV3ToEigen(glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed);
				if (keys.right)
					position += EigenUtils::GLMV3ToEigen(glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed);
			}
		}
		updateViewMatrix();
	}
};
