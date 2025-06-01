#pragma once
#include <chrono>

#define MAL_CAST_TIME(x) std::chrono::duration_cast<std::chrono::duration<double>>(x)

#define MAL_TIME(x) MAL_CAST_TIME(x).count()

namespace malachite
{
  class maltime
  {
    public:
    maltime();

    // Time when application creation is called.
    std::chrono::steady_clock::time_point beginTime;

    // Time when applicaiton initalize is called.
    std::chrono::steady_clock::time_point initalizeTime;

    // Time when application start is called.
    std::chrono::steady_clock::time_point startTime;
    
    // Time when application close is called;
    std::chrono::steady_clock::time_point closeTime;

    //These two are used for delta time for between frame calculations
    // Time of start frame
    std::chrono::steady_clock::time_point updateStartFrameTime;

    // Time of last frame
    std::chrono::steady_clock::time_point updateLastFrameTime;

    //These two are used for delta time for between layer calculations
    // Time of start of layer
    std::chrono::steady_clock::time_point updateStartLayerFrameTime;

    // Time of last of layer
    std::chrono::steady_clock::time_point updateLastLayerFrameTime;

    double getTimeElapsedSinceStart();
    double getFrameDeltaTime();
    double getLayerDeltaTime();

    double getTimeElapsedSinceStart_milliseconds();
    double getFrameDeltaTime_milliseconds();
    double getLayerDeltaTime_milliseconds();
  };
}