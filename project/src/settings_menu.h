// In-game settings menu (F1 or View+LB), styled like the game's front-end and
// localized in the five languages shipped on the disc. Edits tabletennis.toml
// in place, keeping its comments.

#pragma once

#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "tt_ui.h"

REXCVAR_DECLARE(uint32_t, user_language);
REXCVAR_DECLARE(bool, fullscreen);
REXCVAR_DECLARE(bool, discord_enabled);

namespace tabletennis {

// Disc languages, in menu order, with their XLanguage ids.
inline constexpr std::array<uint32_t, 5> kMenuLanguages = {1, 4, 3, 5, 6};
inline constexpr std::array<const char*, 5> kLanguageNames = {"English", "Français", "Deutsch",
                                                                "Español", "Italiano"};

struct MenuText {
  const char* title;
  const char* language;
  const char* resolution;
  const char* aspect;
  const char* volume;
  const char* fullscreen;
  const char* textures;
  const char* textures_original;
  const char* discord;
  const char* yes;
  const char* no;
  const char* apply_restart;
  const char* restart_game;
  const char* quit_game;
  const char* restart_note;
  const char* accept;
  const char* back;
  const char* change;
  const char* help;
  // Achievements overlay.
  const char* achievements;
  const char* unlocked;
  const char* locked_hint;
  const char* secret;
  const char* show_secrets;
  const char* hide_secrets;
};

inline const MenuText& TextFor(uint32_t language) {
  static const MenuText en = {
      "Settings", "Language", "Resolution", "Aspect ratio", "Volume", "Fullscreen", "Textures", "Original", "Discord status",
      "Yes", "No", "Apply and restart", "Restart the game", "Quit the game", "Language and resolution apply after a restart.",
      "Accept", "Back", "Change",
      "F1 settings - F7 achievements - Alt+Enter window - Esc twice quit",
      "Achievements", "unlocked", "Locked", "online", "Show online achievements", "Hide online achievements"};
  static const MenuText fr = {
      "Réglages", "Langue", "Résolution", "Format d'image", "Volume", "Plein écran", "Textures", "D'origine", "Statut Discord",
      "Oui", "Non", "Appliquer et redémarrer", "Relancer le jeu", "Quitter le jeu",
      "La langue et la résolution s'appliquent après un redémarrage.", "Accepter", "Retour",
      "Modifier", "F1 réglages - F7 succès - Alt+Entrée fenêtre - Échap x2 quitter",
      "Succès", "débloqués", "Verrouillé", "en ligne", "Afficher les succès en ligne", "Masquer les succès en ligne"};
  static const MenuText de = {
      "Einstellungen", "Sprache", "Auflösung", "Seitenverhältnis", "Lautstärke", "Vollbild", "Texturen", "Original", "Discord-Status",
      "Ja", "Nein", "Übernehmen und neu starten", "Spiel neu starten", "Spiel beenden",
      "Sprache und Auflösung gelten nach einem Neustart.", "Annehmen", "Zurück", "Ändern",
      "F1 Einstellungen - F7 Erfolge - Alt+Enter Fenster - 2x Esc Beenden",
      "Erfolge", "freigeschaltet", "Gesperrt", "online", "Online-Erfolge zeigen", "Online-Erfolge verbergen"};
  static const MenuText es = {
      "Ajustes", "Idioma", "Resolución", "Formato de imagen", "Volumen", "Pantalla completa", "Texturas", "Original",
      "Estado de Discord", "Sí", "No", "Aplicar y reiniciar", "Reiniciar el juego", "Salir del juego",
      "El idioma y la resolución se aplican tras reiniciar.", "Aceptar", "Atrás", "Cambiar",
      "F1 ajustes - F7 logros - Alt+Intro ventana - Esc x2 salir",
      "Logros", "desbloqueados", "Bloqueado", "en línea", "Mostrar logros en línea", "Ocultar logros en línea"};
  static const MenuText it = {
      "Impostazioni", "Lingua", "Risoluzione", "Formato immagine", "Volume", "Schermo intero", "Texture", "Originale",
      "Stato Discord", "Sì", "No", "Applica e riavvia", "Riavvia il gioco", "Esci dal gioco",
      "Lingua e risoluzione si applicano dopo un riavvio.", "Accetta", "Indietro", "Cambia",
      "F1 impostazioni - F7 obiettivi - Alt+Invio finestra - Esc x2 esci",
      "Obiettivi", "sbloccati", "Bloccato", "online", "Mostra obiettivi online", "Nascondi obiettivi online"};
  switch (language) {
    case 4: return fr;
    case 3: return de;
    case 5: return es;
    case 6: return it;
    default: return en;
  }
}

inline const MenuText& CurrentText() { return TextFor(REXCVAR_GET(user_language)); }

// Sets `key = value` in a TOML file, replacing an existing line or appending.
inline void WriteTomlKey(const std::filesystem::path& path, const std::string& key,
                         const std::string& value) {
  std::vector<std::string> lines;
  {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  bool found = false;
  for (auto& line : lines) {
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos || line[start] == '#') continue;
    if (line.compare(start, key.size(), key) == 0) {
      size_t eq = line.find_first_not_of(" \t", start + key.size());
      if (eq != std::string::npos && line[eq] == '=') {
        line = key + " = " + value;
        found = true;
      }
    }
  }
  if (!found) lines.push_back(key + " = " + value);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  for (auto& line : lines) out << line << "\r\n";
}

class SettingsMenu : public rex::ui::ImGuiDialog {
 public:
  struct Callbacks {
    std::function<void(bool)> set_fullscreen;
    std::function<void(bool)> set_discord;
    std::function<void(int)> set_volume;
    std::function<void()> restart;
    std::function<void()> quit;
    std::function<void()> close;
  };

  SettingsMenu(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
               Callbacks callbacks)
      : ImGuiDialog(drawer), config_path_(std::move(config_path)), cb_(std::move(callbacks)) {
    for (size_t i = 0; i < kMenuLanguages.size(); ++i) {
      if (kMenuLanguages[i] == REXCVAR_GET(user_language)) language_ = int(i);
    }
    // GPU settings live in the GPU plugin DLL: read them by name.
    // Find the menu entry matching the saved video mode + render scale.
    const int scale = std::clamp(FlagInt("resolution_scale", 1), 1, 3);
    std::string mode = rex::cvar::GetFlagByName("resolution");
    if (mode.empty()) mode = "1280x720";
    resolution_ = kDefaultResolution;
    for (int i = 0; i < int(std::size(kResolutions)); ++i) {
      if (mode == kResolutions[i].mode && scale == kResolutions[i].scale) resolution_ = i;
    }
    volume_ = std::clamp(FlagInt("master_volume", 100), 0, 100);
    textures_ = FlagInt("anisotropic_override", 0) >= 5;
    fullscreen_ = REXCVAR_GET(fullscreen);
    discord_ = REXCVAR_GET(discord_enabled);
    initial_language_ = language_;
    initial_resolution_ = resolution_;
  }

 protected:
  enum Row {
    kLanguage, kAspect, kResolution, kFullscreen, kTextures, kVolume, kDiscord, kApply, kQuit,
    kRowCount
  };

  void OnDraw(ImGuiIO& io) override {
    namespace ui = tabletennis::ui;
    // The menu speaks the language being selected.
    const MenuText& t = TextFor(kMenuLanguages[language_]);
    const bool needs_restart = language_ != initial_language_ ||
                               resolution_ != initial_resolution_;
    const int rows = kRowCount;

    // --- Input ---
    ui::MenuInput in = input_.Poll();
    if (in.up) selected_ = (selected_ + rows - 1) % rows;
    if (in.down) selected_ = (selected_ + 1) % rows;
    selected_ = std::min(selected_, rows - 1);
    if (in.left) Change(selected_, -1);
    if (in.right) Change(selected_, +1);
    if (in.accept) Activate(selected_);
    if (in.back) {
      Save();
      if (cb_.close) cb_.close();
    }

    // --- Drawing ---
    ImDrawList* dl = ui::BeginCanvas("##tt_settings");
    ui::Fonts& f = ui::GetFonts();
    ImFont* item_font = f.item ? f.item : ImGui::GetFont();
    ImFont* body_font = f.body ? f.body : ImGui::GetFont();

    const float row_h = 44.0f;
    const float width = 640.0f;
    const float body_h = 24.0f + rows * row_h + 70.0f;
    const float total_h = 78.0f + body_h + 52.0f;
    ImVec2 origin(std::max(40.0f, io.DisplaySize.x - width - 140.0f),
                  std::max(16.0f, (io.DisplaySize.y - total_h) * 0.5f));
    ui::CurrentTilt() = {ImVec2(origin.x + width * 0.5f, origin.y + (78.0f + body_h) * 0.5f), 0.0f};  // straight panel
    const int first_vtx = dl->VtxBuffer.Size;
    ui::PanelLayout L = ui::DrawPanel(dl, origin, width, body_h, t.title);

    for (int r = 0; r < rows; ++r) {
      ImVec2 a(L.body_min.x, L.body_min.y + 16.0f + r * row_h);
      ImVec2 b(L.body_max.x, a.y + row_h);
      // Mouse: hover selects, click activates.
      if (ui::Hover(a, b)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) selected_ = r;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && r != kVolume) Activate(r);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) Change(r, -1);
      }
      const bool sel = r == selected_;
      if (sel) dl->AddRectFilled(a, b, ui::kRowHighlight);
      ImU32 col = sel ? ui::kYellow : ui::kText;

      std::string label, value;
      switch (r) {
        case kLanguage: label = t.language; value = kLanguageNames[language_]; break;
        case kAspect: label = t.aspect; value = kResolutions[resolution_].wide ? "16:9" : "4:3"; break;
        case kResolution: label = t.resolution; value = kResolutions[resolution_].label; break;
        case kVolume: label = t.volume; break;  // drawn as a slider below
        case kFullscreen: label = t.fullscreen; value = fullscreen_ ? t.yes : t.no; break;
        case kTextures: label = t.textures; value = textures_ ? "16x" : t.textures_original; break;
        case kDiscord: label = t.discord; value = discord_ ? t.yes : t.no; break;
        case kApply: label = needs_restart ? t.apply_restart : t.restart_game; break;
        case kQuit: label = t.quit_game; break;
      }
      float ty = a.y + (row_h - 27.0f) * 0.5f;
      ui::TextSkewed(dl, item_font, 27.0f, ImVec2(a.x + 26.0f, ty), col, ui::Upper(label).c_str(),
                     0.0f);
      if (r == kVolume) {
        DrawVolumeSlider(dl, item_font, a, b, ty, col, sel);
      }
      if (!value.empty()) {
        std::string v = sel ? "<  " + ui::Upper(value) + "  >" : ui::Upper(value);
        ImVec2 ts = item_font->CalcTextSizeA(27.0f, FLT_MAX, 0.0f, v.c_str());
        ui::TextSkewed(dl, item_font, 27.0f, ImVec2(b.x - 26.0f - ts.x, ty), col, v.c_str(), 0.0f);
      }
    }

    // Notes under the list.
    float note_y = L.body_min.y + 16.0f + rows * row_h + 10.0f;
    if (needs_restart) {
      dl->AddText(body_font, 19.0f, ImVec2(L.body_min.x + 26.0f, note_y), ui::kYellow, t.restart_note);
    }
    dl->AddText(body_font, 17.0f, ImVec2(L.body_min.x + 26.0f, note_y + 28.0f), ui::kTextDim, t.help);

    ui::DrawFooter(dl, L, {{"A", selected_ >= kApply ? t.accept : t.change}, {"B", t.back}});
    ui::RotateVertices(dl, first_vtx, ui::CurrentTilt());
    ui::EndCanvas();
  }

 private:
  // Volume slider: grey track, yellow fill, knob, percentage on the right.
  // Mouse: click or drag on the track.
  void DrawVolumeSlider(ImDrawList* dl, ImFont* font, ImVec2 row_a, ImVec2 row_b, float text_y,
                        ImU32 text_col, bool selected) {
    namespace ui = tabletennis::ui;
    const float pct_w = 78.0f;
    const float x1 = row_b.x - 26.0f - pct_w;
    const float x0 = x1 - 230.0f;
    const float cy = (row_a.y + row_b.y) * 0.5f;
    const float h = 8.0f;

    // Mouse interaction.
    ImVec2 hit_a(x0 - 8.0f, row_a.y), hit_b(x1 + 8.0f, row_b.y);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ui::Hover(hit_a, hit_b)) dragging_ = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_ = false;
    if (dragging_) {
      float t = std::clamp((ui::Mouse().x - x0) / (x1 - x0), 0.0f, 1.0f);
      int v = int(std::lround(t * 100.0f));
      if (v != volume_) {
        volume_ = v;
        if (cb_.set_volume) cb_.set_volume(volume_);
      }
    }

    const float fill = x0 + (x1 - x0) * (volume_ / 100.0f);
    dl->AddRectFilled(ImVec2(x0, cy - h * 0.5f), ImVec2(x1, cy + h * 0.5f),
                      IM_COL32(70, 70, 70, 255), h * 0.5f);
    dl->AddRectFilled(ImVec2(x0, cy - h * 0.5f), ImVec2(fill, cy + h * 0.5f), ui::kYellow, h * 0.5f);
    const float knob = selected || dragging_ ? 11.0f : 9.0f;
    dl->AddCircleFilled(ImVec2(fill, cy), knob + 2.0f, ui::kBlack, 20);
    dl->AddCircleFilled(ImVec2(fill, cy), knob, selected ? ui::kYellow : ui::kText, 20);

    std::string pct = std::to_string(volume_) + " %";
    ImVec2 ts = font->CalcTextSizeA(27.0f, FLT_MAX, 0.0f, pct.c_str());
    ui::TextSkewed(dl, font, 27.0f, ImVec2(row_b.x - 26.0f - ts.x, text_y), text_col, pct.c_str(),
                   0.0f);
  }

  // Final picture sizes. Each one is a console video mode (what the game sees
  // as its TV) combined with an internal rendering multiplier.
  struct Resolution {
    bool wide;          // 16:9 or 4:3
    const char* label;  // shown in the menu
    const char* mode;   // `resolution` setting (console video mode)
    int scale;          // `resolution_scale` setting
  };
  static constexpr Resolution kResolutions[] = {
      {true, "480p (848×480)", "848x480", 1},       {true, "720p (1280×720)", "1280x720", 1},
      {true, "1080p (1920×1080)", "1920x1080", 1},  {true, "1440p (2560×1440)", "1280x720", 2},
      {true, "4K (3840×2160)", "1280x720", 3},      {false, "480p (640×480)", "640x480", 1},
      {false, "768p (1024×768)", "1024x768", 1},    {false, "1536p (2048×1536)", "1024x768", 2},
  };
  static constexpr int kDefaultResolution = 1;

  // Next/previous resolution with the same aspect ratio.
  int StepResolution(int from, int dir) const {
    const int n = int(std::size(kResolutions));
    int i = from;
    do {
      i = (i + dir + n) % n;
    } while (kResolutions[i].wide != kResolutions[from].wide);
    return i;
  }

  static int FlagInt(const char* name, int fallback) {
    std::string v = rex::cvar::GetFlagByName(name);
    try {
      return v.empty() ? fallback : std::stoi(v);
    } catch (...) {
      return fallback;
    }
  }

  void Change(int row, int dir) {
    switch (row) {
      case kLanguage:
        language_ = (language_ + dir + int(kMenuLanguages.size())) % int(kMenuLanguages.size());
        break;
      case kAspect: {
        // Switch aspect, keeping a comparable size (720p <-> 768p, 480p <-> 480p...).
        const bool wide = !kResolutions[resolution_].wide;
        const int target = wide ? 1 : 6;
        resolution_ = target;
        if (kResolutions[initial_resolution_].wide == wide) resolution_ = initial_resolution_;
        break;
      }
      case kResolution: resolution_ = StepResolution(resolution_, dir); break;
      case kVolume:
        volume_ = std::clamp((volume_ / 5 + dir) * 5, 0, 100);
        if (cb_.set_volume) cb_.set_volume(volume_);
        break;
      case kFullscreen:
        fullscreen_ = !fullscreen_;
        if (cb_.set_fullscreen) cb_.set_fullscreen(fullscreen_);
        break;
      case kTextures:
        textures_ = !textures_;
        rex::cvar::SetFlagByName("anisotropic_override", textures_ ? "5" : "0");
        break;
      case kDiscord:
        discord_ = !discord_;
        if (cb_.set_discord) cb_.set_discord(discord_);
        break;
      default: break;
    }
  }

  void Activate(int row) {
    if (row == kApply) {
      Save();
      if (cb_.restart) cb_.restart();
    } else if (row == kQuit) {
      Save();
      if (cb_.quit) cb_.quit();
    } else if (row < kApply) {
      Change(row, +1);
    }
  }

  void Save() {
    WriteTomlKey(config_path_, "user_language", std::to_string(kMenuLanguages[language_]));
    WriteTomlKey(config_path_, "resolution_scale", std::to_string(kResolutions[resolution_].scale));
    WriteTomlKey(config_path_, "resolution",
                 std::string("\"") + kResolutions[resolution_].mode + "\"");
    WriteTomlKey(config_path_, "master_volume", std::to_string(volume_));
    WriteTomlKey(config_path_, "fullscreen", fullscreen_ ? "true" : "false");
    WriteTomlKey(config_path_, "anisotropic_override", textures_ ? "5" : "0");
    WriteTomlKey(config_path_, "discord_enabled", discord_ ? "true" : "false");
  }

  std::filesystem::path config_path_;
  Callbacks cb_;
  ui::InputReader input_;
  int selected_ = 0;
  int language_ = 0;
  int resolution_ = kDefaultResolution;
  int volume_ = 100;
  bool dragging_ = false;
  bool textures_ = true;
  bool fullscreen_ = true;
  bool discord_ = true;
  int initial_language_ = 0;
  int initial_resolution_ = kDefaultResolution;
};

}  // namespace tabletennis
