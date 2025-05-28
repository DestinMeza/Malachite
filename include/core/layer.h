#pragma once

#include "malpch.h"

namespace malachite
{
  struct layerID
  {
    uint32_t id;
  };

  class layer
  {
    public:
    virtual ~layer() = 0;
    virtual layerID getLayerID() = 0;
  };
}