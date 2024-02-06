#include<LoadGLTF.h>
#include <Singleton.hpp>
#include <..\..\core\include\Settings.hpp>

#if _DEBUG
bool enableValidation = true;
#else
bool enableValidation = false;

#endif

int main()
{
    Singleton<Settings>::Instance()->graphicSettings->validation = enableValidation;
    std::unique_ptr<LoadGLFT> loadGLTFApp = std::make_unique<LoadGLFT>();
    loadGLTFApp->InitFondation();
    loadGLTFApp->InitVulkan();
    loadGLTFApp->Prepare();
    loadGLTFApp->RenderLoop();
    return 0;
}