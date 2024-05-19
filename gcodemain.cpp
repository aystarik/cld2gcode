#include "modular.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

class GCodeInterp  : public FileReader {
    private:
        Transform *tr;
        Cylinder *cyl;
        HeightMap *hmap;
        Point4 last, home;

    public:
        GCodeInterp() { tr = new Out(); }
        ~GCodeInterp() {};
        void parse(const char *fn);
};

void GCodeInterp::parse(const char *fn)
{
    if (!(infile = fopen(fn, "r"))) {
        cerr << "can't find file:" << fn << endl;
        return;
    }
    int type, subtype, res;
    try {
        while (1) {
            rdline();
	}
    } catch (...) {
        tr->end();
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.CLD\n", argv[0]);
        return -1;
    }
    GCodeInterp interp;
    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(3);
    interp.parse(argv[1]);
    return 0;
}
