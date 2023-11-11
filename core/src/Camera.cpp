#include <Camera.h>

void Camera::UpdateViewMatrix()
{
    glm::mat4 rotM = glm::mat4(1.0f);
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 translation = position;
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

    viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

    updated = true;
}

bool Camera::Moving() const
{
    return keys.left || keys.right || keys.up || keys.down;
}

void Camera::SetPerspective(float fov, float aspect, float znear, float zfar)
{
    this->fov = fov;
    this->znear = znear;
    this->zfar = zfar;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    if (flipY) {
        matrices.perspective[1][1] *= -1.0f;
    }
}

void Camera::UpdateAspectRatio(float aspect)
{
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    if (flipY) {
        matrices.perspective[1][1] *= -1.0f;
    }
}

void Camera::SetPosition(glm::vec3 position)
{
    this->position = position;
    UpdateViewMatrix();
}

void Camera::SetRotation(glm::vec3 rotation)
{
    this->rotation = rotation;
    UpdateViewMatrix();
}

void Camera::Rotate(glm::vec3 delta)
{
    this->rotation += delta;
    UpdateViewMatrix();
}

void Camera::SetTranslation(glm::vec3 translation)
{
    this->position = translation;
    UpdateViewMatrix();
}

void Camera::Translate(glm::vec3 delta)
{
    this->position += delta;
    UpdateViewMatrix();
}

void Camera::Update(float deltaTime)
{
    updated = false;
    if (type == CameraType::firstperson)
    {
        if (Moving())
        {
            glm::vec3 camFront;
            camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            camFront.y = sin(glm::radians(rotation.x));
            camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            camFront = glm::normalize(camFront);

            float moveSpeed = deltaTime * movementSpeed;

            if (keys.up)
                position += camFront * moveSpeed;
            if (keys.down)
                position -= camFront * moveSpeed;
            if (keys.left)
                position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
            if (keys.right)
                position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        }
    }
    UpdateViewMatrix();
}

bool Camera::UpdatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
{
    bool retVal = false;

    if (type == CameraType::firstperson)
    {
        // Use the common console thumbstick layout		
        // Left = view, right = move

        const float deadZone = 0.0015f;
        const float range = 1.0f - deadZone;

        glm::vec3 camFront;
        camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
        camFront.y = sin(glm::radians(rotation.x));
        camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
        camFront = glm::normalize(camFront);

        float moveSpeed = deltaTime * movementSpeed * 2.0f;
        float rotSpeed = deltaTime * rotationSpeed * 50.0f;
			 
        // Move
        if (fabsf(axisLeft.y) > deadZone)
        {
            float pos = (fabsf(axisLeft.y) - deadZone) / range;
            position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
            retVal = true;
        }
        if (fabsf(axisLeft.x) > deadZone)
        {
            float pos = (fabsf(axisLeft.x) - deadZone) / range;
            position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
            retVal = true;
        }

        // Rotate
        if (fabsf(axisRight.x) > deadZone)
        {
            float pos = (fabsf(axisRight.x) - deadZone) / range;
            rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
            retVal = true;
        }
        if (fabsf(axisRight.y) > deadZone)
        {
            float pos = (fabsf(axisRight.y) - deadZone) / range;
            rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
            retVal = true;
        }
    }
    else
    {
        // todo: move code from example base class for look-at
    }

    if (retVal)
    {
        UpdateViewMatrix();
    }

    return retVal;
}

void Camera::SetLookAt(glm::vec3 pos, glm::vec3 target)
{
	// glm::vec3 defaultUp(0.0f, 0.0f, 1.0f);
	// matrices.view = glm::lookAt(pos, target, defaultUp);
	//
	// this->position = Eigen::Vector3f(matrices.view[3][0], matrices.view[3][1], matrices.view[3][2]);
	// // ref: Introduction to 3D Game PROGRAMMING With DirectX 12,section 5.6.2
	// this->rotation = MathUtils::GetEulerAngleFromRotationMatrix(matrices.view);
	// UpdateViewMatrix();
}

