// Smoke test + screenshot renderer: loads a file, plays it through the full processor (motor start,
// looper record/overdub, filter and delay moves, scratch), writes the result to a WAV and renders the
// editor in every theme to PNG.  Usage: PlatterboxSmoke <audio file> <out dir>
#include "../Source/plugin/PluginProcessor.h"
#include "../Source/plugin/PluginEditor.h"

using namespace juce;

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::printf ("usage: PlatterboxSmoke <audio file> <out dir>\n"); return 2; }
    const File in { String (argv[1]) }, outDir { String (argv[2]) };
    outDir.createDirectory();

    PlatterboxProcessor proc;
    const double fs = 48000.0; const int bs = 512;
    proc.setPlayConfigDetails (2, 2, fs, bs);
    proc.prepareToPlay (fs, bs);
    String err;
    if (! proc.loadFile (in, err)) { std::printf ("load failed: %s\n", err.toRawUTF8()); return 1; }

    auto set = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set ("motor", 1.0f);

    const int seconds = 14, blocks = (int) (seconds * fs / bs);
    AudioBuffer<float> out (2, blocks * bs), buf (2, bs);
    MidiBuffer midi;
    float peak = 0; bool finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        const double t = b * bs / fs;
        // a little performance script
        if (b == (int) (2.0 * fs / bs)) proc.sendLoopCommand (pb::LoopCommand::recPlayDub);
        if (b == (int) (4.0 * fs / bs)) proc.sendLoopCommand (pb::LoopCommand::recPlayDub);   // 2 s loop
        if (b == (int) (4.0 * fs / bs)) set ("crossfader", 1.0f);                               // deck out, loop keeps playing
        if (b == (int) (6.0 * fs / bs)) { set ("crossfader", -1.0f); set ("loopLevel", 0.0f); }
        if (t > 6.0 && t < 8.0) set ("filter", (float) (-(t - 6.0) / 2.0) * 0.9f);              // low-pass sweep
        if (b == (int) (8.0 * fs / bs)) { set ("filter", 0.0f); set ("delayLevel", 0.5f); set ("feedback", 0.6f); }
        if (t > 9.0 && t < 10.5) proc.setScratch (true, std::sin ((t - 9.0) * 2.0 * MathConstants<double>::pi * 2.0) * 1.2);
        if (b == (int) (10.5 * fs / bs)) proc.setScratch (false, 0.0);
        if (b == (int) (12.0 * fs / bs)) set ("motor", 0.0f);                                   // power down: pitch dives
        buf.clear();
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
        {
            out.copyFrom (c, b * bs, buf, c, 0, bs);
            for (int i = 0; i < bs; ++i) { const float v = buf.getSample (c, i); if (! std::isfinite (v)) finite = false; peak = jmax (peak, std::abs (v)); }
        }
    }
    std::printf ("position %.2f s, platter %.2f\n", proc.getEngine().positionSeconds(), proc.getEngine().platterSpeed());
    std::printf ("rendered %d s, peak %.3f, finite %s, loop %.2f s\n", seconds, peak, finite ? "yes" : "NO", proc.getEngine().getLooper().lengthSeconds());

    WavAudioFormat wav;
    auto f = outDir.getChildFile ("smoke-render.wav"); f.deleteFile();
    if (auto stream = std::unique_ptr<FileOutputStream> (f.createOutputStream()))
        if (auto w = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (stream.get(), fs, 2, 24, {}, 0)))
        {
            stream.release();
            w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }

    // screenshots: put the deck in an interesting state first
    set ("motor", 1.0f); set ("filter", -0.3f); set ("delayLevel", 0.4f); set ("crossfader", -0.2f);
    for (int b = 0; b < 300; ++b) { buf.clear(); proc.processBlock (buf, midi); }
    std::printf ("position %.2f s, platter %.2f\n", proc.getEngine().positionSeconds(), proc.getEngine().platterSpeed());
    for (int th = 0; th < (int) themes().size(); ++th)
    {
        proc.setTheme (th);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto pf = outDir.getChildFile (String ("screenshot-") + String (themes()[(size_t) th].name).toLowerCase() + ".png");
        pf.deleteFile();
        FileOutputStream os (pf);
        PNGImageFormat().writeImageToStream (img, os);
    }
    std::printf ("screenshots written\n");
    return finite ? 0 : 1;
}
