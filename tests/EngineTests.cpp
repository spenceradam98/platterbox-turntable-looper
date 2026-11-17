// Engine tests: plain asserts, no framework. Run via ctest or directly.
#include "Engine.h"
#include <cstdio>
#include <cstdlib>

using namespace pb;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); std::printf (__VA_ARGS__); std::printf ("\n"); ++failures; } } while (0)

static const double fs = 48000.0;

static RecordData makeSine (double freq, double seconds, double sr = 44100.0)
{
    RecordData r; r.sampleRate = sr; r.name = "sine";
    const size_t n = (size_t) (seconds * sr);
    r.left.resize (n); r.right.resize (n);
    for (size_t i = 0; i < n; ++i) r.left[i] = r.right[i] = (float) (0.5 * std::sin (2 * kPi * freq * (double) i / sr));
    return r;
}

static Params quiet()
{
    Params p; p.crackle = p.hiss = p.rumble = p.wow = p.wear = p.drive = 0; p.cartridge = Cartridge::clean;
    p.crossfader = -1; p.delayLevel = 0; p.motorOn = true; p.startTime = 0.05f; p.brakeTime = 0.05f;
    return p;
}

// count positive-going zero crossings in the left output over `seconds`, after `settle` seconds
static double measureFreq (Engine& e, double settle, double seconds)
{
    std::vector<float> zL (512, 0), zR (512, 0), oL (512), oR (512);
    int toSkip = (int) (settle * fs), toMeasure = (int) (seconds * fs), crossings = 0;
    float prev = 0;
    while (toSkip > 0) { const int n = std::min (512, toSkip); e.process (zL.data(), zR.data(), oL.data(), oR.data(), n); toSkip -= n; }
    int left = toMeasure;
    while (left > 0)
    {
        const int n = std::min (512, left);
        e.process (zL.data(), zR.data(), oL.data(), oR.data(), n);
        for (int i = 0; i < n; ++i) { if (prev <= 0 && oL[(size_t) i] > 0) ++crossings; prev = oL[(size_t) i]; }
        left -= n;
    }
    return crossings / seconds;
}

static bool runFinite (Engine& e, int blocks, float inputLevel = 0.3f)
{
    std::vector<float> iL (256), iR (256), oL (256), oR (256);
    Rng rng (99);
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 256; ++i) { iL[(size_t) i] = (float) (rng.bi() * inputLevel); iR[(size_t) i] = iL[(size_t) i]; }
        e.process (iL.data(), iR.data(), oL.data(), oR.data(), 256);
        for (int i = 0; i < 256; ++i) if (! std::isfinite (oL[(size_t) i]) || std::abs (oL[(size_t) i]) > 20.f) return false;
    }
    return true;
}

int main()
{
    auto rec = makeSine (440.0, 4.0);

    { // 33 rpm plays at original pitch, 45 rpm is 1.35x, sample-rate conversion is handled
        Engine e; auto p = quiet(); e.setParams (p); e.prepare (fs, 512); e.setRecord (&rec);
        const double f33 = measureFreq (e, 0.3, 1.0);
        CHECK (std::abs (f33 - 440.0) < 3.0, "33 rpm freq %.1f", f33);
        p.speed = 1; e.setParams (p);
        const double f45 = measureFreq (e, 0.3, 1.0);
        CHECK (std::abs (f45 - 440.0 * 1.35) < 4.0, "45 rpm freq %.1f", f45);
        p.speed = 0; p.pitch = 1.0f; p.pitchRange = 0; e.setParams (p);
        const double fUp = measureFreq (e, 0.3, 1.0);
        CHECK (std::abs (fUp - 440.0 * 1.08) < 4.0, "+8%% freq %.1f", fUp);
    }

    { // belt drive: motor takes startTime to reach speed, and stops when switched off
        Engine e; auto p = quiet(); p.motorOn = false; p.startTime = 1.0f; p.brakeTime = 0.5f; e.setParams (p); e.prepare (fs, 512); e.setRecord (&rec);
        std::vector<float> z (480, 0), o1 (480), o2 (480);
        p.motorOn = true; e.setParams (p);
        for (int b = 0; b < 50; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);   // 0.5 s
        CHECK (std::abs (e.platterSpeed() - 0.5) < 0.02, "half way after 0.5 s: %.3f", e.platterSpeed());
        for (int b = 0; b < 60; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        CHECK (std::abs (e.platterSpeed() - 1.0) < 1e-3, "at speed: %.3f", e.platterSpeed());
        p.motorOn = false; e.setParams (p);
        for (int b = 0; b < 60; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        CHECK (std::abs (e.platterSpeed()) < 1e-3, "stopped: %.3f", e.platterSpeed());
    }

    { // scratch: hand on the platter overrides the motor
        Engine e; auto p = quiet(); e.setParams (p); e.prepare (fs, 512); e.setRecord (&rec);
        std::vector<float> z (480, 0), o1 (480), o2 (480);
        for (int b = 0; b < 20; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        e.setScratch (true, 0.0);
        for (int b = 0; b < 10; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        CHECK (std::abs (e.platterSpeed()) < 0.01, "held still: %.3f", e.platterSpeed());
        e.setScratch (true, -100.0 / 3.0 / 60.0);
        for (int b = 0; b < 10; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        CHECK (std::abs (e.platterSpeed() + 1.0) < 0.01, "pulled back: %.3f", e.platterSpeed());
        e.setScratch (false, 0.0);
        for (int b = 0; b < 50; ++b) e.process (z.data(), z.data(), o1.data(), o2.data(), 480);
        CHECK (std::abs (e.platterSpeed() - 1.0) < 0.01, "recovered: %.3f", e.platterSpeed());
    }

    { // crossfader: sharp curve, hamster swaps the ends
        Engine e; auto p = quiet(); p.motorOn = false; p.crossfader = 1.0f; e.setParams (p); e.prepare (fs, 512);
        std::vector<float> in (480, 0.25f), oL (480), oR (480);
        for (int b = 0; b < 20; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[479] - 0.25f) < 0.02f, "line passes at +1: %.3f", oL[479]);
        p.hamster = true; e.setParams (p);
        for (int b = 0; b < 20; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[479]) < 0.01f, "hamster cuts line at +1: %.3f", oL[479]);
        p.hamster = false; p.crossfader = 0.0f; e.setParams (p);
        for (int b = 0; b < 20; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[479] - 0.25f) < 0.02f, "sharp curve full at centre: %.3f", oL[479]);
    }

    { // looper: record, play back, overdub, undo, half, double
        Engine e; auto p = quiet(); p.motorOn = false; p.crossfader = 1.0f; e.setParams (p); e.prepare (fs, 512);
        std::vector<float> in (480, 0.2f), z (480, 0), oL (480), oR (480);
        for (int b = 0; b < 5; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        e.loopCommand (LoopCommand::recPlayDub);
        for (int b = 0; b < 100; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);   // 1 s
        e.loopCommand (LoopCommand::recPlayDub);
        CHECK (e.getLooper().state() == LoopState::playing, "playing");
        CHECK (std::abs (e.getLooper().lengthSeconds() - 1.0) < 0.01, "length %.3f", e.getLooper().lengthSeconds());
        for (int b = 0; b < 10; ++b) e.process (z.data(), z.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[300] - 0.2f) < 0.01f, "loop plays: %.3f", oL[300]);
        e.loopCommand (LoopCommand::recPlayDub);   // overdub
        CHECK (e.getLooper().state() == LoopState::overdubbing, "overdubbing");
        for (int b = 0; b < 100; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        e.loopCommand (LoopCommand::recPlayDub);
        for (int b = 0; b < 10; ++b) e.process (z.data(), z.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[300] - 0.4f) < 0.02f, "overdub summed: %.3f", oL[300]);
        e.loopCommand (LoopCommand::undo);
        for (int b = 0; b < 10; ++b) e.process (z.data(), z.data(), oL.data(), oR.data(), 480);
        CHECK (std::abs (oL[300] - 0.2f) < 0.01f, "undo: %.3f", oL[300]);
        e.loopCommand (LoopCommand::half);
        CHECK (std::abs (e.getLooper().lengthSeconds() - 0.5) < 0.01, "half %.3f", e.getLooper().lengthSeconds());
        e.loopCommand (LoopCommand::doubleLen);
        CHECK (std::abs (e.getLooper().lengthSeconds() - 1.0) < 0.01, "double %.3f", e.getLooper().lengthSeconds());
        e.loopCommand (LoopCommand::clear);
        CHECK (e.getLooper().state() == LoopState::empty, "cleared");
    }

    { // beat sync: a 1.1 s take at 120 bpm closes as a 1.0 s (2-beat) loop
        Engine e; auto p = quiet(); p.motorOn = false; p.crossfader = 1.0f; p.loopSync = true; p.tempoBpm = 120.0; e.setParams (p); e.prepare (fs, 512);
        std::vector<float> in (480, 0.2f), oL (480), oR (480);
        e.loopCommand (LoopCommand::recPlayDub);
        for (int b = 0; b < 110; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        e.loopCommand (LoopCommand::recPlayDub);
        CHECK (std::abs (e.getLooper().lengthSeconds() - 1.0) < 1e-3, "synced length %.4f", e.getLooper().lengthSeconds());
    }

    { // DJ filter: centre is transparent
        Engine e; auto p = quiet(); p.motorOn = false; p.crossfader = 1.0f; p.filter = 0.0f; e.setParams (p); e.prepare (fs, 512);
        std::vector<float> in (480), oL (480), oR (480);
        for (int i = 0; i < 480; ++i) in[(size_t) i] = (float) (0.3 * std::sin (2 * kPi * 1000.0 * i / fs));
        for (int b = 0; b < 20; ++b) e.process (in.data(), in.data(), oL.data(), oR.data(), 480);
        double err = 0; for (int i = 0; i < 480; ++i) err = std::max (err, (double) std::abs (oL[(size_t) i] - in[(size_t) i]));
        CHECK (err < 1e-3, "filter centre transparent: err %.5f", err);
    }

    { // stress: everything up, all filter types, no NaNs or runaway
        for (int ft = 0; ft < 3; ++ft)
        {
            Engine e; Params p; p.motorOn = true; p.crackle = p.hiss = p.rumble = p.wow = p.wear = p.drive = 1.0f;
            p.filterType = (FilterType) ft; p.filter = 0.9f; p.resonance = 1.0f; p.modDepth = 1.0f; p.modRate = 1.0f;
            p.feedback = 0.95f; p.delayLevel = 1.0f; p.crossfader = 0.0f; p.speaker = true; p.speed = 2; p.pitch = 1; p.pitchRange = 2;
            e.setParams (p); e.prepare (fs, 256); e.setRecord (&rec);
            e.loopCommand (LoopCommand::recPlayDub);
            CHECK (runFinite (e, 400), "finite output, filter type %d", ft);
            e.loopCommand (LoopCommand::recPlayDub); e.loopCommand (LoopCommand::reverse);
            CHECK (runFinite (e, 400), "finite output with reversed loop, filter type %d", ft);
        }
    }

    { // run-out: without repeat, playback stops at the end of the record
        auto shortRec = makeSine (440.0, 0.5);
        Engine e; auto p = quiet(); p.repeat = false; e.setParams (p); e.prepare (fs, 512); e.setRecord (&shortRec);
        std::vector<float> z (480, 0), oL (480), oR (480);
        for (int b = 0; b < 100; ++b) e.process (z.data(), z.data(), oL.data(), oR.data(), 480);
        double mx = 0; for (float v : oL) mx = std::max (mx, (double) std::abs (v));
        CHECK (mx < 1e-3, "silent in the run-out groove: %.4f", mx);
    }

    if (failures == 0) std::printf ("All engine tests passed.\n");
    return failures == 0 ? 0 : 1;
}
