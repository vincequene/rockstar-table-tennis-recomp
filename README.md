# Rockstar Table Tennis — version PC (recompilation statique)

Portage PC non officiel de **Rockstar Table Tennis** (Xbox 360, 2006, title ID `545407DF`),
obtenu par recompilation statique avec le [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

> **Ce dépôt ne contient aucun fichier du jeu**, ni code recompilé, ni exécutable.
> Il faut ta propre copie légale du jeu (ISO Xbox 360). Le script de construction
> recrée tout sur ta machine à partir de ton ISO, qui n'est jamais modifié.

## Fonctionnalités

- Jeu complet jouable, manette Xbox (filaire ou Bluetooth)
- Menu de réglages en jeu (**F1**) en 5 langues : langue, résolution, plein écran, textures, Discord
- Langue au choix parmi celles du disque : anglais, français, allemand, espagnol, italien
- Rendu jusqu'en 4K (le jeu d'origine est en 720p), filtrage des textures 16x
- Plein écran / fenêtre (**Alt+Entrée**)
- Succès (29) avec liste en jeu (**F7**) et notifications
- Statut **Discord** basé sur le statut Xbox Live d'origine (« Exhib hors ligne 2-1 — Kumi - Jesper »)
- Mode réseau local (System Link) fonctionnel
- Quitter : **Échap** deux fois, ou maintenir **Vue + Menu** 2 secondes sur la manette
- Sauvegardes dans `Documents\Rockstar Table Tennis`

## Installation

Prérequis : Windows 10/11 64 bits, environ 25 Go libres, et ton ISO de Rockstar Table Tennis.

1. Télécharge ce dépôt (bouton **Code → Download ZIP**) et décompresse-le.
2. Double-clique sur **`Construire.bat`** et choisis ton ISO.
3. Si des outils manquent (Git, CMake, Ninja, LLVM, Visual Studio Build Tools),
   le script propose de les installer avec `winget`.
4. Patiente (de 20 à 60 minutes la première fois).
5. Le jeu est dans **`dist\Rockstar Table Tennis\`** : lance `Rockstar Table Tennis.exe`.

## Réglages

Appuie sur **F1** en jeu, ou modifie `tabletennis.toml` à côté de l'exécutable (Bloc-notes) :

| Réglage | Valeurs |
|---|---|
| `user_language` | 1 anglais, 3 allemand, 4 français, 5 espagnol, 6 italien |
| `resolution_scale` | 1 = 720p, 2 = 1440p, 3 = 4K |
| `fullscreen` | `true` / `false` |
| `anisotropic_override` | 0 = d'origine … 5 = 16x |
| `discord_enabled` | `true` / `false` |
| `discord_client_id` | identifiant d'application Discord (vide = désactivé) |

## Limites connues

- Le jeu reste à 60 images par seconde : au-delà, sa logique (moteur RAGE) se dérègle.
- Le Xbox Live d'origine n'existe plus.

## Icône

L'icône du jeu n'est pas fournie (illustration du jeu). Pour en avoir une, place un fichier
`project\res\tabletennis.ico` avant de lancer la construction.

## Fonctionnement technique

- `project/` : projet ReXGlue (manifest, corrections de fonctions dans `tabletennis_fixes.toml`,
  code de l'application : sortie, chemins, icône, Discord).
- `patches/rexglue-sdk.patch` : correctifs appliqués au SDK (commit `c94f5eb`) :
  - détection des petites fonctions virtuelles (`mtctr`/`bctr`) oubliées par l'analyse ;
  - exceptions virgule flottante toujours masquées (plantage `0xC000008F`) ;
  - numéros de sockets positifs comme sur la console (le System Link plantait après le générique) ;
  - `XGetLanguage` respecte la langue configurée ;
  - point d'accroche pour la présence en ligne (utilisé pour Discord) ;
  - adresse de plantage dans le journal.
- `scripts/build.ps1` : construction complète depuis l'ISO.

## Mentions légales

Projet de fans, non affilié à Rockstar Games, Take-Two Interactive ou Microsoft.
« Rockstar Table Tennis » est une marque de ses propriétaires respectifs.
N'utilise ce projet qu'avec un jeu que tu possèdes légalement. Ne partage ni ISO,
ni fichiers du jeu, ni le dossier `dist` construit (il contient du code dérivé du jeu).

Le ReXGlue SDK est distribué sous licence BSD 3 clauses (© Tom Clay, avec des parties
issues du projet Xenia) ; le correctif fourni ici est soumis à la même licence.

---

## English

Unofficial PC port of Rockstar Table Tennis (Xbox 360) built with the ReXGlue static
recompilation SDK. **No game files are included**: run `Construire.bat`, pick your own
legally obtained ISO, and the script builds everything locally into `dist\`.
