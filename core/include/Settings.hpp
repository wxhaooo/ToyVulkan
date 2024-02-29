#pragma once
#include <Singleton.hpp>

struct GraphicSettings
{
    bool requiresStencil = false;
    bool validation = false;
    bool fullscreen = false;
    bool vsync = false;

    // ssao
    bool useSSAO = true;
    float ssaoRadius = 0.3f;
    float ssaoBias = 0.025f;
};

struct GuiSettings
{
    bool enableGUI = true;
    bool isStyleDark = true;
    float alpha = 0.9f;
};

struct AnimationSettings
{
    bool useAnimation = true;
};

struct Settings
{
    Settings()
    {
        graphicSettings = Singleton<GraphicSettings>::Instance();
        guiSettings = Singleton<GuiSettings>::Instance();
        animationSettings = Singleton<AnimationSettings>::Instance();
    }
    
    GraphicSettings* graphicSettings;
    GuiSettings* guiSettings;
    AnimationSettings* animationSettings;
};