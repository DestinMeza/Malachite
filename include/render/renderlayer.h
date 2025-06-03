#pragma once
#include "layer.h"

#include <optional>
#include <vulkan/vulkan.h>

class GLFWwindow;

namespace malachite
{
  struct queueFamilyIndices 
  {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }
  };

  struct swapChainSupportDetails
  {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  enum SHADERTYPE
  {
    NONE = 0,
    VERTEX = 1,
    FRAGMENT = 2
  };

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

      void initalizeVulkanInstance();
      void initalizeDebugMessenger();
      void initalizeSurface();
      void initalizePhysicalDevice();
      void initalizeLogicalDevice();
      void initalizeSwapChain();
      void initalizeImageViews();
      void initalizeGraphicsPipeline();

      int rateDeviceSuitability(const VkPhysicalDevice& device);
      queueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device);
      swapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device);
      VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
      VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
      VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

      void createShaderModule(std::string fileName);

      bool checkValidationLayerSupport();
      bool checkDeviceSuitablity(const VkPhysicalDevice& device);
      bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
      std::vector<const char*> getRequiredExtensions();

    private:
      GLFWwindow* m_glfwWindowPtr;
      VkInstance m_vulkanInstancePtr;

      VkDebugUtilsMessengerEXT m_vulkanDebugMessenger;
      VkSurfaceKHR m_vulkanSurface;
      VkPhysicalDevice m_vulkanPhysicalDevice = VK_NULL_HANDLE;
      VkDevice m_vulkanLogicalDevice;
      VkSwapchainKHR m_vulkanSwapChain;

      VkQueue m_vulkanGraphicsQueue;
      VkQueue m_vulkanPresentQueue;

      std::vector<VkImageView> m_vulkanSwapChainImageViews;
      std::vector<VkImage> m_vulkanSwapChainImages;
      VkFormat m_vulkanSwapChainImageFormat;
      VkExtent2D m_vulkanSwapChainExtent;
  };
}