#include "Tool/DraggerTool.h"
#include "Application.h"
#include "DataModelV2/SelectionService.h"
#include <math.h>

DraggerTool::DraggerTool(void)
{
    hasHandles = false;
    handleGrabbed = -1;
    center = Vector3(0, 0, 0);
    dragStartPosition = Vector3(0, 0, 0);
    dragStartAxis = Vector3(0, 1, 0);
    dragStartAxisParameter = 0.0f;
    createHandles();
}

DraggerTool::~DraggerTool(void)
{
}

void DraggerTool::onButton1MouseDown(Mouse mouse)
{
    grabHandle(mouse);

    if (handleGrabbed == -1)
    {
        ArrowTool::onButton1MouseDown(mouse);
        createHandles();
        return;
    }

    std::vector<Instance*> selection =
        g_dataModel->getSelectionService()->getSelection();
    if (selection.size() != 1)
    {
        handleGrabbed = -1;
        return;
    }

    PartInstance* part =
        dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
    {
        handleGrabbed = -1;
        return;
    }

    dragStartPosition = part->getPosition();
    CoordinateFrame frame = part->getCFrame();

    if (handleGrabbed == 0 || handleGrabbed == 1)
        dragStartAxis = frame.upVector();
    else if (handleGrabbed == 2 || handleGrabbed == 3)
        dragStartAxis = frame.lookVector();
    else
        dragStartAxis = frame.rightVector();

    if (handleGrabbed == 1 ||
        handleGrabbed == 3 ||
        handleGrabbed == 5)
    {
        dragStartAxis = -dragStartAxis;
    }

    float axisLength = dragStartAxis.length();
    if (axisLength > 0.0001f)
        dragStartAxis = dragStartAxis / axisLength;
    else
        dragStartAxis = Vector3(0, 1, 0);

    if (!getAxisParameter(mouse, dragStartAxis,
                          dragStartAxisParameter))
    {
        dragStartAxisParameter = 0.0f;
    }
}

void DraggerTool::onButton1MouseUp(Mouse mouse)
{
    if (handleGrabbed == -1)
        ArrowTool::onButton1MouseUp(mouse);

    handleGrabbed = -1;
    createHandles();
}

void DraggerTool::onMouseMoved(Mouse mouse)
{
    if (handleGrabbed == -1)
    {
        ArrowTool::onMouseMoved(mouse);
        createHandles();
        return;
    }

    std::vector<Instance*> selection =
        g_dataModel->getSelectionService()->getSelection();
    if (selection.size() != 1)
        return;

    PartInstance* part =
        dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
        return;

    float currentParameter;
    if (!getAxisParameter(mouse, dragStartAxis,
                          currentParameter))
        return;

    float movement = snapMovement(
        currentParameter - dragStartAxisParameter);

    part->setDragging(true);
    part->setPosition(
        dragStartPosition + dragStartAxis * movement);
    part->setChanged();
    createHandles();
}

bool DraggerTool::getAxisParameter(
    Mouse mouse, Vector3 axis, float& parameter)
{
    G3D::Ray ray = mouse.getRay();
    Vector3 rayDirection = ray.direction;
    Vector3 difference = ray.origin - dragStartPosition;

    float rayLength = rayDirection.length();
    float axisLength = axis.length();
    if (rayLength < 0.0001f || axisLength < 0.0001f)
        return false;

    rayDirection = rayDirection / rayLength;
    axis = axis / axisLength;

    float dot = rayDirection.dot(axis);
    float denominator = 1.0f - dot * dot;

    if (fabs(denominator) < 0.0001f)
    {
        parameter = difference.dot(axis);
        return true;
    }

    parameter =
        (difference.dot(axis) -
         dot * difference.dot(rayDirection)) / denominator;
    return true;
}

float DraggerTool::snapMovement(float value)
{
    if (value >= 0.0f)
        return (float)((int)(value / 0.5f + 0.5f)) * 0.5f;
    return (float)((int)(value / 0.5f - 0.5f)) * 0.5f;
}

void DraggerTool::onSelect(Mouse mouse)
{
    ArrowTool::onSelect(mouse);
    createHandles();
}

void DraggerTool::onKeyDown(int key)
{
    ArrowTool::onKeyDown(key);
}

void DraggerTool::onKeyUp(int key)
{
    ArrowTool::onKeyUp(key);
}

void DraggerTool::grabHandle(Mouse mouse)
{
    handleGrabbed = -1;
    if (!hasHandles)
        return;

    G3D::Ray ray = mouse.getRay();
    float distance = G3D::inf();
    for (int i = 0; i < 6; ++i)
    {
        float hit = ray.intersectionTime(handles[i]);
        if (G3D::isFinite(hit) && hit >= 0.0f &&
            hit < distance)
        {
            distance = hit;
            handleGrabbed = i;
        }
    }
}

void DraggerTool::createHandles()
{
    hasHandles = false;

    std::vector<Instance*> selection =
        g_dataModel->getSelectionService()->getSelection();
    if (selection.size() != 1)
        return;

    PartInstance* part =
        dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
        return;

    center = part->getPosition();
    Vector3 size = part->getSize();
    CoordinateFrame frame = part->getCFrame();
    float offset = 2.0f;

    handles[0] = Sphere(center + frame.upVector() *
                        (size.y / 2.0f + offset), 1.0f);
    handles[1] = Sphere(center - frame.upVector() *
                        (size.y / 2.0f + offset), 1.0f);
    handles[2] = Sphere(center + frame.lookVector() *
                        (size.z / 2.0f + offset), 1.0f);
    handles[3] = Sphere(center - frame.lookVector() *
                        (size.z / 2.0f + offset), 1.0f);
    handles[4] = Sphere(center + frame.rightVector() *
                        (size.x / 2.0f + offset), 1.0f);
    handles[5] = Sphere(center - frame.rightVector() *
                        (size.x / 2.0f + offset), 1.0f);
    hasHandles = true;
}

void DraggerTool::render(RenderDevice* rd, Mouse mouse)
{
    if (!hasHandles)
        return;

    for (int i = 0; i < 6; ++i)
    {
        G3D::Draw::arrow(
            center,
            handles[i].center - center,
            rd,
            Color3::orange(),
            2
        );
    }
}
