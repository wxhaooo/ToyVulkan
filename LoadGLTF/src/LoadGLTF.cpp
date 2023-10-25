#include<iostream>
#include<LoadGLTF.h>

int main()
{
    std::unique_ptr<LoadGLFT> loadGLTFApp = std::make_unique<LoadGLFT>();
    loadGLTFApp->InitVulkan();
    return 0;
}