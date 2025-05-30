#include <ctime>

#include "malpch.h"
#include "application.h"


namespace malachite
{
    application* application::s_instance = nullptr;

    application::application(const std::string& name, appArgs appArgs)
    {
        s_instance = this;
    }

    int application::run()
    {
        m_isRunning = true;

        int errorCode = 0;

        const clock_t begin_time = clock();
        float deltaTime = 0;

        while (m_isRunning)
        {        
            deltaTime = float( clock() - begin_time ) /  CLOCKS_PER_SEC;

            for(auto layer : m_layers)
            {
                layer->update(deltaTime);
            }
        }
        
        return errorCode;
    }

    application::~application()
    {
        s_instance = nullptr;
    }

    void application::close()
    {
        m_isRunning = false;
    }

    void application::addLayer(layer* layer)
    {
        m_layers.emplace_back(layer);
    }
}