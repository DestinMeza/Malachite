#include "malpch.h"
#include "layer.h"

namespace malachite
{
  layer::layer(layerID id, updateFunc updateFunc) 
    : m_layerID(id), m_updateFunc(updateFunc)
  {
        
  }

  layer::~layer(){}

  void layer::update(float delta)
  {
    m_updateFunc(delta);
  }
}