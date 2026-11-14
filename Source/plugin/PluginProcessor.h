#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"

class PlatterboxProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PlatterboxProcessor();
    ~PlatterboxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- message-thread API for the editor ----
    bool loadFile (const juce::File& f, juce::String& error);
    juce::String getRecordName() const { return recordName; }
    juce::File getRecordFile() const { return recordFile; }
    double getRecordSeconds() const { return recordSeconds; }
    const std::vector<float>& getOverview() const { return overview; }   // peak per bin, 0..1
    juce::AudioFormatManager& getFormats() { return formats; }

    void sendLoopCommand (pb::LoopCommand c);
    void requestSeek (double seconds) { seekRequest.store (seconds); }
    void setScratch (bool touching, double revsPerSecond) { handOn.store (touching); handVelocity.store (revsPerSecond); }

    int getTheme() const { return theme.load(); }
    void setTheme (int t) { theme.store (t); }

    const pb::Engine& getEngine() const { return engine; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override;   // frees records the audio thread no longer uses
    pb::Params readParams() const;

    pb::Engine engine;
    juce::AudioFormatManager formats;

    // record ownership: the message thread owns every RecordData; the audio thread only borrows
    std::vector<std::unique_ptr<pb::RecordData>> owned;
    std::atomic<pb::RecordData*> pendingRecord { nullptr };
    std::atomic<pb::RecordData*> recordInUse { nullptr };
    std::atomic<bool> hasPending { false };
    juce::SpinLock recordLock;
    juce::String recordName;
    juce::File recordFile;
    double recordSeconds = 0;
    std::vector<float> overview;

    // commands from the UI (single producer: the message thread)
    juce::AbstractFifo cmdFifo { 64 };
    std::array<int, 64> cmdBuf {};
    std::atomic<double> seekRequest { -1.0 };
    std::atomic<bool> handOn { false };
    std::atomic<double> handVelocity { 0.0 };
    std::atomic<int> theme { 0 };

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlatterboxProcessor)
};
