#include<iostream>
#include<LoadGLTF.h>

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