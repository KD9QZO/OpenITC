#include "app.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace openitc::gui {
namespace {

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c += 32;
    return s;
}

}  // namespace

std::string hexs(const std::vector<uint8_t>& d, size_t from, size_t maxn) {
    static const char* k = "0123456789abcdef";
    std::string s;
    for (size_t i = from; i < d.size() && i < from + maxn; ++i) {
        s.push_back(k[d[i] >> 4]);
        s.push_back(k[d[i] & 0xf]);
    }
    return s;
}

std::string summarize(const PacketEvent& e) {
    char buf[256];
    const std::vector<uint8_t>& p = e.payload;
    if (e.type == 0x02 && e.beacon.valid) {
        std::snprintf(buf, sizeof(buf),
                      "id=%06x lat=%.5f lon=%.5f ch=%d rate=%d util=%d", e.beacon.radio_id,
                      e.beacon.lat, e.beacon.lon, e.beacon.channel, e.beacon.rate,
                      e.beacon.util);
    } else if (e.type == 0x06 && e.time.valid) {
        std::snprintf(buf, sizeof(buf), "TID=%06x  %04d-%02d-%02d %02d:%02d:%02d UTC",
                      e.time.tid, e.time.year, e.time.month, e.time.day, e.time.hour,
                      e.time.minute, e.time.second);
    } else if (e.type == 0x05 && e.cntl.valid) {
        std::snprintf(buf, sizeof(buf), "TID=%06x seq=%d data=%s", e.cntl.tid, e.cntl.seq,
                      hexs(e.cntl.data, 0, 12).c_str());
    } else if ((e.type == 0x33 || e.type == 0x34) && e.wiu.valid) {
        std::snprintf(buf, sizeof(buf), "WIU=%06x data=%s", e.wiu.wiu_id,
                      hexs(e.wiu.indicators, 0, 16).c_str());
    } else if (p.size() >= 3) {
        std::snprintf(buf, sizeof(buf), "TID=%06x  %s", openitc::tid24(p),
                      hexs(p, 3, 16).c_str());
    } else {
        std::snprintf(buf, sizeof(buf), "%s", hexs(p, 0, 16).c_str());
    }
    return buf;
}

void local_hms(double t_unix, char* out, size_t n) {
    std::time_t t = (std::time_t)t_unix;
    std::strftime(out, n, "%H:%M:%S", std::localtime(&t));
}

// Per-user data directory: %LOCALAPPDATA%\OpenITC on Windows,
// ~/Library/Application Support/OpenITC on macOS, and
// $XDG_DATA_HOME/openitc (default ~/.local/share/openitc) elsewhere.
// Falls back to the current directory if none of those can be resolved.
static std::filesystem::path user_data_dir() {
    namespace fs = std::filesystem;
    auto env = [](const char* k) -> const char* {
        const char* v = std::getenv(k);
        return (v && *v) ? v : nullptr;
    };
#if defined(_WIN32)
    if (const char* la = env("LOCALAPPDATA")) return fs::path(la) / "OpenITC";
#elif defined(__APPLE__)
    if (const char* home = env("HOME"))
        return fs::path(home) / "Library" / "Application Support" / "OpenITC";
#else
    if (const char* xdg = env("XDG_DATA_HOME")) return fs::path(xdg) / "openitc";
    if (const char* home = env("HOME")) return fs::path(home) / ".local" / "share" / "openitc";
#endif
    return fs::path(".");
}

std::string default_db_path() {
    const std::filesystem::path dir = user_data_dir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "openitc.db").string();
}

std::string tile_cache_dir() {
    const std::filesystem::path dir = user_data_dir();
    std::error_code ec;
    std::filesystem::create_directories(dir / "tiles", ec);
    return (dir / "tiles").string();
}

bool Filters::match(const PacketEvent& e) const {
    if (channel != 0 && e.channel != channel) return false;
    if (type >= 0 && e.type != type) return false;
    if (text[0]) {
        const std::string hay = lower(summarize(e) + " " + openitc::type_name(e.type));
        if (hay.find(lower(text)) == std::string::npos) return false;
    }
    return true;
}

void AppState::subscribe(MonitorEngine& eng) {
    eng.set_callback([this](const PacketEvent& e) {
        repo.observe(e);
        log.add(e);
        if (recording && store.is_open()) store.observe(e);
    });
}

void AppState::start() {
    engine = std::make_unique<MonitorEngine>(ec);
    subscribe(*engine);
    if (engine->start()) {
        running = true;
        status.clear();
    } else {
        status = "start failed: " + engine->last_error();
    }
}

void AppState::stop() {
    if (engine) engine->stop();
    running = false;
}

}  // namespace openitc::gui
