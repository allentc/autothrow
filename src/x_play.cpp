
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
    Mat pristine, dirty, dirty1, dirty2;

    namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
    resizeWindow(WINDOW_NAME,
                 (int)vc.get(CV_CAP_PROP_FRAME_WIDTH),
                 (int)vc.get(CV_CAP_PROP_FRAME_HEIGHT));
    setMouseCallback(WINDOW_NAME, onMouse, &selection);

    /*
    SimpleBlobDetector::Params params;

    params.filterByCircularity = false;
    params.filterByInertia = false;
    params.filterByConvexity = false;

    params.minDistBetweenBlobs = 0.0f;

    params.filterByArea = false;
    params.minArea = 10.0f;
    params.maxArea = 10000000.0f;

    params.filterByColor = false;

    FeatureDetector* detector = new SimpleBlobDetector(params);
    */

    std::vector<KeyPoint> keypoints;

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

        //pristine.copyTo(dirty);

        //GaussianBlur(pristine, dirty, Size(5, 5), 5.0);

        //medianBlur(pristine, dirty1, 5);

        vector<Mat> rgb;
        split(pristine, rgb);
        rgb[0] = rgb[0] * 2.0;
        rgb[1] = rgb[1] * 3.0;
        rgb[2] = rgb[2] * 2.0;
        merge(&rgb[0], rgb.size(), dirty1);


        cvtColor(dirty1, dirty2, CV_BGR2GRAY);
        threshold(dirty2, dirty1, 75, 255, THRESH_BINARY);

        Mat new_image = Mat::zeros(dirty1.size(), dirty1.type());
        Mat sub_mat = Mat::ones(dirty1.size(), dirty1.type())*255;
        subtract(sub_mat, dirty1, new_image);       

        new_image.copyTo(dirty);

        /*
        keypoints.clear();
        detector->detect(dirty, keypoints);
        for (auto i = keypoints.begin(); i != keypoints.end(); i++) {
            circle(dirty, i->pt, i->size, Scalar(255,0,0), -1);
        }
        std::cout << "keypoints.size()=" << keypoints.size() << std::endl;
        */

        rectangle(dirty,
                  selection - SELECT_HALF_SIZE,
                  selection + SELECT_HALF_SIZE,
                  Scalar(0,255,0), SELECT_LINE_WIDTH);

        std::sprintf(&s[0], "CNT: %5u", (unsigned) vc.get(CV_CAP_PROP_FRAME_COUNT));
        putText(dirty,
                &s[0],
                Point(vc.get(CV_CAP_PROP_FRAME_WIDTH)-200,TEXT_LINE_PITCH * 1),
                FONT_HERSHEY_PLAIN,
                1,
                Scalar(255,255,255));

        std::sprintf(&s[0], "F#:  %5u", (unsigned) vc.get(CV_CAP_PROP_POS_FRAMES));
        putText(dirty,
                &s[0],
                Point(vc.get(CV_CAP_PROP_FRAME_WIDTH)-200,TEXT_LINE_PITCH * 2),
                FONT_HERSHEY_PLAIN,
                1,
                Scalar(255,255,255));

        imshow(WINDOW_NAME, dirty);

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
