#include "define.h"

using namespace cv;
using namespace std;

Matrixf cost(const Mat& original)
{
    int h = original.rows;
    int w = original.cols;
    int c = original.channels();
    Matrixf img(original);
    Matrixf d_color(4, c);
    Matrixf cost(h, w, 8);
    float maxD = -1.0f;

    for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
        {
            for (int k = 0; k < c; k++)
            {
                //D color equation from project page
                d_color(0, k) = abs( (img.get(i-1, j  , k) + img.get(i-1, j+1, k))/2.0f - (img.get(i+1, j  , k) + img.get(i+1, j+1, k))/2.0f ) / 2.0f;
                d_color(2, k) = abs( (img.get(i-1, j-1, k) + img.get(i  , j-1, k))/2.0f - (img.get(i-1, j+1, k) + img.get(i  , j+1, k))/2.0f ) / 2.0f;
                d_color(1, k) = abs( img.get(i-1, j  , k) - img.get(i  , j+1, k) ) / sqrt(2.0f);
                d_color(3, k) = abs( img.get(i  , j-1, k) - img.get(i-1, j  , k) ) / sqrt(2.0f);
            }

            for (int k = 0; k < 4; k++)
            {
                //Sum of squres of color channels
                cost(i, j, k) = sqrt( (pow(d_color.get(k, 0), 2) + pow(d_color.get(k, 1), 2) + pow(d_color.get(k, 2), 2)) / 3.0f);
                //Get max element using the same for loop
                if (cost.get(i, j, k) > maxD)
                    maxD = cost.get(i, j, k);
            }

            //Cost symmetry
            if(cost.inbound(i  , j+1, 4))
                cost(i  , j+1, 4) = cost.get(i, j, 0);
            if(cost.inbound(i-1, j+1, 5))
                cost(i-1, j+1, 5) = cost.get(i, j, 1);
            if(cost.inbound(i-1, j  , 6))
                cost(i-1, j  , 6) = cost.get(i, j, 2);
            if(cost.inbound(i-1, j-1, 7))
                cost(i-1, j-1, 7) = cost.get(i, j, 3);

        }

    //The maxD - D equation from project page
    for (int i = 0; i < h; i++)
        for(int j = 0; j < w; j++)
            for(int k = 0; k < 8; k++)
                cost(i, j, k) = (maxD - cost.get(i, j, k)) * sqrt(k % 2 + 1) / 2.0f;

    return cost;

}

Mat visualize(const Mat& image, const Matrixf& cost)
{
    int h = image.rows;
    int w = image.cols;
    Mat vis = Mat::zeros(Size(3 * w, 3 * h), CV_8UC3);

    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++) 
        {
            //Image pixel in the middle, surrounded by cost values
            vis.at<Vec3b>(3*i+1, 3*j+1) = image.at<Vec3b>(i, j);
            vis.at<Vec3b>(3*i+1, 3*j+2) = Vec3b(cost.get(i, j, 0), cost.get(i, j, 0), cost.get(i, j, 0));
            vis.at<Vec3b>(3*i  , 3*j+2) = Vec3b(cost.get(i, j, 1), cost.get(i, j, 1), cost.get(i, j, 1));
            vis.at<Vec3b>(3*i  , 3*j+1) = Vec3b(cost.get(i, j, 2), cost.get(i, j, 2), cost.get(i, j, 2));
            vis.at<Vec3b>(3*i  , 3*j  ) = Vec3b(cost.get(i, j, 3), cost.get(i, j, 3), cost.get(i, j, 3));
            vis.at<Vec3b>(3*i+1, 3*j  ) = Vec3b(cost.get(i, j, 4), cost.get(i, j, 4), cost.get(i, j, 4));
            vis.at<Vec3b>(3*i+2, 3*j  ) = Vec3b(cost.get(i, j, 5), cost.get(i, j, 5), cost.get(i, j, 5));
            vis.at<Vec3b>(3*i+2, 3*j+1) = Vec3b(cost.get(i, j, 6), cost.get(i, j, 6), cost.get(i, j, 6));
            vis.at<Vec3b>(3*i+2, 3*j+2) = Vec3b(cost.get(i, j, 7), cost.get(i, j, 7), cost.get(i, j, 7));
        }

    return vis;
}

int neighborCost(const Matrixf& cost, Point q, Point r)
{
    //simple mapping for i-1, j-1 etc to array index
    if(q == r)
        return -1;
    int mapping[3][3] = {{3,  2, 1},
                         {4, -1, 0},
                         {5,  6, 7}};
    int x = r.x - q.x + 1;
    int y = r.y - q.y + 1;
    return cost.get(q.y, q.x, mapping[y][x]);
}

Matrix<Point> wire(const Point& seed, const Matrixf& cost)
{
    int h = cost.h;
    int w = cost.w;
    float buf[8];
    FibHeap pq;

    //Construct Node structure for shortest path tree calc
    vector<Pnode *> nodes;
    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++)
            nodes.push_back(new Pnode(Point(j, i), cost.toArray(buf, j, i)));

    //Init seed node
    nodes[seed.y*w + seed.x]->totalCost = 0.0f;
    pq.Insert(nodes[seed.y * w + seed.x]);

    Pnode* q = NULL, *r = NULL;
    while((q = (Pnode*)pq.ExtractMin()) != NULL)
    {
        q->state = EXPANDED;

        for(int i = -1; i < 2; i++)
            for(int j = -1; j < 2; j++)
                if(i != 0 || j != 0)
                {
                    //Get neighbor
                    int ci = q->pt.y + i;
                    int cj = q->pt.x + j;
                    if(ci >= 0 && ci < h && cj >= 0 && cj < w)
                    {
                        //if r is initial state
                        r = nodes[ci*w + cj];
                        if(r->state == INITIAL)
                        {
                            r->prevNode = q;
                            r->totalCost = q->totalCost + neighborCost(cost, q->pt, r->pt);
                            r->state = ACTIVE;
                            pq.Insert(r);
                        }
                        //if r is already in queue
                        else if(r->state == ACTIVE)
                        {
                            float c = q->totalCost + neighborCost(cost, q->pt, r->pt);
                            if(c < r->totalCost)
                            {
                                r->prevNode = q;
                                r->totalCost = c;
                            }
                        }
                    }
                }
    }

    Matrix<Point> path(h, w);
    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++)
        {
            //Extract prevNodes and add it to matrix
            path(i, j) = Point(j, i) == seed? seed: nodes[i * w + j]->prevNode->pt;
            delete nodes[i * w + j];
        }
    return path;
}

void draw(Mat& canvas, const Mat& image, const Point& seed_pt, const Point& move_pt, const Matrix<Point>& path)
{
    canvas = image.clone();
    Point curr_pt = move_pt;
    while(curr_pt != seed_pt)
    {
        //path(i, j) is the next pixel position to link to from Point(j, i) ps. Point uses (x, y) so has to flip ij
        Point prev_pt = path.get(curr_pt.y, curr_pt.x);
        line(canvas, curr_pt, prev_pt, Scalar(0, 0, 255), 2);
        curr_pt = prev_pt;
    }
}

Point prev_pt = Point(-1, -1);
Point seed_pt = Point(-1, -1);
Point move_pt = Point(-1, -1);
void mouseCallback(int event, int x, int y, int flags, void* userdata)
{
    if(event == EVENT_LBUTTONUP)
    {
        cout << "LBUTTON UP " << x << " " << y << endl;
        //Set seed point to trigger path tree compute
        seed_pt = seed_pt.x == -1? Point(x, y): Point(-1, -1);
    }
    else if(event == EVENT_MOUSEMOVE)
        move_pt = Point(x, y);
}

int main()
{
    namedWindow("Image", WINDOW_AUTOSIZE);
    moveWindow("Image", 1000, 0);
    setMouseCallback("Image", mouseCallback, NULL);

    Mat canvas, image = imread("curless.png", CV_LOAD_IMAGE_COLOR);
    resize(image, image, Size(), 0.5, 0.5);
    Matrixf c = cost(image);

    Mat vis = visualize(image, c);

    Matrix<Point> path(image.rows, image.cols);

    while(1)
    {
        if(seed_pt.x != -1 && seed_pt != prev_pt)
        {
            path = wire(seed_pt, c);
            prev_pt = seed_pt;
        }

        //Continous drawing of lasso lines
        if(seed_pt.x != -1)
            draw(canvas, image, seed_pt, move_pt, path);
        else
            canvas = image.clone();

        imshow("Image", canvas);
        if((char)waitKey(1) == 'q') break;
    }

    return 0;
}
