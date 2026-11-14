// Small DSP building blocks. Plain C++17, scalar code only (no SIMD intrinsics, no -march flags),
// so it runs on any x86-64 (macOS 11 Intel) or arm64 machine.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pb
{
constexpr double kPi = 3.14159265358979323846;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double clampd (double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// RBJ biquad, transposed direct form II.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    void setBypass() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void norm (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static double w0 (double fs, double fc) { return 2.0 * kPi * clampd (fc, 5.0, fs * 0.47) / fs; }

    void lowpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double fs, double fc, double q)
    {
        const double w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double fc, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), al = std::sin (w) / (2 * q);
        norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double fc, double db, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
              (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double fs, double fc, double db, double S = 0.8)
    {
        const double A = std::pow (10.0, db / 40.0), w = w0 (fs, fc), c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sa = 2 * std::sqrt (A) * al;
        norm (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
              (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

// Topology-preserving state-variable filter (Zavalishin). Stable under fast modulation,
// which the DJ filter sweeps and the wah need.
struct Svf
{
    double g = 0, k = 1.414, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    double lp = 0, bp = 0, hp = 0;
    void set (double fs, double fc, double q)
    {
        g = std::tan (kPi * clampd (fc, 10.0, fs * 0.45) / fs);
        k = 1.0 / std::max (0.3, q);
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline void process (double x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

struct OnePoleLP
{
    double a = 0, z = 0;
    void set (double fs, double fc) { a = std::exp (-2.0 * kPi * clampd (fc, 0.01, fs * 0.49) / fs); }
    void setTime (double fs, double seconds) { a = seconds <= 0 ? 0 : std::exp (-1.0 / (seconds * fs)); }
    inline double process (double x) { z = x + a * (z - x); return z; }
    void reset (double v = 0) { z = v; }
};

struct DcBlocker
{
    double x1 = 0, y1 = 0, r = 0.9995;
    void set (double fs, double fc = 8.0) { r = std::exp (-2.0 * kPi * fc / fs); }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Deterministic xorshift RNG (no allocation, real-time safe).
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline double uni() { return (next() >> 8) * (1.0 / 16777216.0); }   // [0,1)
    inline double bi() { return uni() * 2.0 - 1.0; }                      // [-1,1)
};

// Smoothly wandering random value, roughly unit RMS (two cascaded one-poles on white noise,
// calibrated at prepare time).
struct Drift
{
    OnePoleLP a, b;
    Rng rng;
    double scale = 1.0;
    void set (double fs, double fc, uint32_t seed)
    {
        a.set (fs, fc); b.set (fs, fc); rng = Rng (seed);
        scale = 1.0;
        double acc = 0; const int n = (int) (fs * 3.0);
        for (int i = 0; i < n; ++i) { const double v = process(); if (i > n / 3) acc += v * v; }
        const double rms = std::sqrt (acc / (double) (n - n / 3 - 1));
        scale = rms > 1e-12 ? 1.0 / rms : 1.0;
    }
    inline double process() { return b.process (a.process (rng.bi())) * scale; }
};

// 4-point, 3rd-order Hermite interpolation.
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return ((((a * t) - b) * t + c) * t + x0);
}

inline double softClip (double x) { return std::tanh (x); }
} // namespace pb
