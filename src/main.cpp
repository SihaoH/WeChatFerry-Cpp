#include "Application.h"
#include "Logger.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);

    return app.exec();
}
