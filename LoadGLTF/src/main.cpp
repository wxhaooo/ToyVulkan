#include<LoadGLTF.h>

#if _DEBUG
bool enableValidation = true;
#else
bool enableValidation = false;
#endif

int main()
{
    std::unique_ptr<LoadGLFT> loadGLTFApp = std::make_unique<LoadGLFT>(enableValidation);
    loadGLTFApp->InitVulkan();
    loadGLTFApp->Prepare();
    loadGLTFApp->RenderLoop();
    return 0;
}