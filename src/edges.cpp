/* vim: set ts=8 sw=8 et : */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
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

const vector<Point2i> KEY_ZERO_OUTSIDE_CONTOUR =
{
        {41, 51}, {39, 59}, {37, 63}, {32, 68},
        {30, 69}, {27, 70}, {21, 70}, {15, 68},
        { 9, 62}, { 7, 56}, { 6, 50}, { 6, 35},
        { 8, 25}, {12, 19}, {14, 17}, {17, 15},
        {22, 14}, {25, 14}, {30, 15}, {34, 17},
        {36, 19}, {38, 22}, {40, 28}, {41, 34}
};

const vector<Point2i> KEY_ZERO_INSIDE_CONTOUR =
{
        {31, 46}, {30, 55}, {29, 58}, {26, 61},
        {21, 61}, {18, 58}, {17, 56}, {16, 49},
        {16, 48}, {17, 30}, {18, 27}, {22, 23},
        {26, 23}, {30, 27}, {31, 32}
};

const double KEY_ZERO_OUTSIDE_ASPECT_RATIO = 1.6;
const double KEY_ZERO_INSIDE_ASPECT_RATIO  = 2.55;
const double KEY_ZERO_CONTOURS_AREA_RATIO  = 3.385;

enum model_state { UNRESOLVED = 0, VALID = 1 };

typedef struct {
        int canny_threshold;

        int      frame_wait; 
        uint64_t frame_last_ts_ms;

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
                Point2f pt;
                Size size;
                unsigned skip;
        } key_zero;

        struct zero_plate_t {
                model_state state;        
                Mat histogram;
        } zero_plate;

        struct zero_tick_t {
                model_state state;        
        } zero_tick;
} model_t;

/**
 * Find the acute angle of the major axes of two RotatedRects
 */
static float rr_major_axis_delta(const RotatedRect& r1, const RotatedRect& r2)
{
        Point2f verts[4];
        Point2f axis1, axis2;
        vector<float> x, y, mag, ng;
        float ng1, ng2;

        r1.points(verts);
        axis1 = verts[1] - verts[0];
        axis2 = verts[3] - verts[0];
        x = { axis1.x, axis2.x };
        y = { axis1.y, axis2.y };
        cartToPolar(x, y, mag, ng);
        ng1 = ng[mag[0] > mag[1] ? 0 : 1] - M_PI;

        r2.points(verts);
        axis1 = verts[1] - verts[0];
        axis2 = verts[3] - verts[0];
        x = { axis1.x, axis2.x };
        y = { axis1.y, axis2.y };
        cartToPolar(x, y, mag, ng);
        ng2 = ng[mag[0] > mag[1] ? 0 : 1] - M_PI;
 
        return abs(atan2(sin(ng1 - ng2), cos(ng1 - ng2)));
}

/**
 * Return the aspect ratio of a RotatedRect, where the major axis is the
 * height, regardless of orientation.
 */
static float rr_aspect_ratio(const RotatedRect& r)
{
        Point2f verts[4];
        Point2f axis1, axis2;
        vector<float> x, y, mag;

        r.points(verts);
        axis1 = verts[1] - verts[0];
        axis2 = verts[3] - verts[0];
        x = { axis1.x, axis2.x };
        y = { axis1.y, axis2.y };
        magnitude(x, y, mag);

        float height = max(mag[0], mag[1]);
        float width  = min(mag[0], mag[1]);
 
        return height / width;
}

static void get_selection_contours(model_t& model, Mat& scene)
{
        static Mat blur1, blur2, gray, binary;
        static vector<vector<Point>> contours;
        static vector<Vec4i> hierarchy;
        static vector<Point> hull;

        model_t::selection_t& selection = model.selection;

        if (selection.state == VALID)
                return;

        //cout << 's' << flush;

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

        if (selection.state == VALID) {
                cout << "selection=" << model.selection.pt << endl;
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

        //cout << 'z' << flush;
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

static void find_key_zero(model_t& model, Mat& scene)
{
        static Mat blur1, blur2, gray, binary;
        static vector<vector<Point>> contours;
        static vector<Vec4i> hierarchy;
        static vector<Point> hull;

        Rect roi;
        double match_ratio, aspect_ratio;
        RotatedRect outside_rr, inside_rr;

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

        model.key_zero.state = UNRESOLVED;

        medianBlur(scene(roi), blur1, 3);
        bilateralFilter(blur1, blur2, 5, 75, 75);
        cvtColor(blur2, gray, CV_BGR2GRAY);
        threshold(gray, binary, 100, 255, THRESH_BINARY_INV);
        findContours(binary, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_NONE);

#define _continue \
{ \
        cout << __LINE__ << endl; \
        continue; \
}

        for (unsigned i = 0; i < contours.size(); i++) {

                // Consider only outside contours
                //
                if (hierarchy[i][3] >= 0)
                        continue;

                // The contour must match the expected key zero outside contour
                //
                match_ratio = matchShapes(KEY_ZERO_OUTSIDE_CONTOUR,
                                contours[i], CV_CONTOURS_MATCH_I3, 0);

                if (match_ratio > 0.10) // poor match
                        continue;

                // The contour must have the correct aspect ratio
                //
                outside_rr = minAreaRect(contours[i]);
                aspect_ratio = rr_aspect_ratio(outside_rr);

                if (aspect_ratio - KEY_ZERO_OUTSIDE_ASPECT_RATIO > 0.1)
                        continue;

                // The contour must have an inside contour
                //
                if (hierarchy[i][2] < 0)
                        continue;

                vector<Point>& inside_contour = contours[hierarchy[i][2]];

                // The inside contour must match the expected key zero inside contour
                //
                match_ratio = matchShapes(KEY_ZERO_INSIDE_CONTOUR,
                                inside_contour, CV_CONTOURS_MATCH_I3, 0);

                if (match_ratio > 0.10)
                        _continue;

                // The inside contour must have the correct aspect ratio
                //
                inside_rr = minAreaRect(inside_contour);
                aspect_ratio = rr_aspect_ratio(inside_rr);

                if (aspect_ratio - KEY_ZERO_INSIDE_ASPECT_RATIO > 0.10) // TODO: 0.20
                        _continue;

                // The areas of the inside and outside contours must have the correct ratio
                //
                convexHull(contours[i], hull);
                double outside_area = contourArea(hull);
                convexHull(inside_contour, hull);
                double inside_area = contourArea(hull);

                if (outside_area / inside_area - KEY_ZERO_CONTOURS_AREA_RATIO > 0.10)  // TODO: 0.15
                        _continue;

                // Orientation of the major axis of the contours must match
                //
                if (rr_major_axis_delta(outside_rr, inside_rr) > 0.1)
                        _continue;

                // The inside and outside contours must be concentric
                //
                Point2f dcenter = outside_rr.center - inside_rr.center;
                
                if (sqrt(pow(dcenter.x, 2) + pow(dcenter.y, 2)) > outside_rr.size.height * 0.1)
                        _continue;

                if (contours[i].size() < 5)
                        _continue;

                model.key_zero.pt = outside_rr.center;
                model.key_zero.pt.x += roi.x;
                model.key_zero.pt.y += roi.y;
                model.key_zero.size = outside_rr.boundingRect().size();
                model.key_zero.state = VALID;

                return;
        }
}

static void find_zero_tick(model_t& model, Mat& scene)
{
        if (model.key_zero.state != VALID)
                return;

        Point p1 = model.key_zero.pt;
        p1.x += model.key_zero.size.width * 1.1;
        p1.y -= model.key_zero.size.height * 0.20;

        Point p2 = model.key_zero.pt;
        p2.x += model.key_zero.size.width * 3.05;
        p2.y += model.key_zero.size.height * 0.20;

        //rectangle(scene, Rect(p1, p2), SELECT_COLOR, SELECT_LINE_WIDTH);
}

// How to find the two most dominant colors in an image
// http://answers.opencv.org/question/5067/how-to-find-the-two-most-dominant-colors-in-an/
// http://docs.opencv.org/modules/core/doc/clustering.html

static void find_zero_plate_right_edge(model_t& model, Mat& scene)
{
        static Mat hsv;
        Mat& hist = model.zero_plate.histogram;
        Point p1, p2;

        model.zero_plate.state = UNRESOLVED;

        if (model.key_zero.state != VALID)
                return;

        const int channels[] = { 0, 1 }; // hue and saturation channels
        const int hbins = 30, sbins = 32;
        const int histSize[] = { hbins, sbins };
        const float hranges[] = { 0, 180 };
        const float sranges[] = { 0, 256 };
        const float* ranges[] = { hranges, sranges };

        p1 = p2 = model.key_zero.pt;
        p1.x += model.key_zero.size.width * 0.8;
        p1.y -= model.key_zero.size.height * 1.1;
        p2.x += model.key_zero.size.width * 2.0;
        p2.y -= model.key_zero.size.height * 0.3;

        cvtColor(scene(Rect(p1, p2)), hsv, CV_BGR2HSV); 

        calcHist(&hsv,         // images (Mat*)
                 1,            // no. images
                 channels,     // channels of images (int*)
                 Mat(),        // mask
                 hist,         // histogram output
                 2,            // histogram dims
                 histSize,     // dimension sizes (int*)
                 ranges,       // dimension ranges (ranges[dims][2])
                 true,         // uniform
                 false);       // accumulate

        p1 = p2 = model.key_zero.pt;
        p1.x += model.key_zero.size.width * 0.8;
        p2.y += model.key_zero.size.height * 0.3;
        p2.x += model.key_zero.size.width * 2.0;
        p1.y += model.key_zero.size.height * 1.1;

        cvtColor(scene(Rect(p1, p2)), hsv, CV_BGR2HSV); 

        calcHist(&hsv,         // images (Mat*)
                 1,            // no. images
                 channels,     // channels of images (int*)
                 Mat(),        // mask
                 hist,         // histogram output
                 2,            // histogram dims
                 histSize,     // dimension sizes (int*)
                 ranges,       // dimension ranges (ranges[dims][2])
                 true,         // uniform
                 true);        // accumulate

        GaussianBlur(hist, hist, Size(3, 3), 0);

        float max = trunc(*max_element(hist.begin<float>(), hist.end<float>()));

        for (int y = 0; y < hist.rows; y++) {
                float* rp = hist.ptr<float>(y);
                for (int x = 0; x < hist.cols; x++) {
                        rp[x] = rp[x] < max * 0.05 ? 0 : trunc(rp[x]);
                }
        }

        model.zero_plate.state = VALID;
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

        if (model.selection.state != VALID)
                return;

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
        if (model.key_zero.state != VALID)
                return;

        Point p1, p2;

        p1 = p2 = model.key_zero.pt;
        p1.x -= 7; p1.y -= 7; p2.x += 7; p2.y += 7;
        line(scene, p1, p2, RED, 3);

        p1 = p2 = model.key_zero.pt;
        p1.x -= 7; p1.y += 7; p2.x += 7; p2.y -= 7;
        line(scene, p1, p2, RED, 3);
}

static string get_mat_type_name(const Mat& mat)
{
        int type = mat.type();

        uchar depth = type & CV_MAT_DEPTH_MASK;
        uchar chans = 1 + (type >> CV_CN_SHIFT);

        string r;

        switch (depth) {
                case CV_8U:
                        r = "8U";
                        break;

                case CV_8S:
                        r = "8S";
                        break;

                case CV_16U:
                        r = "16U";
                        break;

                case CV_16S:
                        r = "16S";
                        break;

                case CV_32S:
                        r = "32S";
                        break;

                case CV_32F:
                        r = "32F";
                        break;

                case CV_64F:
                        r = "64F";
                        break;

                default:
                        r = "User";
                        break;
        }

        r += "C";
        r += (chans + '0');

        return r;
}

static void draw_zero_plate_stuff(model_t& model, Mat& scene)
{
        if (model.zero_plate.state != VALID)
                return;

        Mat& hist = model.zero_plate.histogram;

        float max = trunc(*max_element(hist.begin<float>(), hist.end<float>()));

        cout << "\033[2J\033[0;0f" << "\033[0;0f";
        cout << "------------------------------- " << max << endl;

        int x, y;
        char buf[128];

        for (y = 0; y < hist.rows; y++) {
                sprintf(buf, "[ %-3u ] ", y);
                cout << buf;
                float* rp = hist.ptr<float>(y);
                for (x = 0; x < hist.cols; x++) {
                        cout << rp[x] << ' ';
                }
                cout << endl;
        }

        ///////////////////////////////////////////////////////////////

        static Mat mat;

        Rect rect = Rect(450, 360, 90, 100);
        scene(rect).copyTo(mat);
        cvtColor(mat, mat, CV_BGR2HSV); 

        uchar* p = mat.ptr<uchar>(0);

        for (int i = 0; i < mat.cols * mat.rows * 3; i += 3) {
                p[i + 2] = 255;
        }

        mat.copyTo(scene(Rect(100, 360, 90, 100)));
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

static void dump_model(model_t& model)
{
        ofstream os;

        os.open(DUMP_FNAME, ios::trunc);

        os << "inside: ";
        for (auto it : model.selection.inside.points)
                os << it;
        os << endl;

        os << "outside: ";
        for (auto it : model.selection.outside.points)
                os << it;
        os << endl;

        os << "aspect ratio: " << model.selection.aspect_ratio << endl;
}

int main(int argc, const char** argv)
{
        VideoCapture vc;

        bool pause = false;
        bool run = true;
        int key;

        namedWindow(WINDOW_NAME, CV_WINDOW_NORMAL);
        moveWindow(WINDOW_NAME, WINDOW_X_POS, WINDOW_Y_POS);
        resizeWindow(WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT);

        model_t model = {};

        handle_mouse_event(CV_EVENT_LBUTTONDOWN, 407, 476, 0, (void*) &model);
        setMouseCallback(WINDOW_NAME, handle_mouse_event, (void*) &model); 

        model.canny_threshold = 35;
        createTrackbar("thresh", WINDOW_NAME, &model.canny_threshold, 100);

        Mat scene;

        while (run) {
                read_frame(vc, scene);
//                get_selection_contours(model, scene);
//                find_selection(model, scene);
                find_key_zero(model, scene);
                find_zero_tick(model, scene);
                find_zero_plate_right_edge(model, scene);

                draw_pip(model, scene);
                draw_selection(model, scene);
                draw_metrics(scene, vc);
                draw_match(model, scene);
                draw_zero_plate_stuff(model, scene);

                compute_interval(model);

                switch (key = waitKey(model.frame_wait)) {
                        case 32: //space
                                pause = !pause; //TODO get this working again
                                break;
                        case 27: //esc
                                run = false;
                                break;
                        case 100: //d
                                dump_model(model);
                                break;
                        default:
                                if (key != -1)
                                        cout << "key=" << key << endl;
                }

                // detect a closed window
                if ((unsigned long) cvGetWindowHandle(WINDOW_NAME) == 0UL) {
                        break;
                }

                imshow(WINDOW_NAME, scene);
                //cout << '.' << flush;
        }

        vc.release();

        return 0;
}

