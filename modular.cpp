#include "modular.h"

#include <stdio.h>

bool Out::print_move(int type, const Point4 &end)
{
    static char axis_name[] = "XYZA";
    if (last == end)
        return false;
    if (last_mode != type)
        cout << "G" << type;
    last_mode = type;
    for (int i = 0; i < 4; ++i)
        if (fabs(last.p[i] - end.p[i]) > tiny && end.p[i] != Nan)
            cout << " " << axis_name[i] << end.p[i];
#if 0
    if (fabs(fmodf(last.p[3] - end.p[3], 360.0)) > tiny && end.p[3] != Nan) {
        double a = end.p[3];
        force_feed = true;

        double sign = 1.0;

        // wrapped A axis, as in Fanuc.
        // A is 0-359.999 degrees.
        // Sign before number means direction
        if (a > last.p[3])
            sign = 1.0;
        a = fmod(a, 360.0);
        if (a < -tiny)
            a += 360.0;
        if (fabs(a) < tiny)
            a = tiny;
        cout << " A" << (a * sign);
    }
#endif
    return true;
}

void Out::arc_feed(ArcDir dir, const Point4 &end, float I, float J, float feed)
{
    if (!print_move(dir, end))
        return;
    I -= last.p[0]; J -= last.p[1];
    last = end;
    if (I)
        cout << " I" << I;
    if (J)
        cout << " J" << J;
    if (last_feed != feed)
        cout << " F" << feed;
    last_feed = feed;
    cout << endl;
}

void Out::straight_feed(const Point4 &end, float feed)
{
    if (!print_move(1, end))
        return;
    last = end;
    if (force_feed || last_feed != feed)
        cout << " F" << feed;
    last_feed = feed;
    cout << endl;
}

void Out::traverse(const Point4 &end)
{
    if (!print_move(0, end))
        return;
    last = end;
    cout << endl;
}

static inline void rotate(double &x, double &y, double c, double s) {
    double t = x;
    x = t * c - y * s;
    y = t * s + y * c;
}

void ArcTransform::arc_feed(ArcDir dir, const Point4 &end, float c1, float c2, float feed)
{
    double T1 = atan2(last.p[1] - c2, last.p[0] - c1);
    double T2 = atan2(end.p[1] - c2, end.p[0] - c1);
    double R = hypot(last.p[1] - c2, last.p[0] - c1);
    double dT = min(2 * acos(max(0.0, 1 - fabs(tolerance / R))), M_PI / 4);
    if (dir == CCW_ARC) {
        if (T2 < T1) T2 += M_PI * 2;
    } else {
        if (T2 > T1) T2 -= M_PI * 2;
    }
    int n = int(fabs(T2 - T1) / dT + .5);
    dT = (T2 - T1) / n;
    double sin_t = sin(dT), cos_t = cos(dT);
    double tx = last.p[0] - c1, ty = last.p[1] - c2;
    Point4 cur(last);
    for (int i = 1; i <= n; ++i) {
        rotate(tx, ty, cos_t, sin_t);
        cur.p[0] = tx + c1;
        cur.p[1] = ty + c2;
        double t = double(i) / n;
        cur.p[2] = last.p[2] * (1 - t) + end.p[2] * t;
        wrap_line(1, cur, feed);
    }
    wrap_line(1, end, feed);
}

void ArcTransform::straight_feed(const Point4 &end, float feed)
{
    wrap_line(1, end, feed);
}

void ArcTransform::traverse(const Point4 &end)
{
    wrap_line(0, end);
}

void Cylinder::wrap_line(int mode, const Point4 &end, float feed)
{
    if (Zoff == 0) {
        Point4 cur(end);
        cur.p[3] = end.p[1] / D;
        cur.p[3] *= 360.0 / M_PI;
        cur.p[1] = Yoff;
        if (mode) {
            feed /= end.dist(last);
            tr->force_feed = true;
            tr->straight_feed(cur, feed);
        } else
            tr->traverse(cur);
    } else { // eccentric
        double phi = acos (1 - 2 * tolerance / D);
        int n = ceil(fabs(end.p[1] - last.p[1]) / (phi * D) + 1.0);
        Point4 cur, prev(last);
        for (int i = 1; i <= n; ++i) {
            double t = double(i)/n;
            for (int j = 0; j < 3; ++j)
                cur.p[j] = last.p[j] * (1 - t) + end.p[j] * t;
            double a = cur.p[1] / D;
            Point4 n(cur);
            n.p[1] = Yoff;
            if (a) {
                double u = atan2(sin(a), cos(a) - 2 * Zoff / D);
                n.p[1] += D / 2 * sin(a) / sin(u) * sin(u - a);
                n.p[2] += D / 2 * (sin(a) / sin(u) * cos(u - a) - 1.0) + Zoff;
            }
            n.p[3] = a * 360.0 / M_PI;
            if (mode)
                tr->straight_feed(n, feed/cur.dist(prev));
            else
                tr->traverse(n);
            prev = cur;
        }
    }
    last = end;
}

void FileReader::rdline()
{
    if (getline(&buf, &bsize, infile) == -1)
        throw -1;
}

void Grid::setSize(size_t i, size_t j) {
    cerr << "maxi = " << i << ", maxj = " << j << endl;
    mi = i;
    mj = j;
    data = new double[mi * mj];
}

double &Grid::operator()(size_t i, size_t j)
{
    static double t = 0.0;
    if (i >= 0 && j >= 0 && i < mi && j < mj && data)
        return data[i * mj + j];
    else
        return t;
}

double Grid::operator()(size_t i, size_t j) const
{
    double r = 0.0;
    if (i >= 0 && j >= 0 && i < mi && j < mj && data)
        r =  data[i * mj + j];
    return r;
}

void HeightMap::traverse(const Point4 &end)
{
    last = end;
    last.p[2] += Zmax;
    tr->traverse(last);
}

void HeightMap::parse(const char *gridfile)
{
    infile = fopen(gridfile, "r");
    if (!infile)
        return;
    rdline();
    double t1, t2;
    int res = sscanf(buf, "%lf %lf %lf %lf %lf %lf",
                    &t1, &t2, &XG0, &YG0, &XGstep, &YGstep);
    if (res != 6 || t1==0 || t2==0 || XGstep == 0 || YGstep == 0) {
        cerr << "wrong grid header\n";
        throw (-4);
    }

    grid.setSize(int(t1), int(t2));
    rdline();
    double Z0;
    res = sscanf(buf, "%lf %lf %lf\n", &t1, &t2, &Z0);
    if (res != 3) {
        cerr << "wrong grid data\n";
        throw (-4);
    }
    Zmax = 0;
    for (size_t j = 0; j < grid.maxj(); ++j) {
        for (size_t i = 0; i < grid.maxi(); ++i) {
            rdline();
            double Zt;
            res = sscanf(buf, "%lf %lf %lf\n", &t1, &t2, &Zt);
            if (res != 3) {
                cerr << "wrong grid data\n";
                throw (-4);
            }
            grid(i, j) = Zt - Z0;
            Zmax = max(Zmax, Zt);
        }
    }
    fclose(infile);
}

void HeightMap::offset(Point4 &a) const
{
    int i = (a.p[0] - XG0) / XGstep;
    int j = (a.p[1] - YG0) / YGstep;
//    cerr << "i = " << i << ", j = " << j << endl;
    double t = (a.p[0] - (XG0 + XGstep * i)) / XGstep;
    double u = (a.p[1] - (YG0 + YGstep * j)) / YGstep;
    double dZ = (1 - t) * (1 - u) * grid(i, j) +
              t * (1 - u) * grid(i + 1, j) +
              t * u * grid(i + 1, j + 1) +
              (1 - t) * u * grid(i + 1, j + 1);
//    cerr << "dZ = " << dZ << endl;
    a.p[2] += dZ;
}

void HeightMap::wrap_line(int mode, const Point4 &end, float feed)
{
        int n = max(fabs((end.p[0] - last.p[0]) / XGstep),
                    fabs((end.p[1] - last.p[1]) / YGstep)) + 1;
        Point4 cur;
        for (int i = 1; i <= n; ++i) {
            double t = double(i) / n;
            for (int j = 0; j < 3; ++j)
                cur.p[j] = (1 - t) * last.p[j] + t * end.p[j];
            offset(cur);
            if (mode)
                tr->straight_feed(cur, feed);
            else
                tr->traverse(cur);
        }
        last = end;
}
