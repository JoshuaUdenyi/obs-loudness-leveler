# Loudness Leveler + Limiter: Quick Guide

*For streamers and creators. No audio knowledge needed.*

## What it does

When you talk, your voice gets louder and quieter: you lean back, laugh,
get excited, whisper. Viewers then keep reaching for the volume knob.

**Loudness Leveler** is an OBS filter that does that knob-turning for you:

- Too quiet? It turns you **up**.
- Too loud? It turns you **down**.
- A sudden shout or laugh? The built-in **limiter** catches it so it never
  distorts or clips.

You choose how loud you want to sound, and it keeps you there.

---

## 1. Install

1. **Close OBS.**
2. Run **`Loudness-Leveler-Setup.exe`** and click through. If Windows shows a
   blue "unknown publisher" screen, click **More info**, then **Run anyway**.
3. Click **Install**, then **Finish**.

To remove it later: *Windows Settings → Apps → Loudness Leveler for OBS →
Uninstall*.

---

## 2. Set it up (3 minutes)

**Step 1: Add the filter to your mic**

1. In OBS, find your mic in the **Audio Mixer**.
2. Click the **three dots** (or the gear icon) next to it → **Filters**.
3. Click **+** at the bottom left → **Loudness Leveler + Limiter**.
4. Press OK.

> **Tip:** if you use other filters (like Noise Suppression), keep Loudness
> Leveler **at the bottom of the list**, so it works on your already-cleaned
> voice.

**Step 2: Open the meter panel**

1. In the top menu, click **View → Docks → Loudness Leveler**.
2. Drag the panel next to your Audio Mixer and leave it there.

**Step 3: Talk for 10 seconds**

Say a few sentences in your normal voice. Watch the panel:

| You see | It means |
|---|---|
| **IN** bar | How loud you are *before* the filter |
| **OUT** bar | How loud you are *after*. This is what your viewers hear |
| **Line on the OUT bar** | Your target. The goal is for OUT to sit on this line |
| OUT bar is **green** | You are on target |
| OUT bar is **orange** or **red** | You are off target (see Troubleshooting) |
| **Gain +6.0 dB** | The filter is turning you up by 6 |
| **Gain -4.0 dB** | The filter is turning you down by 4 |
| **Limiter -2.0 dB** | The limiter just caught a loud moment |

That's it. It works by itself from here.

---

## 3. Pick your loudness

Open the filter settings and set **Target loudness**:

| I am... | Target |
|---|---|
| Streaming (Twitch, YouTube, Kick) | **-16** *(the default)* |
| Making a podcast | **-16** |
| Streaming with loud game or music, and I want my voice softer | **-20** |
| Quiet, relaxed content (ASMR, calm talk) | **-23** |

*Numbers closer to 0 are louder. -16 is louder than -23.*

Leave **Limiter ceiling** at **-1.0**. It's the loudest your voice can ever
get, and -1 is safe for every platform.

---

## What each setting does (plain English)

**Meter** *(you can't change these, they are just displays)*

- **Input loudness:** how loud you are going in.
- **Output loudness:** how loud you are coming out.
- **Gain being applied:** how much it is turning you up or down.
- **Limiter reduction:** how hard the limiter is working. 0 means it's idle.

**Auto-level**

- **Target loudness:** how loud you want to be. See the table above.
- **Measurement window:** how quickly it reacts. *Momentary* is fast.
  *Short-term* is slower and smoother. If it sounds like it "pumps" or reacts
  too quickly, choose Short-term.
- **Max boost:** the most it is allowed to turn you **up**. Higher helps with a
  quiet mic, but also raises background noise.
- **Max reduction:** the most it is allowed to turn you **down**.
- **Gate:** it ignores sounds quieter than this. It stops the filter from
  turning up your room noise when you pause. If it does not react to your
  quiet speech, lower this number (for example, from -50 to -60).
- **Tolerance:** if you are already close to the target, it does nothing.
  This stops it from making tiny changes all the time.
- **Reduce speed:** how fast it turns you down when you get loud.
- **Raise speed:** how fast it turns you back up. Slow (about 1500 ms) sounds
  natural.

**Limiter**

- **Ceiling:** the loudest your voice can ever be. Use -1.
- **Lookahead:** how far ahead it looks to catch loud moments. The default is
  fine.
- **Limiter release:** how fast it lets go after catching a loud moment. The
  default is fine.

**Bypass**

Turns the whole filter off while keeping your settings. Use it to hear the
difference (turn it on and off while you talk, with headphones). Keep it
**off** for normal use. While it is on, the filter does nothing.

**Defaults** (button at the bottom of the window) puts everything back to the
recommended settings.

---

## Troubleshooting

| Problem | What to do |
|---|---|
| **I can't find the filter in the list** | You may have installed it while OBS was open. Close OBS completely, then open it again. If it's still missing, run the installer again. If you earlier copied the plugin into the OBS folder by hand, delete that old copy (the file `obs-loudness-leveler.dll` in `obs-studio\obs-plugins\64bit`), so OBS does not see two. |
| **There is no "Loudness Leveler" in View → Docks** | Same as above, restart OBS. The panel only appears after the plugin loads. |
| **The panel says "No Loudness Leveler filter found"** | Add the filter to your mic (Step 1). |
| **The panel shows "BYPASSED"** | Open the filter and untick **Bypass**. |
| **My voice is still too quiet** | Look at **Gain**. If it is stuck at the same number as **Max boost** (+12 by default), it has run out of room. Raise your mic volume in *Windows Sound settings* first, or raise **Max boost** to 18. |
| **Background noise gets louder when I stop talking** | Raise the **Gate** (for example, from -50 to -45) and lower **Max boost**. Also add the **Noise Suppression** filter *above* Loudness Leveler. |
| **The volume seems to pump or breathe** | Set **Measurement window** to *Short-term* and make **Raise speed** slower (for example, 3000 ms). |
| **It does not react to quiet speech** | Lower the **Gate** (for example, to -60). |
| **It's too slow to react when I shout** | Make **Reduce speed** faster (for example, 50 ms). The limiter still protects you in the meantime. |
| **My voice sounds slightly late compared to my camera** | The limiter adds about 5 ms, which you cannot notice. If your video is also delayed, use OBS **Advanced Audio Properties → Sync Offset**. |
| **The meter in the filter window jumps while I move a slider** | Untick **Update this window live**. The meter panel in your main window is the better one to watch. |

---

## One last tip

The filter works best on a **clean** mic signal. Before using it:

1. Set your mic volume in Windows so your normal speaking voice peaks around
   **-18 to -10 dB** on the OBS mixer (the bar should reach the **green and
   yellow** part, and never touch the red).
2. Use a pop filter or place the mic about a fist-width from your mouth.

Do that, and the Leveler only needs to make small, gentle corrections.
