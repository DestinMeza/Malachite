#pragma once
#include <functional>

namespace malachite
{
  class default_layerFuncs
  {
    friend struct layerFunctionConfig;

      static void InitalizeFunc(){ }
      static void PostInitalizeFunc(){ }
      static void StartFunc(double& startTime){ }
      static void UpdateFunc(double& deltaTime){ }
  };

  struct layerFunctionConfig
  {
    //Fire once functions
    std::function<void()>         initalize     = default_layerFuncs::InitalizeFunc;
    std::function<void()>         postInitalize = default_layerFuncs::PostInitalizeFunc;
    std::function<void(double&)>  start         = default_layerFuncs::StartFunc;

    //Per frame functions
    std::function<void(double&)>  update        = default_layerFuncs::UpdateFunc;
  };
}