#pragma once
#include <MathUtils.h>
#include <glm/ext/scalar_constants.hpp>

namespace math
{
    bool IsNearEqual(float a, float b, float eps)
    {
        return std::abs(a - b) < eps;
    }

    float Radius2Degree(float radius)
    {
        return radius * (180.0f / pi);
    }

    float Degree2Radius(float degree)
    {
        return degree * (pi / 180.0f);
    }

    //ref:http://eecs.qmul.ac.uk/~gslabaugh/publications/euler.pdf
    glm::vec3 GetEulerAngleFromRotationMatrix(glm::mat3 mat)
    {
        // glm::mat3 rMat = mat;
        glm::mat3 rMat = inverse(mat);
        glm::vec3 ret;

        float theta0 = 0.0f, theta1 = 0.0f;
        float psi0 = 0.0f, psi1 = 0.0f;
        float phi0 = 0.0f, phi1 = 0.0f;

        if (!IsNearEqual(std::abs(rMat[0][2]), 1.0f))
        {
            theta0 = -asin(rMat[0][2]);
            theta1 = pi - theta0;

            psi0 = atan2(rMat[1][2] / cos(theta0), rMat[2][2] / cos(theta0));
            psi1 = atan2(rMat[1][2] / cos(theta1), rMat[2][2] / cos(theta1));

            phi0 = atan2(rMat[0][1] / cos(theta0), rMat[0][0] / cos(theta0));
            phi1 = atan2(rMat[0][1] / cos(theta1), rMat[0][0] / cos(theta1));
        }
        else
        {
            phi0 = 0.0f;
            if (IsNearEqual(rMat[0][2], -1.0f))
            {
                theta0 = pi / 2.0f;
                psi0 = atan2(rMat[1][0], rMat[2][0]);
            }
            else
            {
                theta0 = -pi / 2.0f;
                psi0 = atan2(-rMat[1][0], -rMat[2][0]);
            }
        }

        ret.x = fmodf(Radius2Degree(psi0), 180.0f);
        ret.y = fmodf(Radius2Degree(theta0), 180.0f);
        ret.z = fmodf(Radius2Degree(phi0), 180.0f);

        return ret;
    }

    glm::vec4 GetTranslateFromTransformMatrix(glm::mat4 mat)
    {
        return mat[3];
    }
}


