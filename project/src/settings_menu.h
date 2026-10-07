// In-game settings menu (F1 or View+LB), styled like the game's front-end and
// localized in the five languages shipped on the disc. Edits tabletennis.toml
// in place, keeping its comments.

#pragma once

#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>

#include <imgui.h>

#include <algorithm>
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
  const char* fullscreen;
  const char* textures;
  const char* textures_original;
  const char* discord;
  const char* yes;
  const char* no;
  const char* apply_restart;
  const char* restart_game;
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
      "Settings", "Language", "Resolution", "Fullscreen", "Textures", "Original", "Discord status",
      "Yes", "No", "Apply and restart", "Restart the game", "Language and resolution apply after a restart.",
      "Accept", "Back", "Change",
      "F1 settings - F7 achievements - Alt+Enter window - Esc twice quit",
      "Achievements", "unlocked", "Locked", "secret", "Show secrets", "Hide secrets"};
  static const MenuText fr = {
      "Réglages", "Langue", "Résolution", "Plein écran", "Textures", "D'origine", "Statut Discord",
      "Oui", "Non", "Appliquer et redémarrer", "Relancer le jeu",
      "La langue et la résolution s'appliquent après un redémarrage.", "Accepter", "Retour",
      "Modifier", "F1 réglages - F7 succès - Alt+Entrée fenêtre - Échap x2 quitter",
      "Succès", "débloqués", "Verrouillé", "secrets", "Voir les secrets", "Masquer les secrets"};
  static const MenuText de = {
      "Einstellungen", "Sprache", "Auflösung", "Vollbild", "Texturen", "Original", "Discord-Status",
      "Ja", "Nein", "Übernehmen und neu starten", "Spiel neu starten",
      "Sprache und Auflösung gelten nach einem Neustart.", "Annehmen", "Zurück", "Ändern",
      "F1 Einstellungen - F7 Erfolge - Alt+Enter Fenster - 2x Esc Beenden",
      "Erfolge", "freigeschaltet", "Gesperrt", "geheim", "Geheime zeigen", "Geheime verbergen"};
  static const MenuText es = {
      "Ajustes", "Idioma", "Resolución", "Pantalla completa", "Texturas", "Original",
      "Estado de Discord", "Sí", "No", "Aplicar y reiniciar", "Reiniciar el juego",
      "El idioma y la resolución se aplican tras reiniciar.", "Aceptar", "Atrás", "Cambiar",
      "F1 ajustes - F7 logros - Alt+Intro ventana - Esc x2 salir",
      "Logros", "desbloqueados", "Bloqueado", "secretos", "Ver secretos", "Ocultar secretos"};
  static const MenuText it = {
      "Impostazioni", "Lingua", "Risoluzione", "Schermo intero", "Texture", "Originale",
      "Stato Discord", "Sì", "No", "Applica e riavvia", "Riavvia il gioco",
      "Lingua e risoluzione si applicano dopo un riavvio.", "Accetta", "Indietro", "Cambia",
      "F1 impostazioni - F7 obiettivi - Alt+Invio finestra - Esc x2 esci",
      "Obiettivi", "sbloccati", "Bloccato", "segreti", "Mostra segreti", "Nascondi segreti"};
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
    std::function<void()> restart;
    std::function<void()> close;
  };

  SettingsMenu(rex::ui::ImGuiDrawer* drawer, std::filesystem::path config_path,
               Callbacks callbacks)
      : ImGuiDialog(drawer), config_path_(std::move(config_path)), cb_(std::move(callbacks)) {
    for (size_t i = 0; i < kMenuLanguages.size(); ++i) {
      if (kMenuLanguages[i] == REXCVAR_GET(user_language)) language_ = int(i);
    }
    // GPU settings live in the GPU plugin DLL: read them by name.
    resolution_ = std::clamp(FlagInt("resolution_scale", 1), 1, 3) - 1;
    textures_ = FlagInt("anisotropic_override", 0) >= 5;
    fullscreen_ = REXCVAR_GET(fullscreen);
    discord_ = REXCVAR_GET(discord_enabled);
    initial_language_ = language_;
    initial_resolution_ = resolution_;
  }

 protected:
  enum Row { kLanguage, kResolution, kFullscreen, kTextures, kDiscord, kApply, kRowCount };

  void OnDraw(ImGuiIO& io) override {
    namespace ui = tabletennis::ui;
    // The menu speaks the language being selected.
    const MenuText& t = TextFor(kMenuLanguages[language_]);
    const bool needs_restart = language_ != initial_language_ || resolution_ != initial_resolution_;
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

    const float row_h = 48.0f;
    const float width = 640.0f;
    const float body_h = 24.0f + rows * row_h + 70.0f;
    ImVec2 origin(std::max(40.0f, io.DisplaySize.x - width - 140.0f),
                  std::max(30.0f, io.DisplaySize.y * 0.14f));
    ui::CurrentTilt() = {ImVec2(origin.x + width * 0.5f, origin.y + (78.0f + body_h) * 0.5f), 0.0f};  // straight panel
    const int first_vtx = dl->VtxBuffer.Size;
    ui::PanelLayout L = ui::DrawPanel(dl, origin, width, body_h, t.title);

    for (int r = 0; r < rows; ++r) {
      ImVec2 a(L.body_min.x, L.body_min.y + 16.0f + r * row_h);
      ImVec2 b(L.body_max.x, a.y + row_h);
      // Mouse: hover selects, click activates.
      if (ui::Hover(a, b)) {
        if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) selected_ = r;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) Activate(r);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) Change(r, -1);
      }
      const bool sel = r == selected_;
      if (sel) dl->AddRectFilled(a, b, ui::kRowHighlight);
      ImU32 col = sel ? ui::kYellow : ui::kText;

      std::string label, value;
      switch (r) {
        case kLanguage: label = t.language; value = kLanguageNames[language_]; break;
        case kResolution: label = t.resolution; value = kResolutionNames[resolution_]; break;
        case kFullscreen: label = t.fullscreen; value = fullscreen_ ? t.yes : t.no; break;
        case kTextures: label = t.textures; value = textures_ ? "16x" : t.textures_original; break;
        case kDiscord: label = t.discord; value = discord_ ? t.yes : t.no; break;
        case kApply: label = needs_restart ? t.apply_restart : t.restart_game; break;
      }
      float ty = a.y + (row_h - 27.0f) * 0.5f;
      ui::TextSkewed(dl, item_font, 27.0f, ImVec2(a.x + 26.0f, ty), col, ui::Upper(label).c_str(),
                     0.0f);
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

    ui::DrawFooter(dl, L, {{"A", selected_ == kApply ? t.accept : t.change}, {"B", t.back}});
    ui::RotateVertices(dl, first_vtx, ui::CurrentTilt());
    ui::EndCanvas();
  }

 private:
  static constexpr const char* kResolutionNames[3] = {"720p", "1440p", "4K"};

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
      case kResolution: resolution_ = (resolution_ + dir + 3) % 3; break;
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
    } else {
      Change(row, +1);
    }
  }

  void Save() {
    WriteTomlKey(config_path_, "user_language", std::to_string(kMenuLanguages[language_]));
    WriteTomlKey(config_path_, "resolution_scale", std::to_string(resolution_ + 1));
    WriteTomlKey(config_path_, "fullscreen", fullscreen_ ? "true" : "false");
    WriteTomlKey(config_path_, "anisotropic_override", textures_ ? "5" : "0");
    WriteTomlKey(config_path_, "discord_enabled", discord_ ? "true" : "false");
  }

  std::filesystem::path config_path_;
  Callbacks cb_;
  ui::InputReader input_;
  int selected_ = 0;
  int language_ = 0;
  int resolution_ = 1;
  bool textures_ = true;
  bool fullscreen_ = true;
  bool discord_ = true;
  int initial_language_ = 0;
  int initial_resolution_ = 1;
};

}  // namespace tabletennis
