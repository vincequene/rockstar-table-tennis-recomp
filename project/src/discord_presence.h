// Discord Rich Presence driven by the title's original Xbox Live presence.
//
// The game reports its presence through XUserSetContext/XUserSetProperty. The
// matching strings live in the title's SPA (XDBF) resource; this module turns
// them into Discord activity updates sent over Discord's local IPC pipe.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace tabletennis {

class DiscordPresence {
 public:
  // spa: raw XDBF resource of the title. language: XLanguage id (1 = English, 4 = French).
  DiscordPresence(std::string client_id, std::vector<uint8_t> spa, uint32_t language);
  ~DiscordPresence();

  DiscordPresence(const DiscordPresence&) = delete;
  DiscordPresence& operator=(const DiscordPresence&) = delete;

  void Start();
  void Stop();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tabletennis
