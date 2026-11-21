# Auto Decorator: a Geode mod for Geometry Dash 2.2081 (Geode v5)

Select a structure in the editor, tap **Deco**, pick one of 20 templates, then Shuffle until you like it and Apply. It works offline.

## Build it (GitHub Actions, like your other mods)
1. Create a new GitHub repository, for example `gd-auto-decorator`.
2. Upload **everything in this folder**: `src/`, `mod.json`, `CMakeLists.txt`, `about.md`, `changelog.md`, `logo.png`, `README.md`.
3. The workflow file is in a hidden folder, and Windows often skips it. If `.github/workflows/build.yml` is missing on GitHub:
   - Click **Add file → Create new file**.
   - Name it `.github/workflows/build.yml`.
   - Paste the contents of `build.yml`, which is also included in this zip as `build.yml.txt`.
   - Commit.
4. Open the **Actions** tab and wait for **Build Auto Decorator** to turn green, which takes about 10 minutes.
5. Download the artifact **Auto-Decorator-geode** and unzip it. You'll get `myriam.auto-decorator.geode`.
6. Copy the `.geode` file to the tablet, into Geode's `mods` folder. Then restart GD.

## Use it
- In the editor, **select the blocks** of a structure, then tap **Deco** (top right).
- If the Deco button covers something, move it in the mod settings ("Deco button: move left/right / up/down").
- **Use** starts a preview:
  - **Shuffle** makes a new variation.
  - **Apply** keeps it. The red **Undo Deco** button under Deco removes the last decoration in one tap.
  - **Cancel** throws the preview away.
- The options on the right are saved: Density, Gradient, Mirror, Animate, Keep my blocks, Random, Match nearby and Save as template.

## Templates
1. Neon Grid
2. Pixel Pop City
3. Blue Spire Glow
4. Golden Fortress
5. Techno Panels
6. Rooftop Villa
7. Flat Modern
8. Mystic Outline
9. Circuit Glow
10. Amethyst Crystal
11. Miku Neon
12. Fortress Stone
13. Overgrown Forest
14. Frozen Peak
15. Inferno
16. Deep Ocean
17. Candy Pop
18. Haunted Night
19. Clockwork
20. Galaxy Dream
21. Microcosm

**Scenes** (no blocks needed, they fill the screen or the selected area): Microcosm, Neon Skyline, Pink Ambition, Galaxy, Crystal Cave, Sunny Hills.

Templates 1–9 are based on your styles. Templates 10–20 are new.

## Safety
- Only decoration objects are added. Anything that isn't decoration is removed straight away. Solid outline pieces are made passable.
- Spikes, orbs, pads, portals and triggers in or near the structure are never covered or changed.
- Colours use their own channels, picked automatically from free ones. Using the same template again reuses its channels.
- Group IDs are only put on objects that spin (with **Animate** on), and only one rotate trigger is added.

## If the build fails
Open the failed step in Actions, copy the red error lines and send them to Claude.
