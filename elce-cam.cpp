#include <iostream>
#include <memory>

#include <libcamera/libcamera.h>

using namespace libcamera;
std::shared_ptr<Camera> camera;

/*
 * --------------------------------------------------------------------
 * Handle RequestComplete
 *
 * For each Camera::requestCompleted Signal emitted from the Camera the
 * connected Slot is invoked.
 *
 * The Slot receives as parameters the Request it refers to and a map of
 * Stream to Buffer containing image data.
 */
static void requestComplete(Request *request,
			    const std::map<Stream *, Buffer *> &buffers)
{
	if (request->status() == Request::RequestCancelled)
		return;

	for (auto const &it : buffers) {
		Buffer *buffer = it.second;

		std::cout << " (" << buffer->index() << ")"
			  << " seq: " << buffer->sequence() << " bytesused: "
			  << buffer->bytesused() << std::endl;

		/* Here you can access image data! */
	}

	/*
	 * Re-queue a Request to the camera.
	 *
	 * Request and Buffer are transient objects, they are re-created on
	 * the fly and re-queued.
	 */
	request = camera->createRequest();
	for (auto const &it : buffers) {
		Stream *stream = it.first;
		Buffer *buffer = it.second;
		unsigned int index = buffer->index();

		std::unique_ptr<Buffer> newBuffer = stream->createBuffer(index);
		request->addBuffer(std::move(newBuffer));
	}

	camera->queueRequest(request);
}

int main()
{
	/*
	 * --------------------------------------------------------------------
	 * Create a Camera Manager.
	 *
	 * The Camera Manager is responsible for enumerating all the Camera
	 * in the system, by associating Pipeline Handlers with media entities
	 * registered in the system.
	 *
	 * The CameraManager provides a list of available Camera that
	 * applications can operate on.
	 */
	CameraManager *cm = new CameraManager();
	cm->start();

	/*
	 * Just as a test, list all names of the Camera registered in the
	 * system. They are indexed by name by the CameraManager.
	 */
	for (auto const &camera : cm->cameras())
		std::cout << camera->name() << std::endl;

	/*
	 * --------------------------------------------------------------------
	 * Camera
	 *
	 * Camera are entities created by pipeline handlers, inspecting the
	 * entities registered in the system and reported to applications
	 * by the CameraManager.
	 *
	 * In general terms, a Camera corresponds to a single image source
	 * available in the system, such as an image sensor.
	 *
	 * Application lock usage of Camera by 'acquiring' them.
	 * Once done with it, application shall similarly 'release' the Camera.
	 *
	 * As an example, use the first available camera in the system.
	 */
	std::string cameraName = cm->cameras()[0]->name();
	camera = cm->get(cameraName);
	camera->acquire();

	/*
	 * Stream
	 *
	 * Each Camera supports a variable number of Stream. A Stream is
	 * produced by processing data produced by an image source, usually
	 * by an ISP.
	 *
	 *   +-------------------------------------------------------+
	 *   | Camera                                                |
	 *   |                +-----------+                          |
	 *   | +--------+     |           |------> [  Main output  ] |
	 *   | | Image  |     |           |                          |
	 *   | |        |---->|    ISP    |------> [   Viewfinder  ] |
	 *   | | Source |     |           |                          |
	 *   | +--------+     |           |------> [ Still Capture ] |
	 *   |                +-----------+                          |
	 *   +-------------------------------------------------------+
	 *
	 * The number and capabilities of the Stream in a Camera are
	 * a platform dependent property, and it's the pipeline handler
	 * implementation that has the responsibility of correctly
	 * report them.
	 */

	/*
	 * --------------------------------------------------------------------
	 * Camera Configuration.
	 *
	 * Camera configuration is tricky! It boils down to assign resources
	 * of the system (such as DMA engines, scalers, format converters) to
	 * the different image streams an application has requested.
	 *
	 * Depending on the system characteristics, some combinations of
	 * sizes, formats and stream usages might or might not be possible.
	 *
	 * A Camera produces a CameraConfigration based on a set of intended
	 * roles for each Stream the application requires.
	 */
	std::unique_ptr<CameraConfiguration> config =
		camera->generateConfiguration( { StreamRole::Viewfinder } );

	/*
	 * The CameraConfiguration contains a StreamConfiguration instance
	 * for each StreamRole requested by the application, provided
	 * the Camera can support all of them.
	 *
	 * Each StreamConfiguration has default size and format, assigned
	 * by the Camera depending on the Role the application has requested.
	 */
	StreamConfiguration &streamConfig = config->at(0);
	std::cout << "Default viewfinder configuration is: "
		  << streamConfig.toString() << std::endl;

	/*
	 * Each StreamConfiguration parameter which is part of a
	 * CameraConfiguration can be independently modified by the
	 * application.
	 *
	 * In order to validate the modified parameter, the CameraConfiguration
	 * should be validated -before- the CameraConfiguration gets applied
	 * to the Camera.
	 *
	 * The CameraConfiguration validation process adjusts each
	 * StreamConfiguration to a valid value.
	 */

	/*
	 * The Camera configuration procedure fails with invalid parameters.
	 */
#if 0
	streamConfig.size.width = 0; //4096
	streamConfig.size.height = 0; //2560

	int ret = camera->configure(config.get());
	if (ret) {
		std::cout << "CONFIGURATION FAILED!" << std::endl;
		return -1;
	}
#endif

	/*
	 * Validating a CameraConfiguration -before- applying it adjust it
	 * to a valid configuration as closest as possible to the requested one.
	 */
	config->validate();
	std::cout << "Validated viewfinder configuration is: "
		  << streamConfig.toString() << std::endl;

	/*
	 * Once we have a validate configuration, we can apply it
	 * to the Camera.
	 */
	camera->configure(config.get());

	/*
	 * --------------------------------------------------------------------
	 * Buffer Allocation
	 *
	 * Now that a camera has been configured, it knows all about its
	 * Streams sizes and formats, so we now have to ask it to reserve
	 * memory for all of them.
	 */
	camera->allocateBuffers();

	/*
	 * --------------------------------------------------------------------
	 * Frame Capture
	 *
	 * Libcamera frames capture model is based on the 'Request' concept.
	 * For each frame a Request has to be queued to the Camera.
	 *
	 * A Request refers to (at least one) Stream for which a Buffer that
	 * will be filled with image data shall be added to the Request.
	 *
	 * A Request is associated with a list of Controls, which are tunable
	 * parameters (similar to v4l2_controls) that have to be applied to
	 * the image.
	 *
	 * Once a request completes, all its buffers will contain image data
	 * that applications can access and for each of them a list of metadata
	 * properties that reports the capture parameters applied to the image.
	 */
	Stream *stream = streamConfig.stream();
	std::vector<Request *> requests;
	for (unsigned int i = 0; i < streamConfig.bufferCount; ++i) {
		Request *request = camera->createRequest();
		std::unique_ptr<Buffer> buffer = stream->createBuffer(i);
		request->addBuffer(std::move(buffer));

		requests.push_back(request);

		/*
		 * todo: Set controls
		 *
		 * ControlList &Request::controls();
		 * controls.set(controls::Brightness, 255);
		 */
	}

	/*
	 * --------------------------------------------------------------------
	 * Signal&Slots
	 *
	 * Libcamera uses a Signal&Slot based system to connect events to
	 * callback operations meant to handle them, inspired by the QT graphic
	 * toolkit.
	 *
	 * Signals are events 'emitted' by a class instance.
	 * Slots are callbacks that can be 'connected' to a Signal.
	 *
	 * A Camera exposes Signals, to report the completion of a Request and
	 * the completion of a Buffer part of a Request to support partial
	 * Request completions.
	 *
	 * In order to receive the notification for request completions,
	 * applications shall connecte a Slot to the Camera 'requestCompleted'
	 * Signal before the camera is started.
	 */
	camera->requestCompleted.connect(requestComplete);

	/*
	 * --------------------------------------------------------------------
	 * Start Capture
	 *
	 * In order to capture frames the Camera has to be started and
	 * Request queued to it. Enough Request to fill the Camera pipeline
	 * depth have to be queued before the Camera start delivering frames.
	 *
	 * For each delivered frame, the Slot connected to the
	 * Camera::requestCompleted Signal is called.
	 */
	camera->start();
	for (Request *request : requests)
		camera->queueRequest(request);

	/*
	 * --------------------------------------------------------------------
	 * Run an EventLoop
	 *
	 * In order to dispatch events received from the video devices, such
	 * as buffer completions, an even loop as to be run.
	 *
	 * Libcamera provides its own default event dispatcher realized by
	 * polling a set of file descriptors, but applications can integrate
	 * their own even loop with the Libcamera EventDispatcher.
	 *
	 * Here, as an example, run the poll-based EventDispatcher for 3
	 * seconds.
	 */
	EventDispatcher *dispatcher = cm->eventDispatcher();
	Timer timer;
	timer.start(3000);
	while (timer.isRunning())
		dispatcher->processEvents();

	/*
	 * --------------------------------------------------------------------
	 * Clean Up
	 *
	 * Stop the Camera, release resources and stop the CameraManager.
	 * Libcamera has now released all resources it owned.
	 */
	camera->stop();
	camera->freeBuffers();
	camera->release();
	camera.reset();
	cm->stop();

	return 0;
}