#ifndef MBGL_MAP_MAP
#define MBGL_MAP_MAP

#include <mbgl/map/tile.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/util/noncopyable.hpp>
#include <mbgl/util/uv.hpp>
#include <mbgl/util/ptr.hpp>
#include <mbgl/util/vec.hpp>

#include <cstdint>
#include <atomic>
#include <thread>
#include <iosfwd>
#include <set>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>

namespace mbgl {

class Sprite;
class Style;
class StyleLayerGroup;
class FileSource;
class View;
class Environment;
class EnvironmentScope;
class MapData;
class MapContext;

class Map : private util::noncopyable {
    friend class View;

public:
    explicit Map(View&, FileSource&);
    ~Map();

    // Start the map render thread. It is asynchronous.
    void start(bool startPaused = false);

    // Stop the map render thread. This call will block until the map rendering thread stopped.
    // The optional callback function will be invoked repeatedly until the map thread is stopped.
    // The callback function should wait until it is woken up again by view.notify(), otherwise
    // this will be a busy waiting loop.
    void stop(std::function<void ()> callback = std::function<void ()>());

    // Pauses the render thread. The render thread will stop running but will not be terminated and will not lose state until resumed.
    void pause(bool waitForPause = false);

    // Resumes a paused render thread
    void resume();

    // Runs the map event loop. ONLY run this function when you want to get render a single frame
    // with this map object. It will *not* spawn a separate thread and instead block until the
    // frame is completely rendered.
    void run();

    // Triggers a synchronous or asynchronous render.
    void renderSync();

    // Unconditionally performs a render with the current map state. May only be called from the Map
    // thread.
    void render();

    // Notifies the Map thread that the state has changed and an update might be necessary.
    using UpdateType = uint32_t;
    enum class Update : UpdateType {
        Nothing                   = 0,
        StyleInfo                 = 1 << 0,
        Debug                     = 1 << 1,
        DefaultTransitionDuration = 1 << 2,
        Classes                   = 1 << 3,
    };
    void triggerUpdate(Update = Update::Nothing);

    // Triggers a render. Can be called from any thread.
    void triggerRender();

    // Releases resources immediately
    void terminate();

    // Styling
    void addClass(const std::string&);
    void removeClass(const std::string&);
    bool hasClass(const std::string&) const;
    void setClasses(const std::vector<std::string>&);
    std::vector<std::string> getClasses() const;

    void setDefaultTransitionDuration(std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    std::chrono::steady_clock::duration getDefaultTransitionDuration();
    void setStyleURL(const std::string& url);
    void setStyleJSON(const std::string& json, const std::string& base = "");
    std::string getStyleJSON() const;

    // Transition
    void cancelTransitions();

    // Position
    void moveBy(double dx, double dy, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    void setLatLng(LatLng latLng, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    LatLng getLatLng() const;
    void startPanning();
    void stopPanning();
    void resetPosition();

    // Scale
    void scaleBy(double ds, double cx = -1, double cy = -1, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    void setScale(double scale, double cx = -1, double cy = -1, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    double getScale() const;
    void setZoom(double zoom, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    double getZoom() const;
    void setLatLngZoom(LatLng latLng, double zoom, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    void resetZoom();
    void startScaling();
    void stopScaling();
    double getMinZoom() const;
    double getMaxZoom() const;

    // Rotation
    void rotateBy(double sx, double sy, double ex, double ey, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    void setBearing(double degrees, std::chrono::steady_clock::duration duration = std::chrono::steady_clock::duration::zero());
    void setBearing(double degrees, double cx, double cy);
    double getBearing() const;
    void resetNorth();
    void startRotating();
    void stopRotating();

    // API
    void setAccessToken(const std::string &token);
    std::string getAccessToken() const;

    // Projection
    void getWorldBoundsMeters(ProjectedMeters &sw, ProjectedMeters &ne) const;
    void getWorldBoundsLatLng(LatLng &sw, LatLng &ne) const;
    double getMetersPerPixelAtLatitude(const double lat, const double zoom) const;
    const ProjectedMeters projectedMetersForLatLng(const LatLng latLng) const;
    const LatLng latLngForProjectedMeters(const ProjectedMeters projectedMeters) const;
    const vec2<double> pixelForLatLng(const LatLng latLng) const;
    const LatLng latLngForPixel(const vec2<double> pixel) const;

    // Annotations
    void setDefaultPointAnnotationSymbol(const std::string&);
    uint32_t addPointAnnotation(const LatLng&, const std::string& symbol);
    std::vector<uint32_t> addPointAnnotations(const std::vector<LatLng>&,
                                              const std::vector<std::string>& symbols);
    void removeAnnotation(uint32_t);
    void removeAnnotations(const std::vector<uint32_t>&);
    std::vector<uint32_t> getAnnotationsInBounds(const LatLngBounds&) const;
    LatLngBounds getBoundsForAnnotations(const std::vector<uint32_t>&) const;

    // Debug
    void setDebug(bool value);
    void toggleDebug();
    bool getDebug() const;

private:
    // This may only be called by the View object.
    void resize(uint16_t width, uint16_t height, float ratio = 1);
    void resize(uint16_t width, uint16_t height, float ratio, uint16_t fbWidth, uint16_t fbHeight);

    util::ptr<Sprite> getSprite();

    // Checks if render thread needs to pause
    void checkForPause();

    // Setup
    void setup();

    void updateTiles();
    void updateSources();

    // Triggered by triggerUpdate();
    void update();

    // Loads the style set in the data object. Called by Update::StyleInfo
    void reloadStyle();
    void loadStyleJSON(const std::string& json, const std::string& base);

    // Prepares a map render by updating the tiles we need for the current view, as well as updating
    // the stylesheet.
    void prepare();

    void updateAnnotationTiles(const std::vector<Tile::ID>&);

    enum class Mode : uint8_t {
        None, // we're not doing any processing
        Continuous, // continually updating map
        Static, // a once-off static image.
    };

    Mode mode = Mode::None;

    const std::unique_ptr<Environment> env;
    std::unique_ptr<EnvironmentScope> scope;
    View &view;
    const std::unique_ptr<MapData> data;
    std::unique_ptr<MapContext> context;

private:
    std::thread thread;
    std::unique_ptr<uv::async> asyncTerminate;
    std::unique_ptr<uv::async> asyncUpdate;
    std::unique_ptr<uv::async> asyncRender;

    bool terminating = false;
    bool pausing = false;
    bool isPaused = false;
    std::mutex mutexRun;
    std::condition_variable condRun;
    std::mutex mutexPause;
    std::condition_variable condPause;

    // Used to signal that rendering completed.
    bool rendered = false;
    std::condition_variable condRendered;
    std::mutex mutexRendered;

    // Stores whether the map thread has been stopped already.
    std::atomic_bool isStopped;

    util::ptr<Style> style;

    std::atomic<UpdateType> updated;
};

}

#endif
