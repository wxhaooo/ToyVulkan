#include<LoadGLTF.h>
#include <Singleton.hpp>
#include <GraphicSettings.hpp>

#if _DEBUG
bool enableValidation = true;
#else
bool enableValidation = false;

#endif

int main()
{
    Singleton<GraphicSettings>::Instance()->validation = enableValidation;
    std::unique_ptr<LoadGLFT> loadGLTFApp = std::make_unique<LoadGLFT>();
    loadGLTFApp->InitVulkan();
    loadGLTFApp->Prepare();
    loadGLTFApp->RenderLoop();
    return 0;
}