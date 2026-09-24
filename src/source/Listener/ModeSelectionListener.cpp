#include "DataModelV2/SelectionService.h"
#include "DataModelV2/PartInstance.h"
#include "Listener/ModeSelectionListener.h"
#include "Application.h"
#include "Globals.h"
#include "Tool/ArrowTool.h"
#include "Tool/DraggerTool.h"
#include "Tool/ResizeTool.h"

#define CURSOR 0
#define ARROWS 1
#define RESIZE 2

void ModeSelectionListener::onButton1MouseClick(BaseButtonInstance* button)
{
    std::vector<Instance*> instances_2D =
        g_dataModel->getGuiRoot()->getAllChildren();

    for (size_t i = 0; i < instances_2D.size(); ++i)
    {
        if (instances_2D[i]->name == "Cursor" ||
            instances_2D[i]->name == "Resize" ||
            instances_2D[i]->name == "Arrows")
        {
            ((BaseButtonInstance*)instances_2D[i])->selected = false;
        }
    }

    button->selected = true;

    if (button->name == "Cursor")
    {
        g_usableApp->changeTool(new ArrowTool());
        g_usableApp->setMode(CURSOR);
    }
    else if (button->name == "Resize")
    {
        g_usableApp->changeTool(new ResizeTool());
        g_usableApp->setMode(RESIZE);
    }
    else if (button->name == "Arrows")
    {
        g_usableApp->changeTool(new DraggerTool());
        g_usableApp->setMode(ARROWS);
    }
}
