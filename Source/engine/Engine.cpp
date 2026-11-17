#include "Engine.h"

namespace pb
{
namespace
{
constexpr double kRanges[] = { 0.08, 0.16, 0.50 };
inline double sq (double x) { return x * x; }

// Asymmetric tube-ish stage with unity small-signal gain.
inline double tubeStage (double x, double drive)
{
    if (drive <= 0.001) return x;
    const double g = 1.0 + drive * 5.0, b = 0.15 * drive;
    const double tb = std::tanh (b);
    const double slope = g * (1.0 - tb * tb);
    return (std::tanh (g * x + b) - tb) / slope;
}
}

// =====================================================================================  Looper
void Looper::prepare (double sampleRate, double maxSeconds)
{
    fs = sampleRate;
    maxLen = (size_t) std::max (1.0, maxSeconds * fs);
    L.assign (maxLen, 0.0f); R.assign (maxLen, 0.0f);
    undoL.assign (maxLen, 0.0f); undoR.assign (maxLen, 0.0f);
    touchedGen.assign (maxLen, 0u);
    fadeStep = 1.0 / (0.005 * fs);
    reset();
}

void Looper::reset()
{
    len = 0; readPos = 0; st = LoopState::empty; reversed = false; undoAvailable = false; fade = 0; ++gen;
}

void Looper::startOverdub()
{
    ++gen;
    if (gen == 0) { std::fill (touchedGen.begin(), touchedGen.end(), 0u); gen = 1; }
    undoAvailable = true;
}

void Looper::closeRecording (double beatSamples)
{
    if (beatSamples > 1.0)
    {
        const double beats = std::max (1.0, std::round ((double) len / beatSamples));
        size_t newLen = (size_t) std::llround (beats * beatSamples);
        while (newLen > maxLen && newLen > (size_t) beatSamples) newLen -= (size_t) beatSamples;   // drop whole beats to fit
        newLen = std::min (newLen, maxLen);
        if (newLen > len)
        {
            std::fill (L.begin() + (long) len, L.begin() + (long) newLen, 0.0f);
            std::fill (R.begin() + (long) len, R.begin() + (long) newLen, 0.0f);
        }
        len = newLen;
    }
    readPos = 0;
}

void Looper::command (LoopCommand c, double rateNow, double beatSamples)
{
    const size_t minLen = (size_t) (0.05 * fs);
    switch (c)
    {
        case LoopCommand::recPlayDub:
            switch (st)
            {
                case LoopState::empty:
                    len = 0; readPos = 0; recordedAtRate = rateNow > 1e-6 ? rateNow : 1.0; reversed = false;
                    undoAvailable = false; st = LoopState::recording; break;
                case LoopState::recording:
                    if (len < minLen) { reset(); break; }
                    closeRecording (beatSamples); st = LoopState::playing; break;
                case LoopState::playing: startOverdub(); st = LoopState::overdubbing; break;
                case LoopState::overdubbing: st = LoopState::playing; break;
                case LoopState::stopped: readPos = reversed ? (double) len - 1 : 0; st = LoopState::playing; break;
            }
            break;
        case LoopCommand::stop:
            if (st == LoopState::recording) { if (len < minLen) reset(); else { closeRecording (beatSamples); st = LoopState::stopped; } }
            else if (st == LoopState::playing || st == LoopState::overdubbing) st = LoopState::stopped;
            break;
        case LoopCommand::clear: reset(); break;
        case LoopCommand::half:
            if (st != LoopState::recording && len >= 2 * minLen)
            {
                len /= 2;
                readPos = std::fmod (readPos, (double) len);
                undoAvailable = false;
            }
            break;
        case LoopCommand::doubleLen:
            if (st != LoopState::recording && len > 0 && len * 2 <= maxLen)
            {
                std::copy (L.begin(), L.begin() + (long) len, L.begin() + (long) len);
                std::copy (R.begin(), R.begin() + (long) len, R.begin() + (long) len);
                len *= 2;
                undoAvailable = false;
            }
            break;
        case LoopCommand::reverse: if (st != LoopState::recording) reversed = ! reversed; break;
        case LoopCommand::undo:
            if (undoAvailable)
            {
                for (size_t i = 0; i < len; ++i)
                    if (touchedGen[i] == gen) { L[i] = undoL[i]; R[i] = undoR[i]; }
                ++gen;
                undoAvailable = false;
                if (st == LoopState::overdubbing) st = LoopState::playing;
            }
            break;
    }
}

void Looper::process (float inL, float inR, float& outL, float& outR, double rateRatio, bool followSpeed)
{
    outL = outR = 0.0f;
    if (st == LoopState::recording)
    {
        L[len] = inL; R[len] = inR;
        if (++len >= maxLen) { readPos = 0; st = LoopState::playing; }
        return;
    }

    const bool active = st == LoopState::playing || st == LoopState::overdubbing;
    fade = active ? std::min (1.0, fade + fadeStep) : std::max (0.0, fade - fadeStep);
    if (fade <= 0.0 || len < 4) return;

    double rate = followSpeed ? rateRatio / recordedAtRate : 1.0;
    rate = clampd (rate, 0.0, 4.0);
    if (reversed) rate = -rate;

    const double n = (double) len;
    const size_t i0 = (size_t) readPos;
    const float t = (float) (readPos - (double) i0);
    auto at = [&] (const std::vector<float>& b, long i) { long m = (long) len; i %= m; if (i < 0) i += m; return b[(size_t) i]; };
    const long ii = (long) i0;
    outL = (float) fade * hermite (at (L, ii - 1), at (L, ii), at (L, ii + 1), at (L, ii + 2), t);
    outR = (float) fade * hermite (at (R, ii - 1), at (R, ii), at (R, ii + 1), at (R, ii + 2), t);

    if (st == LoopState::overdubbing)
    {
        const size_t w = std::min (i0, len - 1);
        if (touchedGen[w] != gen) { undoL[w] = L[w]; undoR[w] = R[w]; touchedGen[w] = gen; }
        L[w] += inL; R[w] += inR;
    }

    readPos += rate;
    while (readPos >= n) readPos -= n;
    while (readPos < 0) readPos += n;
}

// =====================================================================================  Engine
double Engine::rpmOf (int speedIndex)
{
    switch (speedIndex) { case 1: return 45.0; case 2: return 78.0; default: return 100.0 / 3.0; }
}

double Engine::nominalSpeed() const
{
    const double range = kRanges[std::clamp (params.pitchRange, 0, 2)];
    return rpmOf (params.speed) / rpmOf (params.cutAt) * (1.0 + clampd (params.pitch, -1, 1) * range);
}

double Engine::motorTargetSpeed() const
{
    if (! params.motorOn) return 0.0;
    return params.reverse ? -nominalSpeed() : nominalSpeed();
}

void Engine::prepare (double sampleRate, int)
{
    fs = sampleRate > 0 ? sampleRate : 44100.0;
    wowDrift.set (fs, 0.7, 0xA5A5A5u);
    rumbleDrift.set (fs, 14.0, 0x5A5A5Au);
    hissHp.highpass (fs, 1800.0, 0.6);
    crackleLp.set (fs, 4500.0);
    dcL.set (fs, 6.0); dcR.set (fs, 6.0);
    filterSmooth.setTime (fs, 0.015);
    dlyTimeSmooth.setTime (fs, 0.12);
    dlyToneL.set (fs, 4500.0); dlyToneR.set (fs, 4500.0);
    outGainSmooth.setTime (fs, 0.02);
    xfDeckSmooth.setTime (fs, 0.002); xfLineSmooth.setTime (fs, 0.002);
    spkHp.highpass (fs, 300.0, 0.8);
    spkPeak.peak (fs, 1300.0, 1.2, 5.0);
    spkLp.lowpass (fs, 5200.0, 0.9);

    looper.prepare (fs, 60.0);

    int n = 1; while (n < (int) (fs * 2.2)) n <<= 1;
    dlyL.assign ((size_t) n, 0.0f); dlyR.assign ((size_t) n, 0.0f);
    dlyMask = n - 1; dlyW = 0;
    dlyTimeSmooth.reset (params.delayMs * 0.001 * fs);

    svfL.reset(); svfR.reset();
    filterSmooth.reset (params.filter);
    outGainSmooth.reset (dbToGain (params.outputDb));
    staticDirty = true;
    coeffCounter = 0;
    platter = motorTargetSpeed();
}

void Engine::setRecord (const RecordData* r)
{
    record = r;
    pos = 0; atRunout = false;
}

void Engine::seek (double seconds)
{
    if (record == nullptr || record->length() == 0) return;
    pos = clampd (seconds * record->sampleRate, 0.0, (double) record->length() - 1);
    atRunout = false;
}

void Engine::loopCommand (LoopCommand c)
{
    const double beat = params.loopSync ? 60.0 / std::max (20.0, params.tempoBpm) * fs : 0.0;
    looper.command (c, nominalSpeed(), beat);
}

double Engine::positionSeconds() const
{
    return record && record->sampleRate > 0 ? pos / record->sampleRate : 0.0;
}

void Engine::updateStaticFilters()
{
    const auto& p = params;
    for (int ch = 0; ch < 2; ++ch)
    {
        Biquad* c = ch == 0 ? cartL : cartR;
        switch (p.cartridge)
        {
            case Cartridge::ceramic:   // high-output ceramic: thin low end, mid bump, early roll-off
                c[0].highpass (fs, 90.0, 0.7); c[1].peak (fs, 2400.0, 0.9, 4.5); c[2].lowpass (fs, 6500.0, 0.8); break;
            case Cartridge::magnetic:  // moving magnet: full range with a slight presence lift
                c[0].highpass (fs, 22.0, 0.7); c[1].peak (fs, 5000.0, 0.8, 1.5); c[2].lowpass (fs, 17000.0, 0.7); break;
            case Cartridge::clean:
                c[0].setBypass(); c[1].setBypass(); c[2].setBypass(); break;
        }
    }
    const double wearFc = 20000.0 * std::pow (0.2, clampd (p.wear, 0, 1));   // 20 kHz .. 4 kHz
    wearL.set (fs, wearFc); wearR.set (fs, wearFc);
    lastStatic = p;
    staticDirty = false;
}

void Engine::updateFilterCoeffs()
{
    const auto& p = params;
    const double v = filterSmooth.z;
    const double q = 0.707 + clampd (p.resonance, 0, 1) * 6.0;
    switch (p.filterType)
    {
        case FilterType::dj:
            if (v < 0) { const double fc = 20000.0 * std::pow (60.0 / 20000.0, -v); svfL.set (fs, fc, q); svfR.set (fs, fc, q); }
            else       { const double fc = 20.0 * std::pow (9000.0 / 20.0, v);      svfL.set (fs, fc, q); svfR.set (fs, fc, q); }
            break;
        case FilterType::eq:
        {
            // low isolator: left kills the bass (-30 dB), right boosts it (+10 dB); resonance sets the high band
            const double lowDb = v < 0 ? v * 30.0 : v * 10.0;
            isoL.lowShelf (fs, 180.0, lowDb, 0.9); isoR.lowShelf (fs, 180.0, lowDb, 0.9);
            const double hiDb = (clampd (p.resonance, 0, 1) - 0.3) / 0.7 * 6.0 - (p.resonance < 0.3 ? (0.3 - p.resonance) / 0.3 * 24.0 : 0.0);
            isoHiL.highShelf (fs, 3500.0, hiDb, 0.9); isoHiR.highShelf (fs, 3500.0, hiDb, 0.9);
            break;
        }
        case FilterType::wah:
        {
            const double lfo = std::sin (lfoPhase * 2.0 * kPi);
            double fc = 320.0 * std::pow (10.0, (v + 1.0) * 0.5);        // 320 Hz .. 3.2 kHz
            fc *= std::pow (2.0, clampd (p.modDepth, 0, 1) * 2.0 * lfo);  // up to +-2 octaves
            const double wq = 2.0 + clampd (p.resonance, 0, 1) * 8.0;
            svfL.set (fs, fc, wq); svfR.set (fs, fc, wq);
            break;
        }
    }
}

void Engine::readRecord (double p, float& l, float& r) const
{
    const auto& rec = *record;
    const long n = (long) rec.length();
    const long i = (long) std::floor (p);
    const float t = (float) (p - (double) i);
    auto at = [&] (const std::vector<float>& b, long k)
    {
        if (params.repeat) { k %= n; if (k < 0) k += n; }
        else k = std::clamp (k, 0L, n - 1);
        return b[(size_t) k];
    };
    l = hermite (at (rec.left, i - 1), at (rec.left, i), at (rec.left, i + 1), at (rec.left, i + 2), t);
    r = hermite (at (rec.right, i - 1), at (rec.right, i), at (rec.right, i + 1), at (rec.right, i + 2), t);
}

void Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const auto& p = params;
    if (staticDirty || p.cartridge != lastStatic.cartridge || p.wear != lastStatic.wear) updateStaticFilters();

    const bool hasRecord = record != nullptr && record->length() > 4;
    const double srcRatio = hasRecord ? record->sampleRate / fs : 1.0;
    const double revsAtNominal = rpmOf (p.cutAt) / 60.0;   // revs per second at speed 1.0
    const double target = motorTargetSpeed();
    const double nominal = nominalSpeed();
    const double accelStep = 1.0 / (std::max (0.02, (double) p.startTime) * fs);
    const double brakeStep = 1.0 / (std::max (0.02, (double) p.brakeTime) * fs);
    const double recoverStep = 1.0 / (std::max (0.02, std::min (0.25, (double) p.startTime * 0.35)) * fs);
    const double handTarget = handVelocity / revsAtNominal;
    const double handCoef = 1.0 - std::exp (-1.0 / (0.004 * fs));

    // crossfader gains
    double x = clampd (p.crossfader, -1, 1) * (p.hamster ? -1.0 : 1.0);
    double gDeck, gLine;
    if (p.faderCurve == 0) { const double a = (x + 1.0) * 0.25 * kPi; gDeck = std::cos (a); gLine = std::sin (a); }
    else { gDeck = clampd ((1.0 - x) / 0.1, 0, 1); gLine = clampd ((1.0 + x) / 0.1, 0, 1); }
    gDeck *= p.deckLevel; gLine *= p.lineLevel;

    const double crackleProb = sq (clampd (p.crackle, 0, 1)) * (1.0 + 2.0 * p.wear) * 60.0 / fs;
    const double popProb = sq (clampd (p.crackle, 0, 1)) * (0.6 + p.wear) * 0.5 / fs;
    const double lfoInc = (0.1 * std::pow (80.0, clampd (p.modRate, 0, 1))) / fs;   // 0.1..8 Hz
    const double outGain = dbToGain (p.outputDb);
    const double fb = clampd (p.feedback, 0, 0.95);

    double dlySamples = p.delaySync ? kDelayDivisions[(size_t) std::clamp (p.delayDiv, 0, (int) kDelayDivisions.size() - 1)]
                                          * 60.0 / std::max (20.0, p.tempoBpm) * fs
                                    : clampd (p.delayMs, 10.0, 1500.0) * 0.001 * fs;
    dlySamples = clampd (dlySamples, 1.0, (double) dlyMask - 4.0);

    float pk = 0;
    for (int i = 0; i < n; ++i)
    {
        // ---------------- platter / motor ----------------
        if (handOn)
        {
            platter += (handTarget - platter) * handCoef;
            recovering = true;
        }
        else
        {
            const double d = target - platter;
            double step;
            if (recovering) step = recoverStep;
            else if (std::abs (target) > std::abs (platter) && target * platter >= 0) step = accelStep;
            else step = brakeStep;
            platter += clampd (d, -step, step);
            if (std::abs (d) < 1e-4) recovering = false;
        }

        const double wowMod = 1.0 + clampd (p.wow, 0, 1) * (0.004 * wowDrift.process() + 0.0025 * std::sin (angle * 2.0 * kPi));
        const double speedNow = platter * wowMod;
        const double prevAngle = angle;
        angle += speedNow * revsAtNominal / fs;
        angle -= std::floor (angle);
        const bool revolution = (speedNow > 0 && angle < prevAngle) || (speedNow < 0 && angle > prevAngle);
        const double speedAbs = std::min (1.5, std::abs (speedNow));

        // ---------------- record ----------------
        double dl = 0, dr = 0;
        if (hasRecord)
        {
            const double len = (double) record->length();
            if (! atRunout)
            {
                float l, r;
                readRecord (pos, l, r);
                const double needleGain = std::min (1.0, std::abs (speedNow) * 6.0);   // a stopped groove makes no sound
                dl = l * needleGain; dr = r * needleGain;
                pos += speedNow * srcRatio;
                if (pos >= len) { if (p.repeat) pos -= len; else { pos = len - 1; atRunout = true; } }
                else if (pos < 0) { if (p.repeat) pos += len; else pos = 0; }
            }
            else if (speedNow < 0) atRunout = false;   // pulled back out of the run-out groove

            // surface noise travels with the record
            const double hiss = hissHp.process (noiseRng.bi()) * p.hiss * 0.03 * speedAbs;
            if (crackleRng.uni() < crackleProb * speedAbs) { crackleEnv = (0.05 + 0.25 * crackleRng.uni()) * p.crackle; crackleSign = crackleRng.uni() < 0.5 ? -1 : 1; }
            if (crackleRng.uni() < popProb * speedAbs) { crackleEnv = (0.4 + 0.5 * crackleRng.uni()) * p.crackle; crackleSign = crackleRng.uni() < 0.5 ? -1 : 1; }
            if (atRunout && revolution) { crackleEnv = 0.35 * (0.3 + p.crackle); crackleSign = 1; }
            const double click = crackleLp.process (crackleEnv * crackleSign * (0.6 + 0.4 * crackleRng.bi()));
            crackleEnv *= 0.55;
            dl += hiss + click; dr += hiss * 0.9 + click;
        }
        if (p.motorOn || std::abs (platter) > 0.01)
        {
            const double rum = rumbleDrift.process() * p.rumble * 0.02 * std::min (1.0, std::abs (platter));
            dl += rum; dr += rum;
        }

        // cartridge, wear, preamp drive
        for (auto& b : cartL) dl = b.process (dl);
        for (auto& b : cartR) dr = b.process (dr);
        dl = wearL.process (dl); dr = wearR.process (dr);
        const double wearDrive = p.drive + p.wear * 0.3;
        dl = dcL.process (tubeStage (dl, wearDrive));
        dr = dcR.process (tubeStage (dr, wearDrive));
        if (p.mono) { const double m = 0.5 * (dl + dr); dl = dr = m; }

        // ---------------- crossfader mix ----------------
        const double gd = xfDeckSmooth.process (gDeck), gl = xfLineSmooth.process (gLine);
        double ml = dl * gd + (double) inL[i] * gl;
        double mr = dr * gd + (double) inR[i] * gl;

        // ---------------- looper (records the mix, plays on top of it) ----------------
        float lpL, lpR;
        looper.process ((float) ml, (float) mr, lpL, lpR, nominal, p.loopFollowsSpeed);
        ml += lpL * p.loopLevel; mr += lpR * p.loopLevel;

        // ---------------- filter ----------------
        filterSmooth.process (clampd (p.filter, -1, 1));
        lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        if (coeffCounter-- <= 0) { updateFilterCoeffs(); coeffCounter = 16; }
        switch (p.filterType)
        {
            case FilterType::dj:
            {
                const double v = filterSmooth.z;
                const double amt = clampd ((std::abs (v) - 0.02) / 0.06, 0, 1);
                svfL.process (ml); svfR.process (mr);
                if (amt > 0)
                {
                    const double fl = v < 0 ? svfL.lp : svfL.hp, fr = v < 0 ? svfR.lp : svfR.hp;
                    ml += (fl - ml) * amt; mr += (fr - mr) * amt;
                }
                break;
            }
            case FilterType::eq:
                ml = isoHiL.process (isoL.process (ml)); mr = isoHiR.process (isoR.process (mr));
                break;
            case FilterType::wah:
                svfL.process (ml); svfR.process (mr);
                ml = 0.25 * ml + 1.3 * svfL.bp * svfL.k;
                mr = 0.25 * mr + 1.3 * svfR.bp * svfR.k;
                break;
        }

        // ---------------- delay ----------------
        const double dt = dlyTimeSmooth.process (dlySamples);
        const int di = (int) dt; const double df = dt - di;
        auto tap = [&] (const std::vector<float>& b, int d) { return (double) b[(size_t) ((dlyW - d) & dlyMask)]; };
        const double wl = tap (dlyL, di) + (tap (dlyL, di + 1) - tap (dlyL, di)) * df;
        const double wr = tap (dlyR, di) + (tap (dlyR, di + 1) - tap (dlyR, di)) * df;
        dlyL[(size_t) dlyW] = (float) softClip (ml + dlyToneL.process (wl) * fb);
        dlyR[(size_t) dlyW] = (float) softClip (mr + dlyToneR.process (wr) * fb);
        dlyW = (dlyW + 1) & dlyMask;
        ml += wl * p.delayLevel; mr += wr * p.delayLevel;

        // ---------------- speaker ----------------
        if (p.speaker)
        {
            double s = 0.5 * (ml + mr);
            s = spkLp.process (spkPeak.process (spkHp.process (s)));
            s = std::tanh (s * 1.6) / 1.6 * 1.4;
            ml = mr = s;
        }

        const double g = outGainSmooth.process (outGain);
        outL[i] = (float) (ml * g);
        outR[i] = (float) (mr * g);
        pk = std::max (pk, std::max (std::abs (outL[i]), std::abs (outR[i])));
    }
    peak = pk;
}
} // namespace pb
