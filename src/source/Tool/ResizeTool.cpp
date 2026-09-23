#include "Tool/ResizeTool.h"
#include "Application.h"
#include "DataModelV2/SelectionService.h"
#include "AudioPlayer.h"
#include <math.h>

ResizeTool::ResizeTool(void)
{
    resizing = false;
    resizeSoundPlayed = false;
    lastResizeAxisSize = 0;
    handleGrabbed = -1;
    startMouseX = 0;
    startMouseY = 0;
    startPosition = Vector3(0, 0, 0);
    startSize = Vector3(1, 1, 1);
    resizeAxis = Vector3(0, 1, 0);
    startAxisParameter = 0.0f;
    hasHandles = false;
    createHandles();
}

ResizeTool::~ResizeTool(void)
{
}

void ResizeTool::onButton1MouseDown(Mouse mouse)
{
    handleGrabbed = findHandle(mouse);

    if (handleGrabbed == -1)
    {
        resizing = false;
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

    PartInstance* part = dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
    {
        handleGrabbed = -1;
        return;
    }

    startMouseX = mouse.x;
    startMouseY = mouse.y;
    startPosition = part->getPosition();
    startSize = part->getSize();
    CoordinateFrame frame = part->getCFrame();
    if (handleGrabbed == 0 || handleGrabbed == 1)
        resizeAxis = frame.upVector();
    else if (handleGrabbed == 2 || handleGrabbed == 3)
        resizeAxis = frame.lookVector();
    else
        resizeAxis = frame.rightVector();

    if (handleGrabbed == 1 ||
        handleGrabbed == 3 ||
        handleGrabbed == 5)
    {
        resizeAxis = -resizeAxis;
    }

    float axisLength = resizeAxis.length();
    if (axisLength > 0.0001f)
        resizeAxis = resizeAxis / axisLength;
    else
        resizeAxis = Vector3(0, 1, 0);

    if (!getAxisParameter(mouse, resizeAxis, startAxisParameter))
        startAxisParameter = 0.0f;

    if (handleGrabbed == 0 || handleGrabbed == 1)
        lastResizeAxisSize = (int)startSize.y;
    else if (handleGrabbed == 2 || handleGrabbed == 3)
        lastResizeAxisSize = (int)startSize.z;
    else
        lastResizeAxisSize = (int)startSize.x;
    resizing = true;

    if (!resizeSoundPlayed)
    {
        SoundService* soundService = g_dataModel->getSoundService();
        if (soundService != NULL)
        {
            Instance* switchSound = soundService->findFirstChild("Step");
            if (switchSound != NULL)
                soundService->playSound(switchSound);
        }
        resizeSoundPlayed = true;
    }
}

void ResizeTool::onButton1MouseUp(Mouse mouse)
{
    if (!resizing)
    {
        ArrowTool::onButton1MouseUp(mouse);
    }

    resizing = false;
    resizeSoundPlayed = false;
    handleGrabbed = -1;
    createHandles();
}

void ResizeTool::onMouseMoved(Mouse mouse)
{
    if (!resizing || handleGrabbed == -1)
    {
        ArrowTool::onMouseMoved(mouse);
        createHandles();
        return;
    }

    std::vector<Instance*> selection =
        g_dataModel->getSelectionService()->getSelection();

    if (selection.size() != 1)
        return;

    PartInstance* part = dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
        return;

    updatePart(part, mouse);
    createHandles();
}

void ResizeTool::updatePart(PartInstance* part, Mouse mouse)
{
    Vector3 newSize = startSize;
    Vector3 newPosition = startPosition;
    float currentParameter;
    if (!getAxisParameter(mouse, resizeAxis, currentParameter))
        return;

    float amount = snapResize(currentParameter - startAxisParameter);
    float newAxisSize;
    float oldAxisSize;

    if (handleGrabbed == 0 || handleGrabbed == 1)
    {
        oldAxisSize = startSize.y;
        newAxisSize = oldAxisSize + amount;
        newSize.y = newAxisSize;
    }
    else if (handleGrabbed == 2 || handleGrabbed == 3)
    {
        oldAxisSize = startSize.z;
        newAxisSize = oldAxisSize + amount;
        newSize.z = newAxisSize;
    }
    else
    {
        oldAxisSize = startSize.x;
        newAxisSize = oldAxisSize + amount;
        newSize.x = newAxisSize;
    }

    if (newAxisSize < 1.0f)
    {
        newAxisSize = 1.0f;
        if (handleGrabbed == 0 || handleGrabbed == 1)
            newSize.y = newAxisSize;
        else if (handleGrabbed == 2 || handleGrabbed == 3)
            newSize.z = newAxisSize;
        else
            newSize.x = newAxisSize;
    }

    if ((int)newAxisSize != lastResizeAxisSize)
    {
        SoundService* soundService = g_dataModel->getSoundService();
        if (soundService != NULL)
        {
            Instance* resizeSound = soundService->findFirstChild("Step");
            if (resizeSound != NULL)
                soundService->playSound(resizeSound);
        }
        lastResizeAxisSize = (int)newAxisSize;
    }

    newPosition = startPosition +
        resizeAxis * ((newAxisSize - oldAxisSize) * 0.5f);

    part->setSize(newSize);
    part->setPosition(newPosition);
    part->setChanged();
}

bool ResizeTool::getAxisParameter(
    Mouse mouse, Vector3 axis, float& parameter)
{
    G3D::Ray ray = mouse.getRay();
    Vector3 direction = ray.direction;
    Vector3 difference = ray.origin - startPosition;
    float directionLength = direction.length();
    float axisLength = axis.length();

    if (directionLength < 0.0001f || axisLength < 0.0001f)
        return false;

    direction = direction / directionLength;
    axis = axis / axisLength;

    float dot = direction.dot(axis);
    float denominator = 1.0f - dot * dot;
    if (fabs(denominator) < 0.0001f)
    {
        parameter = difference.dot(axis);
        return true;
    }

    parameter =
        (difference.dot(axis) -
         dot * difference.dot(direction)) / denominator;
    return true;
}

float ResizeTool::snapResize(float value)
{
    if (value >= 0.0f)
        return (float)((int)(value + 0.5f));
    return (float)((int)(value - 0.5f));
}

int ResizeTool::findHandle(Mouse mouse)
{
    if (!hasHandles)
        return -1;

    G3D::Ray ray = mouse.getRay();
    float distance = G3D::inf();
    int result = -1;

    for (int i = 0; i < 6; ++i)
    {
        float hit = ray.intersectionTime(handles[i]);
        if (G3D::isFinite(hit) && hit >= 0.0f && hit < distance)
        {
            distance = hit;
            result = i;
        }
    }

    return result;
}

void ResizeTool::createHandles()
{
    hasHandles = false;

    std::vector<Instance*> selection =
        g_dataModel->getSelectionService()->getSelection();
    if (selection.size() != 1)
        return;

    PartInstance* part = dynamic_cast<PartInstance*>(selection[0]);
    if (part == NULL)
        return;

    CoordinateFrame frame = part->getCFrame();
    Vector3 center = part->getPosition();
    Vector3 size = part->getSize();
    float offset = 2.0f;

    handles[0] = Sphere(center + frame.upVector() * (size.y / 2.0f + offset), 1.0f);
    handles[1] = Sphere(center - frame.upVector() * (size.y / 2.0f + offset), 1.0f);
    handles[2] = Sphere(center + frame.lookVector() * (size.z / 2.0f + offset), 1.0f);
    handles[3] = Sphere(center - frame.lookVector() * (size.z / 2.0f + offset), 1.0f);
    handles[4] = Sphere(center + frame.rightVector() * (size.x / 2.0f + offset), 1.0f);
    handles[5] = Sphere(center - frame.rightVector() * (size.x / 2.0f + offset), 1.0f);
    hasHandles = true;
}

void ResizeTool::onSelect(Mouse mouse)
{
    ArrowTool::onSelect(mouse);
    createHandles();
}

void ResizeTool::onKeyDown(int key)
{
    ArrowTool::onKeyDown(key);
}

void ResizeTool::onKeyUp(int key)
{
    ArrowTool::onKeyUp(key);
}

void ResizeTool::render(RenderDevice* rd, Mouse mouse)
{
}
