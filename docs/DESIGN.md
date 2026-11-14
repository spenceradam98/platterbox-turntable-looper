# Platterbox v0.1: design and features

## What it is

The idea on the list was a Handytraxx emulator "for audio files, including its loop recording and FX
features". The Korg Handytraxx Play is a portable belt-drive turntable with a built-in looper, three
filter types (DJ filter, EQ filter with a low isolator, peaking wah with modulation), a delay whose
level sits on a vertical fader, a scratch crossfader with a reverse ("hamster") option, 33/45 rpm with
fine speed control, and a 2.5 W built-in speaker. Platterbox recreates that workflow with an audio file
in place of the vinyl. It does not use Korg's name, artwork or layout.

Sources for the unit's feature set: the [Korg Handytraxx Play product page](https://www.korg.com/us/products/dj/handytraxx_play/index.php)
and the [handytraxx 1bit page](https://www.korg.com/au/products/audio/handytraxx_1bit). Korg does not
publish looper length, delay times or filter ranges, so those values are my own choices (listed below).

## Signal flow

```
file ─▶ platter (motor, pitch, scratch, wow) ─▶ surface noise ─▶ cartridge ─▶ wear ─▶ tube preamp ─▶ mono? ─┐
                                                                                                          ├─▶ crossfader ─▶ looper ─▶ filter ─▶ delay ─▶ speaker? ─▶ output
line input (plugin input / interface in standalone) ──────────────────────────────────────────────────────┘
```

The looper records the post-crossfader mix and plays on top of it, so you can loop a phrase, then cut
or scratch the deck over the loop (as on the hardware).

## Controls

| Section | Control | Behaviour |
|---|---|---|
| Deck | Load / drag-and-drop | WAV, AIFF, FLAC, OGG and MP3 (up to 30 min). The file path is saved with the session and reloaded. |
| | Platter | Drag on the record to scratch: grabbing it holds it still, moving it plays at the hand's speed in either direction, letting go hands it back to the motor. |
| | Waveform strip | Click or drag to drop the needle. |
| | Start/Stop | Belt-drive motor. Start time (0.05–3 s) and brake time (0.05–4 s) are adjustable, so stops give the classic pitch dive. |
| | 33 / 45 / 78 | Platter speed. "Files cut at" says which speed counts as original pitch (default 33, so 45 plays 1.35x faster). |
| | Reverse, Repeat | Motor backwards. Repeat off: the needle runs into a locked groove at the end (silence plus a click each revolution). |
| Pitch | Fader + range | ±8 %, ±16 % or ±50 %. Double-click resets. |
| Vinyl & motor | Crackle, Hiss, Rumble, Wow, Wear, Tube | Surface noise moves with the record (it slows and stops when the platter does). Wow includes a once-per-revolution eccentricity. Wear dulls the top end and adds grit. Tube is an asymmetric preamp stage. |
| | Cartridge | Ceramic (thin, mid-forward, rolled off, like a stock portable), Magnetic (full range), Clean (bypassed). Mono switch. |
| Looper | REC / PLAY / DUB | Rec, then press to close the loop and play, press again to overdub. Stop, Clear, Undo (last overdub), 1/2, x2, Rev. Up to 60 s. Follow Pitch makes the loop follow the speed and pitch controls. |
| Filter | Type DJ | One knob: left is low-pass, right is high-pass, centre is off. Res sets resonance. |
| | Type EQ | Low isolator: left kills the bass (−30 dB), right boosts it (+10 dB). Res/High becomes a high-band cut/boost. |
| | Type Wah | Peaking band-pass wah, knob sets the centre (320 Hz–3.2 kHz), Mod Rate/Depth add an LFO sweep. |
| Delay | Time, Feedback, Sync, Level fader | 10–1500 ms free, or host-tempo divisions from 1/32 to 1/2. Feedback darkens and saturates each repeat. The fader is the echo return level. |
| Mixer | Deck, Line In, Crossfader, Hamster, Curve | Sharp curve: both sources full across the middle, cut in the last 5 % (scratch curve). Smooth: equal power. Hamster swaps the ends. |
| | Speaker | Small built-in speaker: mono, band-limited, a little breakup. |
| | Output | −24 to +12 dB, with a meter. |

Header: 8 presets (Fresh Pressing, Dusty Crate, Beach Speaker, Lo-Fi Ceramic, Wah Loop Jam, Dub Throw,
Bass Isolator, Warped 78). Presets change the sound, not the transport. Four themes: Cream, Midnight,
Seafoam, Cherry (saved with the session).

## Engineering notes

- `Source/engine` has no JUCE dependency and no allocation in `process()`. File loading happens on the
  message thread; the audio thread picks the new record up through a try-lock, and old records are
  freed on the message thread.
- Playback uses Hermite interpolation and converts the file's sample rate to the host rate.
- Filters are a TPT state-variable filter (smooth under fast sweeps); coefficients update every 16 samples.
- Engine tests (`tests/EngineTests.cpp`) check speed ratios at 33/45 and with pitch, belt start and stop
  times, scratch hold/reverse/recovery, crossfader and hamster, looper record/play/overdub/undo/half/
  double/clear, filter transparency at centre, the run-out groove, and that nothing goes NaN or runs
  away with everything turned up.

## Choices I made that you may want to change

- Name "Platterbox" (no trademark found in a quick check, but not cleared).
- Effect plugin with a stereo input used as the "Line In" side of the crossfader. Alternative: an
  instrument with no input.
- Looper max 60 s, records the post-crossfader mix, loop audio is not saved with the session.
- 78 rpm added (the hardware has 33/45 only).

## Not yet done

- No anti-aliasing when the record is sped up a lot (fine at normal speeds; can add oversampled
  resampling).
- Looper is free-length; tempo-quantised loop lengths and host-sync would be the next step.
- MIDI control mapping (e.g. for a DJ controller) and keyboard shortcuts.
- Installers and a GitHub repo (on request).
