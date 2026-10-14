
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bg = boost::geometry;

using Point        = bg::model::d2::point_xy<double>;
using Polygon      = bg::model::polygon<Point>;
using MultiPolygon = bg::model::multi_polygon<Polygon>;
using Box          = bg::model::box<Point>;

struct Layer {
    std::string name;
    int number = 0;
};

struct Shape {
    int layer = 0;
    Polygon poly;
    bool is_rectangle = false;
};

struct TextLabel {
    int layer = 0;
    std::string name;
    Point pos;
};

struct Layout {
    std::vector<Layer> layers;
    std::vector<Shape> polygons;
    std::vector<Shape> rectangles;
    std::vector<TextLabel> texts;
};

struct Pin {
    std::string name;
    int layer = 0;
    Polygon shape;
    Point label_pos;
};

struct Rect {
    double xl, yl, xh, yh;
};

static constexpr double EPS = 1e-9;
static constexpr double MIN_SPACING = 0.5;

static std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static bool starts_with(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

static std::vector<std::string> split_by_comma(const std::string& s) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) parts.push_back(trim(item));
    return parts;
}

static Polygon make_rectangle_polygon(double x1, double y1, double x2, double y2) {
    double xl = std::min(x1, x2), xr = std::max(x1, x2);
    double yl = std::min(y1, y2), yr = std::max(y1, y2);

    Polygon p;
    bg::append(p.outer(), Point(xl, yl));
    bg::append(p.outer(), Point(xl, yr));
    bg::append(p.outer(), Point(xr, yr));
    bg::append(p.outer(), Point(xr, yl));
    bg::append(p.outer(), Point(xl, yl));
    bg::correct(p);
    return p;
}

static Box polygon_box(const Polygon& p) {
    Box b;
    bg::envelope(p, b);
    return b;
}

static void expand_box(Box& dst, const Box& src, bool& initialized) {
    if (!initialized) {
        dst = src;
        initialized = true;
    } else {
        bg::expand(dst, src);
    }
}

static Layout parse_layout(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) throw std::runtime_error("Cannot open input file: " + filename);

    Layout layout;
    std::string line;
    enum class Section { NONE, LAYER, POLYGON, RECTANGLE, TEXT };
    Section section = Section::NONE;

    while (std::getline(fin, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (starts_with(line, "number_of_layers")) {
            section = Section::LAYER; continue;
        }
        if (line == "end_of_layer") {
            section = Section::NONE; continue;
        }
        if (starts_with(line, "number_of_polygons")) {
            section = Section::POLYGON; continue;
        }
        if (line == "end_of_polygon") {
            section = Section::NONE; continue;
        }
        if (starts_with(line, "number_of_rectangles")) {
            section = Section::RECTANGLE; continue;
        }
        if (line == "end_of_rectangle") {
            section = Section::NONE; continue;
        }
        if (starts_with(line, "number_of_texts")) {
            section = Section::TEXT; continue;
        }
        if (line == "end_of_text") {
            section = Section::NONE; continue;
        }

        if (section == Section::LAYER) {
            std::stringstream ss(line);
            Layer l;
            if (!(ss >> l.name >> l.number))
                throw std::runtime_error("Bad layer line: " + line);
            layout.layers.push_back(l);
        }
        else if (section == Section::POLYGON) {
            std::stringstream head(line);
            int layer;
            if (!(head >> layer))
                throw std::runtime_error("Bad polygon line: " + line);

            std::string rest;
            std::getline(head, rest);
            auto pts = split_by_comma(trim(rest));
            if (pts.size() < 3)
                throw std::runtime_error("Polygon has fewer than 3 points: " + line);

            Polygon poly;
            for (const auto& token : pts) {
                std::stringstream ps(token);
                double x, y;
                if (!(ps >> x >> y))
                    throw std::runtime_error("Bad polygon point: " + token);
                bg::append(poly.outer(), Point(x, y));
            }

            if (!poly.outer().empty()) {
                const Point& a = poly.outer().front();
                const Point& b = poly.outer().back();
                if (std::abs(bg::get<0>(a) - bg::get<0>(b)) > EPS ||
                    std::abs(bg::get<1>(a) - bg::get<1>(b)) > EPS) {
                    bg::append(poly.outer(), a);
                }
            }

            bg::correct(poly);
            if (!bg::is_valid(poly))
                throw std::runtime_error("Invalid polygon geometry: " + line);

            layout.polygons.push_back({layer, poly, false});
        }
        else if (section == Section::RECTANGLE) {
            std::stringstream head(line);
            int layer;
            if (!(head >> layer))
                throw std::runtime_error("Bad rectangle line: " + line);

            std::string rest;
            std::getline(head, rest);
            auto pts = split_by_comma(trim(rest));
            if (pts.size() != 2)
                throw std::runtime_error("Rectangle must have 2 corner points: " + line);

            double x1, y1, x2, y2;
            std::stringstream s1(pts[0]), s2(pts[1]);
            if (!(s1 >> x1 >> y1) || !(s2 >> x2 >> y2))
                throw std::runtime_error("Bad rectangle coordinates: " + line);

            layout.rectangles.push_back(
                {layer, make_rectangle_polygon(x1, y1, x2, y2), true}
            );
        }
        else if (section == Section::TEXT) {
            std::stringstream ss(line);
            TextLabel t;
            double x, y;
            if (!(ss >> t.layer >> t.name >> x >> y))
                throw std::runtime_error("Bad text line: " + line);
            t.pos = Point(x, y);
            layout.texts.push_back(t);
        }
    }
    return layout;
}

static int m1_layer_number(const Layout& layout) {
    for (const auto& layer : layout.layers) {
        if (layer.name == "M1") return layer.number;
    }
    throw std::runtime_error("Input does not define an M1 layer.");
}

static Box layer_bbox(const Layout& layout, int layer) {
    Box bbox;
    bool initialized = false;

    for (const auto& s : layout.polygons)
        if (s.layer == layer)
            expand_box(bbox, polygon_box(s.poly), initialized);
    for (const auto& s : layout.rectangles)
        if (s.layer == layer)
            expand_box(bbox, polygon_box(s.poly), initialized);

    if (!initialized)
        throw std::runtime_error("No M1 polygons or rectangles in input.");

    return bbox;
}

static std::vector<Pin> identify_pins(const Layout& layout) {
    std::vector<Pin> pins;

    for (const auto& t : layout.texts) {
        bool found = false;

        for (const auto& s : layout.polygons) {
            if (s.layer == t.layer && bg::covered_by(t.pos, s.poly)) {
                pins.push_back({t.name, t.layer, s.poly, t.pos});
                found = true;
            }
        }

        for (const auto& s : layout.rectangles) {
            if (s.layer == t.layer && bg::covered_by(t.pos, s.poly)) {
                pins.push_back({t.name, t.layer, s.poly, t.pos});
                found = true;
            }
        }

        if (!found) {
            std::cerr << "Warning: text \"" << t.name
                      << "\" does not touch any shape on layer "
                      << t.layer << "\n";
        }
    }

    return pins;
}

static MultiPolygon buffer_polygon(const Polygon& poly, double dist) {
    MultiPolygon out;

    bg::strategy::buffer::distance_symmetric<double> distance_strategy(dist);
    bg::strategy::buffer::side_straight side_strategy;
    bg::strategy::buffer::join_miter join_strategy;
    bg::strategy::buffer::end_flat end_strategy;
    bg::strategy::buffer::point_square point_strategy;

    bg::buffer(poly, out,
               distance_strategy,
               side_strategy,
               join_strategy,
               end_strategy,
               point_strategy);

    for (auto& p : out) bg::correct(p);
    return out;
}

static MultiPolygon carve_blockage(const Box& bbox,
                                    const std::vector<Pin>& pins,
                                    double spacing) {
    Polygon big = make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(bbox),
        bg::get<bg::min_corner, 1>(bbox),
        bg::get<bg::max_corner, 0>(bbox),
        bg::get<bg::max_corner, 1>(bbox)
    );

    MultiPolygon current;
    current.push_back(big);

    for (const auto& pin : pins) {
        MultiPolygon expanded = buffer_polygon(pin.shape, spacing);

        for (const auto& cut : expanded) {
            MultiPolygon next;

            for (const auto& region : current) {
                MultiPolygon diff;
                bg::difference(region, cut, diff);

                for (auto& p : diff) {
                    bg::correct(p);
                    if (std::abs(bg::area(p)) > EPS)
                        next.push_back(p);
                }
            }

            current = std::move(next);
        }
    }

    return current;
}

static void collect_ring_coords(const Polygon::ring_type& ring,
                                std::vector<double>& xs,
                                std::vector<double>& ys) {
    for (const auto& p : ring) {
        xs.push_back(bg::get<0>(p));
        ys.push_back(bg::get<1>(p));
    }
}

static void unique_sort(std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    std::vector<double> out;

    for (double x : v) {
        if (out.empty() || std::abs(x - out.back()) > EPS)
            out.push_back(x);
    }

    v.swap(out);
}

static bool covered_by_any(const Polygon& cell, const MultiPolygon& mp) {
    for (const auto& poly : mp) {
        if (bg::covered_by(cell, poly))
            return true;
    }
    return false;
}

static std::vector<Rect> decompose_to_rectangles(const MultiPolygon& blockage) {
    std::vector<double> xs, ys;

    for (const auto& poly : blockage) {
        collect_ring_coords(poly.outer(), xs, ys);
        for (const auto& inner : poly.inners())
            collect_ring_coords(inner, xs, ys);
    }

    unique_sort(xs);
    unique_sort(ys);

    if (xs.size() < 2 || ys.size() < 2)
        return {};

    int nx = static_cast<int>(xs.size()) - 1;
    int ny = static_cast<int>(ys.size()) - 1;

    std::vector<std::vector<bool>> occupied(
        ny, std::vector<bool>(nx, false)
    );
    std::vector<std::vector<bool>> used(
        ny, std::vector<bool>(nx, false)
    );

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            Polygon cell = make_rectangle_polygon(
                xs[i], ys[j], xs[i + 1], ys[j + 1]);
            // A center-point check can include part of a pin's clearance when
            // a pin has a diagonal edge. Keep only fully covered cells.
            occupied[j][i] = covered_by_any(cell, blockage);
        }
    }

    std::vector<Rect> rects;

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            if (!occupied[j][i] || used[j][i]) continue;

            int i2 = i;
            while (i2 + 1 < nx &&
                   occupied[j][i2 + 1] &&
                   !used[j][i2 + 1]) {
                ++i2;
            }

            int j2 = j;
            while (j2 + 1 < ny) {
                bool ok = true;
                for (int k = i; k <= i2; ++k) {
                    if (!occupied[j2 + 1][k] || used[j2 + 1][k]) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) break;
                ++j2;
            }

            for (int y = j; y <= j2; ++y)
                for (int x = i; x <= i2; ++x)
                    used[y][x] = true;

            rects.push_back({xs[i], ys[j], xs[i2 + 1], ys[j2 + 1]});
        }
    }

    return rects;
}

static std::string escape_xml(const std::string& value) {
    std::string result;
    for (char ch : value) {
        switch (ch) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += ch;
        }
    }
    return result;
}

static Box padded_display_box(const Box& box) {
    double min_x = bg::get<bg::min_corner, 0>(box);
    double min_y = bg::get<bg::min_corner, 1>(box);
    double max_x = bg::get<bg::max_corner, 0>(box);
    double max_y = bg::get<bg::max_corner, 1>(box);
    double padding = std::max(max_x - min_x, max_y - min_y) * 0.02;
    return Box(Point(min_x - padding, min_y - padding),
               Point(max_x + padding, max_y + padding));
}

static void write_pin_labels(std::ostream& svg,
                             const std::vector<Pin>& pins,
                             const Box& display_bbox) {
    constexpr double canvas = 800.0;
    double min_x = bg::get<bg::min_corner, 0>(display_bbox);
    double min_y = bg::get<bg::min_corner, 1>(display_bbox);
    double max_x = bg::get<bg::max_corner, 0>(display_bbox);
    double max_y = bg::get<bg::max_corner, 1>(display_bbox);
    double scale = std::min(canvas / (max_x - min_x),
                            canvas / (max_y - min_y));
    double pad_x = (canvas - (max_x - min_x) * scale) / 2.0;
    double pad_y = (canvas - (max_y - min_y) * scale) / 2.0;

    for (const auto& pin : pins) {
        double x = pad_x + (bg::get<0>(pin.label_pos) - min_x) * scale;
        double y = pad_y + (max_y - bg::get<1>(pin.label_pos)) * scale;
        svg << "<text x=\"" << x << "\" y=\"" << y
            << "\" text-anchor=\"middle\" dominant-baseline=\"middle\""
            << " font-family=\"sans-serif\" font-size=\"16\""
            << " fill=\"white\" stroke=\"#2f5d9f\" stroke-width=\"0.5\""
            << " paint-order=\"stroke\">" << escape_xml(pin.name)
            << "</text>\n";
    }
}

static void write_svg_original(const std::string& filename,
                               const Layout& layout,
                               const std::vector<Pin>& pins,
                               const Box& bbox) {
    std::ofstream svg(filename);
    bg::svg_mapper<Point> mapper(svg, 800, 800);
    svg << "<rect x=\"0\" y=\"0\" width=\"800\" height=\"800\" fill=\"white\"/>\n";
    Box display_bbox = bbox;

    for (const auto& s : layout.polygons) {
        mapper.add(s.poly);
        bg::expand(display_bbox, polygon_box(s.poly));
    }
    for (const auto& s : layout.rectangles) {
        mapper.add(s.poly);
        bg::expand(display_bbox, polygon_box(s.poly));
    }

    Polygon border = make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(bbox),
        bg::get<bg::min_corner, 1>(bbox),
        bg::get<bg::max_corner, 0>(bbox),
        bg::get<bg::max_corner, 1>(bbox)
    );
    mapper.add(border);
    display_bbox = padded_display_box(display_bbox);
    mapper.add(make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(display_bbox),
        bg::get<bg::min_corner, 1>(display_bbox),
        bg::get<bg::max_corner, 0>(display_bbox),
        bg::get<bg::max_corner, 1>(display_bbox)));

    for (const auto& s : layout.polygons) {
        mapper.map(s.poly,
            "fill:#93addb;stroke:#416da9;stroke-width:1");
    }

    for (const auto& s : layout.rectangles) {
        mapper.map(s.poly,
            "fill:#93addb;stroke:#416da9;stroke-width:1");
    }

    for (const auto& p : pins) {
        mapper.map(p.shape,
            "fill:#4779bf;stroke:#315e9d;stroke-width:1");
    }

    mapper.map(border,
        "fill:none;stroke:#315e9d;stroke-width:1");
    write_pin_labels(svg, pins, display_bbox);
}

static void write_svg_final(const std::string& filename,
                            const std::vector<Rect>& rects,
                            const std::vector<Pin>& pins,
                            const Box& bbox) {
    std::ofstream svg(filename);
    bg::svg_mapper<Point> mapper(svg, 800, 800);
    svg << "<rect x=\"0\" y=\"0\" width=\"800\" height=\"800\" fill=\"white\"/>\n";
    Box display_bbox = bbox;

    std::vector<Polygon> rect_polys;
    for (const auto& r : rects) {
        rect_polys.push_back(make_rectangle_polygon(r.xl, r.yl, r.xh, r.yh));
        mapper.add(rect_polys.back());
        bg::expand(display_bbox, polygon_box(rect_polys.back()));
    }

    for (const auto& p : pins) {
        mapper.add(p.shape);
    }

    Polygon border = make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(bbox),
        bg::get<bg::min_corner, 1>(bbox),
        bg::get<bg::max_corner, 0>(bbox),
        bg::get<bg::max_corner, 1>(bbox)
    );
    mapper.add(border);
    display_bbox = padded_display_box(display_bbox);
    mapper.add(make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(display_bbox),
        bg::get<bg::min_corner, 1>(display_bbox),
        bg::get<bg::max_corner, 0>(display_bbox),
        bg::get<bg::max_corner, 1>(display_bbox)));

    for (const auto& p : rect_polys) {
        mapper.map(p,
            "fill:#d0cece;stroke:#505050;stroke-width:1");
    }

    for (const auto& p : pins) {
        mapper.map(p.shape,
            "fill:#4779bf;stroke:#315e9d;stroke-width:1");
    }

    mapper.map(border,
        "fill:none;stroke:#315e9d;stroke-width:1");
    write_pin_labels(svg, pins, display_bbox);
}

static std::string svg_body(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("Cannot open SVG file: " + filename);
    std::ostringstream contents;
    contents << file.rdbuf();
    std::string xml = contents.str();
    std::size_t start = xml.find('>', xml.find("<svg"));
    std::size_t end = xml.rfind("</svg>");
    if (start == std::string::npos || end == std::string::npos || end <= start)
        throw std::runtime_error("Invalid SVG file: " + filename);
    return xml.substr(start + 1, end - start - 1);
}

static void write_comparison_svg(const std::string& filename) {
    std::string original = svg_body("A_original.svg");
    std::string blockage = svg_body("B_blockage.svg");
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG file: " + filename);
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"1680\" height=\"800\" viewBox=\"0 0 1680 800\">\n"
        << "<rect width=\"1680\" height=\"800\" fill=\"white\"/>\n"
        << "<g>" << original << "</g>\n"
        << "<path d=\"M 808,385 L 844,385 L 844,373 "
        << "L 875,400 L 844,427 L 844,415 L 808,415 Z\" "
        << "fill=\"#ffeb00\" stroke=\"#315e9d\" stroke-width=\"2\"/>\n"
        << "<g transform=\"translate(880 0)\">" << blockage << "</g>\n"
        << "</svg>\n";
}

static void print_results(const Box& bbox,
                          const std::vector<Pin>& pins,
                          const std::vector<Rect>& rects) {
    std::cout << std::fixed << std::setprecision(3);

    std::cout << "=== Smallest enclosing rectangle ===\n";
    std::cout << "("
              << bg::get<bg::min_corner, 0>(bbox) << ", "
              << bg::get<bg::min_corner, 1>(bbox) << ") -> ("
              << bg::get<bg::max_corner, 0>(bbox) << ", "
              << bg::get<bg::max_corner, 1>(bbox) << ")\n\n";

    std::cout << "=== Pins identified from text locations ===\n";
    for (const auto& p : pins) {
        Box b = polygon_box(p.shape);
        std::cout << "Pin " << p.name
                  << "  layer=" << p.layer
                  << "  bbox=("
                  << bg::get<bg::min_corner, 0>(b) << ", "
                  << bg::get<bg::min_corner, 1>(b) << ") -> ("
                  << bg::get<bg::max_corner, 0>(b) << ", "
                  << bg::get<bg::max_corner, 1>(b) << ")\n";
    }

    std::cout << "\n=== Final blockage rectangles ===\n";
    for (std::size_t i = 0; i < rects.size(); ++i) {
        const auto& r = rects[i];
        std::cout << "R" << (i + 1) << ": "
                  << r.xl << " " << r.yl << ", "
                  << r.xh << " " << r.yh << "\n";
    }

    std::cout << "\nGenerated:\n"
              << "  A_original.svg\n"
              << "  B_blockage.svg\n"
              << "  AB_comparison.svg\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input.txt>\n";
            return 1;
        }

        Layout layout = parse_layout(argv[1]);
        int m1_layer = m1_layer_number(layout);
        Box bbox = layer_bbox(layout, m1_layer);
        std::vector<Pin> all_pins = identify_pins(layout);
        std::vector<Pin> pins;
        for (const auto& pin : all_pins) {
            if (pin.layer == m1_layer) pins.push_back(pin);
        }

        write_svg_original("A_original.svg", layout, all_pins, bbox);

        MultiPolygon final_blockage =
            carve_blockage(bbox, pins, MIN_SPACING);

        std::vector<Rect> rectangles =
            decompose_to_rectangles(final_blockage);

        write_svg_final("B_blockage.svg", rectangles, pins, bbox);
        write_comparison_svg("AB_comparison.svg");

        print_results(bbox, pins, rectangles);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
