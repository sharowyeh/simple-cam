/*
test code from libcamera offical documantation, 
refer to: https://github.com/raspberrypi/libcamera/blob/main/Documentation/guides/application-developer.rst
*/

//#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

#include <libcamera/libcamera.h>
#include <libcamera/camera_manager.h>

using namespace libcamera;
using namespace std::chrono_literals;

static std::shared_ptr<Camera> camera;

int main()
{
    std::string camId;

    std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
    cm->start();
    // list all cameras
    for (auto const &c : cm->cameras()) {
        std::cout << c->id() << std::endl;
        if (camId.empty()) {
            camId = c->id();
        }
    }

    if (camId.empty()) {
        std::cout << "no camera found" << std::endl;
        return -1;
    }

    camera = cm->get(camId);
    // acquire an exclusive lock on the camera
    auto ret = camera->acquire();

    std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration( { StreamRole::Viewfinder } );
    
    // get default config from the camera
    StreamConfiguration &streamConfig = config->at(0);
    std::cout << "Default viewfinder configuration is: " << streamConfig.toString() << std::endl;
    
    // change config for validation
    streamConfig.size.width = 1920;
    streamConfig.size.height = 1080;
    config->validate();
    std::cout << "Validated viewfinder configuration is: " << streamConfig.toString() << std::endl;



    // clean up and quit
    camera->stop();
    // TODO: release allocated frame buffer
    // delete buff;
    camera->release();
    camera.reset(); // reset weak ptr
    cm->stop();

    return 0;
}
