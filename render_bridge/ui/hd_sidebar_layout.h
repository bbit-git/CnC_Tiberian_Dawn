#pragma once

// HD sidebar layout resolver.
//
// Resolves named component rects (top_bar, sidebar, …) from an embedded
// sidebar-layout.json, scaling uniformly from the authored reference
// resolution (1920x1080) and re-anchoring to the target screen size.
//
// The JSON is embedded at build time as `kSidebarLayoutJson`; there is no
// runtime file I/O. See `Engine/EA/cnc/render_bridge/assets/sidebar-layout.json`.

#include <string>
#include <string_view>
#include <unordered_map>

namespace render_bridge {

enum class Anchor {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
};

struct LayoutRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool valid() const { return w > 0 && h > 0; }
};

class HDSidebarLayout {
public:
    static HDSidebarLayout& Instance();

    // Resolve `id` against a `screen_w` x `screen_h` target. On miss, returns
    // an empty (invalid) rect. Cheap — caller may call per frame.
    LayoutRect Resolve(std::string_view id, int screen_w, int screen_h) const;

    bool Has(std::string_view id) const;

    // Reference-space dimensions as authored in the JSON.
    int ReferenceWidth()  const { return ref_w_; }
    int ReferenceHeight() const { return ref_h_; }

private:
    HDSidebarLayout();

    struct Component {
        Anchor anchor = Anchor::TopLeft;
        int x = 0, y = 0, w = 0, h = 0;
    };

    struct StringHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    int ref_w_ = 1920;
    int ref_h_ = 1080;
    std::unordered_map<std::string, Component, StringHash, std::equal_to<>> components_;
};

}  // namespace render_bridge
