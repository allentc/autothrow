/* vim: set ts=8 sw=8: */

#include <cmath>
#include <cstdio>
#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;
using namespace std;

/**
 * Find the acute angle of the major axes of two RotatedRects
 */
static float rotatedRectsMajorAxisDelta(const RotatedRect& r1, const RotatedRect& r2)
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

int main()
{
        const char* vertlabels[] = { "0", "1", "2", "3" };
        const char* WIN_NAME = "rects";

        bool run = true;
        float angle = 0;
        Mat image(200, 400, CV_8UC3, Scalar(0));
        RotatedRect rect1, rect2;
        Point2f vertices[4];

        namedWindow(WIN_NAME, WINDOW_NORMAL);
        moveWindow(WIN_NAME, 2100, 400);

        while (run) {
                rect1 = RotatedRect(Point2f(100,100), Size2f(100,50), angle);
                rect2 = RotatedRect(Point2f(100,100), Size2f(100,50), 0);

                image = Mat(200, 400, CV_8UC3, Scalar(0));

                rect1.points(vertices);

                for (int i = 0; i < 4; i++) {
                        line(image, vertices[i], vertices[(i + 1) % 4], Scalar(0, 255, 0));
                        putText(image, vertlabels[i], vertices[i] + Point2f(2, -5),
                                        FONT_HERSHEY_PLAIN, 1, Scalar(255, 255, 255));
                }

                Point2f p0 = Point2f(275,100);
                //Point2f p1 = vertices[1] - vertices[0];
                Point2f p1 = Point2f(50, 0);
                Point2f p2 = vertices[3] - vertices[0];

                line(image, p0, p0 + p1, Scalar(0,0,255));
                line(image, p0, p0 + p2, Scalar(255,255,255));

                vector<float> x { p1.x, p2.x };
                vector<float> y { p1.y, p2.y };
                vector<float> mag, ng;

                cartToPolar(x, y, mag, ng);

                imshow(WIN_NAME, image);

                /*
                printf("angles %7.2f ", angle);
                cout << p1 << ' ' << p2
                     << "    mag[0]=" << mag[0] << " mag[1]=" << mag[1];
                printf("    ng[0]=%7.2f ng[1]=%7.2f", ng[0] - M_PI, ng[1] - M_PI);
                printf("    d ng = %7.2f", abs(atan2(sin(ng[0] - M_PI - ng[1] - M_PI), cos(ng[0] - M_PI - ng[1] - M_PI))));
                printf("    d ng = %7.2f", abs(atan2(sin(ng[1] - M_PI - ng[0] - M_PI), cos(ng[1] - M_PI - ng[0] - M_PI))));
                cout << endl;
                */

                cout << rotatedRectsMajorAxisDelta(rect1, rect2) << endl;

                while(true) {
                        int key = waitKey(1);

                        if (cvGetWindowHandle(WIN_NAME) == 0UL) {
                                run = false;
                                break;
                        }

                        switch(key) {
                                case 27:
                                        run = false;
                                        break;
                                case 32:
                                        angle += 1;
                                        break;
                                case 8:
                                        angle -= 1;
                                        break;
                                default:
                                        continue;
                        }
                        break;
                }
        }

        return 0;
}
