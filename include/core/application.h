#pragma once

#include "malpch.h"

namespace malachite
{
  struct appArgs
  {
    int argCount;
    char** args = nullptr;
  };

  class application
  {
  public:
    application(const std::string& name = "Malachite", appArgs args = appArgs());

    int run();
    void close();
  private:
    bool isRunning;

   static application* s_Instance;
  };

  application* createApplication(appArgs args);
}