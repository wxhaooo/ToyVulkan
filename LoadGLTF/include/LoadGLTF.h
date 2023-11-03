#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>

class LoadGLFT :public VulkanApplicationBase
{
public:
    LoadGLFT(bool validation):VulkanApplicationBase("Load GLTF",1280,960,validation){}
    ~LoadGLFT() =default;

    void Prepare() override;
    void LoadAsset();
    void Render() override;
};