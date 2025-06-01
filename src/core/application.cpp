#include "malpch.h"

#include "application.h"
#include "renderlayer.h"

namespace malachite
{
    application* application::s_instance = nullptr;

    application::application(const std::string& name, appArgs appArgs)
    {
        s_instance = this;

        addLayer(new renderlayer());
    }

    void application::initalize()
    {
        m_time.initalizeTime = std::chrono::steady_clock::now();

        //First call after application creation.
        for (auto layer : m_layers)
        {
            layer->initalize();
        }

        for (auto layer : m_layers)
        {
            layer->postInitalize();
        }
    }

    void application::start()
    {
        m_isRunning = true;
        m_time.startTime = std::chrono::steady_clock::now();

        for (auto layer : m_layers)
        {
            double layerStartTime = m_time.getTimeElapsedSinceStart();

            layer->start(layerStartTime);
        }
    }

    void application::run()
    {
        start();
        update();
    }
    
    void application::update()
    {
        //This is to ensure last frame is older than start frame.
        m_time.updateLastFrameTime = std::chrono::steady_clock::now();

        while (m_isRunning)
        {        
            m_time.updateStartFrameTime = std::chrono::steady_clock::now();

            for (auto layer : m_layers)
            {
                double deltaTime = m_time.getFrameDeltaTime();

                layer->update(deltaTime);
            }

            m_time.updateLastFrameTime = std::chrono::steady_clock::now();
        }
    }

    application::~application()
    {
        for (auto layer : m_layers)
        {
            free(layer);
        }

        s_instance = nullptr;
    }

    void application::close()
    {
        m_isRunning = false;
    }

    void application::addLayer(layer* layer)
    {
        m_layers.push_back(layer);
    }
}