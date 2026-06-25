#include "vulkanwindow.h"
#include "vulkanrenderer.h"

VulkanWindow::VulkanWindow()
{
}

QVulkanWindowRenderer* VulkanWindow::createRenderer() {
    m_renderer = new VulkanRenderer(this);
    return m_renderer;
}