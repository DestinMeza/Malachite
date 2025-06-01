#pragma once
#include <functional>

#define MAL_BIND_FUNCTION(func, instance) std::bind(&func, instance);
#define MAL_BIND_FUNCTION_PARAMS(func, instance, ...) std::bind(&func, instance, __VA_ARGS__);

namespace malachite
{
  class default_layerFuncs
  {
    friend struct layerFunctionConfig;

      static void InitalizeFunc(){ }
      static void PostInitalizeFunc(){ }
      static void StartFunc(double& startTime){ }
      static void UpdateFunc(double& deltaTime){ }
      static void PostCloseFunc(){}
  };

  struct layerFunctionConfig
  {
    //Fire once functions
    std::function<void()>         initalize     = default_layerFuncs::InitalizeFunc;
    std::function<void()>         postInitalize = default_layerFuncs::PostInitalizeFunc;
    std::function<void(double&)>  start         = default_layerFuncs::StartFunc;
    std::function<void()>           postClose     = default_layerFuncs::PostCloseFunc;

    //Per frame functions
    std::function<void(double&)>  update        = default_layerFuncs::UpdateFunc;
  };
}