# NOTE: for picamera2 requres libcamera and pykms that are native cython wrappers
#       to OS dependency libs, and are not duplicated to virtual env.
#       just like https://github.com/JungLearnBot/RPi5_yolov8/blob/main/Readme.RPi5.cpu.picam.qt.md says
#       manually copy from /usr/lib/python3/dist-packages/ to .venv/lib64/site-packages/

import cv2
import numpy as np
import time
from picamera2 import Picamera2, Preview

picam = Picamera2()
#config = picam.create_preview_configuration(main={"format": 'XRGB8888', "size": (1280, 720)})
config = picam.create_preview_configuration(main={"format": 'BGR888', "size": (1280, 720)})
picam.configure(config)

# NOTE: need install PyQt
#picam.start_preview(Preview.QTGL)

picam.start()
time.sleep(2)
picam.capture_file("test.jpg")

# NOTE: beaware of pixel format bgr->rgb
pimg = picam.capture_array()
cv2.imwrite("picam.jpg", pimg)

picam.close()

exit()

# NOTE: cv2 imshow cannot works this way in rpi
cv2.startWindowThread()
width = 640
height = 480
while True:
    pimg = picam.capture_array()
    
    #cv2.imshow("picam", pimg)

    img = np.zeros((width,height,3), np.uint8)
    img[:, 0:width//2] = (255, 0, 0)
    img[:, width//2:width] = (0, 255, 0)

    #cv2.imshow("dbg", img)

    # NOTE: just press ctrl+c maybe...
    cv2.waitKey(100)

