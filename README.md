# VoxPill - talk-to-it vocal chain (VST3)

A vocal chain plugin with a retro CRT screen. Type what you want ("darker and wider", "hard tune in f minor",
"less harsh, more air") and the chain moves. Tap a style chip to load a full chain. RAW / AFTER = A/B.

Chain: gate > pitch correction (key/scale + speed) > EQ (lo-mid cut, presence, air, tone) > de-esser >
compressor > saturation > chorus + doubler > stereo width > ping-pong echo (tempo-synced) > reverb > soft safety limiter.

Knobs: VOLUME, DOSE (overall intensity), TONE, MIX, AIR, GATE, COMP. TWEAK tab exposes every module + key/scale.
Everything is automatable in FL Studio.

## Get the plugin file (no coding)

1. Create a free GitHub account, make a new repo, upload this whole folder (keep the `.github` folder).
2. Open the repo's **Actions** tab - the "Build VoxPill" run starts by itself (about 10 min).
3. Open the finished run, download **VoxPill-Windows-VST3** (or macOS). Unzip it - you get `VoxPill.vst3`.

## Install in FL Studio

- Windows: copy `VoxPill.vst3` (it is a folder) into `C:\Program Files\Common Files\VST3\`
- macOS: copy it into `/Library/Audio/Plug-Ins/VST3/` and run `xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/VoxPill.vst3`
- FL Studio: **Options > Manage plugins > Find installed plugins** (tick "Verify plugins"), then load **VoxPill** on a mixer insert with your vocal.

## Build yourself

Needs CMake 3.22+, a C++17 compiler (Visual Studio 2022 / Xcode) and git:

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target VoxPill_VST3

Output: `build/VoxPill_artefacts/Release/VST3/VoxPill.vst3` (JUCE is downloaded automatically).
Use `-DVOXPILL_JUCE_PATH=/path/to/JUCE` to use a local JUCE checkout.

## Honest notes

- The "talk" box is an offline keyword interpreter, not an LLM. Plugins can't ship a language model; it understands
  the words in the HELP tab and many close variants. Add more rules in `VoxPillProcessor::runPrompt` (Source/PluginProcessor.cpp).
- Style chips are generic genre chains (melodic trap, dark rnb, hyperpop...). Edit `kPresets` to add your own.
- Pitch correction is a lightweight delay-line shifter: good for hard-tune effects and light correction on a single
  mono vocal, not Melodyne-grade. Works best with a dry, solo vocal.
- JUCE is GPL/commercial: fine for personal use; read JUCE's license before selling the plugin.
