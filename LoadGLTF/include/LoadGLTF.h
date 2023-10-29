#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>

class LoadGLFT :public VulkanApplicationBase
{
public:
    LoadGLFT(bool validation):VulkanApplicationBase("Load GLTF",validation){}
    ~LoadGLFT() =default;
};