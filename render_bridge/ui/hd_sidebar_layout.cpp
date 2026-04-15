#include "hd_sidebar_layout.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

// Embedded JSON string (see cmake/embed_text.cmake +
// Engine/EA/cnc/render_bridge/assets/sidebar-layout.json).
extern const std::string_view kSidebarLayoutJson;

namespace render_bridge {

namespace {

// Tiny JSON parser — scoped to the layout schema. Matches the style used by
// tools/hd_sidebar_preview/fixture.cpp; good enough for a hand-authored config.
struct Parser {
    const char* p;
    const char* end;
    bool ok = true;

    void fail() { ok = false; }

    void skip_ws() {
        while (ok && p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool accept(char c) {
        skip_ws();
        if (p < end && *p == c) { ++p; return true; }
        return false;
    }

    void expect(char c) { if (!accept(c)) fail(); }

    bool parse_string(std::string& out) {
        skip_ws();
        if (!accept('"')) return false;
        out.clear();
        while (ok && p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                char esc = p[1];
                p += 2;
                switch (esc) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    default:   out.push_back(esc);  break;
                }
            } else {
                out.push_back(*p++);
            }
        }
        if (p >= end) { fail(); return false; }
        ++p;  // consume closing quote
        return true;
    }

    bool parse_number(double& out) {
        skip_ws();
        const char* start = p;
        auto digits = [&]() {
            const char* d0 = p;
            while (p < end && *p >= '0' && *p <= '9') ++p;
            return p > d0;
        };
        if (p < end && (*p == '-' || *p == '+')) ++p;
        if (!digits()) return false;
        if (p < end && *p == '.') { ++p; if (!digits()) return false; }
        if (p < end && (*p == 'e' || *p == 'E')) {
            ++p;
            if (p < end && (*p == '-' || *p == '+')) ++p;
            if (!digits()) return false;
        }
        char* tail = nullptr;
        out = std::strtod(start, &tail);
        return tail == p;
    }

    void skip_value() {
        skip_ws();
        if (p >= end) { fail(); return; }
        char c = *p;
        if (c == '"') { std::string s; parse_string(s); return; }
        if (c == '{') { skip_object(); return; }
        if (c == '[') { skip_array();  return; }
        if ((c >= '0' && c <= '9') || c == '-' || c == '+') { double d; parse_number(d); return; }
        // true / false / null — just scan an identifier.
        while (p < end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))) ++p;
    }

    void skip_object() {
        if (!accept('{')) { fail(); return; }
        skip_ws();
        if (accept('}')) return;
        while (ok) {
            std::string k; if (!parse_string(k)) { fail(); return; }
            expect(':');
            skip_value();
            skip_ws();
            if (accept(',')) continue;
            expect('}');
            return;
        }
    }

    void skip_array() {
        if (!accept('[')) { fail(); return; }
        skip_ws();
        if (accept(']')) return;
        while (ok) {
            skip_value();
            skip_ws();
            if (accept(',')) continue;
            expect(']');
            return;
        }
    }
};

Anchor anchor_from_string(const std::string& s) {
    if (s == "top-left")     return Anchor::TopLeft;
    if (s == "top-right")    return Anchor::TopRight;
    if (s == "bottom-left")  return Anchor::BottomLeft;
    if (s == "bottom-right") return Anchor::BottomRight;
    if (s == "center")       return Anchor::Center;
    std::fprintf(stderr, "[hd_sidebar_layout] unknown anchor '%s', defaulting to top-left\n", s.c_str());
    return Anchor::TopLeft;
}

int round_i(double d) { return static_cast<int>(std::lround(d)); }

}  // namespace

HDSidebarLayout& HDSidebarLayout::Instance() {
    static HDSidebarLayout inst;
    return inst;
}

HDSidebarLayout::HDSidebarLayout() {
    Parser ps{ kSidebarLayoutJson.data(),
               kSidebarLayoutJson.data() + kSidebarLayoutJson.size() };

    // Tracks the current parse context so a failure reports which key was
    // being consumed (instead of the silent "empty layout" the old code gave).
    std::string ctx;
    auto bail = [&](const char* what) {
        std::fprintf(stderr,
                     "[hd_sidebar_layout] parse error at '%s' (%s); using empty layout\n",
                     ctx.c_str(), what);
        ps.fail();
    };

    ps.skip_ws();
    if (!ps.accept('{')) { bail("expected '{' at root"); }

    while (ps.ok) {
        ps.skip_ws();
        if (ps.accept('}')) break;

        std::string key;
        if (!ps.parse_string(key)) { bail("expected top-level key"); break; }
        ctx = key;
        ps.expect(':');
        ps.skip_ws();

        if (key == "reference") {
            if (!ps.accept('{')) { bail("reference: expected '{'"); break; }
            while (ps.ok) {
                ps.skip_ws();
                if (ps.accept('}')) break;
                std::string rk;
                if (!ps.parse_string(rk)) { bail("reference: expected key"); break; }
                ctx = "reference." + rk;
                ps.expect(':');
                double v = 0;
                if (!ps.parse_number(v)) { bail("expected number"); break; }
                if (rk == "w") ref_w_ = round_i(v);
                else if (rk == "h") ref_h_ = round_i(v);
                ps.skip_ws();
                if (ps.accept(',')) continue;
                ps.expect('}');
                break;
            }
        } else if (key == "components") {
            if (!ps.accept('{')) { bail("components: expected '{'"); break; }
            while (ps.ok) {
                ps.skip_ws();
                if (ps.accept('}')) break;
                std::string name;
                if (!ps.parse_string(name)) { bail("components: expected name"); break; }
                ctx = "components." + name;
                ps.expect(':');
                if (!ps.accept('{')) { bail("expected '{' for component"); break; }
                Component c{};
                while (ps.ok) {
                    ps.skip_ws();
                    if (ps.accept('}')) break;
                    std::string fk;
                    if (!ps.parse_string(fk)) { bail("expected field key"); break; }
                    const std::string field_ctx = "components." + name + "." + fk;
                    ctx = field_ctx;
                    ps.expect(':');
                    if (fk == "anchor") {
                        std::string av;
                        if (!ps.parse_string(av)) { bail("anchor: expected string"); break; }
                        c.anchor = anchor_from_string(av);
                    } else {
                        double v = 0;
                        if (!ps.parse_number(v)) { bail("expected number"); break; }
                        if      (fk == "x") c.x = round_i(v);
                        else if (fk == "y") c.y = round_i(v);
                        else if (fk == "w") c.w = round_i(v);
                        else if (fk == "h") c.h = round_i(v);
                    }
                    ps.skip_ws();
                    if (ps.accept(',')) continue;
                    ps.expect('}');
                    break;
                }
                components_.emplace(std::move(name), c);
                ps.skip_ws();
                if (ps.accept(',')) continue;
                ps.expect('}');
                break;
            }
        } else if (key == "scaling") {
            // Only "uniform" is supported. Validate so a future "proportional"
            // entry surfaces instead of silently falling through to uniform.
            std::string mode;
            if (!ps.parse_string(mode)) { bail("scaling: expected string"); break; }
            if (mode != "uniform") {
                std::fprintf(stderr,
                             "[hd_sidebar_layout] unsupported scaling '%s'; only 'uniform' is implemented\n",
                             mode.c_str());
            }
        } else {
            // Unknown top-level key: tolerate forward-compat additions.
            ps.skip_value();
        }

        ps.skip_ws();
        if (ps.accept(',')) continue;
        ps.expect('}');
        break;
    }

    if (!ps.ok) {
        components_.clear();
    }
}

bool HDSidebarLayout::Has(std::string_view id) const {
    return components_.find(id) != components_.end();
}

LayoutRect HDSidebarLayout::Resolve(std::string_view id, int screen_w, int screen_h) const {
    auto it = components_.find(id);
    if (it == components_.end() || ref_w_ <= 0 || ref_h_ <= 0 ||
        screen_w <= 0 || screen_h <= 0) {
        return {};
    }
    const Component& c = it->second;

    const double sx = static_cast<double>(screen_w) / static_cast<double>(ref_w_);
    const double sy = static_cast<double>(screen_h) / static_cast<double>(ref_h_);
    const double s  = (sx < sy) ? sx : sy;

    LayoutRect r{};
    r.w = round_i(c.w * s);
    r.h = round_i(c.h * s);

    const double left_margin   = c.x;
    const double top_margin    = c.y;
    const double right_margin  = static_cast<double>(ref_w_) - (c.x + c.w);
    const double bottom_margin = static_cast<double>(ref_h_) - (c.y + c.h);

    switch (c.anchor) {
        case Anchor::TopLeft:
            r.x = round_i(left_margin * s);
            r.y = round_i(top_margin  * s);
            break;
        case Anchor::TopRight:
            r.x = screen_w - round_i(right_margin * s) - r.w;
            r.y = round_i(top_margin * s);
            break;
        case Anchor::BottomLeft:
            r.x = round_i(left_margin * s);
            r.y = screen_h - round_i(bottom_margin * s) - r.h;
            break;
        case Anchor::BottomRight:
            r.x = screen_w - round_i(right_margin  * s) - r.w;
            r.y = screen_h - round_i(bottom_margin * s) - r.h;
            break;
        case Anchor::Center: {
            const double cx = c.x + c.w * 0.5;
            const double cy = c.y + c.h * 0.5;
            const double screen_cx = (cx - ref_w_ * 0.5) * s + screen_w * 0.5;
            const double screen_cy = (cy - ref_h_ * 0.5) * s + screen_h * 0.5;
            r.x = round_i(screen_cx - r.w * 0.5);
            r.y = round_i(screen_cy - r.h * 0.5);
            break;
        }
    }
    return r;
}

}  // namespace render_bridge
