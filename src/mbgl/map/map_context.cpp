#include <mbgl/map/map_context.hpp>
#include <mbgl/map/environment.hpp>

#include <mbgl/renderer/painter.hpp>

#include <mbgl/text/glyph_store.hpp>

#include <mbgl/geometry/glyph_atlas.hpp>
#include <mbgl/geometry/sprite_atlas.hpp>
#include <mbgl/geometry/line_atlas.hpp>

#include <mbgl/style/style_layer.hpp>
#include <mbgl/style/style_layer_group.hpp>
#include <mbgl/style/style_bucket.hpp>
#include <mbgl/style/style_source.hpp>

#include <mbgl/util/std.hpp>
#include <mbgl/util/uv_detail.hpp>
#include <mbgl/util/texture_pool.hpp>

namespace mbgl {

MapContext::MapContext(Environment& env)
    : glyphStore(util::make_unique<GlyphStore>(env)),
      glyphAtlas(util::make_unique<GlyphAtlas>(1024, 1024)),
      spriteAtlas(util::make_unique<SpriteAtlas>(512, 512)),
      lineAtlas(util::make_unique<LineAtlas>(512, 512)),
      texturePool(util::make_unique<TexturePool>()),
      painter(util::make_unique<Painter>(*spriteAtlas, *glyphAtlas, *lineAtlas)) {
}

uv::worker& MapContext::getWorker() {
    assert(workers);
    return *workers;
}

void MapContext::updateSources(const util::ptr<StyleLayerGroup>& group) {
    assert(Environment::currentlyOn(ThreadType::Map));
    if (!group) {
        return;
    }
    for (const util::ptr<StyleLayer>& layer : group->layers) {
        if (!layer) {
            continue;
        }
        if (layer->bucket && layer->bucket->style_source) {
            (*activeSources.emplace(layer->bucket->style_source).first)->enabled = true;
        }
    }
}

}