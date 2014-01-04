/* vim: set ts=8 sw=8 : */

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <vector>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;
using namespace std;

const char*       VIDEO_FILE          = "../data/balance.m4v";

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

enum model_state { UNRESOLVED = 0, VALID = 1 };

typedef struct {
        int canny_threshold;

        struct selection_t {
                model_state state;
                Point pt;
                Rect rect;        
                double aspect_ratio;

                struct contour_t {
                        vector<Point> points;
                        double area;
                } inside, outside;
        } selection;

        struct key_zero_t {
                model_state state;        
                Point pt;
                Size size;
                unsigned skip;
        } key_zero;
} model_t;

static void get_selection_contours(model_t& model, Mat& scene)
{
        static Mat blur1, blur2, gray, binary;
        static vector<vector<Point>> contours;
        static vector<Vec4i> hierarchy;
        static vector<Point> hull;

        model_t::selection_t& selection = model.selection;

        if (selection.state == VALID)
                return;

        cout << 's' << flush;

        medianBlur(scene(model.selection.rect), blur1, 3);
        bilateralFilter(blur1, blur2, 5, 75, 75);
        cvtColor(blur2, gray, CV_BGR2GRAY);
        threshold(gray, binary, 100, 255, THRESH_BINARY_INV);
        findContours(binary, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_NONE);

        for (unsigned i = 0; i < hierarchy.size(); i++) {
                convexHull(contours[i], hull);
                double area = contourArea(hull);

                model_t::selection_t::contour_t& contour =
                        (hierarchy[i][3] == -1 ? selection.outside : selection.inside);

                contour.area = area;
                contour.points = hull;
        }

        if (selection.outside.points.size() > 2) {
                const RotatedRect& bounds = minAreaRect(selection.outside.points);
                selection.aspect_ratio = bounds.size.height / (double) bounds.size.width;

                selection.state = VALID;
        }
}

static void find_selection(model_t& model, Mat& scene)
{
        static Mat blur1, blur2, gray, binary;
        static vector<vector<Point>> contours;
        static vector<Vec4i> hierarchy;
        static vector<Point> hull;

        if (model.selection.state != VALID)
                return;

        Rect roi;

        if (model.key_zero.state == VALID) {
                if (++model.key_zero.skip < 5)
                        return;

                model.key_zero.skip = 0;

                Point p1, p2;
                p1 = p2 = model.key_zero.pt;
                p1.x -= model.key_zero.size.width * 0.75;
                p1.y -= model.key_zero.size.height * 0.75;
                p2.x += model.key_zero.size.width * 0.75;
                p2.y += model.key_zero.size.height * 0.75;
                roi = Rect(p1, p2);
        }
        else {
                roi = Rect(Point(),Point(scene.size().width,scene.size().height));
        }

        cout << 'z' << flush;
        model.key_zero.state = UNRESOLVED;

        medianBlur(scene(roi), blur1, 3);
        bilateralFilter(blur1, blur2, 5, 75, 75);
        cvtColor(blur2, gray, CV_BGR2GRAY);
        threshold(gray, binary, 100, 255, THRESH_BINARY_INV);
        findContours(binary, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_NONE);

        for (unsigned i = 0; i < contours.size(); i++) {
                if (hierarchy[i][3] != -1)
                        continue;

                double ratio = matchShapes(model.selection.outside.points,
                                contours[i], CV_CONTOURS_MATCH_I3, 0);

                if (ratio > 0.10) // poor match
                        continue;

                const RotatedRect bounds = minAreaRect(contours[i]);
                double aspect_ratio = bounds.size.height / (double) bounds.size.width; 

                if (fabs(aspect_ratio - model.selection.aspect_ratio) > 0.1)
                        continue;

                convexHull(contours[i], hull);

                if (fabs(contourArea(hull) - model.selection.outside.area) >
                                model.selection.outside.area * 0.1)
                        continue;

                if (contours[i].size() < 5)
                        continue;

                RotatedRect rect = fitEllipse(contours[i]);
                model.key_zero.pt = rect.center;
                model.key_zero.pt.x += roi.x;
                model.key_zero.pt.y += roi.y;
                model.key_zero.size = rect.boundingRect().size();
                model.key_zero.state = VALID;

                return;
        }
}

static void handle_mouse_event(int e, int x, int y, int flags, void* param)
{
        if (e != CV_EVENT_LBUTTONDOWN)
                return;

        model_t::selection_t& selection = ((model_t*) param)->selection;

        selection.state = UNRESOLVED;

        selection.pt.x = x;
        selection.pt.y = y;

        selection.rect.x = selection.pt.x - SELECT_SIZE.width / 2;
        selection.rect.y = selection.pt.y - SELECT_SIZE.height / 2;
        selection.rect.width = SELECT_SIZE.width;
        selection.rect.height = SELECT_SIZE.height;
}

static void draw_pip(model_t& model, Mat& scene)
{
        Rect pip_rect(Point(), SELECT_SIZE);
        scene(model.selection.rect).copyTo(scene(pip_rect));
}

static void draw_selection(model_t& model, Mat& scene)
{
        rectangle(scene, model.selection.rect, SELECT_COLOR, SELECT_LINE_WIDTH);

        vector<vector<Point>> cx;
        cx.push_back(model.selection.inside.points);
        cx.push_back(model.selection.outside.points);

        unsigned i;
        vector<Point> c;

        c = model.selection.inside.points;
        for (i = 0; i < c.size(); i++) c[i] *= 10;
        cx.push_back(c);

        c = model.selection.outside.points;
        for (i = 0; i < c.size(); i++) c[i] *= 10;
        cx.push_back(c);

        drawContours(scene, cx, -1, WHITE, 1, 8);
}

static void draw_match(model_t& model, Mat& scene)
{
        Point p1, p2;

        p1 = p2 = model.key_zero.pt;
        p1.x -= 7; p1.y -= 7; p2.x += 7; p2.y += 7;
        line(scene, p1, p2, RED, 3);

        p1 = p2 = model.key_zero.pt;
        p1.x -= 7; p1.y += 7; p2.x += 7; p2.y -= 7;
        line(scene, p1, p2, RED, 3);
}

static void draw_metrics(Mat& scene, VideoCapture& vc)
{
        static char s[4096];

        const int LEFT = vc.get(CV_CAP_PROP_FRAME_WIDTH) - 200;

        sprintf(s, "TOTAL: %5u", (unsigned) vc.get(CV_CAP_PROP_FRAME_COUNT));
        putText(scene, s, Point(LEFT, TEXT_LINE_PITCH * 1), FONT_HERSHEY_PLAIN, 1, WHITE);

        sprintf(s, "F#:  %5u", (unsigned) vc.get(CV_CAP_PROP_POS_FRAMES));
        putText(scene, s, Point(LEFT, TEXT_LINE_PITCH * 2), FONT_HERSHEY_PLAIN, 1, WHITE);
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

int main(int argc, const char** argv)
{
        VideoCapture vc;

        bool pause = false;
        bool run = true;

        namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
        moveWindow(WINDOW_NAME, WINDOW_X_POS, WINDOW_Y_POS);
        resizeWindow(WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT);

        model_t model = {};

        handle_mouse_event(CV_EVENT_LBUTTONDOWN, 200, 200, 0, (void*) &model);
        setMouseCallback(WINDOW_NAME, handle_mouse_event, (void*) &model); 

        model.canny_threshold = 35;
        createTrackbar("thresh", WINDOW_NAME, &model.canny_threshold, 100);

        Mat scene;

        while (run) {
                read_frame(vc, scene);
                get_selection_contours(model, scene);
                find_selection(model, scene);

                draw_pip(model, scene);
                draw_selection(model, scene);
                draw_metrics(scene, vc);
                draw_match(model, scene);

                // TODO: calculate the correct interval

                switch (waitKey(28)) {
                        case 32:
                                pause = !pause;
                                break;
                        case 27:
                                run = false;
                                break;
                }

                // detect a closed window
                if ((unsigned long) cvGetWindowHandle(WINDOW_NAME) == 0UL) {
                        break;
                }

                imshow(WINDOW_NAME, scene);
                cout << '.' << flush;
        }

        vc.release();

        return 0;
}

