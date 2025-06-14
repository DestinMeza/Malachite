#include "malpch.h"
#include "renderLayer.h"
#include "application.h"

#include <iostream>
#include <stdexcept>
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream>
#include <filesystem>

#include <shaderc/shaderc.hpp>

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
    MAL_LOG_ERROR("Validation Layer Error: ", pCallbackData->pMessage);

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

static VkResult createDebugUtilsMessengerEXT
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

static void destroyDebugUtilsMessengerEXT
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

static std::vector<char> readShaderFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

static void parseShaderTextFile
(   
    const std::filesystem::path& filePath, 
    malachite::shader_schematic& shaderSchematic
)
{
    std::ifstream file =  std::ifstream(filePath);

    if (!file.is_open()) 
    {
        throw std::runtime_error("failed to open file!");
    }

    std::map<std::string, malachite::e_shaderType> shaderTypeMap = 
    {
        {"#vertex", malachite::e_shaderType::vertex},
        {"#fragment", malachite::e_shaderType::fragment}
    };

    std::string line;
    std::string fileSection = "";
    int lineNumber = 0;
    int tagCount = 0;
    int submittedSectionsCount = 0;
    bool isSkipLine = false;

    MAL_LOG_TRACE("Starting Line Read For File: ", filePath);
    while (std::getline(file, line))
    {
        lineNumber++;
        bool isTagLine = false;

        if(isSkipLine)
        {
            line.clear();
            isSkipLine = false;
            continue;
        }

        for (auto i = shaderTypeMap.begin(); i != shaderTypeMap.end(); i++)
        {
            std::string tag = i->first;
            malachite::e_shaderType shaderType = i->second;
            
            isTagLine = line == tag;

            if (isTagLine)
            {
                isSkipLine = true;
                tagCount++;

                MAL_LOG_TRACE("Found Tag: ", tag, "Line: ", lineNumber);

                shaderSchematic.shaderTypes.push_back(shaderType);
                break;
            }
        }

        //Ignore all lines above first tag line. Useful for documentation
        if (tagCount == 0)
        {
            line.clear();
            continue;
        }

        if (isTagLine)
        {
            if (tagCount > 1)
            {
                submittedSectionsCount++;

                std::string submitSection = fileSection;

                MAL_LOG_TRACE("Submitted: \n", submitSection);

                // Submit then clear file section string
                shaderSchematic.fileContents.push_back(submitSection);
                fileSection.clear();
            }

            //Clear line to add more data
            line.clear();
            continue;
        }

        //Add data from line to fileSection then clear line contents
        fileSection += line;
        fileSection.push_back('\n');
        line.clear();
    }

    //This is to ensure if end of file is found and no submitted section is found to
    //Push back the currently cached fileSection to fileContents list
    if (fileSection.size() > 0)
    {
        std::string submitSection = fileSection;

        MAL_LOG_TRACE("Submitted: \n", submitSection);

        // Submit then clear file section string
        shaderSchematic.fileContents.push_back(submitSection);
        fileSection.clear();
    }
}

static void generateSPRVBinaries(std::filesystem::path shaderFilePath, std::filesystem::path generatedFileDirPath)
{
    malachite::shader_schematic shaderSchematic;
    shaderSchematic.fileName = shaderFilePath.filename();

    parseShaderTextFile(shaderFilePath, shaderSchematic);
    
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    std::map<malachite::e_shaderType, shaderc_shader_kind> shaderTypeMap = 
    {
        {malachite::e_shaderType::vertex, shaderc_vertex_shader},
        {malachite::e_shaderType::fragment, shaderc_fragment_shader},
        {malachite::e_shaderType::compute, shaderc_compute_shader},
        {malachite::e_shaderType::geometry, shaderc_geometry_shader},
        {malachite::e_shaderType::tess_control, shaderc_tess_control_shader},
        {malachite::e_shaderType::tess_evaluation, shaderc_tess_evaluation_shader},
    };

    std::map<malachite::e_shaderType, const char*> shaderFileExtMap = 
    {
        {malachite::e_shaderType::vertex, "vert"},
        {malachite::e_shaderType::fragment, "frag"},
        {malachite::e_shaderType::compute, "comp"},
        {malachite::e_shaderType::geometry, "geom"},
        {malachite::e_shaderType::tess_control, "tessc"},
        {malachite::e_shaderType::tess_evaluation, "tesse"},
    };

    MAL_LOG_TRACE("Compiling Shader: ", shaderSchematic.fileName);

    for (int i = 0; i < shaderSchematic.fileContents.size(); i++)
    {
        const char* fileContent = shaderSchematic.fileContents[i].c_str();
        size_t fileContentSize = strlen(fileContent);
        malachite::e_shaderType shaderType = shaderSchematic.shaderTypes[i];

        MAL_LOG_TRACE("Shader Section Content: \n", shaderSchematic.fileContents[i]);

        const char* shaderFileExt = shaderFileExtMap[shaderType];
        shaderc_shader_kind compilerShaderType = shaderTypeMap[shaderType];

        std::string fileSectionTag = shaderSchematic.fileName.c_str();
        fileSectionTag.append(".");
        fileSectionTag.append(shaderFileExt);

        const char* fileSectionTagConst = fileSectionTag.c_str();

        //Preprocessed Source Compilation
        //{ 
        //    shaderc::PreprocessedSourceCompilationResult result = compiler.PreprocessGlsl
        //    (
        //        fileContent,                        //file data
        //        fileContentSize,                    //file data size
        //        compilerShaderType,                 //shader type
        //        fileSectionTagConst,                //file tag (identifier)
        //        options     
        //    );
        //
        //    size_t errorCount = result.GetNumErrors();
        //    size_t warningCount = result.GetNumWarnings();
        //
        //    std::string errorMessage = result.GetErrorMessage();
        //
        //    if(errorCount > 0)
        //    {
        //        MAL_LOG_ERROR("ERRORS ", errorCount, " WARNINGS ", warningCount);
        //        MAL_LOG_ERROR("Shader Compiler Error: \n", errorMessage);
        //        continue;
        //    }
        //
        //    std::filesystem::path generatedFilePath = generatedFileDirPath / shaderSchematic.fileName;
        //    generatedFilePath.replace_extension(shaderFileExt);
        //
        //    MAL_LOG_TRACE("Creating Output Binary of Shader Module: ", generatedFilePath);
        //
        //    std::ofstream outputFile(generatedFilePath, std::ios::binary);
        //    outputFile.write
        //    (
        //        result.cbegin(),                          //Data
        //        result.cend() - result.cbegin() //Data Size
        //    );
        //
        //    outputFile.close();
        //}
        
        //Make module from preprocess
        { 
            shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv
            (            
                fileContent,                        //file data
                fileContentSize,                    //file data size
                compilerShaderType,                 //shader type
                fileSectionTagConst,                //file tag (identifier)
                "main",                             //entry point
                options                             //options
            );

            size_t errorCount = result.GetNumErrors();
            size_t warningCount = result.GetNumWarnings();

            std::string errorMessage = result.GetErrorMessage();

            if(errorCount > 0)
            {
                MAL_LOG_ERROR("ERRORS ", errorCount, " WARNINGS ", warningCount);
                MAL_LOG_ERROR("Shader Compiler Error: \n", errorMessage);
                continue;
            }

            std::filesystem::path generatedFilePath = generatedFileDirPath / shaderSchematic.fileName;
            generatedFilePath.replace_extension(shaderFileExt);

            MAL_LOG_TRACE("Creating Output Binary of Shader Module: ", generatedFilePath);

            std::ofstream outputFile(generatedFilePath, std::ios::binary);

            outputFile.write
            (
                reinterpret_cast<const char*>(result.cbegin()),      //Data
                (result.cend() - result.cbegin()) * sizeof(uint32_t) //Data Size
            );

            outputFile.close();
        }
    }
}

///
/// Render Layer
/// this layer handles the Render setup, cleanup, and flow.
///

namespace malachite
{
    renderLayer::renderLayer()
        : layer(0, layerFunctionConfig())
    {
        m_config.initalize = MAL_BIND_FUNCTION(renderLayer::initalizeDependencies, this);
        m_config.update = MAL_BIND_FUNCTION_PARAMS(renderLayer::render, this, std::placeholders::_1);
        m_config.postClose = MAL_BIND_FUNCTION(renderLayer::cleanup, this);
    }

    void renderLayer::initalizeDependencies()
    {
        initalizeWindow();
        initalizeVulkan();
    }

    void renderLayer::initalizeVulkan()
    {
        MAL_LOG_TRACE("Initalizing Vulkan...");

        initalizeVulkanInstance();
        initalizeDebugMessenger();
        initalizeSurface();
        initalizePhysicalDevice();
        initalizeLogicalDevice();
        initalizeSwapChain();
        initalizeImageViews();
        initalizeRenderPass();
        initalizeGraphicsPipeline();
        initalizeFrameBuffers();
        initalizeCommandPool();
        initalizeCommandBuffer();
        initalizeSyncObjects();

        MAL_LOG_TRACE("Vulkan Initalization Sucessful.");
    }

    void renderLayer::initalizeWindow()
    {
        MAL_LOG_TRACE("Initalizing Window...");
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        //Handle resize later...
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        //TODO have app pass window width and height args
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;
        
        m_glfwWindowPtr = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        MAL_LOG_TRACE("Window Initalization Sucessful.");
    }

    void renderLayer::initalizeVulkanInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) 
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Malachite App";
        appInfo.applicationVersion = VK_API_VERSION_1_4;
        appInfo.pEngineName = "Malachite";
        appInfo.engineVersion = VK_API_VERSION_1_4;
        appInfo.apiVersion = VK_API_VERSION_1_4;

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
    }

    void renderLayer::initalizeDebugMessenger()
    {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerInfo(createInfo);

        if (createDebugUtilsMessengerEXT(m_vulkanInstancePtr, &createInfo, nullptr, &m_vulkanDebugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void renderLayer::initalizeSurface()
    {
        if (glfwCreateWindowSurface(m_vulkanInstancePtr, m_glfwWindowPtr, nullptr, &m_vulkanSurface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void renderLayer::initalizePhysicalDevice()
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

    int renderLayer::rateDeviceSuitability(const VkPhysicalDevice& device)
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
    queueFamilyIndices renderLayer::findQueueFamilies(const VkPhysicalDevice& device)
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

    void renderLayer::initalizeLogicalDevice()
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

    void renderLayer::initalizeSwapChain()
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

    swapChainSupportDetails renderLayer::querySwapChainSupport(const VkPhysicalDevice& device)
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

    VkSurfaceFormatKHR renderLayer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats) 
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR renderLayer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) 
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

    VkExtent2D renderLayer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
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

    void renderLayer::initalizeImageViews()
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

    void renderLayer::initalizeRenderPass()
    {
        //Attachment description
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_vulkanSwapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        //Subpasses and attachment references
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        //The index of the attachment in this array is directly referenced 
        //from the fragment shader with the layout(location = 0) out vec4 outColor directive!
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        //Render pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(m_vulkanLogicalDevice, &renderPassInfo, nullptr, &m_vulkanRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void renderLayer::initalizeGraphicsPipeline()
    {
        //Shaders

        //TODO add resource system
        //path is relative to running process, but an actual resource structure is missing

        const char* shaderPath = "bin/res/shaders/simple.shader";
        const char* shaderBinaryOutputPath = "bin/res/shaderbinaries/";

        const char* vertexPath = "bin/res/shaderbinaries/simple.vert";
        const char* fragmentPath = "bin/res/shaderbinaries/simple.frag";

        std::filesystem::path mainPath = std::filesystem::current_path();
        MAL_LOG_TRACE("Current Working Directory Path: ", mainPath);
        MAL_LOG_TRACE("Generating SPV Binaries: ", shaderPath);

        generateSPRVBinaries(shaderPath, shaderBinaryOutputPath);
        VkPipelineShaderStageCreateInfo vertexStage = createShaderModule(vertexPath, e_shaderType::vertex);
        VkPipelineShaderStageCreateInfo fragmentStage = createShaderModule(fragmentPath, e_shaderType::fragment);

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertexStage, fragmentStage};
        
        //
        //Input Assembly//
        //

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        //
        //Viewports and scissors//
        //

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) m_vulkanSwapChainExtent.width;
        viewport.height = (float) m_vulkanSwapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        //This acts like a mask for the rasterizer.
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_vulkanSwapChainExtent;

        std::vector<VkDynamicState> dynamicStates = 
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        //Rasterizing
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        //
        //Depth and Stencil testing//
        //

        //Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        //Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(m_vulkanLogicalDevice, &pipelineLayoutInfo, nullptr, 
            &m_vulkanPipelineLayout) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_vulkanPipelineLayout;
        pipelineInfo.renderPass = m_vulkanRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        if (vkCreateGraphicsPipelines(m_vulkanLogicalDevice, VK_NULL_HANDLE, 1, 
            &pipelineInfo, nullptr, &m_vulkanGraphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        //Clean up after sending to VK
        for (VkShaderModule shaderModule: m_vulkanShaderModules)
        {
            vkDestroyShaderModule(m_vulkanLogicalDevice, shaderModule, nullptr);
        }

        m_vulkanShaderModules.clear();
    }

    VkPipelineShaderStageCreateInfo renderLayer::createShaderModule(std::filesystem::path filePath, e_shaderType shaderType)
    {
        MAL_LOG_TRACE("Reading Shader at path: ", filePath);

        std::vector<char> shaderCode = readShaderFile(filePath);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
        
        //Handle this error better
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(m_vulkanLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";

        switch (shaderType)
        {
            case e_shaderType::vertex:
            {
                shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            }
            case e_shaderType::fragment:
            {
                shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            }
            default:
            {
                throw std::runtime_error("shader type is not supported!");
                break;
            }
        }

        m_vulkanShaderModules.push_back(shaderModule);

        return shaderStageInfo;
    }

    void renderLayer::initalizeFrameBuffers()
    {
        m_vulkanSwapChainFrameBuffers.resize(m_vulkanSwapChainImageViews.size());

        for (size_t i = 0; i < m_vulkanSwapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                m_vulkanSwapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_vulkanRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_vulkanSwapChainExtent.width;
            framebufferInfo.height = m_vulkanSwapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_vulkanLogicalDevice, &framebufferInfo, nullptr, &m_vulkanSwapChainFrameBuffers[i]) != VK_SUCCESS) 
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void renderLayer::initalizeCommandPool()
    {
        malachite::queueFamilyIndices queueFamilyIndices = findQueueFamilies(m_vulkanPhysicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_vulkanLogicalDevice, &poolInfo, nullptr, &m_vulkanCommandPool) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void renderLayer::initalizeCommandBuffer()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_vulkanCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(m_vulkanLogicalDevice, &allocInfo, &m_vulkanCommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void renderLayer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_vulkanRenderPass;
        renderPassInfo.framebuffer = m_vulkanSwapChainFrameBuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_vulkanSwapChainExtent;

        //TODO make a wrapper for safe and easy command buffer manipulation
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_vulkanGraphicsPipeline);

            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_vulkanSwapChainExtent.width);
            viewport.height = static_cast<float>(m_vulkanSwapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_vulkanSwapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // vertex count
            // instance count
            // first vertex
            // first instance
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
            
            // Always call this.
            vkCmdEndRenderPass(commandBuffer);
        }

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void renderLayer::initalizeSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(m_vulkanLogicalDevice, &semaphoreInfo, nullptr, &m_vulkanImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(m_vulkanLogicalDevice, &semaphoreInfo, nullptr, &m_vulkanRenderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(m_vulkanLogicalDevice, &fenceInfo, nullptr, &m_vulkanInFlightFence) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void renderLayer::render(double& deltaTime)
    {
        if(glfwWindowShouldClose(m_glfwWindowPtr))
        {
            vkDeviceWaitIdle(m_vulkanLogicalDevice);

            application::closeApp();
            return;
        }

        glfwPollEvents();
        drawFrame(deltaTime);
    }

    void renderLayer::drawFrame(double& deltaTime)
    {
        vkWaitForFences(m_vulkanLogicalDevice, 1, &m_vulkanInFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_vulkanLogicalDevice, 1, &m_vulkanInFlightFence);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(m_vulkanLogicalDevice, m_vulkanSwapChain, UINT64_MAX, m_vulkanImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        vkResetCommandBuffer(m_vulkanCommandBuffer, 0);
        recordCommandBuffer(m_vulkanCommandBuffer, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {m_vulkanImageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_vulkanCommandBuffer;

        VkSemaphore signalSemaphores[] = {m_vulkanRenderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_vulkanGraphicsQueue, 1, &submitInfo, m_vulkanInFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;

        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {m_vulkanSwapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optionals

        vkQueuePresentKHR(m_vulkanPresentQueue, &presentInfo);
    }

    void renderLayer::cleanup()
    {
        vkDestroySemaphore(m_vulkanLogicalDevice, m_vulkanImageAvailableSemaphore, nullptr);

        vkDestroySemaphore(m_vulkanLogicalDevice, m_vulkanRenderFinishedSemaphore, nullptr);

        vkDestroyFence(m_vulkanLogicalDevice, m_vulkanInFlightFence, nullptr);

        vkDestroyCommandPool(m_vulkanLogicalDevice, m_vulkanCommandPool, nullptr);

        for (auto framebuffer : m_vulkanSwapChainFrameBuffers) 
        {
            vkDestroyFramebuffer(m_vulkanLogicalDevice, framebuffer, nullptr);
        }

        vkDestroyPipeline(m_vulkanLogicalDevice, m_vulkanGraphicsPipeline, nullptr);

        vkDestroyPipelineLayout(m_vulkanLogicalDevice, m_vulkanPipelineLayout, nullptr);

        vkDestroyRenderPass(m_vulkanLogicalDevice, m_vulkanRenderPass, nullptr);

        for (auto imageView : m_vulkanSwapChainImageViews)
        {
            vkDestroyImageView(m_vulkanLogicalDevice, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_vulkanLogicalDevice, m_vulkanSwapChain, nullptr);

        vkDestroyDevice(m_vulkanLogicalDevice, nullptr);

        if (enableValidationLayers) {
            destroyDebugUtilsMessengerEXT(m_vulkanInstancePtr, m_vulkanDebugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(m_vulkanInstancePtr, m_vulkanSurface, nullptr);

        vkDestroyInstance(m_vulkanInstancePtr, nullptr);

        glfwDestroyWindow(m_glfwWindowPtr);

        glfwTerminate();
    }

    ///
    /// DEBUG / VALIDATION
    ///

    bool renderLayer::checkValidationLayerSupport()
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

    bool renderLayer::checkDeviceSuitablity(const VkPhysicalDevice& device)
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

    bool renderLayer::checkDeviceExtensionSupport(const VkPhysicalDevice& device)
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

    std::vector<const char*> renderLayer::getRequiredExtensions() 
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