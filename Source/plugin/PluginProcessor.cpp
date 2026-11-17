#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // id -> value in parameter units
};

// Values not listed fall back to the parameter default.
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Fresh Pressing", {} },
        { "Dusty Crate",     { { "crackle", 0.6f }, { "hiss", 0.35f }, { "wear", 0.45f }, { "wow", 0.35f }, { "drive", 0.3f } } },
        { "Beach Speaker",   { { "crackle", 0.35f }, { "speaker", 1.0f }, { "cartridge", 0.0f }, { "wow", 0.4f }, { "mono", 1.0f } } },
        { "Lo-Fi Ceramic",   { { "cartridge", 0.0f }, { "wear", 0.7f }, { "crackle", 0.5f }, { "drive", 0.55f }, { "filter", -0.35f } } },
        { "Wah Loop Jam",    { { "filterType", 2.0f }, { "filter", 0.0f }, { "resonance", 0.55f }, { "modDepth", 0.6f }, { "modRate", 0.35f } } },
        { "Dub Throw",       { { "delaySync", 1.0f }, { "delayDiv", 6.0f }, { "feedback", 0.7f }, { "delayLevel", 0.6f }, { "filter", 0.25f } } },
        { "Bass Isolator",   { { "filterType", 1.0f }, { "filter", 0.6f }, { "crackle", 0.15f } } },
        { "Warped 78",       { { "speed", 2.0f }, { "cutAt", 2.0f }, { "wow", 0.9f }, { "wear", 0.8f }, { "crackle", 0.8f }, { "cartridge", 0.0f }, { "mono", 1.0f } } },
    };
    return list;
}
}

AudioProcessorValueTreeState::ParameterLayout PlatterboxProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit = "")
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f), def,
                                                            AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto boolean = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    // turntable
    boolean ("motor", "Start/Stop", false);
    choice ("speed", "Speed", { "33", "45", "78" }, 0);
    choice ("cutAt", "Files Cut At", { "33", "45", "78" }, 0);
    flt ("pitch", "Pitch", -1.0f, 1.0f, 0.0f);
    choice ("pitchRange", "Pitch Range", { "8%", "16%", "50%" }, 0);
    boolean ("reverse", "Reverse", false);
    boolean ("repeat", "Repeat", true);
    flt ("startTime", "Start Time", 0.05f, 3.0f, 0.7f, "s");
    flt ("brakeTime", "Brake Time", 0.05f, 4.0f, 0.6f, "s");
    // vinyl
    flt ("crackle", "Crackle", 0, 1, 0.25f);
    flt ("hiss", "Hiss", 0, 1, 0.15f);
    flt ("rumble", "Rumble", 0, 1, 0.15f);
    flt ("wow", "Wow", 0, 1, 0.25f);
    flt ("wear", "Wear", 0, 1, 0.15f);
    flt ("drive", "Drive", 0, 1, 0.2f);
    choice ("cartridge", "Cartridge", { "Ceramic", "Magnetic", "Clean" }, 1);
    boolean ("mono", "Mono", false);
    // mixer
    flt ("deckLevel", "Deck Level", 0, 1.5f, 1.0f);
    flt ("lineLevel", "Line Level", 0, 1.5f, 1.0f);
    flt ("crossfader", "Crossfader", -1, 1, -1.0f);
    boolean ("hamster", "Hamster", false);
    choice ("faderCurve", "Fader Curve", { "Smooth", "Sharp" }, 1);
    // filter
    choice ("filterType", "Filter Type", { "DJ", "EQ", "Wah" }, 0);
    flt ("filter", "Filter", -1, 1, 0.0f);
    flt ("resonance", "Resonance", 0, 1, 0.3f);
    flt ("modRate", "Mod Rate", 0, 1, 0.3f);
    flt ("modDepth", "Mod Depth", 0, 1, 0.0f);
    // looper
    boolean ("loopFollow", "Loop Follows Speed", true);
    boolean ("loopSync", "Loop Sync", false);
    flt ("loopLevel", "Loop Level", 0, 1.5f, 1.0f);
    // delay
    flt ("delayMs", "Delay Time", 10.0f, 1500.0f, 375.0f, "ms");
    boolean ("delaySync", "Delay Sync", false);
    StringArray divs; for (auto* d : pb::kDelayDivisionNames) divs.add (d);
    choice ("delayDiv", "Delay Division", divs, 5);
    flt ("feedback", "Feedback", 0, 0.95f, 0.4f);
    flt ("delayLevel", "Delay Level", 0, 1, 0.0f);
    // output
    boolean ("speaker", "Speaker", false);
    flt ("output", "Output", -24.0f, 12.0f, 0.0f, "dB");
    return { p.begin(), p.end() };
}

PlatterboxProcessor::PlatterboxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Line In", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Platterbox", createLayout())
{
    formats.registerBasicFormats();
    startTimerHz (4);
}

PlatterboxProcessor::~PlatterboxProcessor() { stopTimer(); }

bool PlatterboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet(), in = layouts.getMainInputChannelSet();
    const bool okOut = out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
    const bool okIn = in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return okOut && okIn;
}

void PlatterboxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setRecord (recordInUse.load());
}

pb::Params PlatterboxProcessor::readParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&] (const char* id) { return v (id) > 0.5f; };
    auto i = [&] (const char* id) { return (int) std::lround (v (id)); };
    pb::Params p;
    p.motorOn = b ("motor"); p.speed = i ("speed"); p.cutAt = i ("cutAt"); p.pitch = v ("pitch"); p.pitchRange = i ("pitchRange");
    p.reverse = b ("reverse"); p.repeat = b ("repeat"); p.startTime = v ("startTime"); p.brakeTime = v ("brakeTime");
    p.crackle = v ("crackle"); p.hiss = v ("hiss"); p.rumble = v ("rumble"); p.wow = v ("wow"); p.wear = v ("wear"); p.drive = v ("drive");
    p.cartridge = (pb::Cartridge) i ("cartridge"); p.mono = b ("mono");
    p.deckLevel = v ("deckLevel"); p.lineLevel = v ("lineLevel"); p.crossfader = v ("crossfader"); p.hamster = b ("hamster"); p.faderCurve = i ("faderCurve");
    p.filterType = (pb::FilterType) i ("filterType"); p.filter = v ("filter"); p.resonance = v ("resonance"); p.modRate = v ("modRate"); p.modDepth = v ("modDepth");
    p.loopFollowsSpeed = b ("loopFollow"); p.loopSync = b ("loopSync"); p.loopLevel = v ("loopLevel");
    p.delayMs = v ("delayMs"); p.delaySync = b ("delaySync"); p.delayDiv = i ("delayDiv"); p.feedback = v ("feedback"); p.delayLevel = v ("delayLevel");
    p.speaker = b ("speaker"); p.outputDb = v ("output");
    return p;
}

void PlatterboxProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();

    for (int c = inCh; c < outCh; ++c) buffer.clear (c, 0, n);
    if (inCh == 1 && outCh > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);   // mono line in feeds both sides

    // a newly loaded record? (the lock is only contended for an instant while the timer tidies up)
    if (hasPending.load())
    {
        const SpinLock::ScopedTryLockType lock (recordLock);
        if (lock.isLocked())
            if (auto* r = pendingRecord.exchange (nullptr))
            {
                engine.setRecord (r);
                recordInUse.store (r);
                hasPending.store (false);
            }
    }

    // UI commands
    {
        const auto scope = cmdFifo.read (cmdFifo.getNumReady());
        for (int k = 0; k < scope.blockSize1; ++k) engine.loopCommand ((pb::LoopCommand) cmdBuf[(size_t) (scope.startIndex1 + k)]);
        for (int k = 0; k < scope.blockSize2; ++k) engine.loopCommand ((pb::LoopCommand) cmdBuf[(size_t) (scope.startIndex2 + k)]);
    }
    const double seek = seekRequest.exchange (-1.0);
    if (seek >= 0) engine.seek (seek);
    engine.setScratch (handOn.load(), handVelocity.load());

    auto p = readParams();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) p.tempoBpm = *bpm;
    engine.setParams (p);

    float* L = buffer.getWritePointer (0);
    float* R = outCh > 1 ? buffer.getWritePointer (1) : L;
    engine.process (L, R, L, R, n);   // with a mono output L == R; the engine reads input before writing
}

void PlatterboxProcessor::sendLoopCommand (pb::LoopCommand c)
{
    const auto scope = cmdFifo.write (1);
    if (scope.blockSize1 > 0) cmdBuf[(size_t) scope.startIndex1] = (int) c;
    else if (scope.blockSize2 > 0) cmdBuf[(size_t) scope.startIndex2] = (int) c;
}

bool PlatterboxProcessor::loadFile (const File& f, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) { error = "Can't read " + f.getFileName() + " (supported: " + formats.getWildcardForAllFormats() + ")"; return false; }
    const auto maxSamples = (int64) (reader->sampleRate * 60.0 * 30.0);   // 30 minutes, about one LP
    if (reader->lengthInSamples <= 0) { error = f.getFileName() + " is empty"; return false; }
    const int len = (int) jmin (reader->lengthInSamples, maxSamples);

    AudioBuffer<float> tmp ((int) jmax (1u, jmin (2u, reader->numChannels)), len);
    reader->read (&tmp, 0, len, 0, true, tmp.getNumChannels() > 1);

    auto rec = std::make_unique<pb::RecordData>();
    rec->sampleRate = reader->sampleRate;
    rec->name = f.getFileNameWithoutExtension().toStdString();
    rec->left.assign (tmp.getReadPointer (0), tmp.getReadPointer (0) + len);
    if (tmp.getNumChannels() > 1) rec->right.assign (tmp.getReadPointer (1), tmp.getReadPointer (1) + len);
    else rec->right = rec->left;

    // overview for the waveform strip
    const int bins = 1024;
    overview.assign ((size_t) bins, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((int64) len * b / bins), s1 = jmax (s0 + 1, (int) ((int64) len * (b + 1) / bins));
        float mx = 0;
        for (int s = s0; s < s1 && s < len; ++s) mx = jmax (mx, std::abs (rec->left[(size_t) s]), std::abs (rec->right[(size_t) s]));
        overview[(size_t) b] = jmin (1.0f, mx);
    }

    recordName = f.getFileNameWithoutExtension();
    recordFile = f;
    recordSeconds = rec->seconds();

    auto* raw = rec.get();
    const SpinLock::ScopedLockType lock (recordLock);
    owned.push_back (std::move (rec));
    pendingRecord.store (raw);   // a pending record that was never picked up is freed by the timer
    hasPending.store (true);
    return true;
}

void PlatterboxProcessor::timerCallback()
{
    // free every record that is neither playing nor waiting to be picked up (deleted outside the lock)
    std::vector<std::unique_ptr<pb::RecordData>> toFree;
    {
        const SpinLock::ScopedLockType lock (recordLock);
        auto* inUse = recordInUse.load();
        auto* pending = pendingRecord.load();
        for (auto& r : owned)
            if (r != nullptr && r.get() != inUse && r.get() != pending) toFree.push_back (std::move (r));
        owned.erase (std::remove (owned.begin(), owned.end(), nullptr), owned.end());
    }
}

int PlatterboxProcessor::getNumPrograms() { return (int) presets().size(); }

const String PlatterboxProcessor::getProgramName (int index)
{
    return isPositiveAndBelow (index, (int) presets().size()) ? String (presets()[(size_t) index].name) : String();
}

void PlatterboxProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    // presets set the sound, not the transport: leave motor, speed position and the crossfader alone
    static const StringArray keep { "motor", "crossfader", "pitch", "reverse", "repeat" };
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (! keep.contains (r->getParameterID())) r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, value] : presets()[(size_t) index].values)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (value));
}

void PlatterboxProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("theme", theme.load(), nullptr);
    state.setProperty ("recordFile", recordFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PlatterboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            theme.store ((int) tree.getProperty ("theme", 0));
            const String path = tree.getProperty ("recordFile", "").toString();
            apvts.replaceState (tree);
            if (path.isNotEmpty() && File::isAbsolutePath (path))
            {
                const File f (path);
                String err;
                if (f.existsAsFile() && (recordFile != f)) loadFile (f, err);
            }
        }
}

AudioProcessorEditor* PlatterboxProcessor::createEditor() { return new PlatterboxEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlatterboxProcessor(); }
