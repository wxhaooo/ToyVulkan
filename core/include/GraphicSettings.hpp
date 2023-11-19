#pragma once

class GraphicSettings
{
public:
    bool requiresStencil = false;
    bool validation = false;
    bool fullscreen = false;
    bool vsync = false;
    bool overlay = true;
    bool standaloneGUI = true;
};