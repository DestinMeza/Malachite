#include "malpch.h"
#include "maltime.h"

namespace malachite
{
  maltime::maltime()
  {
    beginTime = std::chrono::steady_clock::now();
  }

  //Seconds
  double maltime::getTimeElapsedSinceStart()
  {
    return getTimeElapsedSinceStart_milliseconds() * 100;
  }

  double maltime::getFrameDeltaTime()
  {
    return getFrameDeltaTime_milliseconds() * 100;
  }

  double maltime::getLayerDeltaTime()
  {
    return getLayerDeltaTime_milliseconds() * 100;
  }

  //Milliseconds
  double maltime::getTimeElapsedSinceStart_milliseconds()
  {
    return MAL_TIME(startTime - std::chrono::steady_clock::now());
  }

  double maltime::getFrameDeltaTime_milliseconds()
  {
    return MAL_TIME(updateStartFrameTime - updateLastFrameTime);
  }

  double maltime::getLayerDeltaTime_milliseconds()
  {
    return MAL_TIME(updateStartLayerFrameTime - updateLastFrameTime);
  }
}