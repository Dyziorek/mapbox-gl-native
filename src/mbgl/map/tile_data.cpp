#include <mbgl/map/tile_data.hpp>
#include <mbgl/map/environment.hpp>
#include <mbgl/map/source.hpp>

#include <mbgl/storage/file_source.hpp>
#include <mbgl/util/worker.hpp>
#include <mbgl/util/work_request.hpp>
#include <mbgl/platform/log.hpp>

using namespace mbgl;

TileData::TileData(const TileID& id_, const SourceInfo& source_)
    : id(id_),
      name(id),
      source(source_),
      env(Environment::Get()),
      state(State::initial),
      debugBucket(debugFontBuffer) {
    // Initialize tile debug coordinates
    debugFontBuffer.addText(name.c_str(), 50, 200, 5);
}

TileData::~TileData() {
    cancel();
}

const std::string TileData::toString() const {
    return std::string { "[tile " } + name + "]";
}

void TileData::setState(const State& state_) {
    assert(!isImmutable());

    state = state_;
}

void TileData::request(Worker& worker, float pixelRatio, std::function<void()> callback) {
    std::string url = source.tileURL(id, pixelRatio);
    state = State::loading;

    req = env.request({ Resource::Kind::Tile, url }, [url, callback, &worker, this](const Response &res) {
        req = nullptr;

        if (res.status != Response::Successful) {
            Log::Error(Event::HttpRequest, "[%s] tile loading failed: %s", url.c_str(), res.message.c_str());
            return;
        }

        state = State::loaded;
        data = res.data;

        // Schedule tile parsing in another thread
        reparse(worker, callback);
    });
}

void TileData::cancel() {
    if (state != State::obsolete) {
        state = State::obsolete;
    }
    if (req) {
        env.cancelRequest(req);
        req = nullptr;
    }
    workRequest.reset();
}

bool TileData::mayStartParsing() {
    return !parsing.test_and_set(std::memory_order_acquire);
}

void TileData::endParsing() {
    parsing.clear(std::memory_order_release);
}

void TileData::reparse(Worker& worker, std::function<void()> callback) {
    if (!mayStartParsing()) {
        return;
    }

    workRequest = worker.send([this] { parse(); endParsing(); }, callback);
}
