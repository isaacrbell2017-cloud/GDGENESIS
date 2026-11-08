# GdGenesis - build & install guide

GdGenesis is a Geode mod for Geometry Dash **2.2081** (Geode v5). This folder is the full source.
GitHub builds the `.geode` file for you, so you don't need a PC compiler.

## 1. Put the project on GitHub
1. Make a **new private repository** called `GdGenesis` (private keeps it just for you).
2. Upload everything in this folder, keeping the folders as they are:
   - `mod.json`, `CMakeLists.txt`, `about.md`, `changelog.md`, `logo.png`, `.gitignore`
   - `src/` (9 files), `resources/genesis_spark.png`
   - `.github/workflows/build.yml`  <- this one makes GitHub build the mod

   **On a PC:** unzip, open the repo page, click *Add file > Upload files* and drag in the *contents* of the folder (folders are kept).
   **On a tablet:** use *Add file > Create new file*, type the full path as the name (for example `src/main.cpp` or `.github/workflows/build.yml`), and paste that file's text. For the two PNGs, open the right folder and use *Add file > Upload files*.

## 2. Let GitHub build it
1. Open the **Actions** tab. A run called *Build GdGenesis* starts on every upload (or press *Run workflow*).
2. Wait for the green tick (about 5-10 minutes).
3. Open the run and download the **GdGenesis** artifact. Inside the zip is `litchin.gdgenesis.geode`.

If a step goes red, open it, copy the red error lines, and send them to Claude to fix.

## 3. Install
1. Copy `litchin.gdgenesis.geode` into your Geode mods folder
   (Android: `Android/media/com.geode.launcher/game/geode/mods`).
2. Restart Geometry Dash. Make sure Geode itself is up to date (v5.11.2 or newer).

## 4. Use it
1. In the editor, select a block (or a whole layout).
2. Tap the pause button (the two lines) > tap the green **GdGenesis** sparkle button.
3. Type a prompt, or tap a quick chip (Neon, Space, Nature, Castle, Lava), then **Generate**.
4. Not happy? Press the editor's **Undo** and generate again.

## Settings (Geode > GdGenesis > gear)
| Setting | What it does |
|---|---|
| AI Mode | Auto / Claude API / Offline |
| Claude API Key | From console.anthropic.com. Needed for Claude mode only |
| Claude Model | Haiku (fastest) or Sonnet |
| AI Timeout | Falls back to the instant offline generator if Claude is slower than this |
| Key Activation + Activation Key | Open GdGenesis with a key instead of the pause button |
| Main / Accent Color Channel | Channels the deco uses (default 50 and 51) |
| Add Color Triggers | Sets those channels to the prompt's colors |
| Put Deco In Group | Gives all generated deco a group ID |
| Max Objects Per Generate | Object budget per press |
| Select Generated Deco | Select the new deco instead of keeping your blocks selected |
| Reuse Selected Deco / Custom Deco IDs | Lets GdGenesis scatter your own deco objects (say "custom" in the prompt) |
| Button Sound Effects | GD sounds on every button |
| Litchin-Only Lock | Only opens on the Litchin account (or logged out) |

## What the generator builds
Light dots along exposed block edges, soft background orbs, floating particles, hanging chains,
grass, bushes, clouds, spires, and your own deco objects, all colored through two color channels.
