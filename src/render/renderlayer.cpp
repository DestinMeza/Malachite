#include "malpch.h"
#include "renderlayer.h"
#include "application.h"

#include <iostream>
#include <stdexcept>
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream>
#include <filesystem>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

const std::vector<const char*> validationLayers = 
{
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = 
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

///
/// Vulkan specifc static proxy functions
/// These help with validaiton layer setup.
///

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback
(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
)
{
    std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

static void populateDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{    
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional
}

static VkResult CreateDebugUtilsMessengerEXT
(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger
)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) 
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else 
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT
(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static std::vector<std::string> readShaderFile(const std::string& fileName, std::vector<malachite::SHADERTYPE>& shaderTypes)
{
    std::ifstream file =  std::ifstream(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open()) 
    {
        throw std::runtime_error("failed to open file!");
    }

    std::map<std::string, malachite::SHADERTYPE> shaderTypeMap = 
    {
        {"#vertex", malachite::SHADERTYPE::VERTEX},
        {"#fragment", malachite::SHADERTYPE::FRAGMENT}
    };

    std::vector<std::string> fileContents;

    std::string line;
        
    std::string fileSection = "";
    while(std::getline(file, line))
    {
        bool isTagLine = false;

        for (auto [tag, shaderConstType] : shaderTypeMap)
        {
            isTagLine = line == tag;

            if (isTagLine)
            {
                shaderTypes.push_back(shaderConstType);
                break;
            }
        }

        if (!isTagLine)
        {
            std::string submitSection = fileSection;
            fileContents.push_back(submitSection);
        }
    }

    return fileContents;
}

///
/// Render Layer
/// this layer handles the Render setup, cleanup, and flow.
///

namespace malachite
{
    renderlayer::renderlayer()
        : layer(0, layerFunctionConfig())
    {
        m_config.initalize = MAL_BIND_FUNCTION(renderlayer::initalizeDependencies, this);
        m_config.update = MAL_BIND_FUNCTION_PARAMS(renderlayer::render, this, std::placeholders::_1);
        m_config.postClose = MAL_BIND_FUNCTION(renderlayer::cleanup, this);
    }

    void renderlayer::initalizeDependencies()
    {
        initalizeWindow();
        initalizeVulkan();
    }

    void renderlayer::initalizeVulkan()
    {
        initalizeVulkanInstance();
        initalizeDebugMessenger();
        initalizeSurface();
        initalizePhysicalDevice();
        initalizeLogicalDevice();
        initalizeSwapChain();
        initalizeImageViews();
        initalizeGraphicsPipeline();
    }

    void renderlayer::initalizeWindow()
    {
        std::cout << "Initalizing Window..." << std::endl;
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        //Handle resize later...
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        //TODO have app pass window width and height args
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;
        
        m_glfwWindowPtr = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        std::cout << "Window Initalization Sucessful." << std::endl;
    }

    void renderlayer::initalizeVulkanInstance()
    {
        std::cout << "Initalizing Vulkan..." << std::endl;

        if (enableValidationLayers && !checkValidationLayerSupport()) 
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Malachite App";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "Malachite";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        //Extensions
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        //Validation Layer Check
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }
        if (vkCreateInstance(&createInfo, nullptr, &m_vulkanInstancePtr) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create instance!");
        }

        std::cout << "Vulkan Initalization Sucessful." << std::endl;
    }

    void renderlayer::initalizeDebugMessenger()
    {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(m_vulkanInstancePtr, &createInfo, nullptr, &m_vulkanDebugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void renderlayer::initalizeSurface()
    {
        if (glfwCreateWindowSurface(m_vulkanInstancePtr, m_glfwWindowPtr, nullptr, &m_vulkanSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void renderlayer::initalizePhysicalDevice()
    {
        uint32_t deviceCount = 0;

        vkEnumeratePhysicalDevices(m_vulkanInstancePtr, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_vulkanInstancePtr, &deviceCount, devices.data());

        // Use an ordered map to automatically sort candidates by increasing score
        std::multimap<int, VkPhysicalDevice> candidates;

        for (const auto& device : devices) {
            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }

        // Check if the best candidate is suitable at all
        if (candidates.rbegin()->first <= 0)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        m_vulkanPhysicalDevice = candidates.rbegin()->second;
        if (m_vulkanPhysicalDevice == VK_NULL_HANDLE) 
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    int renderlayer::rateDeviceSuitability(const VkPhysicalDevice& device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        //TODO improve scoring device selection in the future. Using simple
        //selector atm.
        int score = 0;

        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }

        // Maximum possible size of textures affects graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        // Application can't function without geometry shaders
        if (!deviceFeatures.geometryShader) {
            return -1; //-1 meaning incompatible
        }

        return score;
    }

    //Right now Qyeye only supports graphics commands.
    //TODO extend this to have wider range such as
    // - compute commands
    // - memory transfer commands
    queueFamilyIndices renderlayer::findQueueFamilies(const VkPhysicalDevice& device)
    {
        queueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 hasPresentFamilySupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_vulkanSurface, &hasPresentFamilySupport);

            if (hasPresentFamilySupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    void renderlayer::initalizeLogicalDevice()
    {
        queueFamilyIndices indices = findQueueFamilies(m_vulkanPhysicalDevice);
        
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = 
        {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) 
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(m_vulkanPhysicalDevice, &createInfo, nullptr, &m_vulkanLogicalDevice) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_vulkanLogicalDevice, indices.graphicsFamily.value(), 0, &m_vulkanGraphicsQueue);
        vkGetDeviceQueue(m_vulkanLogicalDevice, indices.presentFamily.value(), 0, &m_vulkanPresentQueue);
    }

    void renderlayer::initalizeSwapChain()
    {
        swapChainSupportDetails swapChainSupport = querySwapChainSupport(m_vulkanPhysicalDevice);
            
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.surfaceFormats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.surfaceCapabilities);

        uint32_t imageCount = swapChainSupport.surfaceCapabilities.minImageCount + 1;

        if (swapChainSupport.surfaceCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.surfaceCapabilities.maxImageCount)
        {
            imageCount = swapChainSupport.surfaceCapabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_vulkanSurface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        queueFamilyIndices indices = findQueueFamilies(m_vulkanPhysicalDevice);
        uint32_t queueFamilyIndices[] = 
        {
            indices.graphicsFamily.value(), 
            indices.presentFamily.value()
        };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = swapChainSupport.surfaceCapabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_vulkanLogicalDevice, &createInfo, nullptr, &m_vulkanSwapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_vulkanLogicalDevice, m_vulkanSwapChain, &imageCount, nullptr);
        m_vulkanSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_vulkanLogicalDevice, m_vulkanSwapChain, &imageCount, m_vulkanSwapChainImages.data());

        m_vulkanSwapChainImageFormat = surfaceFormat.format;
        m_vulkanSwapChainExtent = extent;
    }

    swapChainSupportDetails renderlayer::querySwapChainSupport(const VkPhysicalDevice& device)
    {
        swapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_vulkanSurface, &details.surfaceCapabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vulkanSurface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.surfaceFormats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_vulkanSurface, &formatCount, details.surfaceFormats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vulkanSurface, &presentModeCount, nullptr);

        if (presentModeCount != 0) 
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_vulkanSurface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR renderlayer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats) 
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR renderlayer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) 
    {
        for (const auto& availablePresentMode : availablePresentModes) 
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D renderlayer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        } 
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_glfwWindowPtr, &width, &height);

            VkExtent2D actualExtent = 
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void renderlayer::initalizeImageViews()
    {
        m_vulkanSwapChainImageViews.resize(m_vulkanSwapChainImages.size());

        for (size_t i = 0; i < m_vulkanSwapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_vulkanSwapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_vulkanSwapChainImageFormat;

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_vulkanLogicalDevice, &createInfo, nullptr, &m_vulkanSwapChainImageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void renderlayer::initalizeGraphicsPipeline()
    {
        std::filesystem::path mainPath = std::filesystem::current_path();
        mainPath = mainPath.parent_path().parent_path().parent_path();
        mainPath.append("aggregate/bin/");

        std::cout << "Reading Shader at path: " << mainPath << std::endl;
        
        std::filesystem::current_path(mainPath);

        //TODO add resource system
        //path is relative to running process, but an actual structure is missing
        createShaderModule("res/shaders/simple.malshader");
    }

    void renderlayer::createShaderModule(std::string fileName)
    {
        std::vector<SHADERTYPE> shaderTypes;
        std::vector<std::string> shaderContents = readShaderFile(fileName, shaderTypes);

        std::vector<VkShaderModule> shaderModules;

        std::map<SHADERTYPE, std::string> shaderTypesName = 
        {
            {SHADERTYPE::NONE, "None"},
            {SHADERTYPE::VERTEX, "Vertex"},
            {SHADERTYPE::FRAGMENT, "Fragment"}
        };

        for (int i = 0; i < shaderContents.size(); i++)
        {
            SHADERTYPE shaderIndex = (SHADERTYPE) i;

            std::cout << "Shader | " << std::endl << shaderTypesName[shaderIndex] << std::endl << "Contents:" << std::endl << shaderContents[i];

            VkPipelineShaderStageCreateInfo shaderStages[shaderContents[i].size()];  
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = shaderContents[i].size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderContents[i].data());
            
            //Handle this error better
            VkShaderModule shaderModule;
            if (vkCreateShaderModule(m_vulkanLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create shader module!");
            }

            VkPipelineShaderStageCreateInfo shaderStageInfo{};
            shaderStageInfo.module = shaderModule;
            shaderStageInfo.pName = "main";

            switch (shaderTypes[i])
            {
                case SHADERTYPE::VERTEX:
                {
                    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                    break;
                }
                case SHADERTYPE::FRAGMENT:
                {
                    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                    break;
                }
                default:
                {
                    throw std::runtime_error("failed identify shader type!");
                    break;
                }
            }

            shaderModules.push_back(shaderModule);
        }

        //Clean up after sending to VK
        for (auto& shaderModule : shaderModules)
        {
            vkDestroyShaderModule(m_vulkanLogicalDevice, shaderModule, nullptr);
        }
    }

    void renderlayer::render(double& deltaTime)
    {
        if(glfwWindowShouldClose(m_glfwWindowPtr))
        {
            application::closeApp();
            return;
        }

        glfwPollEvents();
    }

    void renderlayer::cleanup()
    {
        for (auto imageView : m_vulkanSwapChainImageViews)
        {
            vkDestroyImageView(m_vulkanLogicalDevice, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_vulkanLogicalDevice, m_vulkanSwapChain, nullptr);

        vkDestroyDevice(m_vulkanLogicalDevice, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(m_vulkanInstancePtr, m_vulkanDebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_vulkanInstancePtr, m_vulkanSurface, nullptr);

        vkDestroyInstance(m_vulkanInstancePtr, nullptr);

        glfwDestroyWindow(m_glfwWindowPtr);

        glfwTerminate();
    }

    ///
    /// DEBUG / VALIDATION
    ///

    bool renderlayer::checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) 
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) 
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    bool renderlayer::checkDeviceSuitablity(const VkPhysicalDevice& device)
    {
        queueFamilyIndices indices = findQueueFamilies(device);

        bool hasExtensionsSupport = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (hasExtensionsSupport) 
        {
            swapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.surfaceFormats.empty() && !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && hasExtensionsSupport && swapChainAdequate;
    }

    bool renderlayer::checkDeviceExtensionSupport(const VkPhysicalDevice& device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    std::vector<const char*> renderlayer::getRequiredExtensions() 
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }
}