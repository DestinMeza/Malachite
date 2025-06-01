#pragma once
#include "application.h"

extern malachite::application* malachite::createApplication(malachite::appArgs args);

int main(int argc, char** argv)
{
    malachite::application* application = malachite::createApplication({argc, argv});

    application->initalize();
    application->run();

    return 0;
}