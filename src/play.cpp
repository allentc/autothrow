
#include <cstdlib>
#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;

const char* VIDEO_FILE = "../data/balance.m4v";
const char* WINDOW_NAME = "mainwindow";
const Point SELECT_RECT_SIZE(125, 125); /*radius*/

static void onMouse(int, int, int, int, void*);

int main(int argc, const char** argv)
{
    VideoCapture vc;

    if (!vc.open(VIDEO_FILE)) {
        CV_Error_(-1, ("failed to open video: \"%s\"", VIDEO_FILE));
        std::exit(1);
    }

    int key = 0;
    bool pause = false;
    Point p(-1000, -1000);
    Mat frame;

    namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
    setMouseCallback(WINDOW_NAME, onMouse, &p);

    while (true)
    {
        if ((unsigned long) cvGetWindowHandle(WINDOW_NAME) == 0UL) {
            break;
        }

        if (!pause) {
            vc >> frame;

            rectangle(frame, p - SELECT_RECT_SIZE, p + SELECT_RECT_SIZE, Scalar(0,255,0), 5);
            imshow(WINDOW_NAME, frame);
        }

        key = waitKey(5);

        if (key == 27) {
            break;
        }
        else if (key == 32) {
            pause = !pause;
        }

        if (key != -1) {
            std::cerr << "key=" << key << std::endl;
        }
    }

    vc.release();

    return 0;
}

void onMouse(int e, int x, int y, int flags, void* param)
{
    if (e != 1) {
        return;
    }

    Point& p = *(Point*)param;
    p.x = x;
    p.y = y;
}
