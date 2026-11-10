# OBS Loudness Leveler + Limiter

An OBS audio filter that keeps your voice at a target loudness (LUFS), shows
you what it is doing live, and protects the output with a lookahead limiter.

- **Too quiet** -> gain is raised slowly (up to "Max boost").
- **Too loud** -> gain is lowered quickly (up to "Max reduction").
- **Limiter** -> catches any peak above the ceiling before it happens.
- **Live meter** -> input loudness, output loudness, applied gain and limiter
  reduction, shown at the top of the filter window.

Built on the official [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate),
so GitHub Actions builds Windows, macOS and Linux packages for you.

## Get builds for all platforms (no local setup)
1. Create a new GitHub repository and push this folder to it.
2. Open the **Actions** tab. The `Push` workflow builds Windows, macOS and
   Ubuntu automatically on every push. Download the packages from the run's
   artifacts.
3. Optional: edit `author`, `website`, `email` and the macOS `bundleId` in
   `buildspec.json`.

## Build locally
Follow the template's presets (needs CMake 3.28+):

    # Windows (Visual Studio 2022)
    cmake --preset windows-x64
    cmake --build --preset windows-x64

    # macOS
    cmake --preset macos
    cmake --build --preset macos

    # Linux (needs libobs dev packages: obs-studio dev files)
    cmake --preset ubuntu-x86_64
    cmake --build --preset ubuntu-x86_64

## Use it
Audio Mixer -> gear icon on your mic -> **Filters** -> "+" ->
**Loudness Leveler + Limiter**. Put it LAST in the chain (after noise
suppression / EQ / compressor).

| Use                | Target   | Ceiling   |
|--------------------|----------|-----------|
| YouTube / Twitch   | -16 LUFS | -1.0 dBFS |
| Podcast            | -16 LUFS | -1.0 dBFS |
| Quieter broadcast  | -23 LUFS | -1.0 dBFS |

## How it works
1. The input is measured with an ITU-R BS.1770 K-weighted loudness meter
   (momentary 400 ms or short-term 3 s).
2. Gain needed = target - measured loudness, clamped to the boost/cut limits.
3. A gate freezes the gain when you are silent (room noise is not boosted);
   a tolerance band prevents constant tiny adjustments.
4. Gain moves with separate attack (down) and release (up) speeds.
5. A lookahead limiter (sliding minimum + smoothed gain, instant attack,
   exponential release) enforces the ceiling.

## The meter panel (always visible)
After installing, open **View -> Docks -> Loudness Leveler** in OBS and drag
the panel next to the Audio Mixer. It shows one block per mic that has the
filter: an **IN** bar, an **OUT** bar with a line at your target loudness, the
gain being applied, and limiter reduction. The OUT bar is green when you are
within 2 LU of the target, orange within 5, red beyond. It updates 20 times
per second and works without opening the filter window.

OBS does not allow plugins to draw inside the mixer's own meter, so a docked
panel is the closest option.

The meter inside the filter window still exists (off by default, "Update this
window live"). It redraws the whole window, which can interrupt slider drags.

## Test the audio code without OBS

    gcc -O2 -Wall -Isrc tests/dsp_test.c src/leveler.c src/loudness.c -lm -o dsp_test && ./dsp_test

## Known limits
- The limiter checks **sample peaks**, not inter-sample (true) peaks. Keep the
  ceiling at -1 dBFS or lower.
- The limiter adds a small delay (= lookahead, 5 ms default). Use the OBS
  "Sync Offset" if you need to realign with video.
- Loudness is measured on the filter's input (feed-forward).

## License
GPL-2.0-or-later (same as the template and OBS).
