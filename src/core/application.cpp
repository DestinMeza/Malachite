#include "core/application.h"

namespace malachite
{
    application::application(const std::string& name, appArgs appArgs)
    {

    }

    int application::run()
    {
        int errorCode = 0;
        while (isRunning)
        {
            /* code */
        }
        
        return errorCode;
    }

    void application::close()
    {
        isRunning = true;
    }
}