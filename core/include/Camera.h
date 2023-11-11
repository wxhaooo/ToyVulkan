/*
* Basic camera class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
private:
	float fov;
	float znear, zfar;

	void UpdateViewMatrix();
	
public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 position = glm::vec3();
	glm::vec4 viewPos = glm::vec4();

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
	
	float GetNearClip() const
	{ 
		return znear;
	}

	float GetFarClip() const
	{
		return zfar;
	}

	void SetRotationSpeed(float rotationSpeed)
	{
		this->rotationSpeed = rotationSpeed;
	}

	void SetMovementSpeed(float movementSpeed)
	{
		this->movementSpeed = movementSpeed;
	}

	bool Moving() const;
	void SetPerspective(float fov, float aspect, float znear, float zfar);
	void UpdateAspectRatio(float aspect);
	void SetPosition(glm::vec3 position);
	void SetRotation(glm::vec3 rotation);
	void Rotate(glm::vec3 delta);
	void SetTranslation(glm::vec3 translation);
	void Translate(glm::vec3 delta);
	void SetLookAt(glm::vec3 pos, glm::vec3 target);
	void Update(float deltaTime);

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool UpdatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime);
};