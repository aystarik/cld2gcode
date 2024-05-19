#include "modular.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <iconv.h>

class CldInterp : public FileReader {
    private:
        Transform *tr;
        Cylinder *cyl;
        HeightMap *hmap;
        Point4 last, home;
        float depth, feed_xy, feed_z, Zclear, Zsafe, Zsis, S;
        bool tool_up, engr_off, coolant, coolant_on, spindle_on;
	int multax;
        void r2000(int subtype);
        void r5000(int subtype);
    public:
        CldInterp() : cyl(0), hmap(0), tool_up(true), engr_off(false), coolant(false),
                        coolant_on(false), spindle_on(false), multax(0) { tr = new Out(); }
        ~CldInterp() {};
        void parse(const char *fn);
};

void CldInterp::parse(const char *fn) {
    if (!(infile = fopen(fn, "r"))) {
        cerr << "can't find file:" << fn << endl;
        return;
    }
    int type, subtype, res;
    try {
        while (1) {
            rdline();
            res = sscanf(buf, "R%d %d\n", &type, &subtype);
            if (res < 2) {
                type = subtype = -1;
                continue;
            }
            switch (type) {
            case 2000:
                r2000(subtype);
                break;
            case 5000:
                r5000(subtype);
                break;
            case 14000:
                throw(0);
            }
        }
    } catch (...) {
        tr->end();
    }
}

static iconv_t ic;
static const char *gridfile = "/home/aystarik/emc2/nc_files/probe.txt";
void CldInterp::r2000(int subtype)
{
    int res, flag;
    float F;
    if (engr_off && subtype != 1119)
        return;
    switch (subtype) {
    case 2:
        cout << "M1\n";
        break;
    case 17: 
		if (coolant_on) {
			coolant_on = false;
			cout << "M9" << endl;
		}
		if (spindle_on) {
			spindle_on = false;
			cout << "M5" << endl;
		}
		last.p[2] = home.p[2]; // Z first
		tr->traverse(last);
		last = home;
		tr->traverse(last);
        break;
	case 1003: // mode
		break;
    case 1004:
        rdline();
        res = sscanf(buf, "%f\n", &Zclear);
        break;
    case 1009:
        rdline();
        res = sscanf(buf, "%d\n", &flag);
        rdline();
        res = sscanf(buf, "%f\n", &F);
        if (res == 1) {
            if (flag == 602)
                feed_xy = F;
            else
                feed_z = F;
        }
        break;
    case 1025: {
        int N;
        float D, A;
        rdline();
        res = sscanf(buf, "%d %f %f", &N, &D, &A);
        if (res == 3)
            cout << "( Инструмент " << N << ": / D = " << D << "мм, U = " << 
			A * 180 / M_PI << " гр. / )" << endl;
        break;
        }
    case 1030:
        coolant = true;
        break;
    case 1031:
        rdline();
        rdline();
        res = sscanf(buf, "%f\n", &S);
        break;
    case 1045: {
            rdline();
            char *ibuf = buf, tbuf[256], *ebuf = tbuf;
            memset(tbuf, 0, 256);
            size_t ibsize, esize = 256;
	    ibsize = (size_t)min(strchrnul(buf, 0xa), strchrnul(buf, 0xd));
	    ibsize -= (size_t)buf;
            while (ibsize)
                iconv(ic, &ibuf, &ibsize, &ebuf, &esize);
            cout << "( " << tbuf << " )" << endl;
        }
        break;
	case 1055: {
		int tool = 0;
		rdline();
		sscanf(buf, "%d", &tool);
		//cout << "M6T" << tool << endl;
	}
		break;
    case 1071:
        rdline();
        res = sscanf(buf, "%f\n", &Zsafe);
        break;
    case 1101:
        rdline();
        res = sscanf(buf, "%f\n", &Zsis);
        break;
    case 1102:
        rdline();
        res = sscanf(buf, "%f\n", &depth);
        break;
    case 1104:
        last.p[2] = Zclear;
        tr->traverse(last);
        tool_up = true;
        break;
    case 1105: {
            last.p[2] = Zclear;
            tr->traverse(last);
            int wait = 0;
            if (coolant && !coolant_on) {
                wait = 5;
                coolant_on = 1;
                cout << "M8" << endl;
            }
            if (!spindle_on) {
                spindle_on = 1;
                wait = max(wait, int(S / 1000 + .5));
                cout << "S" << S << " M3" << endl;
            }
            if (wait)
                cout << "G4P" << wait << endl;
            last.p[2] = Zsafe + Zsis;
            tr->traverse(last);
            last.p[2] = -depth + Zsis;
            tr->straight_feed(last, feed_z);
            tool_up = false;
        }
        break;
    case 1106: // color
        break;
    case 1107: // window
        break;
    case 1108: // zone
        break;
    case 1110:
        rdline();
        {
        float A;
        res = sscanf(buf, "ROTTBL,%f", &A);
        if (res == 1) {
            last.p[3] = A;
            home.p[3] = 0;
            tr->traverse(last);
        }
        res = strncmp(buf, "GRID", 4);
	if (res)
            break;
        if (!hmap) {
            hmap = new HeightMap(tr);
            tr = hmap;
            hmap->parse(gridfile);
        }
        }
        break;
    case 1112: { //CYLINDER
        float X, Y, D, Z;
        rdline();
        res = sscanf(buf, "%f %f %f %f", &X, &Y, &D, &Z);
        if (res != 4)
            break;
        if (!cyl) {
            cyl = new Cylinder(tr);
            tr = cyl;
        }
        cyl->setY(Y); // Y offset
        cyl->setD(D); // wrap Y over this cylinder
        cyl->setZ(Z); // this is rotation center offset
        home.p[3] = last.p[1] / D; // remember A home;
        break;
    }
    case 1114:
        rdline();
        res = sscanf(buf, "%f", &F);
        if (res != 1)
            break;
        tolerance = F;
        break;
    case 1115:
	rdline();
	sscanf(buf, "%d", &multax);
	break;
    case 1118: // clear screen???
        break;
    case 1119:
        rdline();
        res = sscanf(buf, "%d\n", &flag);
        engr_off = (flag == 72);
        break;
    case 1120: // color aux
        break;
    default:;
        cerr << "skipping unknown tech subtype:" << subtype << endl;
    }
}

void CldInterp::r5000(int subtype)
{
    int res;
    if (engr_off)
	return;
    switch (subtype) {
    case 3:
        rdline();
        res = sscanf(buf, "%f %f %f\n", &home.p[0], &home.p[1], &home.p[2]);
        last = home;
        tr->traverse(home);
        break;
    case 5:
        rdline();
        res = sscanf(buf, "%f %f %f\n", &last.p[0], &last.p[1], &last.p[2]);
        if (res == 3)
	    if (multax == 1)
            	last.p[2] += Zsis;
        if (tool_up)
            tr->traverse(last);
        else
            tr->straight_feed(last, feed_xy);
        break;
    case 6: {
        int dir;
        float I, J;
        rdline();
        res = sscanf(buf, "%d %f %f %f %f\n", &dir, &last.p[0], &last.p[1], &I, &J);
        if (res != 5)
            break;
        if (!tool_up)
            tr->arc_feed((dir == 2) ? CW_ARC : CCW_ARC, last, I, J, feed_xy);
        else
            tr->traverse(last);
        }
        break;
    default:;
    }
}


int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.CLD\n", argv[0]);
        return -1;
    }
    CldInterp interp;
    cout.setf(ios::fixed, ios::floatfield);
    cout.precision(3);
    ic = iconv_open("UTF-8", "CP866");
    if (errno == EINVAL)
        cerr << "iconv_open failed\n";
    interp.parse(argv[1]);
    return 0;
}
