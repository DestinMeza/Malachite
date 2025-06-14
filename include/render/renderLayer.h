#pragma once
#include "layer.h"

#include <optional>
#include <filesystem>
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

  enum e_shaderType
  {
    none = 0,
    vertex = 1,
    fragment = 2,
    compute = 3,
    geometry = 4,
    tess_control = 5,
    tess_evaluation = 6
  };

  struct shader_schematic
  {
    std::filesystem::path fileName;
    std::vector<std::string> fileContents;
    std::vector<malachite::e_shaderType> shaderTypes;
  };

  class renderLayer : public layer
  {
    public:
      renderLayer();

    private:
      //called through layer binding

      void initalizeDependencies();
      void render(double& deltaTime);
      void drawFrame(double& deltaTime);
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
      void initalizeRenderPass();
      void initalizeGraphicsPipeline();
      void initalizeFrameBuffers();
      void initalizeCommandPool();
      void initalizeCommandBuffer();
      void initalizeSyncObjects();

      int rateDeviceSuitability(const VkPhysicalDevice& device);
      queueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device);
      swapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& device);
      VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
      VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
      VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

      VkPipelineShaderStageCreateInfo createShaderModule(std::filesystem::path fileName, e_shaderType shaderType);
      
      //writes the commands we want to execute into a command buffer.
      void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

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
      VkRenderPass m_vulkanRenderPass;
      VkPipelineLayout m_vulkanPipelineLayout;
      VkPipeline m_vulkanGraphicsPipeline;
      VkCommandPool m_vulkanCommandPool;
      VkCommandBuffer m_vulkanCommandBuffer;

      VkQueue m_vulkanGraphicsQueue;
      VkQueue m_vulkanPresentQueue;

      VkSemaphore m_vulkanImageAvailableSemaphore;
      VkSemaphore m_vulkanRenderFinishedSemaphore;
      VkFence m_vulkanInFlightFence;

      std::vector<VkShaderModule> m_vulkanShaderModules;
      std::vector<VkImageView> m_vulkanSwapChainImageViews;
      std::vector<VkImage> m_vulkanSwapChainImages;
      std::vector<VkFramebuffer> m_vulkanSwapChainFrameBuffers;
      VkFormat m_vulkanSwapChainImageFormat;
      VkExtent2D m_vulkanSwapChainExtent;
  };
}