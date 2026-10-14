#include <boost/geometry.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

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

using Point = bg::model::d2::point_xy<double>;
using Polygon = bg::model::polygon<Point>;
using MultiPolygon = bg::model::multi_polygon<Polygon>;
using Box = bg::model::box<Point>;

static constexpr double EPS = 1e-9;
static constexpr double EOL_WIDTH = 0.6;
static constexpr double EOL_SPACE = 1.0;
static constexpr double EOL_PARALLEL = 0.7;
static constexpr double EOL_WITHIN = 0.4;

struct Layer {
    std::string name;
    int number = 0;
};

struct Shape {
    int id = 0;
    int layer = 0;
    Polygon poly;
    Box bbox;
};

struct Layout {
    std::vector<Layer> layers;
    std::vector<Shape> shapes;
};

struct Violation {
    int source_id = 0;
    int target_id = 0;
    std::string end_name;
    std::string reason;
    Box region;
};

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

static double minx(const Box& b) { return bg::get<bg::min_corner, 0>(b); }
static double miny(const Box& b) { return bg::get<bg::min_corner, 1>(b); }
static double maxx(const Box& b) { return bg::get<bg::max_corner, 0>(b); }
static double maxy(const Box& b) { return bg::get<bg::max_corner, 1>(b); }

static Polygon make_rectangle(double x1, double y1, double x2, double y2) {
    const double xl = std::min(x1, x2);
    const double xh = std::max(x1, x2);
    const double yl = std::min(y1, y2);
    const double yh = std::max(y1, y2);

    Polygon p;
    bg::append(p.outer(), Point(xl, yl));
    bg::append(p.outer(), Point(xl, yh));
    bg::append(p.outer(), Point(xh, yh));
    bg::append(p.outer(), Point(xh, yl));
    bg::append(p.outer(), Point(xl, yl));
    bg::correct(p);
    return p;
}

static Box polygon_box(const Polygon& p) {
    Box b;
    bg::envelope(p, b);
    return b;
}

static Shape make_shape(int id, int layer, const Polygon& poly) {
    Polygon corrected = poly;
    bg::correct(corrected);
    if (!bg::is_valid(corrected)) {
        throw std::runtime_error("Invalid polygon geometry.");
    }
    return {id, layer, corrected, polygon_box(corrected)};
}

static void append_clean_polygons(const MultiPolygon& input,
                                  MultiPolygon& output) {
    for (auto poly : input) {
        bg::correct(poly);
        if (std::abs(bg::area(poly)) > EPS) {
            output.push_back(poly);
        }
    }
}

static std::vector<Shape> merge_same_layer_shapes(const std::vector<Shape>& shapes,
                                                  int layer) {
    MultiPolygon merged;

    for (const auto& shape : shapes) {
        if (shape.layer != layer) continue;

        if (merged.empty()) {
            merged.push_back(shape.poly);
            continue;
        }

        MultiPolygon next;
        bg::union_(merged, shape.poly, next);
        merged.swap(next);
    }

    std::vector<Shape> result;
    int id = 1;
    for (const auto& poly : merged) {
        result.push_back(make_shape(id++, layer, poly));
    }
    return result;
}

static Layout parse_layout(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) throw std::runtime_error("Cannot open input file: " + filename);

    Layout layout;
    std::string line;
    enum class Section { NONE, LAYER, POLYGON, RECTANGLE };
    Section section = Section::NONE;
    int next_shape_id = 1;

    while (std::getline(fin, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (starts_with(line, "number_of_layers")) {
            section = Section::LAYER;
            continue;
        }
        if (line == "end_of_layer") {
            section = Section::NONE;
            continue;
        }
        if (starts_with(line, "number_of_polygons")) {
            section = Section::POLYGON;
            continue;
        }
        if (line == "end_of_polygon") {
            section = Section::NONE;
            continue;
        }
        if (starts_with(line, "number_of_rectangles")) {
            section = Section::RECTANGLE;
            continue;
        }
        if (line == "end_of_rectangle") {
            section = Section::NONE;
            continue;
        }
        if (starts_with(line, "number_of_texts")) {
            section = Section::NONE;
            continue;
        }
        if (line == "end_of_text") {
            section = Section::NONE;
            continue;
        }

        if (section == Section::LAYER) {
            std::stringstream ss(line);
            Layer layer;
            if (!(ss >> layer.name >> layer.number)) {
                throw std::runtime_error("Bad layer line: " + line);
            }
            layout.layers.push_back(layer);
        } else if (section == Section::POLYGON) {
            std::stringstream head(line);
            int layer = 0;
            if (!(head >> layer)) {
                throw std::runtime_error("Bad polygon line: " + line);
            }

            std::string rest;
            std::getline(head, rest);
            const auto points = split_by_comma(rest);
            if (points.size() < 3) {
                throw std::runtime_error("Polygon needs at least 3 points: " + line);
            }

            Polygon poly;
            for (const auto& token : points) {
                std::stringstream ps(token);
                double x = 0.0;
                double y = 0.0;
                if (!(ps >> x >> y)) {
                    throw std::runtime_error("Bad polygon point: " + token);
                }
                bg::append(poly.outer(), Point(x, y));
            }
            if (!poly.outer().empty()) {
                const Point& first = poly.outer().front();
                const Point& last = poly.outer().back();
                if (std::abs(bg::get<0>(first) - bg::get<0>(last)) > EPS ||
                    std::abs(bg::get<1>(first) - bg::get<1>(last)) > EPS) {
                    bg::append(poly.outer(), first);
                }
            }
            layout.shapes.push_back(make_shape(next_shape_id++, layer, poly));
        } else if (section == Section::RECTANGLE) {
            std::stringstream head(line);
            int layer = 0;
            if (!(head >> layer)) {
                throw std::runtime_error("Bad rectangle line: " + line);
            }

            std::string rest;
            std::getline(head, rest);
            const auto corners = split_by_comma(rest);
            if (corners.size() != 2) {
                throw std::runtime_error("Rectangle needs 2 corner points: " + line);
            }

            double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
            std::stringstream c1(corners[0]);
            std::stringstream c2(corners[1]);
            if (!(c1 >> x1 >> y1) || !(c2 >> x2 >> y2)) {
                throw std::runtime_error("Bad rectangle coordinates: " + line);
            }
            layout.shapes.push_back(make_shape(next_shape_id++, layer,
                                               make_rectangle(x1, y1, x2, y2)));
        }
    }

    return layout;
}

static int m1_layer_number(const Layout& layout) {
    for (const auto& layer : layout.layers) {
        if (layer.name == "M1") return layer.number;
    }
    return layout.layers.empty() ? 1 : layout.layers.front().number;
}

static bool box_has_positive_area(const Box& b) {
    return maxx(b) - minx(b) > EPS && maxy(b) - miny(b) > EPS;
}

static bool shape_overlaps_region(const Shape& shape, const Box& region) {
    if (!box_has_positive_area(region)) return false;

    Polygon region_poly = make_rectangle(minx(region), miny(region),
                                         maxx(region), maxy(region));
    std::vector<Polygon> out;
    bg::intersection(shape.poly, region_poly, out);
    for (const auto& p : out) {
        if (std::abs(bg::area(p)) > EPS) return true;
    }
    return false;
}

static void add_violation(std::vector<Violation>& violations,
                          const Shape& source,
                          const Shape& target,
                          const std::string& end_name,
                          const std::string& reason,
                          const Box& region) {
    violations.push_back({source.id, target.id, end_name, reason, region});
}

static void check_region(const std::vector<Shape>& shapes,
                         const Shape& source,
                         const std::string& end_name,
                         const std::string& reason,
                         const Box& region,
                         std::vector<Violation>& violations) {
    for (const auto& target : shapes) {
        if (target.id == source.id || target.layer != source.layer) continue;
        if (shape_overlaps_region(target, region)) {
            add_violation(violations, source, target, end_name, reason, region);
        }
    }
}

static std::vector<Violation> check_eol(const std::vector<Shape>& shapes) {
    std::vector<Violation> violations;

    for (const auto& shape : shapes) {
        const double w = maxx(shape.bbox) - minx(shape.bbox);
        const double h = maxy(shape.bbox) - miny(shape.bbox);

        if (h <= EOL_WIDTH + EPS && w > EOL_WIDTH + EPS) {
            const Box left_front(Point(minx(shape.bbox) - EOL_SPACE, miny(shape.bbox)),
                                 Point(minx(shape.bbox), maxy(shape.bbox)));
            const Box right_front(Point(maxx(shape.bbox), miny(shape.bbox)),
                                  Point(maxx(shape.bbox) + EOL_SPACE, maxy(shape.bbox)));
            check_region(shapes, shape, "left", "EOL spacing < 1.0", left_front, violations);
            check_region(shapes, shape, "right", "EOL spacing < 1.0", right_front, violations);

            const double lx0 = minx(shape.bbox);
            const double lx1 = std::min(maxx(shape.bbox), minx(shape.bbox) + EOL_PARALLEL);
            const double rx0 = std::max(minx(shape.bbox), maxx(shape.bbox) - EOL_PARALLEL);
            const double rx1 = maxx(shape.bbox);

            check_region(shapes, shape, "left", "side halo violation",
                         Box(Point(lx0, maxy(shape.bbox)), Point(lx1, maxy(shape.bbox) + EOL_WITHIN)),
                         violations);
            check_region(shapes, shape, "left", "side halo violation",
                         Box(Point(lx0, miny(shape.bbox) - EOL_WITHIN), Point(lx1, miny(shape.bbox))),
                         violations);
            check_region(shapes, shape, "right", "side halo violation",
                         Box(Point(rx0, maxy(shape.bbox)), Point(rx1, maxy(shape.bbox) + EOL_WITHIN)),
                         violations);
            check_region(shapes, shape, "right", "side halo violation",
                         Box(Point(rx0, miny(shape.bbox) - EOL_WITHIN), Point(rx1, miny(shape.bbox))),
                         violations);
        }

        if (w <= EOL_WIDTH + EPS && h > EOL_WIDTH + EPS) {
            const Box bottom_front(Point(minx(shape.bbox), miny(shape.bbox) - EOL_SPACE),
                                   Point(maxx(shape.bbox), miny(shape.bbox)));
            const Box top_front(Point(minx(shape.bbox), maxy(shape.bbox)),
                                Point(maxx(shape.bbox), maxy(shape.bbox) + EOL_SPACE));
            check_region(shapes, shape, "bottom", "EOL spacing < 1.0", bottom_front, violations);
            check_region(shapes, shape, "top", "EOL spacing < 1.0", top_front, violations);

            const double by0 = miny(shape.bbox);
            const double by1 = std::min(maxy(shape.bbox), miny(shape.bbox) + EOL_PARALLEL);
            const double ty0 = std::max(miny(shape.bbox), maxy(shape.bbox) - EOL_PARALLEL);
            const double ty1 = maxy(shape.bbox);

            check_region(shapes, shape, "bottom", "side halo violation",
                         Box(Point(maxx(shape.bbox), by0), Point(maxx(shape.bbox) + EOL_WITHIN, by1)),
                         violations);
            check_region(shapes, shape, "bottom", "side halo violation",
                         Box(Point(minx(shape.bbox) - EOL_WITHIN, by0), Point(minx(shape.bbox), by1)),
                         violations);
            check_region(shapes, shape, "top", "side halo violation",
                         Box(Point(maxx(shape.bbox), ty0), Point(maxx(shape.bbox) + EOL_WITHIN, ty1)),
                         violations);
            check_region(shapes, shape, "top", "side halo violation",
                         Box(Point(minx(shape.bbox) - EOL_WITHIN, ty0), Point(minx(shape.bbox), ty1)),
                         violations);
        }
    }

    return violations;
}

static void expand_bbox(Box& dst, const Box& src, bool& initialized) {
    if (!initialized) {
        dst = src;
        initialized = true;
    } else {
        bg::expand(dst, src);
    }
}

static Box full_bbox(const std::vector<Shape>& shapes,
                     const std::vector<Violation>& violations) {
    Box bbox;
    bool initialized = false;
    for (const auto& shape : shapes) expand_bbox(bbox, shape.bbox, initialized);
    for (const auto& v : violations) expand_bbox(bbox, v.region, initialized);
    if (!initialized) bbox = Box(Point(0, 0), Point(1, 1));

    const double width = std::max(1.0, maxx(bbox) - minx(bbox));
    const double height = std::max(1.0, maxy(bbox) - miny(bbox));
    const double pad = std::max(width, height) * 0.08;
    return Box(Point(minx(bbox) - pad, miny(bbox) - pad),
               Point(maxx(bbox) + pad, maxy(bbox) + pad));
}

static std::string xml_escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

static std::string format_num(double value) {
    if (std::abs(value) < EPS) value = 0.0;
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    std::string s = out.str();
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

struct SvgView {
    double width = 1000.0;
    double height = 760.0;
    double left = 86.0;
    double top = 92.0;
    double plot_w = 828.0;
    double plot_h = 540.0;
    Box world;
    double scale = 1.0;
    double used_w = 0.0;
    double used_h = 0.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
};

static SvgView make_view(const std::vector<Shape>& shapes,
                         const std::vector<Violation>& violations) {
    SvgView view;
    view.world = full_bbox(shapes, violations);

    const double world_w = std::max(1.0, maxx(view.world) - minx(view.world));
    const double world_h = std::max(1.0, maxy(view.world) - miny(view.world));
    view.scale = std::min(view.plot_w / world_w, view.plot_h / world_h);
    view.used_w = world_w * view.scale;
    view.used_h = world_h * view.scale;
    view.offset_x = view.left + (view.plot_w - view.used_w) * 0.5;
    view.offset_y = view.top + (view.plot_h - view.used_h) * 0.5;
    return view;
}

static double sx(const SvgView& view, double x) {
    return view.offset_x + (x - minx(view.world)) * view.scale;
}

static double sy(const SvgView& view, double y) {
    return view.offset_y + (maxy(view.world) - y) * view.scale;
}

static std::string polygon_path(const Polygon& poly, const SvgView& view) {
    std::ostringstream path;
    bool first = true;
    for (const auto& p : poly.outer()) {
        path << (first ? "M " : " L ")
             << sx(view, bg::get<0>(p)) << " "
             << sy(view, bg::get<1>(p));
        first = false;
    }
    path << " Z";
    return path.str();
}

static void write_svg_header(std::ostream& svg, const std::string& title) {
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"760\" "
        << "viewBox=\"0 0 1000 760\" role=\"img\" aria-label=\""
        << xml_escape(title) << "\">\n";
    svg << "<rect width=\"1000\" height=\"760\" fill=\"#ffffff\"/>\n";
    svg << "<style><![CDATA[\n";
    svg << ".title{font:700 24px Arial,sans-serif;fill:#111827}"
        << ".sub{font:14px Arial,sans-serif;fill:#4b5563}"
        << ".axis{stroke:#374151;stroke-width:1.2}"
        << ".grid{stroke:#e5e7eb;stroke-width:1}"
        << ".tick{font:12px Arial,sans-serif;fill:#4b5563}"
        << ".metal{fill:#8ecae6;stroke:#145f86;stroke-width:1.6;fill-opacity:.9}"
        << ".viol{fill:#ef4444;stroke:#991b1b;stroke-width:1.8;fill-opacity:.34}"
        << ".label{font:13px Arial,sans-serif;fill:#111827}"
        << ".legend{font:14px Arial,sans-serif;fill:#111827}";
    svg << "\n]]></style>\n";
}

static void write_axes(std::ostream& svg, const SvgView& view) {
    const double x0 = view.offset_x;
    const double y0 = view.offset_y + view.used_h;
    const double x1 = view.offset_x + view.used_w;
    const double y1 = view.offset_y;

    svg << "<line class=\"axis\" x1=\"" << x0 << "\" y1=\"" << y0
        << "\" x2=\"" << x1 << "\" y2=\"" << y0 << "\"/>\n";
    svg << "<line class=\"axis\" x1=\"" << x0 << "\" y1=\"" << y0
        << "\" x2=\"" << x0 << "\" y2=\"" << y1 << "\"/>\n";
    svg << "<text class=\"label\" x=\"" << x1 + 16 << "\" y=\"" << y0 + 5
        << "\">X</text>\n";
    svg << "<text class=\"label\" x=\"" << x0 - 5 << "\" y=\"" << y1 - 16
        << "\" text-anchor=\"end\">Y</text>\n";

    constexpr int ticks = 5;
    for (int i = 0; i <= ticks; ++i) {
        const double t = static_cast<double>(i) / ticks;
        const double wx = minx(view.world) + (maxx(view.world) - minx(view.world)) * t;
        const double px = sx(view, wx);
        svg << "<line class=\"grid\" x1=\"" << px << "\" y1=\"" << y1
            << "\" x2=\"" << px << "\" y2=\"" << y0 << "\"/>\n";
        svg << "<text class=\"tick\" x=\"" << px << "\" y=\"" << y0 + 20
            << "\" text-anchor=\"middle\">" << format_num(wx) << "</text>\n";

        const double wy = miny(view.world) + (maxy(view.world) - miny(view.world)) * t;
        const double py = sy(view, wy);
        svg << "<line class=\"grid\" x1=\"" << x0 << "\" y1=\"" << py
            << "\" x2=\"" << x1 << "\" y2=\"" << py << "\"/>\n";
        svg << "<text class=\"tick\" x=\"" << x0 - 10 << "\" y=\"" << py + 4
            << "\" text-anchor=\"end\">" << format_num(wy) << "</text>\n";
    }
}

static void write_shapes(std::ostream& svg,
                         const std::vector<Shape>& shapes,
                         const SvgView& view) {
    for (const auto& shape : shapes) {
        svg << "<path class=\"metal\" d=\"" << polygon_path(shape.poly, view) << "\"/>\n";
        const double cx = (minx(shape.bbox) + maxx(shape.bbox)) * 0.5;
        const double cy = (miny(shape.bbox) + maxy(shape.bbox)) * 0.5;
        svg << "<text class=\"label\" x=\"" << sx(view, cx) << "\" y=\"" << sy(view, cy) + 4
            << "\" text-anchor=\"middle\">M" << shape.id << "</text>\n";
    }
}

static void write_clean_metal_svg(const std::string& filename,
                                  const std::vector<Shape>& shapes) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG: " + filename);

    const std::vector<Violation> no_violations;
    const SvgView view = make_view(shapes, no_violations);
    write_svg_header(svg, "Clean M1 metal layout");
    svg << "<text class=\"title\" x=\"40\" y=\"42\">Clean M1 Metal Layout</text>\n";
    svg << "<text class=\"sub\" x=\"40\" y=\"66\">Input layout only, no DRC markers</text>\n";
    write_axes(svg, view);
    write_shapes(svg, shapes, view);
    svg << "<rect x=\"760\" y=\"34\" width=\"20\" height=\"12\" class=\"metal\"/>\n";
    svg << "<text class=\"legend\" x=\"790\" y=\"45\">M1 metal</text>\n";
    svg << "</svg>\n";
}

static void write_drc_svg(const std::string& filename,
                          const std::vector<Shape>& shapes,
                          const std::vector<Violation>& violations) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG: " + filename);

    const SvgView view = make_view(shapes, violations);
    write_svg_header(svg, "EOL DRC result");
    svg << "<text class=\"title\" x=\"40\" y=\"42\">EOL DRC Result</text>\n";
    svg << "<text class=\"sub\" x=\"40\" y=\"66\">"
        << violations.size()
        << " violation(s), eolWidth=0.6, eolSpace=1.0, eolParallel=0.7, eolWithin=0.4 um</text>\n";
    write_axes(svg, view);
    write_shapes(svg, shapes, view);
    for (const auto& v : violations) {
        const Polygon region = make_rectangle(minx(v.region), miny(v.region),
                                              maxx(v.region), maxy(v.region));
        svg << "<path class=\"viol\" d=\"" << polygon_path(region, view) << "\"/>\n";
    }

    svg << "<rect x=\"740\" y=\"30\" width=\"20\" height=\"12\" class=\"metal\"/>\n";
    svg << "<text class=\"legend\" x=\"770\" y=\"41\">M1 metal</text>\n";
    svg << "<rect x=\"740\" y=\"54\" width=\"20\" height=\"12\" class=\"viol\"/>\n";
    svg << "<text class=\"legend\" x=\"770\" y=\"65\">Violated EOL check region</text>\n";
    svg << "</svg>\n";
}

static void print_box(const Box& b) {
    std::cout << "(" << minx(b) << ", " << miny(b) << ") - ("
              << maxx(b) << ", " << maxy(b) << ")";
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <layout_input.txt>\n";
            return 1;
        }

        Layout layout = parse_layout(argv[1]);
        const int m1 = m1_layer_number(layout);
        std::vector<Shape> m1_shapes;
        for (const auto& shape : layout.shapes) {
            if (shape.layer == m1) m1_shapes.push_back(shape);
        }
        std::vector<Shape> merged_m1_shapes = merge_same_layer_shapes(layout.shapes, m1);

        const auto violations = check_eol(merged_m1_shapes);
        write_clean_metal_svg("metal_clean.svg", merged_m1_shapes);
        write_drc_svg("eol_drc.svg", merged_m1_shapes, violations);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Original M1 shape count: " << m1_shapes.size() << "\n";
        std::cout << "Merged M1 shape count: " << merged_m1_shapes.size() << "\n";
        std::cout << "EOL violations: " << violations.size() << "\n";
        for (std::size_t i = 0; i < violations.size(); ++i) {
            const auto& v = violations[i];
            std::cout << "V" << (i + 1)
                      << ": shape " << v.source_id
                      << " " << v.end_name
                      << " end vs shape " << v.target_id
                      << " -- " << v.reason
                      << " at ";
            print_box(v.region);
            std::cout << "\n";
        }
        std::cout << "Generated: metal_clean.svg\n";
        std::cout << "Generated: eol_drc.svg\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
