#include <cmath>
#include <iostream>

using namespace std;

// define some uninitialized value... 1e10 is enougth to spot it in results...
static const float Nan = 1e10;
static float tolerance = 0.01;
static float tiny = 0.001;

struct Point4 {
    Point4() {
        for (int i = 0; i < 4; ++i)
            p[i] = Nan;
    }
    bool operator==(const Point4 &c) const {
        for (int i = 0; i < 4; ++i)
            if (fabs(p[i] - c.p[i]) > tiny) return false;
        return true;
    }
    float dist(const Point4 &b) const {
        return hypotf(hypotf(p[0] - b.p[0], p[1] - b.p[1]), p[2] - b.p[2]);
    }
    float p[4];
};

enum ArcDir {
    CW_ARC = 2,
    CCW_ARC = 3
};

class Transform {
    protected:
        class Transform *tr;
        Point4 last;
    public:
        bool force_feed; 
        Transform(Transform *n) : tr(n), force_feed(false) {}
        virtual void arc_feed(ArcDir dir, const Point4 &end, float cx, float cy, float feed) = 0;
        virtual void straight_feed(const Point4 &end, float feed) = 0;
        virtual void traverse(const Point4 &end) = 0;
        virtual void end() {cout << "M2" << endl;}
        virtual ~Transform() { delete tr; }
};

class Out: public Transform {
        int last_mode;
        float last_feed;
    public:
        Out(Transform *tr = 0) : Transform(tr), last_mode(-1), last_feed(Nan)
            { cout << "G21" << endl; }
        void arc_feed(ArcDir dir, const Point4 &end, float c1, float c2, float feed);
        void straight_feed(const Point4 &end, float feed);
        void traverse(const Point4 &end);
    private:
        bool print_move(int type, const Point4 &end);
};

class ArcTransform: public Transform {
    public:
        ArcTransform(Transform *tr) : Transform(tr) {}
        void arc_feed(ArcDir dir, const Point4 &end, float c1, float c2, float feed);
        void straight_feed(const Point4 &end, float feed);
        void traverse(const Point4 &end);
    protected:
        virtual void wrap_line(int mode, const Point4 &end, float feed = 0) = 0;
};

class Cylinder: public ArcTransform {
		float Yoff, Zoff, D;
	public:
        Cylinder(Transform *tr) : ArcTransform(tr), Yoff(Nan), Zoff(Nan), D(Nan) { cout << "G93" << endl; }
		void setY(float y) {Yoff = y;}
		void setZ(float z) {Zoff = z;}
		void setD(float d) {D = d;}
    protected:
        void wrap_line(int mode, const Point4 &end, float feed = 0);
};

class FileReader {
    protected:
        FILE *infile;
        char *buf;
        size_t bsize;
        void rdline();
    public:
        FileReader() : infile(0), buf(0), bsize(0) {}
        virtual void parse(const char *fn) = 0;
};

class Grid {
        double *data;
        size_t mi, mj;
    public:
        Grid(): data(0), mi(-1), mj(-1) {}
        ~Grid() {delete [] data;}
        void setSize(size_t i, size_t j);
        size_t maxi() const {return mi;}
        size_t maxj() const {return mj;}
        double operator()(size_t i, size_t j) const;
        double &operator()(size_t i, size_t j);
};

class HeightMap: public ArcTransform, FileReader {
    double XG0, YG0, XGstep, YGstep, Zmax;
    Grid grid;
    public:
        HeightMap(Transform *tr = 0) : ArcTransform(tr), FileReader() {}
        void parse(const char *gridfile);
        void traverse(const Point4 &end);
    protected:
        void offset(Point4 &cur) const;
        void wrap_line(int mode, const Point4 &end, float feed = 0);
};
