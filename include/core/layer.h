#pragma once

#include <functional>
#include <cstdint>

namespace malachite
{

  typedef uint32_t layerID;
  typedef std::function<void(float&)> updateFunc;

  class layer
  {
    public:
      layer(layerID id, updateFunc updateFunc);

      const layerID& getLayerID()
      {
          return m_layerID;
      }

    public:
      virtual ~layer();
      virtual void update(float deltaTime);
      
    protected:
      layerID m_layerID;

    private:
      updateFunc m_updateFunc;
  };
}