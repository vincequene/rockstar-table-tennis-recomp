// Achievements overlay (F7 or View+RB) styled like the game's front-end.

#pragma once

#include <rex/system/achievement_manager.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/overlay/achievement_icon_cache.h>

#include <imgui.h>

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

#include "settings_menu.h"
#include "tt_ui.h"

namespace tabletennis {

// Rockstar Table Tennis achievements that require Xbox Live (online / ranked).
inline constexpr std::array<uint32_t, 10> kOnlineAchievements = {24, 25, 26, 27, 28,
                                                                  29, 31, 32, 33, 34};

class AchievementsMenu : public rex::ui::ImGuiDialog {
 public:
  AchievementsMenu(rex::ui::ImGuiDrawer* drawer, rex::ui::ImmediateDrawer* immediate_drawer,
                   rex::Runtime* runtime, rex::system::AchievementManager* achievements,
                   std::function<void()> close)
      : ImGuiDialog(drawer),
        achievements_(achievements),
        icons_(immediate_drawer, runtime),
        close_(std::move(close)) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    const MenuText& t = CurrentText();
    // Achievements that can no longer be earned (they need the defunct Xbox
    // Live service) are hidden unless unlocked or revealed with X.
    const auto all = achievements_->ListAchievements();
    std::vector<rex::system::AchievementInfo> list;
    int hidden = 0;
    for (const auto& a : all) {
      const bool online = std::find(kOnlineAchievements.begin(), kOnlineAchievements.end(),
                                    a.id) != kOnlineAchievements.end();
      if (online && !achievements_->IsUnlocked(a.id) && !show_secrets_) {
        ++hidden;
        continue;
      }
      list.push_back(a);
    }
    const int count = int(list.size());

    ui::MenuInput in = input_.Poll();
    if (in.extra) show_secrets_ = !show_secrets_;
    if (count) {
      if (in.up) selected_ = (selected_ + count - 1) % count;
      if (in.down) selected_ = (selected_ + 1) % count;
      selected_ = std::clamp(selected_, 0, count - 1);
    }
    if (in.back || in.accept) {
      if (close_) close_();
    }

    int unlocked = 0, earned = 0, total = 0;
    for (const auto& a : all) {
      total += int(a.gamerscore);
      if (achievements_->IsUnlocked(a.id)) {
        ++unlocked;
        earned += int(a.gamerscore);
      }
    }

    ImDrawList* dl = ui::BeginCanvas("##tt_achievements");
    ui::Fonts& f = ui::GetFonts();
    ImFont* item_font = f.item ? f.item : ImGui::GetFont();
    ImFont* body_font = f.body ? f.body : ImGui::GetFont();

    const float width = 760.0f;
    const float row_h = 84.0f;
    const float header_h = 56.0f;
    const int visible = std::max(3, int((io.DisplaySize.y * 0.62f - header_h) / row_h));
    const float body_h = header_h + visible * row_h + 12.0f;
    ImVec2 origin(std::max(40.0f, io.DisplaySize.x - width - 140.0f),
                  std::max(30.0f, io.DisplaySize.y * 0.10f));
    ui::CurrentTilt() = {ImVec2(origin.x + width * 0.5f, origin.y + (78.0f + body_h) * 0.5f), 0.0f};  // straight panel
    const int first_vtx = dl->VtxBuffer.Size;
    ui::PanelLayout L = ui::DrawPanel(dl, origin, width, body_h, t.achievements);

    // Summary + progress bar.
    char summary[96];
    std::snprintf(summary, sizeof(summary), "%d / %d %s   -   %dG / %dG", unlocked, int(all.size()),
                  ui::Upper(t.unlocked).c_str(), earned, total);
    ui::TextSkewed(dl, item_font, 25.0f, ImVec2(L.body_min.x + 24.0f, L.body_min.y + 10.0f),
                   ui::kText, summary, 0.0f);
    float bar_x0 = L.body_min.x + 24.0f, bar_x1 = L.body_max.x - 24.0f;
    float bar_y = L.body_min.y + 42.0f;
    dl->AddRectFilled(ImVec2(bar_x0, bar_y), ImVec2(bar_x1, bar_y + 6.0f), IM_COL32(60, 60, 60, 255));
    if (count) {
      dl->AddRectFilled(ImVec2(bar_x0, bar_y),
                        ImVec2(bar_x0 + (bar_x1 - bar_x0) * unlocked / int(all.size()), bar_y + 6.0f),
                        ui::kYellow);
    }

    // Mouse wheel scrolls the selection.
    ImVec2 list_min(L.body_min.x, L.body_min.y + header_h);
    ImVec2 list_max(L.body_max.x, list_min.y + visible * row_h);
    if (ui::Hover(list_min, list_max) && io.MouseWheel != 0.0f && count) {
      selected_ = std::clamp(selected_ - int(io.MouseWheel), 0, count - 1);
    }
    if (selected_ < first_) first_ = selected_;
    if (selected_ >= first_ + visible) first_ = selected_ - visible + 1;
    first_ = std::clamp(first_, 0, std::max(0, count - visible));

    for (int i = first_; i < std::min(count, first_ + visible); ++i) {
      const auto& a = list[i];
      const bool done = achievements_->IsUnlocked(a.id);
      ImVec2 ra(list_min.x, list_min.y + (i - first_) * row_h);
      ImVec2 rb(list_max.x, ra.y + row_h);
      if (ui::Hover(ra, rb) && (io.MouseDelta.x || io.MouseDelta.y)) selected_ = i;
      if (i == selected_) dl->AddRectFilled(ra, rb, ui::kRowHighlight);

      // Icon (dimmed when locked).
      const float icon = 64.0f;
      ImVec2 ia(ra.x + 18.0f, ra.y + (row_h - icon) * 0.5f);
      if (auto* tex = icons_.GetIcon(a)) {
        dl->AddImage(reinterpret_cast<ImTextureID>(tex), ia, ImVec2(ia.x + icon, ia.y + icon),
                     ImVec2(0, 0), ImVec2(1, 1),
                     done ? IM_COL32_WHITE : IM_COL32(110, 110, 110, 200));
      }
      dl->AddRect(ia, ImVec2(ia.x + icon, ia.y + icon), ui::kNavyEdge, 0.0f, 0, 2.0f);

      ImU32 title_col = done ? ui::kYellow : (i == selected_ ? ui::kText : ui::kTextDim);
      float tx = ia.x + icon + 18.0f;
      ui::TextSkewed(dl, item_font, 25.0f, ImVec2(tx, ra.y + 12.0f), title_col,
                     ui::Upper(a.label).c_str(), 0.0f);
      std::string gs = std::to_string(a.gamerscore) + "G";
      ImVec2 gts = item_font->CalcTextSizeA(25.0f, FLT_MAX, 0.0f, gs.c_str());
      ui::TextSkewed(dl, item_font, 25.0f, ImVec2(rb.x - 24.0f - gts.x, ra.y + 12.0f), title_col,
                     gs.c_str(), 0.0f);
      const std::string& desc = done ? a.description : a.unachieved_description;
      dl->AddText(body_font, 18.0f, ImVec2(tx, ra.y + 46.0f), done ? ui::kText : ui::kTextDim,
                  desc.empty() ? t.locked_hint : desc.c_str(), nullptr, rb.x - 24.0f - tx);
    }

    // X shows / hides the online-only achievements.
    if (hidden || show_secrets_) {
      ui::DrawFooter(dl, L, {{"X", show_secrets_ ? t.hide_secrets : t.show_secrets}, {"B", t.back}});
    } else {
      ui::DrawFooter(dl, L, {{"B", t.back}});
    }
    ui::RotateVertices(dl, first_vtx, ui::CurrentTilt());
    ui::EndCanvas();
  }

 private:
  rex::system::AchievementManager* achievements_;
  rex::ui::AchievementIconCache icons_;
  std::function<void()> close_;
  ui::InputReader input_;
  int selected_ = 0;
  int first_ = 0;
  bool show_secrets_ = false;
};

}  // namespace tabletennis
