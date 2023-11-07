#include<iostream>
#include<LoadGLTF.h>
#include <VulkanHelper.h>
#include <VulkanGLTFModel.h>

#if _DEBUG
bool enableValidation = true;
#else
bool enablleValidation = false;
#endif

void LoadGLFT::Prepare()
{
    VulkanApplicationBase::Prepare();
    LoadAsset();
}

void LoadGLFT::LoadAsset()
{
    std::unique_ptr<vks::geometry::VulkanGLTFModel> gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
    gltfModel->LoadGLTFFile("D:/Code/Vulkan/ToyVulkan/ToyVulkan/assets/models/FlightHelmet/glTF/FlightHelmet.gltf",vulkanDevice.get(),queue);
}

void LoadGLFT::Render()
{
    
}

int main()
{
    std::unique_ptr<VulkanApplicationBase> loadGLTFApp = std::make_unique<LoadGLFT>(enableValidation);
    loadGLTFApp->InitVulkan();
    loadGLTFApp->Prepare();
    loadGLTFApp->RenderLoop();
    return 0;
}