#pragma once
#include "layer.h"

#include <vulkan/vulkan.h>

namespace malachite
{

  class renderlayer : public layer
  {
    public:
      renderlayer();

    private:
      //called through layer binding

      void initalizeDependencies();
      void render(double& deltaTime);
      void cleanup();

      void initalizeWindow();
      void initalizeVulkan();
      
      void initalizeDebugMessenger();

      bool checkValidationLayerSupport();
      std::vector<const char*> getRequiredExtensions();

    private:
      void* m_windowPtr;
      VkInstance m_vulkanInstancePtr;
      VkDebugUtilsMessengerEXT m_vulkanDebugMessenger;
  };
}