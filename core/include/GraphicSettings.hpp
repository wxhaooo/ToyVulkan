#pragma once

constexpr int LightCount = 2;

class GraphicSettings
{
public:
    bool requiresStencil = false;
    bool validation = false;
    bool fullscreen = false;
    bool vsync = false;

    // ssao
    bool useSSAO = true;
    uint32_t ssaoNoiseDim = 4;
    uint32_t ssaoKernelSize = 64;
    float ssaoRadius = 0.3f;
};

class GuiSettings
{
public:
    bool enableGUI = true;
    bool isStyleDark = true;
    float alpha = 0.6f;
};