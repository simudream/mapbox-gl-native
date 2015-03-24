#ifndef MBGL_MAP_MAP_CONTEXT
#define MBGL_MAP_MAP_CONTEXT

#include <mbgl/map/tile.hpp>
#include <mbgl/util/ptr.hpp>
#include <mbgl/util/signal.hpp>

#include <set>
#include <vector>
namespace uv {
class worker;
class async;
}

namespace mbgl {

class Environment;
class View;
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
    using UpdateType = uint32_t;
    enum class Update : UpdateType {
        Nothing                   = 0,
        StyleInfo                 = 1 << 0,
        Debug                     = 1 << 1,
        DefaultTransitionDuration = 1 << 2,
        Classes                   = 1 << 3,
    };

public:
    MapContext(Environment&, View&, MapData&);

    // Starts the async handles.
    void start();

    // Terminates the map thread
    void terminate();

    // Triggers a render. Can be called from any thread.
    void triggerRender();

    // Notifies the Map thread that the state has changed and an update might be necessary.
    void triggerUpdate(Update = Update::Nothing);

    // These can only be called from the Map thread.
private:
    uv::worker& getWorker();
    util::ptr<Sprite> getSprite();
    void updateSources();
    void updateSources(const util::ptr<StyleLayerGroup>&);
    void updateTiles();

    // TODO: Make these private
    public: void updateAnnotationTiles(const std::vector<Tile::ID>& ids);

    // Triggered by triggerUpdate();
    private: void update();

    // Prepares a map render by updating the tiles we need for the current view, as well as updating
    // the stylesheet.
    public: void prepare();

    // Unconditionally performs a render with the current map state.
    public: void render();

private:
    // Loads the style set in the data object. Called by Update::StyleInfo
    private: void reloadStyle();

    // Loads the actual JSON object an creates a new Style object.
    private: void loadStyleJSON(const std::string& json, const std::string& base);

    // TODO: Make all of these private
public:
    private: Environment& env;
    private: View& view;
    private: MapData& data;

    private: std::atomic<UpdateType> updated { static_cast<UpdateType>(Update::Nothing) };

    private: std::unique_ptr<uv::async> asyncUpdate;
    private: std::unique_ptr<uv::async> asyncRender;
    private: std::unique_ptr<uv::async> asyncTerminate;

    public: std::unique_ptr<uv::worker> workers;
    private: const std::unique_ptr<GlyphStore> glyphStore;
    private: const std::unique_ptr<GlyphAtlas> glyphAtlas;
    private: const std::unique_ptr<SpriteAtlas> spriteAtlas;
    private: const std::unique_ptr<LineAtlas> lineAtlas;
    private: const std::unique_ptr<TexturePool> texturePool;
    public: const std::unique_ptr<Painter> painter;
    private: util::ptr<Sprite> sprite;
    public: util::ptr<Style> style;
    public: std::set<util::ptr<StyleSource>> activeSources;

    // Used to signal that rendering completed.
    public: util::Signal rendered;

    // TODO: document this
    public: bool terminating = false;
};

}

#endif
