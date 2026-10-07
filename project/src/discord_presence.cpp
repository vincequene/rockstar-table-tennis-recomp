// Discord Rich Presence driven by the title's original Xbox Live presence.

#include "discord_presence.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/system/presence.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace tabletennis {

namespace {

constexpr uint32_t kContextPresence = 0x8001;

// Rockstar Table Tennis (545407DF) presence modes -> SPA string ids, in the
// order of the SPA presence (XRPT) records.
constexpr uint16_t kPresenceModeStrings[] = {14, 18, 81, 82, 83, 84, 86, 87, 88, 142, 226, 227};

// Character context (2 = player 1, 6 = player 2) values -> SPA string ids.
constexpr uint16_t kCharacterStrings[] = {16, 17, 48, 49, 50, 51, 52, 53, 54, 55, 56};

uint16_t BE16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
uint32_t BE32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint64_t BE64(const uint8_t* p) { return (uint64_t(BE32(p)) << 32) | BE32(p + 4); }

// Loads the XSTR string table for a language from an XDBF blob.
std::unordered_map<uint16_t, std::string> LoadStrings(const std::vector<uint8_t>& spa,
                                                      uint32_t language) {
  std::unordered_map<uint16_t, std::string> out;
  if (spa.size() < 24 || std::memcmp(spa.data(), "XDBF", 4) != 0) return out;
  uint32_t entry_table_len = BE32(&spa[8]);
  uint32_t entry_count = BE32(&spa[12]);
  uint32_t free_table_len = BE32(&spa[16]);
  size_t base = 24 + size_t(entry_table_len) * 18 + size_t(free_table_len) * 8;
  for (uint32_t i = 0; i < entry_count; ++i) {
    const uint8_t* e = &spa[24 + size_t(i) * 18];
    if (24 + size_t(i) * 18 + 18 > spa.size()) break;
    uint16_t section = BE16(e);
    uint64_t id = BE64(e + 2);
    uint32_t offset = BE32(e + 10);
    uint32_t size = BE32(e + 14);
    if (section != 3 || id != language) continue;
    if (base + offset + size > spa.size() || size < 14) break;
    const uint8_t* b = &spa[base + offset];
    uint16_t count = BE16(b + 12);
    size_t p = 14;
    for (uint16_t j = 0; j < count && p + 4 <= size; ++j) {
      uint16_t sid = BE16(b + p);
      uint16_t len = BE16(b + p + 2);
      if (p + 4 + len > size) break;
      out[sid] = std::string(reinterpret_cast<const char*>(b + p + 4), len);
      p += 4 + size_t(len);
    }
  }
  return out;
}

std::string JsonEscape(std::string_view s) {
  std::string o;
  for (char c : s) {
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': break;
      case '\t': o += ' '; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) break;
        o += c;
    }
  }
  return o;
}

std::string Trim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && s[i] == ' ') ++i;
  return s.substr(i);
}

}  // namespace

struct DiscordPresence::Impl {
  std::string client_id;
  std::unordered_map<uint16_t, std::string> strings;

  std::mutex mutex;
  std::condition_variable cv;
  std::map<uint32_t, int64_t> contexts;
  std::map<uint32_t, int64_t> properties;
  bool dirty = true;
  std::atomic<bool> running{false};
  std::thread thread;
  int64_t start_time = 0;

#if defined(_WIN32)
  HANDLE pipe = INVALID_HANDLE_VALUE;
#endif

  std::string Str(uint16_t id) const {
    auto it = strings.find(id);
    return it == strings.end() ? std::string() : it->second;
  }

  std::string ContextText(uint32_t id, int64_t value) const {
    if (id == 2 || id == 6) {
      if (value >= 0 && value < int64_t(std::size(kCharacterStrings))) {
        return Str(kCharacterStrings[value]);
      }
    }
    return std::to_string(value);
  }

  // Expands {cN} and {p0xNNNNNNNN} tokens of a presence string.
  std::string Expand(const std::string& fmt) const {
    std::string out;
    for (size_t i = 0; i < fmt.size();) {
      if (fmt[i] == '{') {
        size_t end = fmt.find('}', i);
        if (end != std::string::npos) {
          std::string token = fmt.substr(i + 1, end - i - 1);
          if (!token.empty() && token[0] == 'c') {
            uint32_t id = uint32_t(std::strtoul(token.c_str() + 1, nullptr, 10));
            auto it = contexts.find(id);
            out += it != contexts.end() ? ContextText(id, it->second) : "?";
          } else if (!token.empty() && token[0] == 'p') {
            uint32_t id = uint32_t(std::strtoul(token.c_str() + 1, nullptr, 16));
            auto it = properties.find(id);
            out += it != properties.end() ? std::to_string(it->second) : "0";
          }
          i = end + 1;
          continue;
        }
      }
      out += fmt[i++];
    }
    return out;
  }

  // Builds (details, state) from the current presence. Caller holds mutex.
  std::pair<std::string, std::string> BuildActivity() const {
    int64_t mode = 0;
    if (auto it = contexts.find(kContextPresence); it != contexts.end()) mode = it->second;
    std::string text;
    if (mode >= 0 && mode < int64_t(std::size(kPresenceModeStrings))) {
      text = Expand(Str(kPresenceModeStrings[mode]));
    }
    std::string details = text, state;
    if (size_t nl = text.find('\n'); nl != std::string::npos) {
      details = text.substr(0, nl);
      state = text.substr(nl + 1);
    }
    details = Trim(details);
    state = Trim(state);
    if (details.size() < 2) details = "Rockstar Table Tennis";
    if (!state.empty() && state.size() < 2) state += " ";
    return {details, state};
  }

#if defined(_WIN32)
  bool WriteFrame(uint32_t opcode, const std::string& json) {
    std::string buf(8 + json.size(), '\0');
    uint32_t len = uint32_t(json.size());
    std::memcpy(&buf[0], &opcode, 4);
    std::memcpy(&buf[4], &len, 4);
    std::memcpy(&buf[8], json.data(), json.size());
    DWORD written = 0;
    return WriteFile(pipe, buf.data(), DWORD(buf.size()), &written, nullptr) &&
           written == buf.size();
  }

  bool ReadFrame(uint32_t* opcode, std::string* json) {
    uint32_t header[2];
    DWORD read = 0;
    if (!ReadFile(pipe, header, 8, &read, nullptr) || read != 8) return false;
    *opcode = header[0];
    json->assign(header[1], '\0');
    DWORD total = 0;
    while (total < header[1]) {
      if (!ReadFile(pipe, json->data() + total, header[1] - total, &read, nullptr) || !read) {
        return false;
      }
      total += read;
    }
    return true;
  }

  void Disconnect() {
    if (pipe != INVALID_HANDLE_VALUE) {
      CloseHandle(pipe);
      pipe = INVALID_HANDLE_VALUE;
    }
  }

  bool Connect() {
    for (int i = 0; i < 10; ++i) {
      std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(i);
      pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                         0, nullptr);
      if (pipe != INVALID_HANDLE_VALUE) break;
    }
    if (pipe == INVALID_HANDLE_VALUE) return false;
    uint32_t op = 0;
    std::string reply;
    if (!WriteFrame(0, "{\"v\":1,\"client_id\":\"" + JsonEscape(client_id) + "\"}") ||
        !ReadFrame(&op, &reply) || op != 1) {
      REXLOG_WARN("[discord] handshake failed: {}", reply);
      Disconnect();
      return false;
    }
    REXLOG_INFO("[discord] connected");
    return true;
  }

  bool SendActivity(const std::string& details, const std::string& state) {
    std::string activity = "{\"details\":\"" + JsonEscape(details) + "\"";
    if (!state.empty()) activity += ",\"state\":\"" + JsonEscape(state) + "\"";
    activity += ",\"timestamps\":{\"start\":" + std::to_string(start_time) + "}";
    activity +=
        ",\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Rockstar Table Tennis\"}}";
    static uint64_t nonce = 0;
    std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" +
                       std::to_string(GetCurrentProcessId()) + ",\"activity\":" + activity +
                       "},\"nonce\":\"" + std::to_string(++nonce) + "\"}";
    uint32_t op = 0;
    std::string reply;
    if (!WriteFrame(1, json) || !ReadFrame(&op, &reply)) return false;
    if (reply.find("\"evt\":\"ERROR\"") != std::string::npos) {
      REXLOG_WARN("[discord] SET_ACTIVITY error: {}", reply);
    }
    return true;
  }
#endif

  void Run() {
#if defined(_WIN32)
    using namespace std::chrono;
    auto last_send = steady_clock::now() - seconds(10);
    while (running) {
      if (pipe == INVALID_HANDLE_VALUE) {
        if (!Connect()) {
          std::unique_lock lock(mutex);
          cv.wait_for(lock, seconds(15), [this] { return !running.load(); });
          continue;
        }
        std::lock_guard lock(mutex);
        dirty = true;
      }
      std::pair<std::string, std::string> act;
      {
        std::unique_lock lock(mutex);
        cv.wait_for(lock, seconds(1), [this] { return !running.load() || dirty; });
        if (!running) break;
        // Discord allows ~5 updates per 20 s: keep at least 4 s between sends.
        if (!dirty || steady_clock::now() - last_send < seconds(4)) continue;
        dirty = false;
        act = BuildActivity();
      }
      REXLOG_INFO("[discord] {} | {}", act.first, act.second);
      if (!SendActivity(act.first, act.second)) {
        REXLOG_WARN("[discord] connection lost");
        Disconnect();
        std::lock_guard lock(mutex);
        dirty = true;
      }
      last_send = steady_clock::now();
    }
    Disconnect();
#endif
  }
};

DiscordPresence::DiscordPresence(std::string client_id, std::vector<uint8_t> spa,
                                 uint32_t language)
    : impl_(std::make_unique<Impl>()) {
  impl_->client_id = std::move(client_id);
  impl_->strings = LoadStrings(spa, language);
  if (impl_->strings.empty()) impl_->strings = LoadStrings(spa, 1);
  impl_->start_time = int64_t(std::time(nullptr));
}

DiscordPresence::~DiscordPresence() { Stop(); }

void DiscordPresence::Start() {
  if (impl_->running.exchange(true)) return;
  Impl* impl = impl_.get();
  rex::system::presence::SetListener(
      [impl](uint32_t user_index, rex::system::presence::Kind kind, uint32_t id, int64_t value) {
        if (user_index != 0) return;
        std::lock_guard lock(impl->mutex);
        auto& map = kind == rex::system::presence::Kind::kContext ? impl->contexts
                                                                   : impl->properties;
        auto it = map.find(id);
        if (it != map.end() && it->second == value) return;
        map[id] = value;
        impl->dirty = true;
        if (kind == rex::system::presence::Kind::kContext && id == kContextPresence) {
          REXLOG_INFO("[discord] presence mode {}", value);
        }
        impl->cv.notify_all();
      });
  impl_->thread = std::thread([impl] { impl->Run(); });
}

void DiscordPresence::Stop() {
  if (!impl_->running.exchange(false)) return;
  rex::system::presence::SetListener(nullptr);
  impl_->cv.notify_all();
  if (impl_->thread.joinable()) impl_->thread.join();
}

}  // namespace tabletennis

// Shared "Rockstar Table Tennis" Discord application; can be overridden.
REXCVAR_DEFINE_STRING(discord_client_id, "1557520303423496342", "Discord",
                      "Discord application ID for Rich Presence");
REXCVAR_DEFINE_STRING(export_icon, "", "Tools",
                      "Write the game icon (PNG) to this path and exit");
REXCVAR_DEFINE_STRING(debug_open_menu, "", "Tools", "Open a menu at startup: settings or achievements");
REXCVAR_DEFINE_BOOL(discord_enabled, true, "Discord", "Show the game status on Discord");
