#pragma once

class GraphicSettings
{
public:
    bool requiresStencil = false;
    bool validation = false;
    bool fullscreen = false;
    bool vsync = false;
};

class GuiSettings
{
public:
    bool enableGUI = true;
    bool isStyleDark = true;
    float alpha = 0.6f;
};