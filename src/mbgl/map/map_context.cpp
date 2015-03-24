#include <mbgl/map/map_context.hpp>
#include <mbgl/map/map_data.hpp>
#include <mbgl/map/view.hpp>
#include <mbgl/map/environment.hpp>
#include <mbgl/map/source.hpp>
#include <mbgl/map/sprite.hpp>

#include <mbgl/platform/log.hpp>

#include <mbgl/renderer/painter.hpp>

#include <mbgl/text/glyph_store.hpp>

#include <mbgl/geometry/glyph_atlas.hpp>
#include <mbgl/geometry/sprite_atlas.hpp>
#include <mbgl/geometry/line_atlas.hpp>

#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/response.hpp>

#include <mbgl/style/style.hpp>
#include <mbgl/style/style_layer.hpp>
#include <mbgl/style/style_layer_group.hpp>
#include <mbgl/style/style_bucket.hpp>
#include <mbgl/style/style_source.hpp>

#include <mbgl/util/std.hpp>
#include <mbgl/util/uv_detail.hpp>
#include <mbgl/util/texture_pool.hpp>
#include <mbgl/util/mapbox.hpp>

namespace mbgl {

MapContext::MapContext(Environment& env_, View& view_, MapData& data_)
    : env(env_),
      view(view_),
      data(data_),
      glyphStore(util::make_unique<GlyphStore>(env)),
      glyphAtlas(util::make_unique<GlyphAtlas>(1024, 1024)),
      spriteAtlas(util::make_unique<SpriteAtlas>(512, 512)),
      lineAtlas(util::make_unique<LineAtlas>(512, 512)),
      texturePool(util::make_unique<TexturePool>()),
      painter(util::make_unique<Painter>(*spriteAtlas, *glyphAtlas, *lineAtlas)) {
}

void MapContext::start() {

    // Setup async notifications
    assert(!asyncTerminate);
    asyncTerminate = util::make_unique<uv::async>(env.loop, [this]() {
        assert(Environment::currentlyOn(ThreadType::Map));

        // Remove all of these to make sure they are destructed in the correct thread.
        style.reset();
        workers.reset();
        activeSources.clear();

        terminating = true;

        // Closes all open handles on the loop. This means that the loop will automatically terminate.
        asyncRender.reset();
        asyncUpdate.reset();
        asyncTerminate.reset();
    });

    assert(!asyncUpdate);
    asyncUpdate = util::make_unique<uv::async>(env.loop, [this] {
        update();
    });

    assert(!asyncRender);
    asyncRender = util::make_unique<uv::async>(env.loop, [this] {
        // Must be called in Map thread.
        assert(Environment::currentlyOn(ThreadType::Map));

        render();

        // Finally, notify all listeners that we have finished rendering this frame.
        rendered.notify();
    });
}


void MapContext::terminate() {
    assert(asyncTerminate);
    asyncTerminate->send();
}

void MapContext::triggerRender() {
    assert(asyncRender);
    asyncRender->send();
}

void MapContext::triggerUpdate(const Update u) {
    updated |= static_cast<UpdateType>(u);

    if (data.mode == MapMode::Static) {
        prepare();
    } else if (asyncUpdate) {
        asyncUpdate->send();
    }
}

uv::worker& MapContext::getWorker() {
    assert(workers);
    return *workers;
}

util::ptr<Sprite> MapContext::getSprite() {
    assert(Environment::currentlyOn(ThreadType::Map));
    const float pixelRatio = data.getTransformState().getPixelRatio();
    const std::string &sprite_url = style->getSpriteURL();
    if (!sprite || !sprite->hasPixelRatio(pixelRatio)) {
        sprite = Sprite::Create(sprite_url, pixelRatio, env);
    }

    return sprite;
}

void MapContext::reloadStyle() {
    assert(Environment::currentlyOn(ThreadType::Map));

    style = std::make_shared<Style>();

    const auto styleInfo = data.getStyleInfo();

    if (!styleInfo.url.empty()) {
        // We have a style URL
        env.request({ Resource::Kind::JSON, styleInfo.url }, [this, styleInfo](const Response &res) {
            if (res.status == Response::Successful) {
                loadStyleJSON(res.data, styleInfo.base);
            } else {
                Log::Error(Event::Setup, "loading style failed: %s", res.message.c_str());
            }
        });
    } else {
        // We got JSON data directly.
        loadStyleJSON(styleInfo.json, styleInfo.base);
    }
}
void MapContext::loadStyleJSON(const std::string& json, const std::string& base) {
    assert(Environment::currentlyOn(ThreadType::Map));

    sprite.reset();
    style = std::make_shared<Style>();
    style->base = base;
    style->loadJSON((const uint8_t *)json.c_str());
    style->cascadeClasses(data.getClasses());
    style->setDefaultTransitionDuration(data.getDefaultTransitionDuration());

    const std::string glyphURL = util::mapbox::normalizeGlyphsURL(style->glyph_url, data.getAccessToken());
    glyphStore->setURL(glyphURL);

    triggerUpdate();
}

void MapContext::updateSources() {
    assert(Environment::currentlyOn(ThreadType::Map));

    // First, disable all existing sources.
    for (const auto& source : activeSources) {
        source->enabled = false;
    }

    // Then, reenable all of those that we actually use when drawing this layer.
    if (style) {
        updateSources(style->layers);
    }

    // Then, construct or destroy the actual source object, depending on enabled state.
    for (const auto& source : activeSources) {
        if (source->enabled) {
            if (!source->source) {
                source->source = std::make_shared<Source>(source->info);
                source->source->load(data, env, [this] {
                    assert(Environment::currentlyOn(ThreadType::Map));
                    triggerUpdate();
                });
            }
        } else {
            source->source.reset();
        }
    }

    // Finally, remove all sources that are disabled.
    util::erase_if(activeSources, [](util::ptr<StyleSource> source){
        return !source->enabled;
    });
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

void MapContext::updateTiles() {
    assert(Environment::currentlyOn(ThreadType::Map));
    for (const auto &source : activeSources) {
        source->source->update(data, getWorker(), style, *glyphAtlas,
                               *glyphStore, *spriteAtlas, getSprite(),
                               *texturePool, [this]() {
            assert(Environment::currentlyOn(ThreadType::Map));
            triggerUpdate();
        });
    }
}

void MapContext::updateAnnotationTiles(const std::vector<Tile::ID>& ids) {
    assert(Environment::currentlyOn(ThreadType::Main));
    for (const auto &source : activeSources) {
        if (source->info.type == SourceType::Annotations) {
            source->source->invalidateTiles(ids);
            triggerUpdate();
            return;
        }
    }
}

void MapContext::update() {
    assert(Environment::currentlyOn(ThreadType::Map));

    if (data.getTransformState().hasSize()) {
        prepare();
    }
}


void MapContext::prepare() {
    assert(Environment::currentlyOn(ThreadType::Map));

    const auto u = updated.exchange(static_cast<MapContext::UpdateType>(MapContext::Update::Nothing));
    if (u & static_cast<MapContext::UpdateType>(MapContext::Update::StyleInfo)) {
        reloadStyle();
    }
    if (u & static_cast<MapContext::UpdateType>(MapContext::Update::Debug)) {
        assert(painter);
        painter->setDebug(data.getDebug());
    }
    if (u & static_cast<MapContext::UpdateType>(MapContext::Update::DefaultTransitionDuration)) {
        if (style) {
            style->setDefaultTransitionDuration(data.getDefaultTransitionDuration());
        }
    }
    if (u & static_cast<MapContext::UpdateType>(MapContext::Update::Classes)) {
        if (style) {
            style->cascadeClasses(data.getClasses());
        }
    }

    // Update transform transitions.
    const auto animationTime = std::chrono::steady_clock::now();
    data.setAnimationTime(animationTime);
    if (data.transform.needsTransition()) {
        data.transform.updateTransitions(animationTime);
    }

    data.setTransformState(data.transform.currentState());
    auto& state = data.getTransformState();

    if (style) {
        updateSources();
        style->updateProperties(state.getNormalizedZoom(), animationTime);

        // Allow the sprite atlas to potentially pull new sprite images if needed.
        spriteAtlas->resize(state.getPixelRatio());
        spriteAtlas->setSprite(getSprite());

        updateTiles();
    }

    if (data.mode == MapMode::Continuous) {
        view.invalidate([this] { render(); });
    }
}

void MapContext::render() {
    assert(Environment::currentlyOn(ThreadType::Map));

    // Cleanup OpenGL objects that we abandoned since the last render call.
    env.performCleanup();

    assert(painter);
    painter->render(*style, activeSources, data.getTransformState(), data.getAnimationTime());
    // Schedule another rerender when we definitely need a next frame.
    if (data.transform.needsTransition() || style->hasTransitions()) {
        triggerUpdate();
    }
}

}
