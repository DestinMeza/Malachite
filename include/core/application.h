#pragma once

#include "maltime.h"

#include <string>
#include <vector>

#define MAL_BIND_FUNCTION(func, app, paramCount) std::bind(&func, app, paramCount);

namespace malachite
{
  class layer;

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

    void initalize();
    void run();
    void close();

  private:
    void start();
    void update();

    maltime m_time;
    bool m_isRunning;
    std::vector<layer*> m_layers;
  };

  application* createApplication(appArgs args);
}