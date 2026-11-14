#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

struct Theme
{
    const char* name;
    juce::Colour body, panel, edge, ink, inkDim, accent, accent2, platter, mat, label, knob;
};
const std::vector<Theme>& themes();

class PlatterboxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setTheme (const Theme& t);
    const Theme& theme() const { return *th; }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.0f, juce::Font::bold)); }

private:
    const Theme* th = &themes()[0];
};

// The platter: shows the spinning record, tonearm and strobe, and takes the hand for scratching.
class PlatterView : public juce::Component
{
public:
    explicit PlatterView (PlatterboxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();   // called from the editor timer
    const Theme* theme = nullptr;

private:
    float angleAt (juce::Point<float>) const;
    PlatterboxProcessor& proc;
    bool holding = false;
    float lastAngle = 0;
    double lastTime = 0;
    double velocity = 0;   // rev/s from the hand
    int idleTicks = 0;
};

// Overview of the loaded file with a playhead; click to drop the needle.
class WaveStrip : public juce::Component
{
public:
    explicit WaveStrip (PlatterboxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { seekTo (e.position.x); }
    void mouseDrag (const juce::MouseEvent& e) override { seekTo (e.position.x); }
    const Theme* theme = nullptr;

private:
    void seekTo (float x);
    PlatterboxProcessor& proc;
};

class PlatterboxEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit PlatterboxEditor (PlatterboxProcessor&);
    ~PlatterboxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void applyTheme (int index);
    void loadFile (const juce::File&);

    struct Knob
    {
        juce::Slider slider;
        juce::String label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    Knob& knob (const juce::String& id, const juce::String& label, juce::Slider::SliderStyle style = juce::Slider::RotaryVerticalDrag);
    juce::ToggleButton& toggle (const juce::String& id, const juce::String& label);
    juce::ComboBox& combo (const juce::String& id);
    juce::TextButton& button (const juce::String& text, std::function<void()> onClick);
    void placeKnob (Knob&, juce::Rectangle<int>);
    void section (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    PlatterboxProcessor& proc;
    PlatterboxLookAndFeel lnf;
    PlatterView platter;
    WaveStrip wave;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttach;
    std::map<juce::String, Knob*> knobById;
    std::map<juce::String, juce::Component*> byId;

    juce::TextButton loadButton { "LOAD" }, motorButton { "START / STOP" };
    std::array<juce::TextButton, 3> speedButtons;
    juce::TextButton loopMain { "REC" }, loopStop { "STOP" }, loopClear { "CLEAR" }, loopUndo { "UNDO" },
                     loopHalf { "1/2" }, loopDouble { "x2" }, loopRev { "REV" };
    juce::ComboBox presetBox, themeBox;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String status;
    float meter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlatterboxEditor)
};
