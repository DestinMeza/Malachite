#pragma once
#include <string>
#include <vector>
#include "layer.h"

namespace malachite
{
  struct appArgs
  {
    int argCount;
    char** args = nullptr;
  };

  class application
  {
    static application* s_instance;

  public:
    application(const std::string& name = "Malachite", appArgs args = appArgs());
    ~application();

    void addLayer(layer* layer);

    int run();
    void close();
  private:
    bool m_isRunning;
    std::vector<layer*> m_layers;
  };

  application* createApplication(appArgs args);
}