#include<iostream>
#include<LoadGLTF.h>

#if _DEBUG
bool enableValidation = true;
#else
bool enablleValidation = false;
#endif

int main()
{
    std::unique_ptr<LoadGLFT> loadGLTFApp = std::make_unique<LoadGLFT>(enableValidation);
    loadGLTFApp->InitVulkan();
    return 0;
}