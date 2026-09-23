#pragma once

#include "ArrowTool.h"

class DraggerTool : public ArrowTool
{
public:
    DraggerTool(void);
    ~DraggerTool(void);

    void onButton1MouseDown(Mouse mouse);
    void onButton1MouseUp(Mouse mouse);
    void onMouseMoved(Mouse mouse);
    void onSelect(Mouse mouse);
    void onKeyDown(int key);
    void onKeyUp(int key);
    void render(RenderDevice* rd, Mouse mouse);

private:
    void createHandles();
    void grabHandle(Mouse mouse);
    bool getAxisParameter(Mouse mouse, Vector3 axis, float& parameter);
    float snapMovement(float value);

    bool hasHandles;
    int handleGrabbed;
    Vector3 center;
    Vector3 dragStartPosition;
    Vector3 dragStartAxis;
    float dragStartAxisParameter;
    Sphere handles[6];
};
