#include "define.h"
#include "scissor.h"

Scissor::Scissor(const Mat& image): snap(false), cost(image.rows, image.cols, 8), link(image.rows, image.cols)
{
    original = image.clone();
    cvtColor(original, edge, CV_RGB2GRAY);
    edge = Blur(edge, 5);
    Canny(edge, edge, 40, 180);
    rectangle(edge, Point(0, 0), Point(edge.cols-1, edge.rows-1), Scalar(255));

    Cost();
    Visualize();
}

bool Scissor::GetLock() const
{
    return path.lock;
}

const Mat& Scissor::GetEdge() const
{
    return edge;
}

const Mat& Scissor::GetVisual() const
{
    return visual;
}

int Scissor::NeighborCost(Point q, Point r)
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

Mat Scissor::Blur(const Mat& original, int degree)
{
    Mat image;
    if(degree%2)
        GaussianBlur(original, image, Size(degree, degree), 0);
    return image;
}

bool Scissor::Visualize()
{
    if(cost.empty())
        return false;

    int h = original.rows;
    int w = original.cols;
    visual = Mat::zeros(Size(3 * w, 3 * h), CV_8UC3);

    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++) 
        {
            //Image pixel in the middle, surrounded by cost values
            visual.at<Vec3b>(3*i+1, 3*j+1) = original.at<Vec3b>(i, j);
            visual.at<Vec3b>(3*i+1, 3*j+2) = Vec3b(cost.get(i, j, 0), cost.get(i, j, 0), cost.get(i, j, 0));
            visual.at<Vec3b>(3*i  , 3*j+2) = Vec3b(cost.get(i, j, 1), cost.get(i, j, 1), cost.get(i, j, 1));
            visual.at<Vec3b>(3*i  , 3*j+1) = Vec3b(cost.get(i, j, 2), cost.get(i, j, 2), cost.get(i, j, 2));
            visual.at<Vec3b>(3*i  , 3*j  ) = Vec3b(cost.get(i, j, 3), cost.get(i, j, 3), cost.get(i, j, 3));
            visual.at<Vec3b>(3*i+1, 3*j  ) = Vec3b(cost.get(i, j, 4), cost.get(i, j, 4), cost.get(i, j, 4));
            visual.at<Vec3b>(3*i+2, 3*j  ) = Vec3b(cost.get(i, j, 5), cost.get(i, j, 5), cost.get(i, j, 5));
            visual.at<Vec3b>(3*i+2, 3*j+1) = Vec3b(cost.get(i, j, 6), cost.get(i, j, 6), cost.get(i, j, 6));
            visual.at<Vec3b>(3*i+2, 3*j+2) = Vec3b(cost.get(i, j, 7), cost.get(i, j, 7), cost.get(i, j, 7));
        }
    return true;
}

Point Scissor::Snap(Point cursor)
{
    int h = edge.rows, w = edge.cols;
    int X = w, Y = h;
    int x = 0, y = 0, dx = 0, dy = -1;
    int t = max(X, Y), maxi = t * t;

    for(int i = 0; i < maxi; i++)
    {
        if((-X/2 < x) && (x <= X/2) && (-Y/2 < y) && (y <= Y/2))
        {
            int cx = cursor.x + x, cy = cursor.y + y;
            if(cx >= 0 && cx < w && cy >= 0 && cy < h)
                if(edge.at<uchar>(cy, cx) == 255)
                    return Point(cx, cy);
        }
        if((x == y) || (x < 0 && x == -y) || (x > 0 && x == 1-y))
        {
            t = dx;
            dx = -dy;
            dy = t;
        }
        x += dx;
        y += dy;
    }
    return Point(-1, -1);
}

void Scissor::Cost()
{
    int h = original.rows;
    int w = original.cols;
    int c = original.channels();
    Matrixf img(original);
    Matrixf d_color(4, c);
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
}

bool Scissor::Wire(const Point& seed)
{
    if(cost.empty())
        return false;

    int h = cost.h;
    int w = cost.w;
    float buf[8];
    FibHeap pq;

    //Construct Node structure for shortest path tree calc
    vector<Pnode *> nodes;
    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++)
            nodes.push_back(new Pnode(Point(j, i), cost.toArray(buf, i, j)));

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
                            r->totalCost = q->totalCost + NeighborCost(q->pt, r->pt);
                            r->state = ACTIVE;
                            pq.Insert(r);
                        }
                        //if r is already in queue
                        else if(r->state == ACTIVE)
                        {
                            float c = q->totalCost + NeighborCost(q->pt, r->pt);
                            if(c < r->totalCost)
                            {
                                r->prevNode = q;
                                r->totalCost = c;
                            }
                        }
                    }
                }
    }

    link = Matrix<Point>(h, w);
    for(int i = 0; i < h; i++)
        for(int j = 0; j < w; j++)
        {
            //Extract prevNodes and add it to matrix
            link(i, j) = Point(j, i) == seed? seed: nodes[i * w + j]->prevNode->pt;
            delete nodes[i * w + j];
        }
    return true;
}

void Scissor::Trace(Point seed, Point cursor)
{
    path.mouse = vector<Point>();
    while(cursor != seed)
    {
        path.mouse.push_back(cursor);
        cursor = link.get(cursor.y, cursor.x);
    }
}

void Scissor::Draw(Mat& canvas)
{
    path.lock = true;
    canvas = original.clone();

    //draw lines for all previously recorded seed point to seed point trails
    for(int i = 0; i < path.trail.size(); i++)
        for(int j = 0; j < (int)path.trail[i].size() - 1; j++)
            line(canvas, path.trail[i][j], path.trail[i][j+1], Scalar(0, 0, 255), 2);

    if(path.seeds.size() > 0)
    {
        Trace(path.seeds.back(), path.cursor);
        for(int i = 0; i < (int)path.mouse.size() - 1; i++)
            line(canvas, path.mouse[i], path.mouse[i+1], Scalar(0, 0, 255), 2);

        //pathut empathhasis on seed pathoints by drawing green dots
        for(int i = 0; i < path.seeds.size(); i++)
            circle(canvas, path.seeds[i], 3, Scalar(0, 255, 0), -1);
    }

    circle(canvas, path.cursor, 3, snap? Scalar(255, 0, 255): Scalar(0, 255, 0), -1);

    path.lock = false;
}

void Scissor::OnClick()
{
    if(snap)
    {
        path.seeds.back() = Snap(path.seeds.back());

        if(path.seeds.size() > 1)
            Trace(path.seeds.end()[-2], path.seeds.back());
    }
    //Clicking means confirming the cursor position as seed point, therefore add the entire seed-to-cursor path to trail
    if(path.mouse.size() > 0)
    {
        reverse(path.mouse.begin(), path.mouse.end());
        path.trail.push_back(path.mouse);
    }
    if(path.seeds.size() > 1 && abs(path.mouse.back().x - path.seeds[0].x) + abs(path.mouse.back().y - path.seeds[0].y) < 10)
    {
        path.trail.back().push_back(path.seeds[0]);
        path.seeds.clear();
    }
    else
        Wire(path.seeds.back());
}

void Scissor::PopSeed()
{
    if(path.seeds.size() > 0)
    {
        path.seeds.pop_back();
        path.trail.pop_back();
        Wire(path.seeds.back());
    }
}

void Scissor::SaveContour(const char* filename)
{
    ofstream out(filename);
    for(int i = 0; i < path.trail.size(); i++)
        out << path.trail[i] << " ";
    out << endl;
    cout << "Contour saved to " << filename << endl;
}

void Scissor::ToggleSnap()
{
    snap = !snap;
    cout << "Snap " << (snap? "on": "off") << endl;
}

Mat Scissor::Crop()
{
    Mat cropped, mask = Mat::zeros(original.size(), CV_8UC3);

    //Set up contours
    vector<vector<Point> > contours(1);
    for(int i = 0; i < path.trail.size(); i++)
        contours[0].insert(contours[0].end(), path.trail[i].begin(), path.trail[i].end());
    drawContours(mask, contours, -1, Scalar(255, 255, 255), -1);

    Rect bound = boundingRect(contours[0]);

    //Mask original image by contour and crop by bounding rect
    original.copyTo(cropped, mask);
    cropped = cropped(bound);

    return cropped;
}

void Scissor::MouseCallback(int event, int x, int y)
{
    while(path.lock);
    if(event == EVENT_LBUTTONUP)
        //Set seed point to trigger path tree compute
        path.seeds.push_back(Point(x, y));
    else if(event == EVENT_MBUTTONUP)
    {
        //Reset all points when middle mouse click
        path.seeds.clear();
        path.trail.clear();
        path.mouse.clear();
    }
    else if(event == EVENT_MOUSEMOVE)
        path.cursor = snap? Snap(Point(x, y)): Point(x, y);
}

void Scissor::ButtonCallback(int state)
{
}