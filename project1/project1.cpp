
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/io/svg/svg_mapper.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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

struct LayerResult {
    Layer layer;
    Box bbox;
    std::vector<Polygon> blocks;
    MultiPolygon block_overlaps;
    std::vector<Pin> pins;
    MultiPolygon blockage;
    std::vector<Rect> rectangles;
};

struct PinStyle {
    std::string fill;
    std::string stroke;
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

static bool layer_bbox(const Layout& layout, int layer, Box& bbox) {
    bool initialized = false;

    for (const auto& s : layout.polygons)
        if (s.layer == layer)
            expand_box(bbox, polygon_box(s.poly), initialized);
    for (const auto& s : layout.rectangles)
        if (s.layer == layer)
            expand_box(bbox, polygon_box(s.poly), initialized);

    return initialized;
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

static double clamp_coord(double value, double low, double high) {
    return std::max(low, std::min(value, high));
}

static void collect_expanded_pin_cutlines(const Box& bbox,
                                          const std::vector<Pin>& pins,
                                          double spacing,
                                          std::vector<double>& xs,
                                          std::vector<double>& ys) {
    double min_x = bg::get<bg::min_corner, 0>(bbox);
    double min_y = bg::get<bg::min_corner, 1>(bbox);
    double max_x = bg::get<bg::max_corner, 0>(bbox);
    double max_y = bg::get<bg::max_corner, 1>(bbox);

    xs.push_back(min_x);
    xs.push_back(max_x);
    ys.push_back(min_y);
    ys.push_back(max_y);

    for (const auto& pin : pins) {
        MultiPolygon expanded = buffer_polygon(pin.shape, spacing);
        for (const auto& poly : expanded) {
            Box expanded_box = polygon_box(poly);
            xs.push_back(clamp_coord(
                bg::get<bg::min_corner, 0>(expanded_box), min_x, max_x));
            xs.push_back(clamp_coord(
                bg::get<bg::max_corner, 0>(expanded_box), min_x, max_x));
            ys.push_back(clamp_coord(
                bg::get<bg::min_corner, 1>(expanded_box), min_y, max_y));
            ys.push_back(clamp_coord(
                bg::get<bg::max_corner, 1>(expanded_box), min_y, max_y));
        }
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

static std::vector<Rect> decompose_using_cutlines(
    const MultiPolygon& blockage,
    std::vector<double> xs,
    std::vector<double> ys) {
    unique_sort(xs);
    unique_sort(ys);

    if (xs.size() < 2 || ys.size() < 2)
        return {};

    int nx = static_cast<int>(xs.size()) - 1;
    int ny = static_cast<int>(ys.size()) - 1;

    std::vector<std::vector<bool>> occupied(
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
        int i = 0;
        while (i < nx) {
            if (!occupied[j][i]) {
                ++i;
                continue;
            }

            int i2 = i;
            while (i2 + 1 < nx && occupied[j][i2 + 1]) {
                ++i2;
            }

            rects.push_back({xs[i], ys[j], xs[i2 + 1], ys[j + 1]});
            i = i2 + 1;
        }
    }

    return rects;
}

static std::vector<Rect> decompose_to_rectangles(const MultiPolygon& blockage,
                                                 const Box& bbox,
                                                 const std::vector<Pin>& pins,
                                                 double spacing) {
    std::vector<double> xs, ys;
    collect_expanded_pin_cutlines(bbox, pins, spacing, xs, ys);
    return decompose_using_cutlines(blockage, std::move(xs), std::move(ys));
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

static std::string layer_filename(const Layer& layer) {
    std::string name;
    for (unsigned char ch : layer.name) {
        if (std::isalnum(ch) || ch == '-' || ch == '_') {
            name += static_cast<char>(ch);
        } else {
            name += '_';
        }
    }
    if (name.empty()) name = "layer_" + std::to_string(layer.number);
    return name + ".svg";
}

static void remove_existing_svg_files() {
    for (const auto& entry : std::filesystem::directory_iterator(
             std::filesystem::current_path())) {
        if (!entry.is_regular_file()) continue;

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });
        if (extension == ".svg") std::filesystem::remove(entry.path());
    }
}

static PinStyle pin_style(std::size_t layer_index) {
    static const std::vector<PinStyle> styles = {
        {"#4779bf", "#315e9d"},
        {"#dc741e", "#93450d"},
        {"#319269", "#1f6548"},
        {"#7954b3", "#50337e"},
        {"#c95050", "#873232"},
        {"#24939d", "#17636a"},
        {"#b89a22", "#735f14"},
        {"#b74e8d", "#7b315e"}
    };
    return styles[layer_index % styles.size()];
}

static std::vector<Polygon> rectangle_polygons(const std::vector<Rect>& rects) {
    std::vector<Polygon> polygons;
    polygons.reserve(rects.size());
    for (const auto& rect : rects) {
        polygons.push_back(
            make_rectangle_polygon(rect.xl, rect.yl, rect.xh, rect.yh));
    }
    return polygons;
}

static Polygon box_polygon(const Box& bbox) {
    return make_rectangle_polygon(
        bg::get<bg::min_corner, 0>(bbox),
        bg::get<bg::min_corner, 1>(bbox),
        bg::get<bg::max_corner, 0>(bbox),
        bg::get<bg::max_corner, 1>(bbox));
}

static MultiPolygon find_block_overlaps(const std::vector<Polygon>& blocks) {
    MultiPolygon overlaps;

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        for (std::size_t j = i + 1; j < blocks.size(); ++j) {
            MultiPolygon intersection;
            bg::intersection(blocks[i], blocks[j], intersection);

            for (auto& polygon : intersection) {
                bg::correct(polygon);
                if (std::abs(bg::area(polygon)) > EPS) {
                    overlaps.push_back(std::move(polygon));
                }
            }
        }
    }

    return overlaps;
}

static void write_layer_svg(const std::string& filename,
                            const LayerResult& result) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG file: " + filename);

    bg::svg_mapper<Point> mapper(svg, 800, 800);
    svg << "<rect x=\"0\" y=\"0\" width=\"800\" height=\"800\" fill=\"white\"/>\n";
    Box display_bbox = result.bbox;

    for (const auto& polygon : result.blockage) mapper.add(polygon);
    for (const auto& block : result.blocks) mapper.add(block);
    for (const auto& overlap : result.block_overlaps) mapper.add(overlap);
    for (const auto& pin : result.pins) mapper.add(pin.shape);

    Polygon border = box_polygon(result.bbox);
    mapper.add(border);
    display_bbox = padded_display_box(display_bbox);
    mapper.add(box_polygon(display_bbox));

    const std::string blockage_style =
        "fill:#d0cece;fill-opacity:0.68;stroke:#505050;stroke-width:1";
    const std::string block_style =
        "fill:#9cc5df;fill-opacity:0.72;stroke:#2e6f95;stroke-width:1.2";
    const std::string overlap_style =
        "fill:#6399bd;fill-opacity:0.82;stroke:none";
    const std::string pin_style =
        "fill:#4779bf;fill-opacity:0.78;stroke:#315e9d;stroke-width:1";

    for (const auto& polygon : result.blockage) {
        mapper.map(polygon, blockage_style);
    }
    for (const auto& block : result.blocks) {
        mapper.map(block, block_style);
    }
    for (const auto& overlap : result.block_overlaps) {
        mapper.map(overlap, overlap_style);
    }
    for (const auto& pin : result.pins) {
        mapper.map(pin.shape, pin_style);
    }

    mapper.map(border, "fill:none;stroke:#404040;stroke-width:1.5");
    svg << "<g font-family=\"sans-serif\" font-size=\"15\">\n"
        << "<rect x=\"10\" y=\"10\" width=\"190\" height=\"126\""
        << " rx=\"5\" fill=\"white\" fill-opacity=\"0.88\""
        << " stroke=\"#555\"/>\n"
        << "<text x=\"20\" y=\"30\" fill=\"#202124\">Metal layer: "
        << escape_xml(result.layer.name) << "</text>\n"
        << "<rect x=\"20\" y=\"43\" width=\"16\" height=\"12\""
        << " fill=\"#d0cece\" stroke=\"#505050\"/>\n"
        << "<text x=\"44\" y=\"54\" fill=\"#202124\">Blockage</text>\n"
        << "<rect x=\"20\" y=\"67\" width=\"16\" height=\"12\""
        << " fill=\"#9cc5df\" stroke=\"#2e6f95\"/>\n"
        << "<text x=\"44\" y=\"78\" fill=\"#202124\">Existing block</text>\n"
        << "<rect x=\"20\" y=\"91\" width=\"16\" height=\"12\""
        << " fill=\"#6399bd\" stroke=\"#2e6f95\"/>\n"
        << "<text x=\"44\" y=\"102\" fill=\"#202124\">Block overlap</text>\n"
        << "<rect x=\"20\" y=\"115\" width=\"16\" height=\"12\""
        << " fill=\"#4779bf\" stroke=\"#315e9d\"/>\n"
        << "<text x=\"44\" y=\"126\" fill=\"#202124\">Pin</text>\n"
        << "</g>\n";
}

static MultiPolygon union_pin_keepouts(const std::vector<Pin>& pins,
                                       double spacing) {
    MultiPolygon merged;

    for (const auto& pin : pins) {
        MultiPolygon expanded = buffer_polygon(pin.shape, spacing);
        for (const auto& polygon : expanded) {
            if (merged.empty()) {
                merged.push_back(polygon);
                continue;
            }

            MultiPolygon addition;
            addition.push_back(polygon);
            MultiPolygon next;
            bg::union_(merged, addition, next);
            for (auto& part : next) bg::correct(part);
            merged = std::move(next);
        }
    }

    return merged;
}

static MultiPolygon carve_keepout_union(const Box& bbox,
                                        const MultiPolygon& keepouts) {
    MultiPolygon current;
    current.push_back(box_polygon(bbox));

    for (const auto& cut : keepouts) {
        MultiPolygon next;
        for (const auto& region : current) {
            MultiPolygon difference;
            bg::difference(region, cut, difference);
            for (auto& polygon : difference) {
                bg::correct(polygon);
                if (std::abs(bg::area(polygon)) > EPS) {
                    next.push_back(std::move(polygon));
                }
            }
        }
        current = std::move(next);
    }

    return current;
}

static void collect_keepout_union_cutlines(const Box& bbox,
                                           const MultiPolygon& keepouts,
                                           std::vector<double>& xs,
                                           std::vector<double>& ys) {
    const double min_x = bg::get<bg::min_corner, 0>(bbox);
    const double min_y = bg::get<bg::min_corner, 1>(bbox);
    const double max_x = bg::get<bg::max_corner, 0>(bbox);
    const double max_y = bg::get<bg::max_corner, 1>(bbox);

    xs.push_back(min_x);
    xs.push_back(max_x);
    ys.push_back(min_y);
    ys.push_back(max_y);

    auto collect_ring = [&](const Polygon::ring_type& ring) {
        std::size_t count = ring.size();
        if (count > 1) {
            const Point& first = ring.front();
            const Point& last = ring.back();
            if (std::abs(bg::get<0>(first) - bg::get<0>(last)) < EPS &&
                std::abs(bg::get<1>(first) - bg::get<1>(last)) < EPS) {
                --count;
            }
        }
        if (count < 3) return;

        for (std::size_t i = 0; i < count; ++i) {
            const Point& previous = ring[(i + count - 1) % count];
            const Point& current = ring[i];
            const Point& next = ring[(i + 1) % count];

            const double dx1 = bg::get<0>(current) - bg::get<0>(previous);
            const double dy1 = bg::get<1>(current) - bg::get<1>(previous);
            const double dx2 = bg::get<0>(next) - bg::get<0>(current);
            const double dy2 = bg::get<1>(next) - bg::get<1>(current);
            const double cross = dx1 * dy2 - dy1 * dx2;

            // Boost buffer/union may retain original pin vertices as
            // collinear points on an expanded edge. They are not geometric
            // corners and must not create extra BPV cutlines.
            if (std::abs(cross) <= EPS) continue;

            xs.push_back(clamp_coord(
                bg::get<0>(current), min_x, max_x));
            ys.push_back(clamp_coord(
                bg::get<1>(current), min_y, max_y));
        }
    };

    for (const auto& polygon : keepouts) {
        collect_ring(polygon.outer());
        for (const auto& inner : polygon.inners()) collect_ring(inner);
    }
}

static void write_bpv_svg(const std::string& filename,
                          const LayerResult& result,
                          std::size_t layer_index) {
    std::ofstream svg(filename);
    if (!svg) throw std::runtime_error("Cannot write SVG file: " + filename);

    {
            bg::svg_mapper<Point> mapper(svg, 800, 800);
            svg << "<rect width=\"800\" height=\"800\" fill=\"white\"/>\n";
            mapper.add(box_polygon(padded_display_box(result.bbox)));
            std::vector<Polygon> polygons = rectangle_polygons(result.rectangles);
            for (const auto& polygon : polygons) mapper.add(polygon);
            for (const auto& pin : result.pins) mapper.add(pin.shape);
            Polygon border = box_polygon(result.bbox);
            mapper.add(border);

            for (const auto& polygon : polygons) {
                mapper.map(polygon,
                    "fill:#d0cece;fill-opacity:0.68;stroke:#505050;stroke-width:1.2");
            }
            PinStyle style = pin_style(layer_index);
            const std::string svg_style =
                "fill:" + style.fill + ";fill-opacity:0.72;stroke:" +
                style.stroke + ";stroke-width:1.2";
            for (const auto& pin : result.pins) {
                mapper.map(pin.shape, svg_style);
            }
            mapper.map(border, "fill:none;stroke:#315e9d;stroke-width:1.5");
            svg << "<g font-family=\"sans-serif\" font-size=\"15\">\n"
                  << "<rect x=\"10\" y=\"10\" width=\"190\" height=\"76\" fill=\"white\" fill-opacity=\"0.9\" stroke=\"#555\"/>\n"
                  << "<text x=\"20\" y=\"30\">Metal layer: " << escape_xml(result.layer.name) << "</text>\n"
                  << "<rect x=\"20\" y=\"42\" width=\"16\" height=\"12\" fill=\"#d0cece\" stroke=\"#505050\"/>\n"
                  << "<text x=\"44\" y=\"53\">Blockage</text>\n"
                  << "<rect x=\"20\" y=\"65\" width=\"16\" height=\"12\" fill=\"" << style.fill << "\" stroke=\"" << style.stroke << "\"/>\n"
                  << "<text x=\"44\" y=\"76\">Pin</text>\n</g>\n";
    }
}

static void set_svg_viewbox(const std::string& filename,
                            int width,
                            int height) {
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Cannot open SVG file: " + filename);

    std::ostringstream contents;
    contents << in.rdbuf();
    std::string xml = contents.str();

    std::size_t start = xml.find("<svg");
    std::size_t end = xml.find('>', start);
    if (start == std::string::npos || end == std::string::npos)
        throw std::runtime_error("Invalid SVG file: " + filename);

    std::string tag = xml.substr(start, end - start + 1);
    std::size_t version = tag.find("version=");
    if (version == std::string::npos)
        throw std::runtime_error("Invalid SVG tag in file: " + filename);

    std::ostringstream replacement;
    replacement << "<svg width=\"" << width << "\" height=\"" << height
                << "\" viewBox=\"0 0 " << width << " " << height
                << "\" preserveAspectRatio=\"xMidYMid meet\" ";

    xml.replace(start, end - start + 1,
                replacement.str() + tag.substr(version));

    std::ofstream out(filename);
    if (!out) throw std::runtime_error("Cannot write SVG file: " + filename);
    out << xml;
}

static void print_results(const std::vector<LayerResult>& results) {
    std::cout << std::fixed << std::setprecision(3);

    for (const auto& result : results) {
        std::cout << "=== Layer " << result.layer.name
                  << " (" << result.layer.number << ") ===\n"
                  << "Bounding box: ("
                  << bg::get<bg::min_corner, 0>(result.bbox) << ", "
                  << bg::get<bg::min_corner, 1>(result.bbox) << ") -> ("
                  << bg::get<bg::max_corner, 0>(result.bbox) << ", "
                  << bg::get<bg::max_corner, 1>(result.bbox) << ")\n";

        std::cout << "Pins:\n";
        if (result.pins.empty()) std::cout << "  (none)\n";
        for (const auto& pin : result.pins) {
            Box pin_box = polygon_box(pin.shape);
            std::cout << "  " << pin.name << "  bbox=("
                      << bg::get<bg::min_corner, 0>(pin_box) << ", "
                      << bg::get<bg::min_corner, 1>(pin_box) << ") -> ("
                      << bg::get<bg::max_corner, 0>(pin_box) << ", "
                      << bg::get<bg::max_corner, 1>(pin_box) << ")\n";
        }

        std::cout << "Blockage rectangles:\n";
        for (std::size_t i = 0; i < result.rectangles.size(); ++i) {
            const auto& rect = result.rectangles[i];
            std::cout << "  R" << (i + 1) << ": "
                      << rect.xl << " " << rect.yl << ", "
                      << rect.xh << " " << rect.yh << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "Generated:\n";
    for (const auto& result : results) {
        std::cout << "  " << layer_filename(result.layer) << "\n";
        std::cout << "  BPV_" << layer_filename(result.layer) << "\n";
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input.txt>\n";
            return 1;
        }

        Layout layout = parse_layout(argv[1]);
        std::vector<Pin> all_pins = identify_pins(layout);
        std::vector<LayerResult> results;

        for (const auto& layer : layout.layers) {
            Box bbox;
            if (!layer_bbox(layout, layer.number, bbox)) {
                std::cerr << "Warning: layer " << layer.name
                          << " has no polygons or rectangles; skipped.\n";
                continue;
            }

            std::vector<Pin> pins;
            for (const auto& pin : all_pins) {
                if (pin.layer == layer.number) pins.push_back(pin);
            }

            std::vector<Polygon> blocks;
            for (const auto& shape : layout.polygons) {
                if (shape.layer == layer.number) blocks.push_back(shape.poly);
            }
            for (const auto& shape : layout.rectangles) {
                if (shape.layer == layer.number) blocks.push_back(shape.poly);
            }
            MultiPolygon block_overlaps = find_block_overlaps(blocks);

            MultiPolygon blockage = carve_blockage(bbox, pins, MIN_SPACING);
            std::vector<Rect> rectangles = decompose_to_rectangles(
                blockage, bbox, pins, MIN_SPACING);

            results.push_back({layer, bbox, std::move(blocks),
                               std::move(block_overlaps), std::move(pins),
                               std::move(blockage),
                               std::move(rectangles)});
        }

        if (results.empty()) {
            throw std::runtime_error(
                "No declared layer contains polygons or rectangles.");
        }

        remove_existing_svg_files();

        for (std::size_t i = 0; i < results.size(); ++i) {
            std::string filename = layer_filename(results[i].layer);
            write_layer_svg(filename, results[i]);
            set_svg_viewbox(filename, 800, 800);
            std::string bpv_filename = "BPV_" + filename;
            write_bpv_svg(bpv_filename, results[i], i);
            set_svg_viewbox(bpv_filename, 800, 800);
        }

        print_results(results);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
