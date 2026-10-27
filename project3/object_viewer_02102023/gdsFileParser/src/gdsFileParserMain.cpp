/*
 * MIT License
 *
 * Copyright (c) 2017 EDDR Software, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Changes:
 * 2017-01-01: First & Last Name: What you did.
 * 2017-06-10: Kevin Nesmith: Initial contribution.
 *
 */

#include <stdlib.h>

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <iomanip>

#include "gdsFileParser.h"

using namespace std;

#define SET_2W setfill('0') << setw(2)

#include <vector> // Honda: std::vector

void MyTestDump(); // Honda

enum MyTestObjEnum { // Honda
    dbcUnknownObj,
    dbcRectangle,       /* Boundary xyNo=5 */
    dbcBoundary,        /* Boundary xyNo!=5 */
    dbcPath
};

struct MyTestXY { // Honda: sample code of gds point
    int xCoord;
    int yCoord;
};

class MyTestShape // Honda: sample code of gds shape (base-class)
{
    unsigned char       objType; // MyTestObjEnum
    unsigned char       padByte[3];
    int                 layer;
    int                 dataType;
    int                 xyNo;
    vector<MyTestXY>    xyList;

  friend class MyTestParser;
  friend void MyTestDump();
};

class MyTestBoundary : public MyTestShape // Honda: sample code of gds boundary
{
  public:

  friend class MyTestParser;
  friend void MyTestDump();
};

class MyTestPath : public MyTestShape // Honda: sample code of gds path
{
    unsigned short      pathType;
    int                 width;
    int                 bext; // begin extension
    int                 eext; // end extension
  public:

  friend class MyTestParser;
  friend void MyTestDump();
};

class MyTestInstance { // Honda: sample code of gds instance
    string              masterCell;
    MyTestXY            origin;

  friend class MyTestParser;
  friend void MyTestDump();
};

class MyTestCell // Honda: sample code of gds cell
{
    string                      cellName;
    vector<MyTestShape*>        shapeList;
    vector<MyTestInstance*>     instList;

  friend class MyTestParser;
  friend void MyTestDump();
};

class MyTestLib // Honda: sample code of gds library
{
    // related to library
    string                      libName;
    double                      userUnits;
    double                      dbUnits;
    // related to cells
    vector<MyTestCell*>         cellList;

  friend class MyTestParser;
  friend void MyTestDump();
};
class MyTestShape       *MyGlobalShape; // Honda: active global boundary/path
class MyTestBoundary    *MyGlobalBoundary; // Honda: active global boundary
class MyTestPath        *MyGlobalPath; // Honda: active global path
class MyTestInstance    *MyGlobalInstance; // Honda: active global instance
class MyTestCell        *MyGlobalCell; // Honda: active global cell
class MyTestLib         *MyGlobalLib; // Honda: active global library

class MyTestParser : public gdsfp::gdsFileParser
{
protected:
    virtual void onParsedBeginLib() { // Honda: BGNLIB
        cout << "Lib start" << endl;
        MyGlobalLib = new MyTestLib; // Honda
    }
    virtual void onParsedBeginStructure() { // Honda: BGNSTR
        cout << "Structure start" << endl;
        MyGlobalCell = new MyTestCell; // Honda
        MyGlobalLib->cellList.push_back(MyGlobalCell); // Honda
    }
    virtual void onParsedGDSVersion(unsigned short version) {
        cout << "GDSII Version: " << version << endl;
    };
    virtual void onParsedModTime(short year, short month, short day,
                                 short hour, short minute, short sec) {
        cout << "Modified Time: " << endl;

        if(year==0 && month==0) {
            cout << "\tNot recorded." << endl;
        } else {
            cout << "\t" << year << "-" << SET_2W << month <<  "-" << SET_2W <<
                 day << " " << SET_2W << hour << ":" << SET_2W << minute <<
                 ":" << SET_2W << sec << endl;
        }
    };
    virtual void onParsedAccessTime(short year, short month, short day,
                                    short hour, short minute, short sec) {
        cout << "Accessed Time: " << endl;

        if(year==0 && month==0) {
            cout << "\tNot recorded." << endl;
        } else {
            cout << "\t" << year << "-" << SET_2W << month <<  "-" << SET_2W <<
                 day << " " << SET_2W << hour << ":" << SET_2W << minute <<
                 ":" << SET_2W << sec << endl;
        }
    };
    virtual void onParsedLibName(const char *libName) {
        cout << "LibName: " << libName << endl;
        MyGlobalLib->libName = libName; // Honda
    };
    virtual void onParsedUnits(double userUnits, double dbUnits) {
        cout << "UserUnits: " << setprecision(9) << fixed << userUnits << endl;
        cout << "DBUnits: " << setprecision(9) << fixed << dbUnits << endl;
        MyGlobalLib->userUnits = userUnits; // Honda
        MyGlobalLib->dbUnits = dbUnits; // Honda
    };
    virtual void onParsedStrName(const char *strName) {
        cout << "StrName: " << strName << endl;
        MyGlobalCell->cellName = strName; // Honda
    };
    virtual void onParsedBoundaryStart() {
        cout << "Boundry start" << endl;
        MyGlobalBoundary = new MyTestBoundary; // Honda
        MyGlobalShape = MyGlobalBoundary; // Honda
        MyGlobalBoundary->objType = dbcBoundary; // Honda
        MyGlobalCell->shapeList.push_back(MyGlobalShape); // Honda
    };
    virtual void onParsedPathStart() {
        cout << "Path start" << endl;
        MyGlobalPath = new MyTestPath; // Honda
        MyGlobalShape = MyGlobalPath; // Honda
        MyGlobalPath->objType = dbcPath; // Honda
        MyGlobalPath->pathType = 0; // Honda
        MyGlobalPath->width = 0; // Honda
        MyGlobalPath->bext = MyGlobalPath->eext = 0; // Honda
        MyGlobalCell->shapeList.push_back(MyGlobalShape); // Honda
    };
    virtual void onParsedBoxStart() {
        cout << "Box start" << endl;
        MyGlobalBoundary = new MyTestBoundary; // Honda
        MyGlobalShape = MyGlobalBoundary; // Honda
        MyGlobalBoundary->objType = dbcBoundary; // Honda
        MyGlobalCell->shapeList.push_back(MyGlobalShape); // Honda
    };
    virtual void onParsedEndElement() {
        cout << "Element end" << endl;
        MyGlobalShape = NULL; // Honda
        MyGlobalBoundary = NULL; // Honda
        MyGlobalPath = NULL; // Honda
        MyGlobalInstance = NULL; // Honda
    };
    virtual void onParsedEndStructure() {
        cout << "Structure end" << endl;
    };
    virtual void onParsedEndLib() {
        cout << "Lib end" << endl;
    };
    virtual void onParsedColumnsRows(unsigned short columns,
                                     unsigned short rows) {
        cout << "Columns: " << columns << " Rows: " << rows << endl;
    };
    virtual void onParsedPathType(unsigned short pathType) {
        cout << "PathType: " << pathType << endl;
        MyGlobalPath->pathType = pathType; // Honda
    };
    virtual void onParsedStrans(short strans) {
        cout << "Strans: " << strans << endl;
    };
    virtual void onParsedPresentation(short font, short valign,
                                      short halign) {
        cout << "Font: " << font << endl;
        cout << "Valign: " << valign << endl;
        cout << "Halign: " << halign << endl;
    };
    virtual void onParsedNodeStart() {
        cout << "Node start" << endl;
    };
    virtual void onParsedTextStart() {
        cout << "Text start" << endl;
    };
    virtual void onParsedSrefStart() {
        cout << "Sref start" << endl;
        MyGlobalInstance = new MyTestInstance; // Honda
        MyGlobalCell->instList.push_back(MyGlobalInstance); // Honda
    };
    virtual void onParsedArefStart() {
        cout << "Aref start" << endl;
    };
    virtual void onParsedSname(const char *sname) {
        cout << "Sname: " << sname << endl;
        if (!MyGlobalInstance) { // Honda
            cout << "\t...unsupported yet" << endl; // Honda
            return; // Honda
        } // Honda
        MyGlobalInstance->masterCell = sname; // Honda
    };
    virtual void onParsedString(const char *str) {
        cout << "String: " << str << endl;
    };
    virtual void onParsedPropValue(const char *propValue) {
        cout << "Prop Value: " << propValue << endl;
    };
    virtual void onParsedXY(int count, int x[], int y[]) {
        cout << "XY: " << count << endl;

        for(int i=0; i<count; ++i) {
            cout << "(" << x[i] << "," << y[i] << ")";
        }

        cout << endl;
        if (!MyGlobalInstance && !MyGlobalShape) { // Honda
            cout << "\t...unsupported yet" << endl; // Honda
            return; // Honda
        } // Honda

        if (MyGlobalInstance) { // Honda
            MyGlobalInstance->origin.xCoord = x[0]; // Honda
            MyGlobalInstance->origin.yCoord = y[0]; // Honda
        } // Honda
        else { // Honda
            if (MyGlobalShape->objType == dbcBoundary) // Honda
                if (count == 5) MyGlobalShape->objType = dbcRectangle; // Honda
            MyGlobalShape->xyNo = count; // Honda
            MyTestXY tmpOneXY; // Honda
            for (int j = 0; j < count; ++j) { // Honda
                tmpOneXY.xCoord = x[j]; // Honda
                tmpOneXY.yCoord = y[j]; // Honda
                MyGlobalShape->xyList.push_back(tmpOneXY); // Honda
            } // Honda
        } // Honda
    };
    virtual void onParsedLayer(unsigned short layer) {
        cout << "Layer: " << layer << endl;
        if (!MyGlobalShape) { // Honda
            cout << "\t...unsupported yet" << endl; // Honda
            return; // Honda
        } // Honda
        MyGlobalShape->layer = layer; // Honda
    };
    virtual void onParsedWidth(int width) {
        cout << "Width: " << width << endl;
        if (!MyGlobalPath) { // Honda
            cout << "\t...unsupported yet" << endl; // Honda
            return; // Honda
        } // Honda
        MyGlobalPath->width = width; // Honda
    };
    virtual void onParsedDataType(unsigned short dataType) {
        cout << "Data Type: " << dataType << endl;
        MyGlobalShape->dataType = dataType; // Honda
    };
    virtual void onParsedTextType(unsigned short textType) {
        cout << "Text Type: " << textType << endl;
    };
    virtual void onParsedAngle(double angle) {
        cout << "Angle: " << angle << endl;
    };
    virtual void onParsedMag(double mag) {
        cout << "Mag: " << mag << endl;
    };
    virtual void onParsedBeginExtension(unsigned short bext) {
        cout << "Begin Extension: " << bext << endl;
        MyGlobalPath->bext = bext; // Honda
    };
    virtual void onParsedEndExtension(unsigned short eext) {
        cout << "End Extension: " << eext << endl;
        MyGlobalPath->eext = eext; // Honda
    };
    virtual void onParsedPropertyNumber(unsigned short propNum) {
        cout << "Property Number: " << propNum << endl;
    };
    virtual void onParsedNodeType(unsigned short nodeType) {
        cout << "Node Type: " << nodeType << endl;
    };
    virtual void onParsedBoxType(unsigned short boxType) {
        cout << "Box Type: " << boxType << endl;
    };
};


// ****************************************************************************
// main()
//
// This is the top level function that tests the parser.
// ****************************************************************************
int GdsFileParserMain(int argc, char *argv[])
{
    if(argc<2) {
        cerr << "Missing GDSII file as the only parameter." << endl;
        cerr << "Usage: ./testParser /path/to/file.gds" << endl;
        return 1;
    }

    MyTestParser parser;
#if 0 // Honda
    return parser.parse(argv[1]);
#else // Honda
    int rc = parser.parse(argv[1]); // Honda
    MyTestDump(); // Honda
    return rc; // Honda
#endif // Honda
}

int GdsFileParserMain(const char * fileName)
{
    MyTestParser parser;
    int rc = parser.parse(fileName); // Honda
    MyTestDump(); // Honda
    return rc; // Honda
}

#include <assert.h> // Honda
void MyTestDump() // Honda
{
    if (NULL == MyGlobalLib) return;
    fprintf(stdout, "****************************************************************************\n");
    fprintf(stdout, "library: name=%s unserUnits=%g dbUnit=%g\n", MyGlobalLib->libName.c_str(), MyGlobalLib->userUnits, MyGlobalLib->dbUnits);
    MyTestShape         *shape;
    MyTestBoundary      *boundary;
    MyTestPath          *path;
    MyTestInstance      *inst;
    MyTestCell          *cell;
    int                 i1, i2, i3, i4;
    MyTestXY            *pt;
    for (i1 = 0; i1 < MyGlobalLib->cellList.size(); i1++) {
        cell = MyGlobalLib->cellList[i1];
        fprintf(stdout, "cell: name=%s\n", cell->cellName.c_str());
        //
        for (i2 = 0; i2 < cell->instList.size(); i2++) {
            inst = cell->instList[i2];
            fprintf(stdout, "  instance masterCell=%s origin=(%d,%d)\n", inst->masterCell.c_str(), inst->origin.xCoord, inst->origin.yCoord);
        }
        for (i3 = 0; i3 < cell->shapeList.size(); i3++) {
            shape = cell->shapeList[i3];
            switch (shape->objType) {
            case dbcRectangle:
                fprintf(stdout, "  rectangle layer=%d ", shape->layer);
                break;
            case dbcBoundary:
                fprintf(stdout, "  boundary layer=%d ", shape->layer);
                break;
            case dbcPath:
                fprintf(stdout, "  path layer=%d ", shape->layer);
                path = (MyTestPath*) shape;
                fprintf(stdout, "pathType=%d width=%d bext=%d eext=%d ", path->pathType, path->width, path->bext, path->eext);
                break;
            default:
                assert(0);
                break;
            }
            fprintf(stdout, "xyNo=%d ", shape->xyNo);
            for (i4 = 0; i4 < shape->xyNo; i4++) {
                pt = &(shape->xyList[i4]);
                fprintf(stdout, "(%d,%d)", pt->xCoord, pt->yCoord);
            }
            fprintf(stdout, "\n");
        }
    }
}
