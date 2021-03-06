
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <vector>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;

const char* VIDEO_FILE = "../data/balance.m4v";
const char* WINDOW_NAME = "mainwindow";
const Point SELECT_HALF_SIZE(125, 125);
const unsigned SELECT_LINE_WIDTH = 5;
const unsigned TEXT_LINE_PITCH = 16;

static void onMouse(int, int, int, int, void*);

int main(int argc, const char** argv)
{
    VideoCapture vc;
    std::vector<char> s(4096);

    if (!vc.open(VIDEO_FILE)) {
        CV_Error_(-1, ("failed to open video: \"%s\"", VIDEO_FILE));
        std::exit(1);
    }

    int key = 0;
    bool pause = false;
    Point selection(-1000, -1000);
    Mat pristine, a, b;

    namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
    resizeWindow(WINDOW_NAME,
                 (int)vc.get(CV_CAP_PROP_FRAME_WIDTH),
                 (int)vc.get(CV_CAP_PROP_FRAME_HEIGHT));
    setMouseCallback(WINDOW_NAME, onMouse, &selection);

    while (true)
    {
        if ((unsigned long) cvGetWindowHandle(WINDOW_NAME) == 0UL) {
            break;
        }

        if (!pause) {
            if (! vc.read(pristine)) {
                vc.set(CV_CAP_PROP_POS_FRAMES, 0U);
                vc.read(pristine);
            };
        }

        pristine.copyTo(a);

        Mat& post = a;

        rectangle(post,
                  selection - SELECT_HALF_SIZE,
                  selection + SELECT_HALF_SIZE,
                  Scalar(0,255,0), SELECT_LINE_WIDTH);

        std::sprintf(&s[0], "CNT: %5u", (unsigned) vc.get(CV_CAP_PROP_FRAME_COUNT));
        putText(post,
                &s[0],
                Point(vc.get(CV_CAP_PROP_FRAME_WIDTH)-200,TEXT_LINE_PITCH * 1),
                FONT_HERSHEY_PLAIN,
                1,
                Scalar(255,255,255));

        std::sprintf(&s[0], "F#:  %5u", (unsigned) vc.get(CV_CAP_PROP_POS_FRAMES));
        putText(post,
                &s[0],
                Point(vc.get(CV_CAP_PROP_FRAME_WIDTH)-200,TEXT_LINE_PITCH * 2),
                FONT_HERSHEY_PLAIN,
                1,
                Scalar(255,255,255));

        imshow(WINDOW_NAME, post);

        key = waitKey(1);

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

    std::cerr << "select=(" << p.x << "," << p.y << ")" << std::endl;
}
