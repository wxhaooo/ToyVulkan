#pragma once
#include<glm/glm.hpp>

class InputManager
{
public:
    InputManager() = default;
    ~InputManager() = default;

    glm::vec2 mousePos;

    void Update();
};
