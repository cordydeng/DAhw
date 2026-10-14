# Project 2 - EOL DRC Checker

This program implements the end-of-line rule described in the project spec with Boost.Geometry.

## Rule Parameters

All units are um.

```text
eolWidth    = 0.6
eolSpace    = 1.0
eolParallel = 0.7
eolWithin   = 0.4
```

Before checking, all same-layer M1 metal shapes are merged with Boost.Geometry `union_`.
Overlapping or touching metal therefore becomes one conductor and is checked as one shape.

A merged rectangle/polygon bounding box is treated as an EOL candidate when its smaller dimension is less than or equal to `eolWidth`.
For each candidate end, the checker tests:

- the forward EOL spacing region of length `eolSpace`
- the two side halo regions with parallel length `eolParallel` and outward distance `eolWithin`

If another same-layer shape overlaps any checked region, the program reports an EOL violation.

## Files

```text
project2.cpp       Source code
sample_input.txt   Input copied from the project spec
README.md          Compile/run notes
metal_clean.svg    Generated clean metal layout figure
eol_drc.svg        Generated result figure after running
```

## Compile

Use a C++17 compiler with Boost headers available.

```powershell
cd C:\Users\User\Desktop\DAhw\project2
g++ project2.cpp -std=c++17 -O2 -o project2.exe
```

## Run

```powershell
.\project2.exe sample_input.txt
```

The terminal output lists all detected violations. The program also writes:

```text
metal_clean.svg
eol_drc.svg
```

Open `metal_clean.svg` to view the clean input metal layout. Open `eol_drc.svg` to view the same layout with DRC markers. Blue shapes are metal rectangles, and red transparent regions show the EOL rule-check regions that were violated.

## Input Format

The parser follows the Project 1 style:

```text
number_of_layers 1
M1  1
end_of_layer

number_of_rectangles 1
1  4.0 0.0, 7.8 0.6
end_of_rectangle
```

`number_of_polygons` is also accepted. Polygon EOL detection uses the polygon bounding box, which is suitable for the axis-aligned layout examples in this project.
