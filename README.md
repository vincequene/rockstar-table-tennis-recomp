# Rockstar Table Tennis — PC port (static recompilation)

Unofficial PC port of **Rockstar Table Tennis** (Xbox 360, 2006, title ID `545407DF`),
built by static recompilation with the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

> **This repository contains no game files**, no recompiled code and no executable.
> You need your own legally obtained copy of the game (Xbox 360 ISO). The build script
> recreates everything on your machine from your ISO, which is never modified.

*Version française plus bas.*

## Features

- Full game, playable with an Xbox controller (wired or Bluetooth) or the keyboard
  (arrows = D-pad, **Enter** = Start, **Space** = A, **Backspace** = B, **Tab** = Back)
- In-game settings menu (**F1** or **View + LB**) styled like the game, in the 5 disc languages:
  language, resolution, fullscreen, texture filtering, Discord
- Any language shipped on the disc: English, French, German, Spanish, Italian
- Rendering up to 4K (the original game runs at 720p), 16x texture filtering
- Fullscreen / windowed (**Alt+Enter**)
- Achievements (29) with an in-game list (**F7** or **View + RB**) and pop-ups; the 10 online-only
  achievements (no longer obtainable) are hidden, **X** shows them
- **Discord** Rich Presence driven by the original Xbox Live presence
  ("Offline Exhib 2-1 — Kumi vs Jesper")
- Working local network mode (System Link)
- Quit: **Esc** twice, or hold **View + Menu** for 2 seconds on the controller
- Saves in `Documents\Rockstar Table Tennis`
- The game's own icon, extracted from your copy during the build

## Install with Claude (easiest)

If you use [Claude Code](https://claude.com/claude-code) (desktop app or terminal), it can do
everything for you: download this repository, install the build tools, build the game and fix
problems along the way.

1. Open Claude Code in an empty folder where you want the game (for example `C:\Games`).
2. Paste this prompt, after replacing the line `ISO:` with the full path of **your** ISO
   (it can be anywhere on your PC; tip: in File Explorer, Shift + right-click the ISO →
   "Copy as path"):

```text
I want to build the PC version of Rockstar Table Tennis (Xbox 360 recompilation).
Repository: https://github.com/vincequene/rockstar-table-tennis-recomp
ISO: D:\My Games\Rockstar Table Tennis.iso

My ISO is my own legal copy: never modify, move or delete it.
1. Download the repository into the current folder (git clone, or the ZIP if Git is missing).
2. Read its README.md, then run Build.bat with my ISO path
   (powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Iso "<my ISO path>").
3. Install any missing build tools, but ask me before each download.
4. If a step fails, find the cause and fix it.
5. Explain each step to me in one simple sentence (I am not a programmer).
6. When it is done, launch dist\Rockstar Table Tennis\Rockstar Table Tennis.exe
   and tell me what to test.
```

## Install manually

Requirements: 64-bit Windows 10/11, about 25 GB free, and your Rockstar Table Tennis ISO.

1. Download this repository (**Code → Download ZIP**) and extract it.
2. Double-click **`Build.bat`** and pick your ISO.
3. If tools are missing (Git, CMake, Ninja, LLVM, Visual Studio Build Tools),
   the script offers to install them with `winget`.
4. Wait (20 to 60 minutes the first time).
5. The game is in **`dist\Rockstar Table Tennis\`**: run `Rockstar Table Tennis.exe`.

## Settings

Press **F1** in game, or edit `tabletennis.toml` next to the executable:

| Setting | Values |
|---|---|
| `user_language` | 1 English, 3 German, 4 French, 5 Spanish, 6 Italian |
| `resolution_scale` | 1 = 720p, 2 = 1440p, 3 = 4K |
| `fullscreen` | `true` / `false` |
| `anisotropic_override` | 0 = original … 5 = 16x |
| `discord_enabled` | `true` / `false` |
| `mnk_mode` | keyboard as a controller: `true` / `false` (keys: `keybind_*`) |

**Discord**: works for everyone with nothing to set up. Just keep the Discord app open on your
PC and your profile shows "Playing Rockstar Table Tennis" with your match status.

## Customization

- **Icon**: extracted automatically from your game. To use another one, put a
  `tabletennis.ico` at the root of this folder before building.
- **Title font**: menus use Century Gothic (shipped with Windows/Office). The game's titles use
  Pricedown, which is not included. Download "Pricedown Bl" yourself (e.g. from DaFont, check its
  license) and put the `.otf`/`.ttf` file in a `fonts` folder at the root of this repository
  before building, or in `dist\Rockstar Table Tennisonts`. Otherwise Impact is used.

## Known limitations

- The game stays at 60 FPS: above that, its game logic (RAGE engine) breaks.
- The original Xbox Live service no longer exists: the Xbox Live menu entry shows the game's
  "you must be signed in" message. The game has no System Link: for remote multiplayer, use
  local two-controller play over Parsec.

## How it works

- `project/`: ReXGlue project (manifest, function fixes in `tabletennis_fixes.toml`,
  application code: paths, menus, controller shortcuts, Discord, icon).
- `patches/rexglue-sdk.patch`: fixes applied to the SDK (commit `c94f5eb`):
  - detects small virtual-call thunks (`mtctr`/`bctr`) missed by the analysis;
  - keeps floating-point exceptions masked (crash `0xC000008F`);
  - small positive socket handles like the console (System Link crashed after the intro);
  - `XGetLanguage` honours the configured language;
  - the game's own achievements screen request opens the PC achievements screen;
  - the Xbox Live sign-in screen behaves like "opened then cancelled", so the game shows its
    "sign in to Xbox Live" message instead of hanging;
  - a presence hook (used for Discord);
  - faulting address in the log.
- `scripts/build.ps1`: full build from the ISO.

## Legal

Fan project, not affiliated with Rockstar Games, Take-Two Interactive or Microsoft.
"Rockstar Table Tennis" is a trademark of its respective owners. Only use this project with a
game you legally own. Do not share ISOs, game files, or the built `dist` folder (it contains
code derived from the game).

This project is released under the BSD 3-Clause license (see `LICENSE`). The ReXGlue SDK is
BSD 3-Clause (© Tom Clay, with parts from the Xenia project); the SDK patch follows that license.

---

## Français

Portage PC non officiel de Rockstar Table Tennis (Xbox 360) par recompilation statique avec
ReXGlue. **Aucun fichier du jeu n'est inclus.**

**Le plus simple avec Claude Code :** ouvre Claude Code dans le dossier où tu veux le jeu et colle
le prompt de la section « Install with Claude ». Remplace la ligne `ISO:` par le chemin de ton ISO,
qui peut être n'importe où sur ton PC (dans l'Explorateur : Maj + clic droit sur l'ISO →
« Copier en tant que chemin d'accès »). Claude télécharge lui-même le dépôt.

Le statut Discord marche pour tout le monde sans rien configurer : il suffit que Discord soit ouvert.

**À la main :**

1. Télécharge ce dépôt (**Code → Download ZIP**) et décompresse-le.
2. Double-clique sur **`Build.bat`** et choisis ton ISO (ta propre copie légale).
3. Accepte l'installation des outils manquants si le script le propose.
4. Patiente (20 à 60 minutes la première fois).
5. Lance `dist\Rockstar Table Tennis\Rockstar Table Tennis.exe`.

En jeu : **F1** réglages (langue, résolution, plein écran…), **F7** succès, **Alt+Entrée** fenêtre, **Échap** deux fois pour quitter. Au clavier dans le jeu : flèches =
croix directionnelle, **Entrée** = Start, **Espace** = A, **Retour arrière** = B. À la manette : **Vue + LB**
réglages, **Vue + RB** succès, **Vue + Menu** maintenus 2 secondes pour quitter.
