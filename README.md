# Rotor (working title)

A five-voice, voice-rotating hybrid poly synth plugin (VST3 / AU / Standalone), built with JUCE.
See `CLAUDE.md` for the full spec and milestone plan, `CHANGELOG.md` for what each tag contains.

## Build (Linux or macOS)

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # fetches JUCE + Catch2
cmake --build build
ctest --test-dir build --output-on-failure
```

Plugins land in `build/Rotor_artefacts/Release/` (`VST3/`, `Standalone/`, and `AU/` on macOS).

## Validate

```sh
# Linux: pluginval release zip unpacked into tools/ (gitignored)
xvfb-run -a tools/pluginval --strictness-level 5 --validate build/Rotor_artefacts/Release/VST3/Rotor.vst3
# macOS
auval -v aumu Rtr1 Lvsy
```

The macOS GitHub Actions workflow (`.github/workflows/macos.yml`) builds universal VST3/AU/Standalone,
runs the tests, pluginval, and auval, and uploads the plugins as a downloadable artifact.

## Offline render / CPU check

```sh
cmake -S . -B build -DROTOR_BUILD_DEVTOOLS=ON && cmake --build build --target RotorRender
cd build && ./RotorRender_artefacts/Release/RotorRender render.wav   # prints CPU, peak, RMS; writes a 12 s demo
```

## Layout

- `src/dsp/` — plain C++ DSP (no JUCE), unit-tested in `tests/`
- `src/plugin/` — JUCE plugin glue (parameters, processor)
- `devtools/` — offline render / CPU check tool (optional build)
