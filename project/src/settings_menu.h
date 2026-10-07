// In-game settings menu (F1), localized in the five languages shipped on the
// disc. Edits tabletennis.toml in place, keeping its comments.

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
#include <sstream>
#include <string>
#include <vector>

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
  const char* restart_note;
  const char* save;
  const char* save_restart;
  const char* close;
  const char* saved;
  const char* help;
};

inline const MenuText& TextFor(uint32_t language) {
  static const MenuText en = {
      "Settings", "Language", "Resolution", "Fullscreen", "Texture filtering", "Original",
      "Discord status", "Language and resolution apply after a restart.", "Save",
      "Save and restart", "Close", "Saved.",
      "F1: settings  -  F7: achievements  -  Alt+Enter: window  -  Esc twice: quit"};
  static const MenuText fr = {
      "Réglages", "Langue", "Résolution", "Plein écran", "Filtrage des textures", "D'origine",
      "Statut Discord", "La langue et la résolution s'appliquent après un redémarrage.",
      "Enregistrer", "Enregistrer et redémarrer", "Fermer", "Enregistré.",
      "F1 : réglages  -  F7 : succès  -  Alt+Entrée : fenêtre  -  Échap deux fois : quitter"};
  static const MenuText de = {
      "Einstellungen", "Sprache", "Auflösung", "Vollbild", "Texturfilterung", "Original",
      "Discord-Status", "Sprache und Auflösung gelten nach einem Neustart.", "Speichern",
      "Speichern und neu starten", "Schließen", "Gespeichert.",
      "F1: Einstellungen  -  F7: Erfolge  -  Alt+Enter: Fenster  -  2x Esc: Beenden"};
  static const MenuText es = {
      "Ajustes", "Idioma", "Resolución", "Pantalla completa", "Filtrado de texturas", "Original",
      "Estado de Discord", "El idioma y la resolución se aplican tras reiniciar.", "Guardar",
      "Guardar y reiniciar", "Cerrar", "Guardado.",
      "F1: ajustes  -  F7: logros  -  Alt+Intro: ventana  -  Esc dos veces: salir"};
  static const MenuText it = {
      "Impostazioni", "Lingua", "Risoluzione", "Schermo intero", "Filtro texture", "Originale",
      "Stato Discord", "Lingua e risoluzione si applicano dopo un riavvio.", "Salva",
      "Salva e riavvia", "Chiudi", "Salvato.",
      "F1: impostazioni  -  F7: obiettivi  -  Alt+Invio: finestra  -  Esc due volte: esci"};
  switch (language) {
    case 4: return fr;
    case 3: return de;
    case 5: return es;
    case 6: return it;
    default: return en;
  }
}

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
      size_t after = start + key.size();
      size_t eq = line.find_first_not_of(" \t", after);
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
      if (kMenuLanguages[i] == REXCVAR_GET(user_language)) language_index_ = int(i);
    }
    // GPU settings live in the GPU plugin DLL: read them by name.
    resolution_ = std::clamp(FlagInt("resolution_scale", 1), 1, 3) - 1;
    textures_ = FlagInt("anisotropic_override", 0) >= 5 ? 1 : 0;
    fullscreen_ = REXCVAR_GET(fullscreen);
    discord_ = REXCVAR_GET(discord_enabled);
    initial_language_ = language_index_;
    initial_resolution_ = resolution_;
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    const MenuText& t = TextFor(kMenuLanguages[language_index_]);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    bool open = true;
    std::string window_title = std::string(t.title) + "###tt_settings";
    if (!ImGui::Begin(window_title.c_str(), &open,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::End();
      return;
    }

    ImGui::Combo(t.language, &language_index_, kLanguageNames.data(), int(kLanguageNames.size()));

    const char* resolutions[] = {"720p (1280x720)", "1440p (2560x1440)", "4K (3840x2160)"};
    ImGui::Combo(t.resolution, &resolution_, resolutions, 3);

    if (ImGui::Checkbox(t.fullscreen, &fullscreen_) && cb_.set_fullscreen) {
      cb_.set_fullscreen(fullscreen_);
    }

    const char* textures[] = {t.textures_original, "16x"};
    if (ImGui::Combo(t.textures, &textures_, textures, 2)) {
      rex::cvar::SetFlagByName("anisotropic_override", textures_ ? "5" : "0");
    }

    if (ImGui::Checkbox(t.discord, &discord_) && cb_.set_discord) {
      cb_.set_discord(discord_);
    }

    ImGui::Separator();
    bool needs_restart = language_index_ != initial_language_ || resolution_ != initial_resolution_;
    if (needs_restart) {
      ImGui::TextWrapped("%s", t.restart_note);
    }
    if (ImGui::Button(t.save)) {
      Save();
      saved_ = true;
    }
    if (needs_restart) {
      ImGui::SameLine();
      if (ImGui::Button(t.save_restart)) {
        Save();
        if (cb_.restart) cb_.restart();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(t.close)) open = false;
    if (saved_) {
      ImGui::SameLine();
      ImGui::TextUnformatted(t.saved);
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s", t.help);
    ImGui::End();

    if (!open && cb_.close) cb_.close();
  }

 private:
  static int FlagInt(const char* name, int fallback) {
    std::string v = rex::cvar::GetFlagByName(name);
    try {
      return v.empty() ? fallback : std::stoi(v);
    } catch (...) {
      return fallback;
    }
  }

  void Save() {
    WriteTomlKey(config_path_, "user_language", std::to_string(kMenuLanguages[language_index_]));
    WriteTomlKey(config_path_, "resolution_scale", std::to_string(resolution_ + 1));
    WriteTomlKey(config_path_, "fullscreen", fullscreen_ ? "true" : "false");
    WriteTomlKey(config_path_, "anisotropic_override", textures_ ? "5" : "0");
    WriteTomlKey(config_path_, "discord_enabled", discord_ ? "true" : "false");
  }

  std::filesystem::path config_path_;
  Callbacks cb_;
  int language_index_ = 0;
  int resolution_ = 1;
  int textures_ = 1;
  bool fullscreen_ = true;
  bool discord_ = true;
  bool saved_ = false;
  int initial_language_ = 0;
  int initial_resolution_ = 1;
};

}  // namespace tabletennis
