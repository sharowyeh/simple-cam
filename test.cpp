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

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

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
    std::cout << "Acquire result: " << ret << std::endl;

    std::unique_ptr<CameraConfiguration> config = camera->generateConfiguration( { StreamRole::Viewfinder } );
    
    // get default config from the camera
    StreamConfiguration &streamConfig = config->at(0);
    std::cout << "Default viewfinder configuration is: " << streamConfig.toString() << std::endl;
    
    // change config for validation
    streamConfig.size.width = 1280;
    streamConfig.size.height = 720;
    streamConfig.pixelFormat = PixelFormat::fromString("BGR888");
    config->validate();
    std::cout << "Validated viewfinder configuration is: " << streamConfig.toString() << std::endl;
    camera->configure(config.get());

    // clean up and quit
    camera->stop();
    // TODO: release allocated frame buffer
    // delete buff;
    camera->release();
    camera.reset(); // reset weak ptr
    cm->stop();

    // TODO: following opencv video capture does not work well via plugins...
    //   use above libcamera APIs to allocate frame buffer for image?

    // in rpi5, opencv default using gstreamer(if installed via plugins),
    // but gstreamer default integrated raspivid(by broadcom video core firmware at /opt/vc),
    // and video core userland APIs has been deprecated, that need to try via v4l2src or libcamerasrc
    cv::VideoCapture cap(0, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cout << "Cannot open camera from cv directly" << std::endl;
        return -1;
    }

    // for v4l2 video capture properties, can refer to cmdlet
    // > v4l2-ctl --device /dev/video0 --info --all
    // in `Format Video Capture:` section
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1600);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 900);
    cap.set(cv::CAP_PROP_FPS, 60);
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('B', 'G', 'R', '3'));

    cv::Mat frame/*(720, 1280, CV_8UC3)*/;
    while (true) {
        // auto white = cv::Mat::ones(128, 72, CV_32FC3);
        // cv::imshow("debug", white); // this is work
        // cv::waitKey(100);
        // continue;
        
        auto ret = cap.grab();
        ret &= cap.retrieve(frame);
        //auto ret = cap.read(frame);
        if (!ret) {
            std::cout << "Cannot retrieve frame" << std::endl;
            break;
        }
        std::cout << "cols: " << frame.cols << " rows: " << frame.rows << " dims: " << frame.dims;
        std::cout << " size: " << frame.elemSize() << " depth: " << frame.depth() << std::endl;
        std::cout << "val: " << (int)(frame.at<uchar>(640, 480)) << std::endl;

        cv::imshow("video", frame);

        auto key = cv::waitKey(100);
        if (key == 'c') {
            break;
        }
    }

    return 0;
}
