#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/calib3d/calib3d.hpp>

using namespace cv;
using namespace std;

/**
 * @details Class to save patter point information
 */
class PatterPoint {
public:
    float x;
    float y;
    float radio;
    int h_father;
    PatterPoint() {
        x = 0;
        y = 0;
        radio = 0;
    }
    PatterPoint(float x, float y, float radio, int h_father) {
        this->x = x;
        this->y = y;
        this->radio = radio;
        this->h_father = h_father;
    }
    float distance(PatterPoint p) {
        return sqrt(pow(x - p.x, 2) + pow(y - p.y, 2));
    }
    Point2f to_point2f() {
        return Point2f(x, y);
    }
    Point2f center() {
        return Point2f(x, y);
    }
};

void draw_lines_pattern_from_ellipses(Mat &out, vector<PatterPoint> pattern_centers);
void update_mask_from_points(vector<PatterPoint> points, int w, int h, Point mask_point[][4]);
int mode_from_father(vector<PatterPoint> pattern_points);
int find_pattern_points(Mat &src_gray, Mat &masked, Mat&original, int w, int h, Point mask_point[][4], vector<PatterPoint> &pattern_points, int &keep_per_frames);
float angle_between_two_points(PatterPoint p1, PatterPoint p2);
float distance_to_rect(PatterPoint p1, PatterPoint p2, PatterPoint x);
vector<PatterPoint> more_distante_points(vector<PatterPoint>points);

/**
 * @details Function to support PatterPoint sort by hierarchy
 *
 * @param p1 First point
 * @param p2 Second point
 *
 * @return father hierarchy of point 1 is lower than father hierarchy of point 2
 */
bool sort_patter_point_by_father(PatterPoint p1, PatterPoint p2) {
    return p1.h_father < p2.h_father;
}

/**
 * @details Return the father hierarchy mode from a vector of PatternPoints
 *
 * @param pattern_points Vector of points
 * @return father hierarchy mode
 */
int mode_from_father(vector<PatterPoint> points) {
    if (points.size() == 0) {
        return -1;
    }
    vector<PatterPoint> temp;
    for (int p = 0; p < points.size(); p++) {
        temp.push_back(points[p]);
    }
    sort(temp.begin(), temp.end(), sort_patter_point_by_father);

    int number = temp[0].h_father;
    int mode = number;
    int count = 1;
    int countMode = 1;

    for (int p = 1; p < temp.size(); p++) {
        if (number == temp[p].h_father) {
            count++;
        } else {
            if (count > countMode) {
                countMode = count;
                mode = number;
            }
            count = 1;
            number =  temp[p].h_father;
        }
    }
    if (count > countMode) {
        return number;
    } else {
        return mode;
    }
}


int find_pattern_points(Mat &src_gray, Mat &masked, Mat&original, int w, int h, Point mask_point[][4], vector<PatterPoint> &pattern_points, int &keep_per_frames) {

    vector<vector<Point> > contours;
    vector<Vec4i> hierarchy;
    vector<PatterPoint> ellipses_temp;
    vector<PatterPoint> new_pattern_points;
    float radio_hijo;
    float radio;
    float radio_prom = 0;
    float distance;

    int erosion_size = 2;
    Mat kernel = getStructuringElement( MORPH_ELLIPSE,
                                        Size( 2 * erosion_size + 1, 2 * erosion_size + 1 ),
                                        Point( erosion_size, erosion_size ) );

    erode( src_gray, src_gray, kernel );

    //morphologyEx(src_gray, src_gray, MORPH_CLOSE, kernel);

    findContours( src_gray, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );

    for (int c = 0; c < contours.size(); c++) {
        if (contours[c].size() > 4 && hierarchy[c][3] != -1) {
            RotatedRect elipse  = fitEllipse( Mat(contours[c]) );
            radio = (elipse.size.height + elipse.size.width) / 4;
            if (hierarchy[c][2] != -1) { //If has a son
                int hijo  = hierarchy[c][2];
                if (contours[hijo].size() > 4 ) {
                    RotatedRect elipseHijo  = fitEllipse( Mat(contours[hijo]) );
                    radio_hijo = (elipseHijo.size.height + elipseHijo.size.width) / 4;
                    if ( /*radio <= radio_hijo * 2 &&*/ cv::norm(elipse.center - elipseHijo.center) < radio_hijo / 2) {
                        ellipses_temp.push_back(PatterPoint(elipse.center.x, elipse.center.y, radio, hierarchy[c][3]));
                        ellipse(masked, elipse, Scalar(0, 255, 255), 2);
                    }
                }
            } else {
                ellipse(masked, elipse, Scalar(255, 0, 255), 2);
            }
        }
        drawContours( src_gray, contours, c, Scalar(255, 255, 255), 2, 8, hierarchy, 0, Point() );
    }

    /**/
    int count;
    for (int i = 0; i < ellipses_temp.size(); ++i) {
        count = 0;
        for (int j = 0; j < ellipses_temp.size(); ++j) {
            if (i == j) continue;
            radio = ellipses_temp[j].radio;

            distance = ellipses_temp[i].distance(ellipses_temp[j]);
            if (distance < radio * 5/*3.5*/) {
                line(masked, ellipses_temp[i].center(), ellipses_temp[j].center(), Scalar(0, 0, 255), 2);
                count++;
            }
        }
        if (count >= 2) {
            radio = ellipses_temp[i].radio;
            radio_prom += radio;
            new_pattern_points.push_back(ellipses_temp[i]);
            //circle(masked, ellipses_temp[i].center(), radio, Scalar(0, 255, 0), 5);
        }
    }
    radio_prom /= new_pattern_points.size();

    if (new_pattern_points.size() > 20) {
        int mode = mode_from_father(new_pattern_points);
        if (mode != -1) {
            RotatedRect elipse  = fitEllipse( Mat(contours[mode]) );
            ellipse(masked, elipse, Scalar(0, 0, 255), 5);

            /* CLEAN USING MODE */
            vector<PatterPoint> temp ;
            for (int e = 0; e < new_pattern_points.size(); e++) {
                if (new_pattern_points[e].h_father == mode) {
                    temp.push_back(new_pattern_points[e]);
                    circle(masked, new_pattern_points[e].center(), new_pattern_points[e].radio, Scalar(0, 255, 0), 5);
                }
            }
            new_pattern_points = temp;
        }
    }

    if (new_pattern_points.size() == 20) {
        keep_per_frames = 2;
        pattern_points = new_pattern_points;
    }

    if (keep_per_frames-- > 0) {
        draw_lines_pattern_from_ellipses(original, pattern_points);
    } else {
        pattern_points.clear();
    }
    update_mask_from_points(pattern_points, w, h, mask_point);
    return pattern_points.size();
}

/**
 * @details Build a line using points p1 and p2 and return the distance of this line to the points x
 *
 * @param p1 Point 1 to build the line
 * @param p2 Point 2 to build the line
 * @param x  Point to calc the distance to the line
 * @return Distance from the built line to x point
 */
float distance_to_rect(PatterPoint p1, PatterPoint p2, PatterPoint x) {
    float result = abs((p2.y - p1.y) * x.x - (p2.x - p1.x) * x.y + p2.x * p1.y - p2.y * p1.x) / sqrt(pow(p2.y - p1.y, 2) + pow(p2.x - p1.x, 2));
    return result;
}

/**
 * @details Calc the most distante points in a vector of points
 *
 * @param points Poinst to be evaluate
 * @return Most distante points order by x coordinate
 */
vector<PatterPoint> more_distante_points(vector<PatterPoint> points) {
    float distance = 0;
    double temp;
    int p1, p2;
    for (int i = 0; i < points.size(); i++) {
        for (int j = 0; j < points.size(); j++) {
            if (i != j) {
                temp = points[i].distance(points[j]);
                if (distance < temp) {
                    distance = temp;
                    p1 = i;
                    p2 = j;
                }

            }
        }
    }
    if (points[p1].x < points[p2].x) {
        distance = p1;
        p1 = p2;
        p2 = distance;
    }
    vector<PatterPoint> p;
    p.push_back(points[p1]);
    p.push_back(points[p2]);
    return p;
}
/**
 * @details Draw lines patter in drawing Mat from a vector of points
 *
 * @param drawing Mat to draw patter
 * @param pattern_centers Patter points found
 */
void draw_lines_pattern_from_ellipses(Mat &drawing, vector<PatterPoint> pattern_centers) {
    if (pattern_centers.size() < 20) {
        return;
    }
    vector<Scalar> color_palette(5);
    color_palette[0] = Scalar(255, 0, 255);
    color_palette[1] = Scalar(255, 0, 0);
    color_palette[2] = Scalar(0, 255, 0);
    color_palette[3] = Scalar(0, 0 , 255);

    int coincidendes = 0;
    int centers = pattern_centers.size();
    float pattern_range = 5;
    vector<PatterPoint> temp;
    vector<PatterPoint> line_points;
    vector<PatterPoint> limit_points;
    int line_color = 0;
    for (int i = 0; i < centers; i++) {
        for (int j = 0; j < centers; j++) {
            if (i != j) {
                temp.clear();
                line_points.clear();
                coincidendes = 0;
                for (int k = 0; k < centers; k++) {
                    if (distance_to_rect(pattern_centers[i], pattern_centers[j], pattern_centers[k]) < pattern_range) {
                        coincidendes++;
                        line_points.push_back(pattern_centers[k]);
                    }
                }

                if (coincidendes >= 5) {
                    line_points = more_distante_points(line_points);
                    bool found = false;
                    for (int l = 0; l < limit_points.size(); l++) {
                        if (limit_points[l].x == line_points[0].x && limit_points[l].y == line_points[0].y) {
                            found = true;
                        }
                    }
                    if (!found) {
                        limit_points.push_back(line_points[0]);
                        limit_points.push_back(line_points[1]);
                        if (line_color != 0) {
                            line(drawing, line_points[1].to_point2f(), limit_points[line_color * 2 - 2].to_point2f(), color_palette[line_color], 2);
                        }
                        line(drawing, line_points[0].to_point2f(), line_points[1].to_point2f(), color_palette[line_color], 2);
                        line_color++;
                    }
                }
            }
        }
    }
}

/**
 * @details Update mask points from detected points
 *
 * @param points Patter points found
 * @param w Original image width
 * @param h Original image height
 * @param mask_point Array which store mask points
 */
void update_mask_from_points(vector<PatterPoint> points, int w, int h, Point mask_point[][4]) {
    if (points.size() < 20) {
        mask_point[0][0]  = Point(0, 0);
        mask_point[0][1]  = Point(h, 0);
        mask_point[0][2]  = Point(h, w);
        mask_point[0][3]  = Point(0, w);
        return;
    }
    vector<Point2f> points_2f(points.size());
    for (int p = 0; p < points.size(); p++) {
        points_2f[p] = Point2f(points[p].x, points[p].y);
    }
    RotatedRect boundRect = minAreaRect( Mat(points_2f) );
    Point2f rect_points[4];
    boundRect.points( rect_points );

    double scale = 1.5;
    Point mask_center(( rect_points[0].x +
                        rect_points[1].x +
                        rect_points[2].x +
                        rect_points[3].x) / 4,
                      (rect_points[0].y +
                       rect_points[1].y +
                       rect_points[2].y +
                       rect_points[3].y) / 4);

    mask_point[0][0]  = Point((rect_points[0].x - mask_center.x) * scale + mask_center.x, (rect_points[0].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][1]  = Point((rect_points[1].x - mask_center.x) * scale + mask_center.x, (rect_points[1].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][2]  = Point((rect_points[2].x - mask_center.x) * scale + mask_center.x, (rect_points[2].y - mask_center.y) * scale + mask_center.y);
    mask_point[0][3]  = Point((rect_points[3].x - mask_center.x) * scale + mask_center.x, (rect_points[3].y - mask_center.y) * scale + mask_center.y);
}