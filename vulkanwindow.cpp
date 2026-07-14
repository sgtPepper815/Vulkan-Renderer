#include "vulkanwindow.h"
#include "vulkanrenderer.h"

VulkanWindow::VulkanWindow()
{
    // Wird für VK_POLYGON_MODE_LINE (Wireframe-Darstellung) benötigt.
    setEnabledFeaturesModifier([](VkPhysicalDeviceFeatures& features) {
        features.fillModeNonSolid = VK_TRUE;
    });
}

QVulkanWindowRenderer* VulkanWindow::createRenderer() {
    m_renderer = new VulkanRenderer(this);
    return m_renderer;
}