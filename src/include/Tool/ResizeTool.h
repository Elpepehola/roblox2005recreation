#pragma once

#include "Tool/ArrowTool.h"

class ResizeTool : public ArrowTool
{
public:
    ResizeTool(void);
    ~ResizeTool(void);

    void onButton1MouseDown(Mouse mouse);
    void onButton1MouseUp(Mouse mouse);
    void onMouseMoved(Mouse mouse);
    void onSelect(Mouse mouse);
    void onKeyDown(int key);
    void onKeyUp(int key);
    void render(RenderDevice* rd, Mouse mouse);

private:
    int findHandle(Mouse mouse);
    void createHandles();
    void updatePart(PartInstance* part, Mouse mouse);
    bool getAxisParameter(Mouse mouse, Vector3 axis, float& parameter);
    float snapResize(float value);

    bool resizing;
    bool resizeSoundPlayed;
    int lastResizeAxisSize;
    int handleGrabbed;
    int startMouseX;
    int startMouseY;
    Vector3 startPosition;
    Vector3 startSize;
    Vector3 resizeAxis;
    float startAxisParameter;
    Sphere handles[6];
    bool hasHandles;
};
