#pragma once

#include <iostream>
#include "core/application.h"

extern malachite::application* malachite::createApplication(malachite::appArgs args);

int main(int argc, char** argv)
{
    malachite::application* application = malachite::createApplication({argc, argv});

    int errorCode = application->run();
    std::cout << errorCode << std::endl;
}