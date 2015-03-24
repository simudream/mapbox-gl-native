#ifndef MBGL_MAP_MAP_CONTEXT
#define MBGL_MAP_MAP_CONTEXT

#include <mbgl/util/ptr.hpp>

#include <set>

namespace uv {
class worker;
class async;
}

namespace mbgl {

class Environment;
class MapData;
class GlyphStore;
class GlyphAtlas;
class SpriteAtlas;
class LineAtlas;
class TexturePool;
class Painter;
class Sprite;
class Style;
class StyleSource;
class StyleLayerGroup;

class MapContext {
public:
    MapContext(Environment&, MapData&);

    // Triggers a render. Can be called from any thread.
    void triggerRender();

    // These can only be called from the Map thread.
    uv::worker& getWorker();
    util::ptr<Sprite> getSprite();
    void updateSources(const util::ptr<StyleLayerGroup>&);

public:
    Environment& env;
    MapData& data;

    std::unique_ptr<uv::async> asyncRender;

    std::unique_ptr<uv::worker> workers;
    const std::unique_ptr<GlyphStore> glyphStore;
    const std::unique_ptr<GlyphAtlas> glyphAtlas;
    const std::unique_ptr<SpriteAtlas> spriteAtlas;
    const std::unique_ptr<LineAtlas> lineAtlas;
    const std::unique_ptr<TexturePool> texturePool;
    const std::unique_ptr<Painter> painter;
    util::ptr<Sprite> sprite;
    util::ptr<Style> style;
    std::set<util::ptr<StyleSource>> activeSources;
};

}
#endif