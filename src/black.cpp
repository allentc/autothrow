/* vim: set ts=8 sw=8 et : */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <string>
#include <sstream>
#include <vector>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;
using namespace std;

const char*       VIDEO_FILE          = "../data/balance.m4v";
double            VIDEO_FPS           = 30.293694;

const char*       WINDOW_NAME         = "mainwindow";
const int         WINDOW_WIDTH        = 1046;
const int         WINDOW_HEIGHT       = 641;
const int         WINDOW_X_POS        = 0;
const int         WINDOW_Y_POS        = 0;

const Scalar      WHITE               = Scalar(255, 255, 255);
const Scalar      GREEN               = Scalar(0, 255, 0);
const Scalar      RED                 = Scalar(0, 0, 255);

const Size        SELECT_SIZE         = Size(50, 80);
const unsigned    SELECT_LINE_WIDTH   = 2;
const Scalar      SELECT_COLOR        = GREEN;
const unsigned    TEXT_LINE_PITCH     = 16;

const char*       DUMP_FNAME          = "modeldump";

enum model_state { UNRESOLVED = 0, VALID = 1 };

typedef Vec<uchar, 3> bgr_t;

struct model_t {
        int      frame_wait; 
        uint64_t frame_last_ts_ms;

        struct bar_t {
                const Point roiNW = Point(735, 240), roiWH = Point(16, 480);
                const uint subdivs = 120;
                const uint rows_per_subdiv = roiWH.y / subdivs;
                const uint pix_per_subdiv = rows_per_subdiv * roiWH.x;
                const uint bar_height = subdivs * 0.27;
                const Rect rect = Rect(roiNW, roiNW + roiWH);
                Mat roi;
                vector<uint64> means = vector<uint64>(subdivs);
        } bar;

        struct mark_t {
                const Rect roi;
                const int threshhold_type;
                Vec4f line;
                Mat blurred, gray, binary, points, result;
                Point p1, p2;
        }
        mark    = { Rect(Point(445, 425), Point(499, 525)), THRESH_BINARY_INV },
        pointer = { Rect(Point(527, 425), Point(581, 525)), THRESH_BINARY     };

} model;

static void find_mark(model_t::mark_t& mark, Mat& scene)
{
        Vec4f& line = mark.line;

        blur(scene(mark.roi), mark.blurred, Size(50, 1), Point(-1, -1), BORDER_REFLECT);
        cvtColor(mark.blurred, mark.gray, CV_BGR2GRAY);
        threshold(mark.gray, mark.binary, 100, 255, mark.threshhold_type);
        findNonZero(mark.binary, mark.points);

        if (mark.points.size().height == 0) {
                return;
        }

        fitLine(mark.points, mark.line, CV_DIST_L2, 0, 0.01, 0.01);
        mark.result = scene(mark.roi);
        mark.p1 = Point(line[2] + line[0] * 50, line[3] + line[1] * 50);
        mark.p2 = Point(line[2] + line[0] * -50, line[3] + line[1] * -50);
        cv::line(mark.result, mark.p1, mark.p2, GREEN, 2, CV_AA);
        mark.result.copyTo(scene(mark.roi));
        rectangle(scene, mark.roi, RED, 2); 
}

static void find_beam(model_t& model, Mat& scene)
{
        model_t::bar_t& bar = model.bar;
        bar.roi = scene(bar.rect);
        uint64 acc;

        auto bgr = bar.roi.begin<bgr_t>();
        for (uint i = 0; i < bar.subdivs; i++) {
                acc = 0;
                for (uint j = 0; j < bar.pix_per_subdiv; j++, bgr++) {
                        acc += (*bgr)[0] * (*bgr)[1] * (*bgr)[2];
                }
                bar.means[i] = acc / bar.pix_per_subdiv;
        }

        acc = 0;
        for (uint i = 0; i < bar.bar_height; i++) {
                acc += bar.means[i];
        }

        uint least_idx = 0;
        uint64 least_val = acc;

        for (uint i = 0; i < bar.subdivs - bar.bar_height; i++) {
                if (acc < least_val) {
                        least_idx = i;
                        least_val = acc;
                }
                acc -= bar.means[i];
                acc += bar.means[i + bar.bar_height];
        }

        uint y1 = bar.roiNW.y + bar.rows_per_subdiv * least_idx;
        uint y2 = y1 + bar.rows_per_subdiv * bar.bar_height;
        Rect rect(Point(bar.roiNW.x, y1), Point(bar.roiNW.x + bar.roiWH.x, y2));
        rectangle(scene, rect, RED, CV_FILLED); 
}

static void read_frame(VideoCapture& vc, Mat& m)
{
        unsigned frame_count, frame_num;

        if (vc.isOpened()) {
                frame_count = (unsigned) vc.get(CV_CAP_PROP_FRAME_COUNT);
                frame_num   = (unsigned) vc.get(CV_CAP_PROP_POS_FRAMES);

                if (frame_num == frame_count)
                        vc.release();
        }

        if (!vc.isOpened()) {
                if (!vc.open(VIDEO_FILE)) {
                        CV_Error_(-1, ("failed to open video: \"%s\"", VIDEO_FILE));
                        exit(1);
                }
        }

        vc.read(m);
}

static void compute_interval(model_t& model)
{
        struct timespec ts = {};
        (void) clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t ts_ms = (uint64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        if (model.frame_last_ts_ms == 0) {
                model.frame_last_ts_ms = ts_ms;
                model.frame_wait = 1;
                return;
        }

        uint64_t next_frame_ts_ms = model.frame_last_ts_ms + 1000 / VIDEO_FPS;
        model.frame_wait = next_frame_ts_ms - ts_ms;

        if (model.frame_wait < 1)
                model.frame_wait = 1;

        if (model.frame_wait > 1000 / VIDEO_FPS)
                model.frame_wait = 1000 / VIDEO_FPS;

        model.frame_last_ts_ms = ts_ms;
}

int main(int argc, const char** argv)
{
        VideoCapture vc;

        bool pause = false;
        bool run = true;
        int key;

        cout << "\033[2J";

        namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
        moveWindow(WINDOW_NAME, WINDOW_X_POS, WINDOW_Y_POS);
        resizeWindow(WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT);

        Mat scene;

        while (run) {
                if (!pause) {
                        read_frame(vc, scene);
                        find_beam(model, scene);
                        find_mark(model.mark, scene);
                        find_mark(model.pointer, scene);
                        compute_interval(model);
                }

                switch (key = waitKey(model.frame_wait)) {
                        case 32: //space
                                pause = !pause;
                                break;
                        case 27: //esc
                                run = false;
                                break;
                        default:
                                if (key != -1)
                                        cout << "key=" << key << endl;
                }

                // detect a closed window
                if ((unsigned long) cvGetWindowHandle(WINDOW_NAME) == 0UL) {
                        break;
                }

                if (!pause) {
                        imshow(WINDOW_NAME, scene);
                        //cout << '.' << flush;
                }
        }

        vc.release();

        return 0;
}

