#include "malpch.h"
#include "layer.h"

namespace malachite
{
  layer::layer(uint32_t id, layerFunctionConfig config)
    : m_layerID(id), m_config(config)
  {
        
  }

  layer::~layer(){}

  void layer::initalize()
  {
    m_config.initalize();
  }
  
  void layer::postInitalize()
  {
    m_config.postInitalize();
  }

  void layer::start(double& startTime)
  {
    m_config.start(startTime);
  }

  void layer::update(double& deltaTime)
  {
    m_config.update(deltaTime);
  }
}