#include<iostream>
#include "MyTriangle.h"
int main()
{
    MyTriangle triangleApp("My Vulkan Triangle", 1280, 720);
    triangleApp.Setup();
    triangleApp.Run();
    triangleApp.Cleanup();
    return 0;
}
