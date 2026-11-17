// Platterbox engine: a portable turntable with a looper, DJ filter, delay and a small speaker,
// playing audio files as if they were records. Plain C++17 with no JUCE dependency, so it can be
// unit tested on its own. Everything in process() is allocation-free.
#pragma once

#include "Dsp.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace pb
{
// An audio file "pressed" onto the virtual record. Owned outside the engine.
struct RecordData
{
    std::vector<float> left, right;   // same length; right == left for mono files
    double sampleRate = 44100.0;
    std::string name;
    size_t length() const { return left.size(); }
    double seconds() const { return sampleRate > 0 ? (double) left.size() / sampleRate : 0.0; }
};

enum class FilterType { dj = 0, eq = 1, wah = 2 };
enum class Cartridge { ceramic = 0, magnetic = 1, clean = 2 };

struct Params
{
    // --- turntable ---
    bool motorOn = false;
    int speed = 0;              // 0 = 33 1/3, 1 = 45, 2 = 78 rpm
    int cutAt = 0;              // rpm the file counts as "cut at": playing at this speed = original pitch
    float pitch = 0.0f;         // -1..+1 of the pitch range
    int pitchRange = 0;         // 0 = +-8 %, 1 = +-16 %, 2 = +-50 %
    bool reverse = false;       // motor runs backwards
    bool repeat = true;         // true: needle jumps back to the start at the end; false: locked run-out groove
    float startTime = 0.7f;     // seconds the belt drive takes to reach speed from rest
    float brakeTime = 0.6f;     // seconds to stop
    // --- vinyl character ---
    float crackle = 0.25f, hiss = 0.15f, rumble = 0.15f, wow = 0.25f, wear = 0.15f, drive = 0.2f;   // 0..1
    Cartridge cartridge = Cartridge::magnetic;
    bool mono = false;
    // --- mixer ---
    float deckLevel = 1.0f, lineLevel = 1.0f;   // 0..1.5
    float crossfader = -1.0f;   // -1 = deck only, +1 = line only
    bool hamster = false;       // swaps the crossfader ends (reverse scratching)
    int faderCurve = 1;         // 0 = smooth (equal power), 1 = sharp (scratch cut)
    // --- filter ---
    FilterType filterType = FilterType::dj;
    float filter = 0.0f;        // -1..+1 (DJ: LPF left / HPF right; EQ: low isolator; Wah: centre)
    float resonance = 0.3f;     // 0..1
    float modRate = 0.3f;       // 0..1  (wah LFO 0.1..8 Hz)
    float modDepth = 0.0f;      // 0..1
    // --- looper ---
    bool loopFollowsSpeed = true;
    bool loopSync = false;      // round the loop length to whole beats at the host tempo
    float loopLevel = 1.0f;     // 0..1.5
    // --- delay ---
    float delayMs = 375.0f;     // free time, 10..1500 ms
    bool delaySync = false;
    int delayDiv = 4;           // index into kDelayDivisions
    float feedback = 0.4f;      // 0..0.95
    float delayLevel = 0.0f;    // 0..1 (the vertical fader)
    double tempoBpm = 120.0;
    // --- output ---
    bool speaker = false;       // built-in speaker simulation
    float outputDb = 0.0f;
};

// Tempo-synced delay divisions in beats.
constexpr std::array<double, 9> kDelayDivisions { 1.0 / 8, 1.0 / 6, 1.0 / 4, 1.0 / 3, 3.0 / 8, 1.0 / 2, 3.0 / 4, 1.0, 2.0 };
constexpr std::array<const char*, 9> kDelayDivisionNames { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/8.", "1/4", "1/2" };

enum class LoopState { empty = 0, recording, playing, overdubbing, stopped };
enum class LoopCommand { recPlayDub = 0, stop, clear, half, doubleLen, reverse, undo };

class Looper
{
public:
    void prepare (double fs, double maxSeconds);
    // beatSamples > 0 rounds a newly closed loop to whole beats
    void command (LoopCommand c, double rateNow, double beatSamples = 0);
    void reset();
    // in/out are one stereo frame; rateRatio = current motor speed / speed when recorded
    void process (float inL, float inR, float& outL, float& outR, double rateRatio, bool followSpeed);

    LoopState state() const { return st; }
    double lengthSeconds() const { return fs > 0 ? (double) len / fs : 0; }
    double progress() const { return len > 0 ? readPos / (double) len : 0; }
    bool isReversed() const { return reversed; }
    bool canUndo() const { return undoAvailable; }

private:
    void startOverdub();
    void closeRecording (double beatSamples);
    std::vector<float> L, R, undoL, undoR;
    double fs = 0;
    size_t maxLen = 0, len = 0;
    double readPos = 0;
    LoopState st = LoopState::empty;
    bool reversed = false;
    double recordedAtRate = 1.0;
    std::vector<uint32_t> touchedGen;   // copy-on-write undo: sample i holds undo data when touchedGen[i] == gen
    uint32_t gen = 1;
    bool undoAvailable = false;
    double fade = 0, fadeStep = 0;      // click-free start/stop envelope
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void setParams (const Params& p) { params = p; }
    const Params& getParams() const { return params; }

    // Record handling: the caller owns the RecordData and keeps it alive while it is set here.
    void setRecord (const RecordData* r);
    const RecordData* getRecord() const { return record; }

    // UI / host control (call from the audio thread, e.g. after draining a command queue).
    void seek (double seconds);
    void loopCommand (LoopCommand c);
    // Hand on the platter. velocity is in revolutions per second (0.555 = forwards at 33 1/3 rpm,
    // 0 = held still, negative = pulled back).
    void setScratch (bool touching, double velocity) { handOn = touching; handVelocity = velocity; }

    // inL/inR: line input (may alias outL/outR). Processes n frames.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);

    // --- read-outs for the UI (approximate, read from another thread) ---
    double positionSeconds() const;
    double platterSpeed() const { return platter; }     // signed, 1 = nominal at the cut-at speed
    double platterAngle() const { return angle; }        // revolutions, wraps at 1
    const Looper& getLooper() const { return looper; }
    float outputPeak() const { return peak; }

    static double rpmOf (int speedIndex);
    double motorTargetSpeed() const;    // what the motor is driving towards (signed, 0 when off)
    double nominalSpeed() const;        // speed the motor would run at if on (unsigned)

private:
    void updateFilterCoeffs();
    void updateStaticFilters();
    void readRecord (double pos, float& l, float& r) const;

    Params params, lastStatic;
    bool staticDirty = true;
    double fs = 44100.0;
    const RecordData* record = nullptr;

    // transport
    double pos = 0;           // read position in record samples
    double platter = 0;       // actual platter speed (signed, relative)
    double angle = 0;
    bool handOn = false, recovering = false;
    double lastAngle = 0;
    double handVelocity = 0;
    double handSmoothed = 0;
    bool atRunout = false;
    Drift wowDrift, rumbleDrift;
    double eccentricPhase = 0;

    // vinyl
    Rng noiseRng { 0x1234567u }, crackleRng { 0x7654321u };
    Biquad cartL[3], cartR[3];
    OnePoleLP wearL, wearR, hissLp;
    Biquad hissHp;
    OnePoleLP rumbleLp;
    double crackleEnv = 0, crackleSign = 1;
    OnePoleLP crackleLp;
    DcBlocker dcL, dcR;

    // looper
    Looper looper;

    // filter
    Svf svfL, svfR;
    Biquad isoL, isoR, isoHiL, isoHiR;
    OnePoleLP filterSmooth;
    double lfoPhase = 0;
    int coeffCounter = 0;

    // delay
    std::vector<float> dlyL, dlyR;
    int dlyW = 0, dlyMask = 0;
    OnePoleLP dlyTimeSmooth;
    OnePoleLP dlyToneL, dlyToneR;

    // speaker
    Biquad spkHp, spkLp, spkPeak;

    OnePoleLP outGainSmooth, xfDeckSmooth, xfLineSmooth;
    float peak = 0;
};
} // namespace pb
