// tabletennis - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>
#include <rex/system/xmemory.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/virtual_key.h>
#include <rex/ui/window.h>

#include <atomic>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

#include "discord_presence.h"
#include "achievements_menu.h"
#include "settings_menu.h"
#include "tt_ui.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#endif

REXCVAR_DECLARE(std::string, discord_client_id);
REXCVAR_DECLARE(uint32_t, user_language);
REXCVAR_DECLARE(bool, discord_enabled);
REXCVAR_DECLARE(std::string, export_icon);
REXCVAR_DECLARE(std::string, debug_open_menu);

class TabletennisApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  ~TabletennisApp() override {
    quit_watch_running_ = false;
    if (quit_watch_thread_.joinable()) {
      quit_watch_thread_.join();
    }
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<TabletennisApp>(new TabletennisApp(ctx, "tabletennis",
        PPCImageConfig));
  }

  // Paths are resolved before tabletennis.toml is read, so defaults are set here.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    // Saves, achievements and shader cache: Documents\Rockstar Table Tennis
    // (instead of the SDK default Documents\tabletennis), unless overridden.
    if (REXCVAR_GET(user_data_root).empty()) {
      auto old_dir = paths.user_data_root;
      auto new_dir = old_dir.parent_path() / "Rockstar Table Tennis";
      std::error_code ec;
      if (!std::filesystem::exists(new_dir) && std::filesystem::exists(old_dir)) {
        // One-time migration; the old folder is kept as a backup.
        std::filesystem::copy(old_dir, new_dir, std::filesystem::copy_options::recursive, ec);
      }
      paths.user_data_root = new_dir;
      if (REXCVAR_GET(cache_root).empty()) {
        paths.cache_root = new_dir / "cache";
      }
    }
#if defined(_WIN32)
    // Game data: "game" folder next to the exe (packaged install), otherwise
    // the dev tree's <project>/assets.
    if (paths.game_data_root.empty() || !std::filesystem::exists(paths.game_data_root)) {
      wchar_t exe_path[MAX_PATH] = {0};
      GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
      auto exe_dir = std::filesystem::path(exe_path).parent_path();
      for (auto candidate : {exe_dir / "game", exe_dir / ".." / ".." / ".." / "assets"}) {
        if (std::filesystem::exists(candidate / "default.xex")) {
          paths.game_data_root = std::filesystem::weakly_canonical(candidate);
          break;
        }
      }
    }
#endif
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override { tabletennis::ui::LoadFonts(atlas); }

  void OnPostSetup() override {
    if (!REXCVAR_GET(export_icon).empty()) {
      ExportIconAndExit(REXCVAR_GET(export_icon));
    }
    if (window()) {
      window()->SetTitle("Rockstar Table Tennis");
    }
    ApplyWindowIcon();
    StartDiscord();
    // Developer aid: open a menu at startup (used for UI screenshots).
    if (REXCVAR_GET(debug_open_menu) == "settings") ToggleSettings();
    if (REXCVAR_GET(debug_open_menu) == "achievements") ToggleAchievements();
    // The game ignores the controller while one of our menus is open.
    if (auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system())) {
      input->SetActiveCallback([this] { return !menu_open_.load(); });
    }
#if defined(_WIN32)
    // Controller shortcuts: View+Menu held 2 s = quit, View+LB = settings,
    // View+RB = achievements.
    quit_watch_running_ = true;
    quit_watch_thread_ = std::thread([this]() {
      using clock = std::chrono::steady_clock;
      clock::time_point held_since{};
      bool holding = false;
      WORD prev = 0;
      while (quit_watch_running_) {
        WORD buttons = 0;
        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
          XINPUT_STATE state = {};
          if (XInputGetState(i, &state) == ERROR_SUCCESS) buttons |= state.Gamepad.wButtons;
        }
        const WORD quit = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_START;
        const WORD settings = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_LEFT_SHOULDER;
        const WORD trophies = XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_RIGHT_SHOULDER;
        bool combo = (buttons & quit) == quit;
        if (combo && !holding) {
          holding = true;
          held_since = clock::now();
        } else if (!combo) {
          holding = false;
        }
        if (holding && clock::now() - held_since >= std::chrono::seconds(2)) {
          RequestQuit();
          return;
        }
        if ((buttons & settings) == settings && (prev & settings) != settings) {
          app_context().CallInUIThread([this] { ToggleSettings(); });
        }
        if ((buttons & trophies) == trophies && (prev & trophies) != trophies) {
          app_context().CallInUIThread([this] { ToggleAchievements(); });
        }
        prev = buttons;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
      }
    });
#endif
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    imgui_drawer_ref_ = drawer;
    // Replace the SDK's achievements overlay with the game-styled one.
    rex::ui::UnregisterBind("bind_achievements");
    rex::ui::RegisterBind("bind_tt_settings", "F1", "Toggle game settings menu",
                          [this] { ToggleSettings(); });
    rex::ui::RegisterBind("bind_tt_achievements", "F7", "Toggle achievements",
                          [this] { ToggleAchievements(); });
  }

 private:
  static std::filesystem::path ExeDir() {
#if defined(_WIN32)
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    return std::filesystem::path(exe_path).parent_path();
#else
    return std::filesystem::current_path();
#endif
  }

  // Writes the title's icon (PNG from the game's XDBF data) and exits. Used by
  // the build script so the exe gets the game's own icon.
  void ExportIconAndExit(const std::string& path) {
    int code = 1;
    if (auto* ks = runtime() ? runtime()->kernel_state() : nullptr) {
      auto icon = ks->title_xdbf().icon();
      if (icon) {
        if (FILE* f = std::fopen(path.c_str(), "wb")) {
          std::fwrite(icon.buffer, 1, icon.size, f);
          std::fclose(f);
          code = 0;
        }
      }
    }
    std::_Exit(code);
  }

  void UpdateMenuOpen() { menu_open_ = settings_menu_ || achievements_menu_; }

  void ToggleSettings() {
    if (settings_menu_) {
      settings_menu_.reset();
    } else {
      achievements_menu_.reset();
      OpenSettingsMenu();
    }
    UpdateMenuOpen();
  }

  void ToggleAchievements() {
    if (achievements_menu_) {
      achievements_menu_.reset();
    } else if (imgui_drawer_ref_ && runtime() && runtime()->kernel_state()) {
      settings_menu_.reset();
      achievements_menu_ = std::make_unique<tabletennis::AchievementsMenu>(
          imgui_drawer_ref_, immediate_drawer(), runtime(),
          &runtime()->kernel_state()->achievements(), [this] {
            app_context().CallInUIThreadDeferred([this] {
              achievements_menu_.reset();
              UpdateMenuOpen();
            });
          });
    }
    UpdateMenuOpen();
  }

  void OpenSettingsMenu() {
    if (!imgui_drawer_ref_) return;
    tabletennis::SettingsMenu::Callbacks cb;
    cb.set_fullscreen = [this](bool on) {
      if (window()) window()->SetFullscreen(on);
    };
    cb.set_discord = [this](bool on) {
      rex::cvar::SetFlagByName("discord_enabled", on ? "true" : "false");
      if (on) {
        StartDiscord();
      } else {
        discord_.reset();
      }
    };
    cb.restart = [this] { RestartGame(); };
    cb.close = [this] {
      // Destroy the dialog outside of its own draw call.
      app_context().CallInUIThreadDeferred([this] {
        settings_menu_.reset();
        UpdateMenuOpen();
      });
    };
    settings_menu_ = std::make_unique<tabletennis::SettingsMenu>(
        imgui_drawer_ref_, ExeDir() / "tabletennis.toml", std::move(cb));
  }

  // Starts a fresh copy of the game with the same arguments, then quits.
  void RestartGame() {
#if defined(_WIN32)
    wchar_t exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::wstring cmdline = GetCommandLineW();
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    std::wstring dir = ExeDir().wstring();
    if (CreateProcessW(exe_path, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr,
                       dir.c_str(), &si, &pi)) {
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
    }
#endif
    RequestQuit();
  }

  rex::ui::ImGuiDrawer* imgui_drawer_ref_ = nullptr;
  std::unique_ptr<tabletennis::SettingsMenu> settings_menu_;
  std::unique_ptr<tabletennis::AchievementsMenu> achievements_menu_;
  std::atomic<bool> menu_open_{false};

  // The SDK window has no icon of its own: give every top-level window of this
  // process the exe's icon (title bar, taskbar, Alt+Tab).
  static void ApplyWindowIcon() {
#if defined(_WIN32)
    EnumWindows(
        [](HWND hwnd, LPARAM) -> BOOL {
          DWORD pid = 0;
          GetWindowThreadProcessId(hwnd, &pid);
          if (pid != GetCurrentProcessId()) return TRUE;
          HINSTANCE inst = GetModuleHandleW(nullptr);
          auto icon_big = static_cast<HICON>(LoadImageW(inst, L"IDI_ICON1", IMAGE_ICON,
                                                   GetSystemMetrics(SM_CXICON),
                                                   GetSystemMetrics(SM_CYICON), 0));
          auto icon_small = static_cast<HICON>(LoadImageW(inst, L"IDI_ICON1", IMAGE_ICON,
                                                     GetSystemMetrics(SM_CXSMICON),
                                                     GetSystemMetrics(SM_CYSMICON), 0));
          if (icon_big) SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_big));
          if (icon_small) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_small));
          return TRUE;
        },
        0);
#endif
  }

  // Copies the title's SPA/XDBF resource (achievements, presence strings).
  std::vector<uint8_t> ReadTitleSpa() {
    auto* ks = runtime() ? runtime()->kernel_state() : nullptr;
    if (!ks) return {};
    auto module = ks->GetExecutableModule();
    if (!module) return {};
    uint32_t data = 0, size = 0;
    char name[16];
    std::snprintf(name, sizeof(name), "%08X", module->title_id());
    if (XFAILED(module->GetSection(name, &data, &size)) || !size) return {};
    auto* p = ks->memory()->TranslateVirtual<const uint8_t*>(data);
    return std::vector<uint8_t>(p, p + size);
  }

  void StartDiscord() {
    std::string client_id = REXCVAR_GET(discord_client_id);
    if (client_id.empty() || !REXCVAR_GET(discord_enabled) || discord_) return;
    discord_ = std::make_unique<tabletennis::DiscordPresence>(client_id, ReadTitleSpa(),
                                                              REXCVAR_GET(user_language));
    discord_->Start();
  }

  std::unique_ptr<tabletennis::DiscordPresence> discord_;

  // Keyboard: press Escape twice within 2 seconds to quit.
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    // Alt+Enter toggles fullscreen / windowed.
    if (e.virtual_key() == rex::ui::VirtualKey::kReturn && e.is_alt_pressed() && !e.prev_state()) {
      if (window()) {
        window()->SetFullscreen(!window()->IsFullscreen());
      }
      e.set_handled(true);
      return;
    }
    if (e.virtual_key() == rex::ui::VirtualKey::kEscape && !e.prev_state()) {
      auto now = std::chrono::steady_clock::now();
      if (now - last_escape_ <= std::chrono::seconds(2)) {
        e.set_handled(true);
        RequestQuit();
        return;
      }
      last_escape_ = now;
    }
    rex::ui::ProcessKeyEvent(e);
  }

  void RequestQuit() {
    if (quit_requested_.exchange(true)) {
      return;
    }
    app_context().CallInUIThread([this]() {
      if (window()) {
        window()->RequestClose();
      }
    });
  }

  std::atomic<bool> quit_requested_{false};
  std::atomic<bool> quit_watch_running_{false};
  std::thread quit_watch_thread_;
  std::chrono::steady_clock::time_point last_escape_{};
};
