#include "malpch.h"
#include "renderlayer.h"
#include "application.h"

#include <iostream>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

const std::vector<const char*> validationLayers = 
{
    "VK_LAYER_KHRONOS_validation"
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
        initalizeDebugMessenger();
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
        
        m_windowPtr = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        std::cout << "Window Initalization Sucessful." << std::endl;
    }

    void renderlayer::initalizeVulkan()
    {
        std::cout << "Initalizing Vulkan..." << std::endl;

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

        //Extensions
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

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

        if (CreateDebugUtilsMessengerEXT(m_vulkanInstancePtr, &createInfo, nullptr, &m_vulkanDebugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void renderlayer::render(double& deltaTime)
    {
        GLFWwindow* window = (GLFWwindow*)m_windowPtr;
        if(glfwWindowShouldClose(window))
        {
            application::closeApp();
            return;
        }

        glfwPollEvents();
    }

    void renderlayer::cleanup()
    {
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(m_vulkanInstancePtr, m_vulkanDebugMessenger, nullptr);
        }

        vkDestroyInstance(m_vulkanInstancePtr, nullptr);

        GLFWwindow* window = (GLFWwindow*)m_windowPtr;

        glfwDestroyWindow(window);

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