#pragma once
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>

namespace math
{
    constexpr float pi = glm::pi<float>(); 
    bool IsNearEqual(float a, float b, float eps = 1e-4f);
    float Radius2Degree(float radius);
    float Degree2Radius(float degree);

    //ref:http://eecs.qmul.ac.uk/~gslabaugh/publications/euler.pdf
    glm::vec3 GetEulerAngleFromRotationMatrix(glm::mat3 mat);

    glm::vec4 GetTranslateFromTransformMatrix(glm::mat4 mat);
}


