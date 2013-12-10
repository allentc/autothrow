
#include <cstdlib>
#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;

const char* videoFile = "../data/balance.m4v";
const char* windowName = "mainwindow";

static void onMouse(int, int, int, int, void*);

int main(int argc, const char** argv)
{
    VideoCapture vc;

    if (!vc.open(videoFile)) {
        CV_Error_(-1, ("failed to open video: \"%s\"", videoFile));
        std::exit(1);
    }

    int key = 0;
    bool close = false, pause = false;
    Mat frame;

    namedWindow(windowName, CV_WINDOW_NORMAL);
    setMouseCallback(windowName, onMouse, 0);

    while (!close) {
        if (!pause) {
            vc >> frame;
            imshow(windowName, frame);
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

    std::cerr << "e=" << e << " x=" << x << " y=" << y << " flags=" << flags
            << std::endl;
}
