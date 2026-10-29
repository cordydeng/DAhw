#include <boost/geometry.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
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
    int check_id = 0;
    int source_id = 0;
    int target_id = 0;
    std::string end_name;
    std::string reason;
    Box region;
    int layer = 0;
};

struct DrcResult {
    struct EolEdge {
        int id;
        Point start;
        Point end;
        int layer;
        int source_id = 0;
    };
    std::vector<EolEdge> edges;
    std::vector<Violation> violations;
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

static std::vector<Layer> metal_layers(const Layout& layout) {
    std::vector<Layer> result;
    for (const auto& layer : layout.layers) {
        if (layer.name.size() > 1 && layer.name[0] == 'M' &&
            layer.name.find_first_not_of("0123456789", 1) == std::string::npos) {
            if (std::none_of(result.begin(), result.end(),
                [&](const Layer& item) { return item.number == layer.number; })) {
                result.push_back(layer);
            }
        }
    }
    if (result.empty()) throw std::runtime_error("No metal layers named M1, M2, ... were declared.");
    return result;
}

static std::string layer_name(const std::vector<Layer>& layers, int number) {
    for (const auto& layer : layers) if (layer.number == number) return layer.name;
    return "Layer " + std::to_string(number);
}

static std::string layer_color(const std::vector<Layer>& layers, int number) {
    const auto it = std::find_if(layers.begin(), layers.end(),
        [&](const Layer& layer) { return layer.number == number; });
    const std::size_t index = static_cast<std::size_t>(it - layers.begin());
    // Golden-angle hues keep adjacent layers visually distinct.
    return "hsl(" + std::to_string((200 + index * 137) % 360) + ",65%,48%)";
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
                          int check_id,
                          const Shape& source,
                          const Shape& target,
                          const std::string& end_name,
                          const std::string& reason,
                          const Box& region) {
    violations.push_back({check_id, source.id, target.id, end_name, reason, region, source.layer});
}

static int check_region(const std::vector<Shape>& shapes,
                         const Shape& source,
                         int check_id,
                         const std::string& end_name,
                         const std::string& reason,
                         const Box& region,
                         std::vector<Violation>& violations) {
    int count = 0;
    for (const auto& target : shapes) {
        if (target.id == source.id || target.layer != source.layer) continue;
        if (shape_overlaps_region(target, region)) {
            add_violation(violations, check_id, source, target, end_name, reason, region);
            ++count;
        }
    }
    return count;
}

static Box normalized_box(double x1, double y1, double x2, double y2) {
    return Box(Point(std::min(x1, x2), std::min(y1, y2)),
               Point(std::max(x1, x2), std::max(y1, y2)));
}

static double point_distance(const Point& a, const Point& b) {
    return std::hypot(bg::get<0>(a) - bg::get<0>(b),
                      bg::get<1>(a) - bg::get<1>(b));
}

static void remove_collinear_vertices(std::vector<Point>& vertices) {
    bool removed = true;
    while (removed && vertices.size() >= 3) {
        removed = false;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const Point& a = vertices[(i + vertices.size() - 1) % vertices.size()];
            const Point& b = vertices[i];
            const Point& c = vertices[(i + 1) % vertices.size()];
            const double abx = bg::get<0>(b) - bg::get<0>(a);
            const double aby = bg::get<1>(b) - bg::get<1>(a);
            const double bcx = bg::get<0>(c) - bg::get<0>(b);
            const double bcy = bg::get<1>(c) - bg::get<1>(b);
            const double cross = abx * bcy - aby * bcx;
            const double dot = abx * bcx + aby * bcy;
            if (std::abs(cross) <= EPS && dot > EPS) {
                vertices.erase(vertices.begin() + i);
                removed = true;
                break;
            }
        }
    }
}

// Boost's corrected outer rings are clockwise. For a clockwise edge, the
// polygon interior is on its right and the outward normal is on its left.
static DrcResult find_eol_edges(const std::vector<Shape>& shapes) {
    DrcResult result;
    int next_check_id = 1;

    for (const auto& shape : shapes) {
        std::vector<Point> vertices(shape.poly.outer().begin(), shape.poly.outer().end());
        if (vertices.size() > 1 && point_distance(vertices.front(), vertices.back()) <= EPS) {
            vertices.pop_back();
        }
        remove_collinear_vertices(vertices);

        // Classify actual boundary edges before looking for neighboring shapes.
        const std::size_t n = vertices.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Point& p0 = vertices[i];
            const Point& p1 = vertices[(i + 1) % n];
            const Point& previous = vertices[(i + n - 1) % n];
            const Point& next = vertices[(i + 2) % n];
            const double dx = bg::get<0>(p1) - bg::get<0>(p0);
            const double dy = bg::get<1>(p1) - bg::get<1>(p0);
            const bool vertical = std::abs(dx) <= EPS && std::abs(dy) > EPS;
            const bool horizontal = std::abs(dy) <= EPS && std::abs(dx) > EPS;
            if (!vertical && !horizontal) continue;

            const double edge_length = std::hypot(dx, dy);
            if (edge_length > EOL_WIDTH + EPS) continue;

            const double ox = vertical ? (dy > 0.0 ? -1.0 : 1.0) : 0.0;
            const double oy = horizontal ? (dx > 0.0 ? 1.0 : -1.0) : 0.0;
            const double ix = -ox;
            const double iy = -oy;
            const double tx = dx / edge_length;
            const double ty = dy / edge_length;
            const double prev_dx = bg::get<0>(previous) - bg::get<0>(p0);
            const double prev_dy = bg::get<1>(previous) - bg::get<1>(p0);
            const double next_dx = bg::get<0>(next) - bg::get<0>(p1);
            const double next_dy = bg::get<1>(next) - bg::get<1>(p1);
            const double prev_inward = prev_dx * ix + prev_dy * iy;
            const double next_inward = next_dx * ix + next_dy * iy;
            const double prev_along_edge = prev_dx * tx + prev_dy * ty;
            const double next_along_edge = next_dx * tx + next_dy * ty;
            if (prev_inward <= EPS || next_inward <= EPS ||
                std::abs(prev_along_edge) > EPS || std::abs(next_along_edge) > EPS) {
                continue;
            }

            const int check_id = next_check_id++;
            result.edges.push_back({check_id, p0, p1, shape.layer});
        }
    }
    return result;
}

// Union is used only to determine the physical boundary. Input shapes and
// their IDs remain independent for drawing, ownership and target checks.
static std::vector<Shape> exposed_boundaries(const std::vector<Shape>& shapes) {
    std::vector<int> layers;
    for (const auto& shape : shapes) {
        if (std::find(layers.begin(), layers.end(), shape.layer) == layers.end())
            layers.push_back(shape.layer);
    }
    std::vector<Shape> boundaries;
    for (int layer : layers) {
        MultiPolygon merged;
        for (const auto& shape : shapes) {
            if (shape.layer != layer) continue;
            if (merged.empty()) merged.push_back(shape.poly);
            else {
                MultiPolygon next;
                bg::union_(merged, shape.poly, next);
                merged.swap(next);
            }
        }
        for (const auto& poly : merged)
            boundaries.push_back(make_shape(static_cast<int>(boundaries.size()) + 1, layer, poly));
    }
    return boundaries;
}

// Map a physical EOL back to positive-length portions owned by an input shape.
// Require matching edge direction: the metal must be on the same side of both.
static std::vector<std::pair<Point, Point>> owned_edge_parts(
    const DrcResult::EolEdge& edge, const Shape& source) {
    std::vector<std::pair<Point, Point>> parts;
    const bool vertical = std::abs(bg::get<0>(edge.end) - bg::get<0>(edge.start)) <= EPS;
    const int axis = vertical ? 1 : 0;
    const double e0 = vertical ? bg::get<1>(edge.start) : bg::get<0>(edge.start);
    const double e1 = vertical ? bg::get<1>(edge.end) : bg::get<0>(edge.end);
    const double fixed = vertical ? bg::get<0>(edge.start) : bg::get<1>(edge.start);
    std::vector<Point> ring(source.poly.outer().begin(), source.poly.outer().end());
    if (ring.size() > 1 && point_distance(ring.front(), ring.back()) <= EPS) ring.pop_back();
    remove_collinear_vertices(ring);
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point& a = ring[i];
        const Point& b = ring[(i + 1) % ring.size()];
        const double a_fixed = axis == 1 ? bg::get<0>(a) : bg::get<1>(a);
        const double b_fixed = axis == 1 ? bg::get<0>(b) : bg::get<1>(b);
        if (std::abs(a_fixed - fixed) > EPS || std::abs(b_fixed - fixed) > EPS) continue;
        const double a_along = axis == 1 ? bg::get<1>(a) : bg::get<0>(a);
        const double b_along = axis == 1 ? bg::get<1>(b) : bg::get<0>(b);
        if ((b_along - a_along) * (e1 - e0) <= 0.0) continue;
        double lo = std::max(std::min(e0, e1), std::min(a_along, b_along));
        double hi = std::min(std::max(e0, e1), std::max(a_along, b_along));
        if (hi - lo <= EPS) continue;
        if (e1 < e0) std::swap(lo, hi);
        parts.push_back({vertical ? Point(fixed, lo) : Point(lo, fixed),
                         vertical ? Point(fixed, hi) : Point(hi, fixed)});
    }
    return parts;
}

static DrcResult check_eol(const std::vector<Shape>& shapes) {
    DrcResult result;
    const auto physical = find_eol_edges(exposed_boundaries(shapes));
    for (const auto& edge : physical.edges) {
        for (const auto& source : shapes) {
            if (source.layer != edge.layer) continue;
            for (const auto& part : owned_edge_parts(edge, source)) {
                const Point& p0 = part.first;
                const Point& p1 = part.second;
                const double dx = bg::get<0>(p1) - bg::get<0>(p0);
                const double dy = bg::get<1>(p1) - bg::get<1>(p0);
                const bool vertical = std::abs(dx) <= EPS;
                const double ox = vertical ? (dy > 0.0 ? -1.0 : 1.0) : 0.0;
                const double oy = vertical ? 0.0 : (dx > 0.0 ? 1.0 : -1.0);
                const int check_id = static_cast<int>(result.edges.size()) + 1;
                result.edges.push_back({check_id, p0, p1, source.layer, source.id});
                const std::string end_name = ox < 0 ? "left" : ox > 0 ? "right" : oy < 0 ? "bottom" : "top";
                const double margin_x = vertical ? 0.0 : EOL_WITHIN;
                const double margin_y = vertical ? EOL_WITHIN : 0.0;
                const Box region = normalized_box(
                    std::min(bg::get<0>(p0), bg::get<0>(p1)) + std::min(0.0, ox * EOL_SPACE) - margin_x,
                    std::min(bg::get<1>(p0), bg::get<1>(p1)) + std::min(0.0, oy * EOL_SPACE) - margin_y,
                    std::max(bg::get<0>(p0), bg::get<0>(p1)) + std::max(0.0, ox * EOL_SPACE) + margin_x,
                    std::max(bg::get<1>(p0), bg::get<1>(p1)) + std::max(0.0, oy * EOL_SPACE) + margin_y);
                check_region(shapes, source, check_id, end_name, "EOL spacing < 1.0", region, result.violations);
            }
        }
    }
    return result;
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
    const auto append_ring = [&](const auto& ring) {
        bool first = true;
        for (const auto& p : ring) {
            path << (first ? "M " : " L ")
                 << sx(view, bg::get<0>(p)) << " "
                 << sy(view, bg::get<1>(p));
            first = false;
        }
        path << " Z ";
    };
    append_ring(poly.outer());
    for (const auto& ring : poly.inners()) append_ring(ring);
    return path.str();
}

static void write_svg_header(std::ostream& svg, const std::string& title,
                             std::size_t layer_count) {
    const std::size_t height = 800 + ((layer_count + 3) / 4) * 28;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1000\" height=\"" << height << "\" "
        << "viewBox=\"0 0 1000 " << height << "\" role=\"img\" aria-label=\""
        << xml_escape(title) << "\">\n";
    svg << "<rect width=\"1000\" height=\"" << height << "\" fill=\"#ffffff\"/>\n";
    svg << "<style><![CDATA[\n";
    svg << ".title{font:700 24px Arial,sans-serif;fill:#111827}"
        << ".sub{font:14px Arial,sans-serif;fill:#4b5563}"
        << ".axis{stroke:#374151;stroke-width:1.2}"
        << ".grid{stroke:#e5e7eb;stroke-width:1}"
        << ".tick{font:12px Arial,sans-serif;fill:#4b5563}"
        << ".metal{fill:#8ecae6;stroke:#145f86;stroke-width:1.6;fill-opacity:.9;fill-rule:evenodd}"
        << ".check{fill:#ff8800;stroke:#e67300;stroke-width:1.8;fill-opacity:.6}"
        << ".viol{fill:#ff0000;stroke:#d90000;stroke-width:1.8;fill-opacity:.9}"
        << ".eol{stroke:#166534;stroke-width:3.5;stroke-linecap:round}"
        << ".metal-label{font:700 13px Arial,sans-serif;fill:#111827;paint-order:stroke;stroke:#fff;stroke-width:3;stroke-linejoin:round;pointer-events:none}"
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
                         const SvgView& view,
                         const std::vector<Layer>& layers) {
    for (const auto& shape : shapes) {
        svg << "<path class=\"metal\" style=\"fill:" << layer_color(layers, shape.layer)
            << ";stroke:" << layer_color(layers, shape.layer)
            << ";fill-opacity:.28\" d=\"" << polygon_path(shape.poly, view)
            << "\"><title>" << xml_escape(layer_name(layers, shape.layer))
            << " / S" << shape.id << "</title></path>\n";
    }
}

static void write_shape_labels(std::ostream& svg,
                               const std::vector<Shape>& shapes,
                               const SvgView& view,
                               const std::vector<Layer>& layers) {
    for (const auto& shape : shapes) {
        const double cx = (minx(shape.bbox) + maxx(shape.bbox)) * 0.5;
        const double cy = (miny(shape.bbox) + maxy(shape.bbox)) * 0.5;
        const auto layer_it = std::find_if(layers.begin(), layers.end(),
            [&](const Layer& layer) { return layer.number == shape.layer; });
        const auto label_index = static_cast<std::size_t>(layer_it - layers.begin());
        svg << "<text class=\"metal-label\" x=\"" << sx(view, cx) << "\" y=\""
            << sy(view, cy) + 4 + 16 * (label_index % 4)
            << "\" text-anchor=\"middle\">" << xml_escape(layer_name(layers, shape.layer))
            << "/S" << shape.id << "</text>\n";
    }
}

static void write_layer_legend(std::ostream& svg, const std::vector<Layer>& layers) {
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const int x = 86 + static_cast<int>(i % 4) * 220;
        const int y = 780 + static_cast<int>(i / 4) * 28;
        svg << "<rect x=\"" << x << "\" y=\"" << y - 12
            << "\" width=\"20\" height=\"12\" fill=\"" << layer_color(layers, layers[i].number)
            << "\"/><text class=\"legend\" x=\"" << x + 30 << "\" y=\"" << y
            << "\">" << xml_escape(layers[i].name) << " (layer " << layers[i].number << ")</text>\n";
    }
}

static void write_clean_metal_svg(const std::string& filename,
                                const std::vector<Shape>& shapes,
                                const std::vector<Layer>& layers) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG: " + filename);
    const SvgView view = make_view(shapes, {});
    const std::string title = layers.front().name + " Clean Metal Layout";
    write_svg_header(svg, title, layers.size());
    svg << "<text class=\"title\" x=\"40\" y=\"42\">" << xml_escape(title) << "</text>\n";
    svg << "<text class=\"sub\" x=\"40\" y=\"66\">Metal geometry only, no DRC markers</text>\n";
    write_axes(svg, view);
    write_shapes(svg, shapes, view, layers);
    write_shape_labels(svg, shapes, view, layers);
    write_layer_legend(svg, layers);
    svg << "</svg>\n";
}

static void write_drc_svg(const std::string& filename,
                          const std::vector<Shape>& shapes,
                          const DrcResult& drc,
                          const std::vector<Layer>& layers) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG: " + filename);

    const SvgView view = make_view(shapes, drc.violations);
    const std::string title = "Combined EOL DRC Result";
    write_svg_header(svg, title, layers.size());
    svg << "<text class=\"title\" x=\"40\" y=\"42\">" << xml_escape(title) << "</text>\n";
    svg << "<text class=\"sub\" x=\"40\" y=\"66\">"
        << drc.violations.size()
        << " violation(s), eolWidth=0.6, eolSpace=1.0, eolWithin=0.4 um</text>\n";
    write_axes(svg, view);
    write_shapes(svg, shapes, view, layers);
    std::vector<int> drawn_checks;
    for (const auto& v : drc.violations) {
        if (std::find(drawn_checks.begin(), drawn_checks.end(), v.check_id) != drawn_checks.end()) {
            continue;
        }
        drawn_checks.push_back(v.check_id);
        const Polygon region = make_rectangle(minx(v.region), miny(v.region),
                                              maxx(v.region), maxy(v.region));
        svg << "<path class=\"check\" style=\"stroke:" << layer_color(layers, v.layer)
            << "\" d=\"" << polygon_path(region, view)
            << "\"><title>" << xml_escape(layer_name(layers, v.layer))
            << " / E" << v.check_id << " / S" << v.source_id
            << " spacing check region</title></path>\n";
    }
    // Paint only the intruding metal red, after all orange check regions.
    for (const auto& v : drc.violations) {
        const auto target = std::find_if(shapes.begin(), shapes.end(),
            [&](const Shape& shape) { return shape.id == v.target_id; });
        if (target == shapes.end()) continue;
        const Polygon region = make_rectangle(minx(v.region), miny(v.region),
                                              maxx(v.region), maxy(v.region));
        std::vector<Polygon> overlaps;
        bg::intersection(target->poly, region, overlaps);
        for (const auto& overlap : overlaps) {
            if (std::abs(bg::area(overlap)) <= EPS) continue;
            svg << "<path class=\"viol\" style=\"stroke:" << layer_color(layers, v.layer)
            << "\" fill-rule=\"evenodd\" d=\"" << polygon_path(overlap, view)
            << "\"><title>" << xml_escape(layer_name(layers, v.layer))
            << " / E" << v.check_id << " / S" << v.source_id
            << " vs S" << v.target_id << ": " << xml_escape(v.reason) << "</title></path>\n";
        }
    }
    for (const auto& edge : drc.edges) {
        svg << "<line class=\"eol\" x1=\"" << sx(view, bg::get<0>(edge.start))
            << "\" y1=\"" << sy(view, bg::get<1>(edge.start))
            << "\" x2=\"" << sx(view, bg::get<0>(edge.end))
            << "\" y2=\"" << sy(view, bg::get<1>(edge.end))
            << "\" style=\"stroke:" << layer_color(layers, edge.layer)
            << "\"><title>" << xml_escape(layer_name(layers, edge.layer))
            << " / S" << edge.source_id << " / E" << edge.id << " EOL edge</title></line>\n";
    }

    write_shape_labels(svg, shapes, view, layers);

    svg << "<rect x=\"86\" y=\"686\" width=\"20\" height=\"12\" class=\"metal\"/>\n"
        << "<text class=\"legend\" x=\"116\" y=\"697\">Metal layers</text>\n"
        << "<rect x=\"260\" y=\"686\" width=\"20\" height=\"12\" class=\"check\"/>\n"
        << "<text class=\"legend\" x=\"290\" y=\"697\">Violated EOL check region</text>\n"
        << "<rect x=\"580\" y=\"686\" width=\"20\" height=\"12\" class=\"viol\"/>\n"
        << "<text class=\"legend\" x=\"610\" y=\"697\">Intruding metal</text>\n"
        << "<line x1=\"86\" y1=\"724\" x2=\"106\" y2=\"724\" class=\"eol\" style=\"stroke:#374151\"/>\n"
        << "<text class=\"legend\" x=\"116\" y=\"729\">EOL edges and marker outlines use layer colors</text>\n";
    write_layer_legend(svg, layers);
    svg << "</svg>\n";
}

static void refresh_generated_svgs() {
    namespace fs = std::filesystem;
    // Only remove this program's outputs in the current directory. Never
    // traverse subdirectories or remove unrelated SVG files.
    const std::regex generated(R"(^(metal_clean|eol_drc)(_M[0-9]+(_L-?[0-9]+)?)?\.svg$)");
    std::vector<fs::path> old_outputs;
    for (const auto& entry : fs::directory_iterator(fs::current_path())) {
        if (entry.is_regular_file() &&
            std::regex_match(entry.path().filename().string(), generated)) {
            old_outputs.push_back(entry.path());
        }
    }
    for (const auto& path : old_outputs) fs::remove(path);
}

static void write_layer_svgs(const std::vector<Shape>& shapes,
                            const DrcResult& drc,
                            const std::vector<Layer>& layers) {
    refresh_generated_svgs();
    for (const auto& layer : layers) {
        std::vector<Shape> layer_shapes;
        for (const auto& shape : shapes)
            if (shape.layer == layer.number) layer_shapes.push_back(shape);
        std::string filename = "metal_clean_" + layer.name;
        // Disambiguate a malformed/aliased input that reuses a layer name.
        if (std::count_if(layers.begin(), layers.end(),
            [&](const Layer& item) { return item.name == layer.name; }) > 1) {
            filename += "_L" + std::to_string(layer.number);
        }
        write_clean_metal_svg(filename + ".svg", layer_shapes, {layer});
    }
    write_drc_svg("eol_drc.svg", shapes, drc, layers);
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
        const auto layers = metal_layers(layout);
        std::vector<Shape> metal_shapes;
        for (const auto& shape : layout.shapes) {
            if (std::any_of(layers.begin(), layers.end(),
                [&](const Layer& layer) { return layer.number == shape.layer; })) {
                // A fully covered metal contributes no separate geometry.
                // Keep the outer shape regardless of input order; equal
                // shapes retain the first ID. Partial overlaps stay separate.
                const auto covers = [](const Shape& outer, const Shape& inner) {
                    return outer.layer == inner.layer &&
                           bg::covered_by(inner.bbox, outer.bbox) &&
                           bg::covered_by(inner.poly, outer.poly);
                };
                const bool covered = std::any_of(metal_shapes.begin(), metal_shapes.end(),
                    [&](const Shape& existing) {
                        return covers(existing, shape);
                    });
                if (covered) continue;
                metal_shapes.erase(std::remove_if(metal_shapes.begin(), metal_shapes.end(),
                    [&](const Shape& existing) { return covers(shape, existing); }), metal_shapes.end());
                metal_shapes.push_back(shape);
            }
        }

        const DrcResult drc = check_eol(metal_shapes);
        write_layer_svgs(metal_shapes, drc, layers);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "EOL violations: " << drc.violations.size() << "\n";
        for (const auto& layer : layers) {
            const auto count = std::count_if(drc.violations.begin(), drc.violations.end(),
                [&](const Violation& v) { return v.layer == layer.number; });
            std::cout << layer.name << " (layer " << layer.number << "): " << count << " violation(s)\n";
        }
        for (std::size_t i = 0; i < drc.violations.size(); ++i) {
            const auto& v = drc.violations[i];
            std::cout << "V" << (i + 1)
                      << " [" << layer_name(layers, v.layer) << "] (E" << v.check_id << "): shape " << v.source_id
                      << " " << v.end_name
                      << " end vs shape " << v.target_id
                      << " -- " << v.reason
                      << " at ";
            print_box(v.region);
            std::cout << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
