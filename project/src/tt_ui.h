// Shared look & feel for the PC overlays (settings, achievements), mimicking
// the game's own front-end: blue halftone banner, heavy italic yellow title,
// uppercase white items, yellow selection, grey footer with button prompts.
// Also provides menu input from keyboard, mouse and Xbox controller.

#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#endif

namespace tabletennis::ui {

// Colors sampled from the game's main menu.
inline constexpr ImU32 kNavy = IM_COL32(29, 52, 82, 245);
inline constexpr ImU32 kNavyDots = IM_COL32(52, 86, 128, 255);
inline constexpr ImU32 kNavyEdge = IM_COL32(8, 16, 28, 255);
inline constexpr ImU32 kYellow = IM_COL32(247, 200, 22, 255);
inline constexpr ImU32 kBody = IM_COL32(10, 12, 16, 248);
inline constexpr ImU32 kRowHighlight = IM_COL32(255, 255, 255, 18);
inline constexpr ImU32 kText = IM_COL32(232, 232, 232, 255);
inline constexpr ImU32 kTextDim = IM_COL32(150, 150, 150, 255);
inline constexpr ImU32 kFooter = IM_COL32(205, 205, 205, 250);
inline constexpr ImU32 kBlack = IM_COL32(12, 12, 12, 255);
inline constexpr ImU32 kButtonA = IM_COL32(96, 176, 40, 255);
inline constexpr ImU32 kButtonB = IM_COL32(204, 40, 36, 255);

struct Fonts {
  ImFont* title = nullptr;  // heavy italic
  ImFont* item = nullptr;   // menu entries
  ImFont* body = nullptr;   // descriptions, footer
};

inline Fonts& GetFonts() {
  static Fonts fonts;
  return fonts;
}

// Called from ReXApp::OnConfigureFonts. Titles use a Pricedown-style font
// (fonts\pricedown.ttf next to the exe, if present), menus Century Gothic,
// with fallbacks shipped with Windows.
inline void LoadFonts(ImFontAtlas* atlas) {
  static const ImWchar ranges[] = {0x0020, 0x017F, 0};  // Latin, Latin-1, Latin Ext-A
  std::filesystem::path exe_fonts;
#if defined(_WIN32)
  wchar_t exe_path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
  exe_fonts = std::filesystem::path(exe_path).parent_path() / "fonts";
#endif
  const std::filesystem::path win_fonts = "C:\\Windows\\Fonts";
  auto load = [&](std::initializer_list<std::filesystem::path> paths, float size) -> ImFont* {
    for (const auto& path : paths) {
      if (std::filesystem::exists(path)) {
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        cfg.OversampleV = 1;
        return atlas->AddFontFromFileTTF(path.string().c_str(), size, &cfg, ranges);
      }
    }
    return nullptr;
  };
  Fonts& f = GetFonts();
  f.title = load({exe_fonts / "pricedown.ttf", win_fonts / "impact.ttf"},
                 50.0f);
  f.item = load({win_fonts / "GOTHICB.TTF", win_fonts / "arialbd.ttf"},
                27.0f);
  f.body = load({win_fonts / "GOTHIC.TTF", win_fonts / "arial.ttf"},
                19.0f);
}

// Panel tilt, like the game's menus. Drawing is rotated after the fact; mouse
// positions are rotated back for hit tests.
struct Tilt {
  ImVec2 center{0, 0};
  float angle = 0.0f;
};

inline Tilt& CurrentTilt() {
  static Tilt t;
  return t;
}

inline void RotateVertices(ImDrawList* dl, int start, const Tilt& t) {
  float c = std::cos(t.angle), s = std::sin(t.angle);
  for (int i = start; i < dl->VtxBuffer.Size; ++i) {
    ImVec2& p = dl->VtxBuffer[i].pos;
    float dx = p.x - t.center.x, dy = p.y - t.center.y;
    p = ImVec2(t.center.x + dx * c - dy * s, t.center.y + dx * s + dy * c);
  }
}

// Mouse position in the panel's unrotated space.
inline ImVec2 Mouse() {
  const Tilt& t = CurrentTilt();
  ImVec2 m = ImGui::GetIO().MousePos;
  float c = std::cos(-t.angle), s = std::sin(-t.angle);
  float dx = m.x - t.center.x, dy = m.y - t.center.y;
  return ImVec2(t.center.x + dx * c - dy * s, t.center.y + dx * s + dy * c);
}

inline bool Hover(ImVec2 a, ImVec2 b) {
  ImVec2 m = Mouse();
  return m.x >= a.x && m.x < b.x && m.y >= a.y && m.y < b.y;
}

inline std::string Upper(const std::string& utf8) {
  // Uppercases ASCII and the Latin-1 accented letters used by the menus.
  std::string out;
  for (size_t i = 0; i < utf8.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(utf8[i]);
    if (c >= 'a' && c <= 'z') {
      out += char(c - 32);
    } else if (c == 0xC3 && i + 1 < utf8.size()) {
      unsigned char d = static_cast<unsigned char>(utf8[i + 1]);
      // U+00E0..U+00FE (except U+00F7) -> U+00C0..U+00DE
      if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d = static_cast<unsigned char>(d - 0x20);
      out += char(c);
      out += char(d);
      ++i;
    } else {
      out += char(c);
    }
  }
  return out;
}

// Draws text with an italic shear (and optional outline) by skewing vertices.
inline void TextSkewed(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 color,
                       const char* text, float skew = 0.22f, ImU32 outline = 0,
                       float outline_px = 0.0f) {
  auto emit = [&](ImVec2 p, ImU32 col) {
    int start = dl->VtxBuffer.Size;
    dl->AddText(font, size, p, col, text);
    float base_y = p.y + size;
    for (int i = start; i < dl->VtxBuffer.Size; ++i) {
      dl->VtxBuffer[i].pos.x += (base_y - dl->VtxBuffer[i].pos.y) * skew;
    }
  };
  if (outline && outline_px > 0.0f) {
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        if (dx || dy) emit(ImVec2(pos.x + dx * outline_px, pos.y + dy * outline_px), outline);
      }
    }
  }
  emit(pos, color);
}

// Halftone dots fading from the right edge of a rectangle.
inline void Halftone(ImDrawList* dl, ImVec2 a, ImVec2 b, float spacing = 11.0f) {
  float width = b.x - a.x;
  for (float y = a.y + spacing * 0.5f; y < b.y; y += spacing) {
    int row = int((y - a.y) / spacing);
    float offset = (row % 2) ? spacing * 0.5f : 0.0f;
    for (float x = a.x + offset; x < b.x; x += spacing) {
      float t = (x - a.x) / width;  // 0 left .. 1 right
      float r = spacing * 0.42f * t;
      if (r > 0.6f) dl->AddCircleFilled(ImVec2(x, y), r, kNavyDots, 8);
    }
  }
}

// Round controller button glyph (A/B/X/Y) used in footers.
inline void ButtonGlyph(ImDrawList* dl, ImVec2 center, float radius, ImU32 color, const char* letter) {
  Fonts& f = GetFonts();
  dl->AddCircleFilled(center, radius, kBlack, 24);
  dl->AddCircleFilled(center, radius - 2.0f, color, 24);
  ImFont* font = f.body ? f.body : ImGui::GetFont();
  float size = radius * 1.5f;
  ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, letter);
  dl->AddText(font, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), kBlack, letter);
}

// Frame of a game-styled panel. Returns the inner content rectangle.
struct PanelLayout {
  ImVec2 body_min, body_max;  // list area
  ImVec2 footer_min, footer_max;
};

inline PanelLayout DrawPanel(ImDrawList* dl, ImVec2 origin, float width, float body_height,
                             const std::string& title) {
  Fonts& f = GetFonts();
  const float banner_h = 78.0f;
  const float footer_h = 52.0f;
  PanelLayout L;

  // Banner: navy band with rounded left end and halftone dots.
  ImVec2 b0(origin.x, origin.y), b1(origin.x + width + 60.0f, origin.y + banner_h);
  dl->AddRectFilled(ImVec2(b0.x - 4, b0.y - 4), ImVec2(b1.x + 4, b1.y + 4), kNavyEdge, 30.0f);
  dl->AddRectFilled(b0, b1, kNavy, 26.0f);
  Halftone(dl, ImVec2(b0.x + width * 0.45f, b0.y + 6), ImVec2(b1.x - 20, b1.y - 6));
  if (f.title) {
    TextSkewed(dl, f.title, 50.0f, ImVec2(b0.x + 28.0f, b0.y + 14.0f), kYellow,
               Upper(title).c_str(), 0.0f, kBlack, 2.0f);
  }

  // Body: dark translucent block below the banner.
  L.body_min = ImVec2(origin.x + 18.0f, b1.y);
  L.body_max = ImVec2(origin.x + width, b1.y + body_height);
  dl->AddRectFilled(L.body_min, L.body_max, kBody);

  // Footer: grey strip.
  L.footer_min = ImVec2(L.body_min.x, L.body_max.y);
  L.footer_max = ImVec2(L.body_max.x, L.body_max.y + footer_h);
  dl->AddRectFilled(L.footer_min, L.footer_max, kFooter);
  dl->AddRect(ImVec2(L.body_min.x - 1, L.body_min.y), ImVec2(L.footer_max.x + 1, L.footer_max.y + 1),
              kNavyEdge, 0.0f, 0, 2.0f);
  return L;
}

// Footer prompts, right-aligned: e.g. {{"A", "ACCEPTER"}, {"B", "RETOUR"}}.
struct Prompt {
  const char* button;
  std::string label;
};

inline void DrawFooter(ImDrawList* dl, const PanelLayout& L, std::initializer_list<Prompt> prompts) {
  Fonts& f = GetFonts();
  ImFont* font = f.item ? f.item : ImGui::GetFont();
  const float size = 24.0f;
  float x = L.footer_max.x - 16.0f;
  float cy = (L.footer_min.y + L.footer_max.y) * 0.5f;
  for (auto it = std::rbegin(prompts); it != std::rend(prompts); ++it) {
    const float r = 13.0f;
    ImU32 col = (it->button[0] == 'A')   ? kButtonA
                : (it->button[0] == 'B') ? kButtonB
                                         : IM_COL32(40, 110, 200, 255);  // X
    ButtonGlyph(dl, ImVec2(x - r, cy), r, col, it->button);
    x -= 2 * r + 8.0f;
    std::string label = Upper(it->label);
    ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label.c_str());
    TextSkewed(dl, font, size, ImVec2(x - ts.x, cy - ts.y * 0.5f), kBlack, label.c_str(), 0.0f);
    x -= ts.x + 26.0f;
  }
}

// Edge-triggered menu input from keyboard and every connected Xbox controller.
struct MenuInput {
  bool up = false, down = false, left = false, right = false, accept = false, back = false,
       extra = false;  // X button / Tab
};

class InputReader {
 public:
  MenuInput Poll() {
    MenuInput in;
    in.up = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    in.down = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    in.left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
    in.right = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
    in.accept = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
    in.back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false);
    in.extra = ImGui::IsKeyPressed(ImGuiKey_Tab, false);
#if defined(_WIN32)
    WORD buttons = 0;
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
      XINPUT_STATE s = {};
      if (XInputGetState(i, &s) != ERROR_SUCCESS) continue;
      buttons |= s.Gamepad.wButtons;
      const SHORT dead = 16000;
      if (s.Gamepad.sThumbLY > dead) buttons |= XINPUT_GAMEPAD_DPAD_UP;
      if (s.Gamepad.sThumbLY < -dead) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
      if (s.Gamepad.sThumbLX < -dead) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
      if (s.Gamepad.sThumbLX > dead) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    }
    WORD pressed = buttons & ~prev_;
    // Auto-repeat for held directions.
    const WORD dirs = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT |
                      XINPUT_GAMEPAD_DPAD_RIGHT;
    double now = ImGui::GetTime();
    if ((buttons & dirs) && (buttons & dirs) == (prev_ & dirs)) {
      if (now >= repeat_at_) {
        pressed |= buttons & dirs;
        repeat_at_ = now + 0.12;
      }
    } else if (pressed & dirs) {
      repeat_at_ = now + 0.40;
    }
    prev_ = buttons;
    if (!armed_) {
      // Ignore buttons already held when the menu opened (e.g. the open combo).
      armed_ = true;
      return in;
    }
    in.up |= (pressed & XINPUT_GAMEPAD_DPAD_UP) != 0;
    in.down |= (pressed & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    in.left |= (pressed & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    in.right |= (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    in.accept |= (pressed & XINPUT_GAMEPAD_A) != 0;
    in.back |= (pressed & XINPUT_GAMEPAD_B) != 0;
    in.extra |= (pressed & XINPUT_GAMEPAD_X) != 0;
#endif
    return in;
  }

 private:
  WORD prev_ = 0;
  double repeat_at_ = 0.0;
  bool armed_ = false;
};

// Invisible full-screen ImGui window to host custom drawing and mouse input.
inline ImDrawList* BeginCanvas(const char* id) {
  ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin(id, nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  ImGui::PopStyleVar(2);
  // Dim the game behind the menu.
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, 90));
  return dl;
}

inline void EndCanvas() { ImGui::End(); }

// UI scale relative to a 1080p layout.
inline float Scale() {
  ImGuiIO& io = ImGui::GetIO();
  return std::clamp(io.DisplaySize.y / 1080.0f, 0.6f, 2.0f);
}

}  // namespace tabletennis::ui
