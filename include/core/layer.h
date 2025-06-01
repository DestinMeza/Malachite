#pragma once
#include <cstdint>

#include "layerFuncs.h"

namespace malachite
{
  class application;

  class layer
  {
    friend class application;

    public:
      layer(uint32_t id, layerFunctionConfig config);
      ~layer();

      const uint32_t& getLayerID()
      {
        return m_layerID;
      }

    private:
      void initalize();
      void postInitalize();
      void start(double& startTime);

      void update(double& deltaTime);

    private:
      uint32_t m_layerID;
      layerFunctionConfig m_config;
  };
}