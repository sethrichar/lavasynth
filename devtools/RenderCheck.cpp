// Offline render of the full plugin: CPU cost, peak/RMS, NaN check, optional WAV.
// Build: cmake -DROTOR_BUILD_DEVTOOLS=ON …
// Run:   ./RotorRender [out.wav] [--plain] [paramId=value …]
//        --plain skips the busy demo patch (defaults only); key=value sets any parameter (real units).
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <chrono>
#include <cstdio>

namespace
{
    void set (RotorAudioProcessor& p, const juce::String& id, float value)
    {
        auto* param = p.getState().getParameter (id);
        if (param == nullptr)
        {
            std::printf ("unknown parameter: %s\n", id.toRawUTF8());
            return;
        }
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
} // namespace

// --pitch-check: plays A4 on MIDI channel 2 with a +12 semitone per-note bend (MPE) and
// reports the output frequency, with MPE on (expect 880 Hz) and off (bend range ±2 → ~452 Hz).
static double measureBentPitch (bool mpeOn, int bendValue = 8192 + 2048, int note = 69)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    RotorAudioProcessor p;
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    auto setParam = [&p] (const char* id, float v)
    {
        auto* param = p.getState().getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    };
    setParam ("mpe", mpeOn ? 1.0f : 0.0f);
    for (auto id : { "voice1Waveform", "voice2Waveform", "voice3Waveform", "voice4Waveform", "voice5Waveform" })
        setParam (id, 4.0f); // sine
    setParam ("cutoff", 20000.0f);
    setParam ("resonance", 0.0f);
    setParam ("sustain", 1.0f);

    juce::AudioBuffer<float> buffer (2, block);
    int crossings = 0, counted = 0;
    float prev = 0.0f;
    for (int b = 0; b < (int) (2.0 * sr / block); ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
        {
            midi.addEvent (juce::MidiMessage::noteOn (2, note, 0.8f), 0);
            midi.addEvent (juce::MidiMessage::pitchWheel (2, bendValue), 1); // default: 12/48 of full range
        }
        p.processBlock (buffer, midi);
        if (b * block >= sr) // count over the second second
            for (int i = 0; i < block; ++i)
            {
                const float y = buffer.getSample (0, i);
                if (prev < 0.0f && y >= 0.0f) ++crossings;
                prev = y;
                ++counted;
            }
    }
    return crossings * sr / counted;
}

// --check-presets: end-to-end checks of the preset system through the real processor.
static int checkPresets()
{
    using rotor::presets::Row;
    int failures = 0;
    auto expect = [&failures] (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
        failures += ok ? 0 : 1;
    };

    RotorAudioProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    auto& pm = p.getPresetManager();

    // 1. Every parameter belongs to a row or is a known global option.
    int counts[5] {};
    for (const auto& [id, v] : pm.snapshot())
    {
        const auto r = rotor::presets::rowOf (id);
        ++counts[(int) r];
        if (r == Row::unknown)
            std::printf ("      unclassified parameter: %s\n", id.c_str());
    }
    std::printf ("      row 1: %d, row 2: %d, row 3: %d, global: %d\n", counts[0], counts[1], counts[2], counts[3]);
    expect (counts[4] == 0, "every parameter is classified");

    // 2. Every factory preset loads and renders finite audio at a sane level.
    juce::AudioBuffer<float> buffer (2, 256);
    for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
    {
        pm.loadPreset (i);
        double peak = 0.0;
        bool finite = true;
        for (int b = 0; b < 400; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            if (b == 250)
                for (int n : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            p.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < 256; ++s)
                {
                    const float y = buffer.getSample (ch, s);
                    finite = finite && std::isfinite (y);
                    peak = std::max (peak, (double) std::abs (y));
                }
        }
        const auto name = pm.getPresetNames()[i];
        std::printf ("      %-16s peak %.2f\n", name.toRawUTF8(), peak);
        expect (finite && peak > 0.01 && peak < 1.5, ("factory preset renders: " + name).toRawUTF8());
    }

    // 3. Row vs FULL recall.
    auto value = [&p] (const char* id) { auto* x = p.getState().getParameter (id); return x->convertFrom0to1 (x->getValue()); };
    auto setv = [&p] (const char* id, float v) { auto* x = p.getState().getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    pm.loadPreset (0);
    setv ("cutoff", 500.0f);
    setv ("wow", 0.4f);
    pm.setFullMode (true);
    pm.storeSlot (Row::shaping, 3); // FULL: stores all rows in slot 3
    setv ("cutoff", 3000.0f);
    setv ("wow", 0.9f);
    pm.setFullMode (false);
    pm.recallSlot (Row::shaping, 3); // ROW: only row 2 comes back
    expect (std::abs (value ("cutoff") - 500.0f) < 1.0f && std::abs (value ("wow") - 0.9f) < 1e-3f, "ROW recall restores only its row");
    pm.setFullMode (true);
    pm.recallSlot (Row::shaping, 3);
    expect (std::abs (value ("wow") - 0.4f) < 1e-3f, "FULL recall restores every row");

    // 4. The row memory and preset name survive a save/restore of the plugin state (a DAW session).
    juce::MemoryBlock session;
    p.getStateInformation (session);
    RotorAudioProcessor q;
    q.setStateInformation (session.getData(), (int) session.getSize());
    expect (q.getPresetManager().isSlotFilled (Row::motion, 3), "row slots are saved with the session");
    expect (std::abs (q.getState().getParameter ("cutoff")->convertFrom0to1 (q.getState().getParameter ("cutoff")->getValue()) - 500.0f) < 1.0f,
            "parameters are saved with the session");

    // 5. Old (v1.8) sessions — a bare parameter tree — still load.
    {
        juce::MemoryBlock old;
        if (auto xml = p.getState().copyState().createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
        RotorAudioProcessor r;
        r.setStateInformation (old.getData(), (int) old.getSize());
        expect (std::abs (r.getState().getParameter ("cutoff")->convertFrom0to1 (r.getState().getParameter ("cutoff")->getValue()) - 500.0f) < 1.0f,
                "v1.8-format sessions still load");
    }

    // 6. User presets save to and load from files; global options stay put.
    setv ("cutoff", 777.0f);
    setv ("mpe", 1.0f);
    expect (pm.saveUserPreset ("zz RenderCheck Test"), "user preset saves");
    setv ("cutoff", 2000.0f);
    setv ("mpe", 0.0f);
    const int idx = pm.getPresetNames().indexOf ("zz RenderCheck Test");
    pm.loadPreset (idx);
    expect (std::abs (value ("cutoff") - 777.0f) < 1.0f, "user preset loads");
    expect (value ("mpe") < 0.5f, "loading a preset leaves the global options alone");
    PresetManager::getUserPresetFolder().getChildFile ("zz RenderCheck Test.rotorpreset").deleteFile();

    std::printf ("%s\n", failures == 0 ? "ALL PRESET CHECKS PASSED" : "PRESET CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}

// --screenshot out.png [--preset=N] [--options] [param=value …]: renders the editor at full size.
static int screenshot (int argc, char** argv)
{
    RotorAudioProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    juce::String out = "screenshot.png";
    bool showOptions = false;
    for (int a = 1; a < argc; ++a)
    {
        const juce::String arg (argv[a]);
        if (arg.endsWith (".png")) out = arg;
        else if (arg.startsWith ("--preset=")) p.getPresetManager().loadPreset (arg.fromFirstOccurrenceOf ("=", false, false).getIntValue());
        else if (arg == "--options") showOptions = true;
        else if (arg.contains ("=") && ! arg.startsWith ("--"))
            if (auto* param = p.getState().getParameter (arg.upToFirstOccurrenceOf ("=", false, false)))
                param->setValueNotifyingHost (param->convertTo0to1 (arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
    }
    // A couple of row slots filled so the strips show state.
    p.getPresetManager().setFullMode (false);
    p.getPresetManager().storeSlot (rotor::presets::Row::voices, 0);
    p.getPresetManager().storeSlot (rotor::presets::Row::motion, 2);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    editor->setSize (RotorEditor::designWidth, RotorEditor::designHeight);
    if (showOptions)
    {
        std::function<void (juce::Component&)> find = [&find] (juce::Component& c)
        {
            if (auto* b = dynamic_cast<juce::TextButton*> (&c); b != nullptr && b->getButtonText() == "OPTIONS")
                b->onClick(); // run the click handler synchronously
            for (auto* child : c.getChildren())
                find (*child);
        };
        find (*editor);
    }
    // Snapshot paints synchronously; no event loop needed.
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (out);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("%s %s (%d x %d)\n", ok ? "wrote" : "FAILED", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    return ok ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--screenshot")
            return screenshot (argc, argv);
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--check-presets")
            return checkPresets();
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--pitch-check")
        {
            std::printf ("MPE on:  %.0f Hz (expect 880)\n", measureBentPitch (true));
            std::printf ("MPE off: %.0f Hz (expect ~452: channel bend, ±2 st range)\n", measureBentPitch (false));
            std::printf ("no bend A4: %.0f Hz, A5 unbent: %.0f Hz\n", measureBentPitch (true, 8192), measureBentPitch (true, 8192, 81));
            return 0;
        }
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    double seconds = 12.0;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]).startsWith ("--seconds="))
            seconds = juce::String (argv[a]).fromFirstOccurrenceOf ("=", false, false).getDoubleValue();

    bool extTest = false, mpeTest = false;
    for (int a = 1; a < argc; ++a)
    {
        extTest = extTest || juce::String (argv[a]) == "--ext";
        mpeTest = mpeTest || juce::String (argv[a]) == "--mpe";
    }

    RotorAudioProcessor p;
    if (extTest)
    {
        // Enable the stereo sidechain: 2 in, 2 out.
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        if (! p.setBusesLayout (layout))
        {
            std::printf ("could not enable the sidechain\n");
            return 1;
        }
    }
    p.setPlayConfigDetails (extTest ? 2 : 0, 2, sr, block);
    p.prepareToPlay (sr, block);

    bool plain = false;
    for (int a = 1; a < argc; ++a)
        plain = plain || juce::String (argv[a]) == "--plain";

    // A busy patch: everything on.
    if (! plain)
    {
    set (p, "voiceMode", 0);
    set (p, "oscLevel", 0.8f);
    set (p, "subLevel", 0.6f);
    set (p, "noiseLevel", 0.3f);
    set (p, "noiseColor", 0.5f);
    set (p, "cutoff", 1200.0f);
    set (p, "resonance", 0.7f);
    set (p, "keyTrack", 0.5f);
    set (p, "fold", 0.4f);
    set (p, "release", 1.5f);
    set (p, "modDecay", 0.4f);
    set (p, "modSustain", 0.2f);
    set (p, "modEnvToCutoff", 0.4f);
    set (p, "lfoShape", 0);          // Volcano
    set (p, "lfoRate", 0.7f);
    set (p, "lfoKeyTrack", 0.5f);
    set (p, "lfoToCutoff", 0.2f);
    set (p, "lfoToPd", 0.4f);
    set (p, "spread", 0.8f);
    set (p, "lfoToSpread", 0.3f);
    set (p, "wow", 0.3f);
    set (p, "flutter", 0.2f);
    set (p, "noteDetune", 0.3f);
    set (p, "reelDrag", 0.4f);
    set (p, "chaos", 0.2f);
    set (p, "envScatter", 0.3f);
    set (p, "reverbMix", 0.3f);
    set (p, "reverbAmount", 0.6f);
    set (p, "driveMix", 0.25f);
    set (p, "driveAmount", 0.4f);
    set (p, "lfoToReverbMix", 0.2f);
    set (p, "lfoToFxColor", -0.4f);
    }
    for (int a = 1; a < argc; ++a)
    {
        const juce::String arg (argv[a]);
        if (arg.contains ("=") && ! arg.startsWith ("--"))
            set (p, arg.upToFirstOccurrenceOf ("=", false, false), arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    }

    const int totalBlocks = (int) (seconds * sr / block);
    juce::AudioBuffer<float> buffer (2, block);
    double extPhase = 0.0;
    double heldEnergy = 0.0, silentEnergy = 0.0;
    juce::AudioBuffer<float> recording (2, totalBlocks * block);

    const int chords[4][5] = { { 48, 55, 60, 64, 67 }, { 45, 52, 57, 60, 64 }, { 41, 48, 53, 57, 60 }, { 43, 50, 55, 59, 62 } };
    double peak = 0.0, sumSq = 0.0, sideSq = 0.0;
    bool finite = true;
    auto start = std::chrono::steady_clock::now();

    for (int b = 0; b < totalBlocks; ++b)
    {
        juce::MidiBuffer midi;
        const int blocksPerChord = totalBlocks / 8;
        if (b % blocksPerChord == 0)
        {
            const int c = (b / blocksPerChord) % 4;
            int ch = 2;
            for (int n : chords[c])
                midi.addEvent (juce::MidiMessage::noteOn (mpeTest ? ch++ : 1, n, 0.8f), 0);
        }
        if (b % blocksPerChord == blocksPerChord / 2)
        {
            const int c = (b / blocksPerChord) % 4;
            int ch = 2;
            for (int n : chords[c])
                midi.addEvent (juce::MidiMessage::noteOff (mpeTest ? ch++ : 1, n), 0);
        }
        // Second half: switch to Legato unison with the bandpass filter and heavier fold.
        if (b == totalBlocks / 2 && ! plain)
        {
            set (p, "voiceMode", 4);
            set (p, "filterMode", 1);
            set (p, "fold", 0.8f);
        }

        if (extTest)
        {
            // Sidechain: 220 Hz left, 330 Hz right (written into the shared in/out buffer).
            for (int i = 0; i < block; ++i, extPhase += 1.0 / sr)
            {
                buffer.setSample (0, i, 0.5f * (float) std::sin (2.0 * 3.141592653589793 * 220.0 * extPhase));
                buffer.setSample (1, i, 0.5f * (float) std::sin (2.0 * 3.141592653589793 * 330.0 * extPhase));
            }
        }
        if (mpeTest && b % (totalBlocks / 8) == 10)
        {
            // Bend each member channel differently: +12 st on ch 2, −12 st on ch 3.
            midi.addEvent (juce::MidiMessage::pitchWheel (2, 8192 + 2048), 0);
            midi.addEvent (juce::MidiMessage::pitchWheel (3, 8192 - 2048), 0);
            midi.addEvent (juce::MidiMessage::channelPressureChange (2, 127), 0);
        }

        p.processBlock (buffer, midi);

        // For --ext: energy while a chord is held vs while everything has released.
        const int phaseInChord = b % blocksPerChord;
        double e = 0.0;
        for (int i = 0; i < block; ++i)
            e += buffer.getSample (0, i) * buffer.getSample (0, i);
        if (phaseInChord > 5 && phaseInChord < blocksPerChord / 2)
            heldEnergy += e;
        else if (phaseInChord > blocksPerChord - 5)
            silentEnergy += e;
        for (int i = 0; i < block; ++i)
        {
            const double side = 0.5 * (buffer.getSample (0, i) - buffer.getSample (1, i));
            sideSq += side * side;
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            recording.copyFrom (ch, b * block, buffer, ch, 0, block);
            for (int i = 0; i < block; ++i)
            {
                const double s = buffer.getSample (ch, i);
                finite = finite && std::isfinite (s);
                peak = std::max (peak, std::abs (s));
                sumSq += s * s;
            }
        }
    }

    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const double rms = std::sqrt (sumSq / (2.0 * totalBlocks * block));
    if (extTest)
        std::printf ("ext: held %.1f dB, released %.1f dB\n", 10.0 * std::log10 (heldEnergy + 1e-20),
                     10.0 * std::log10 (silentEnergy + 1e-20));
    std::printf ("rendered %.1f s in %.3f s  →  %.1f%% of one core (realtime = 100%%)\n", seconds, elapsed, 100.0 * elapsed / seconds);
    const double sideRms = std::sqrt (sideSq / (totalBlocks * block));
    std::printf ("stereo width: side %.1f dB below the total\n", 20.0 * std::log10 (rms / std::max (sideRms, 1e-12)));
    std::printf ("peak %.3f (%.1f dBFS), rms %.3f (%.1f dBFS), finite: %s\n",
                 peak, 20.0 * std::log10 (peak), rms, 20.0 * std::log10 (rms), finite ? "yes" : "NO");

    if (argc > 1 && ! juce::String (argv[1]).startsWith ("-") && ! juce::String (argv[1]).contains ("="))
    {
        juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
        out.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (out);
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions {}
                                                       .withSampleRate (sr)
                                                       .withNumChannels (2)
                                                       .withBitsPerSample (24));
        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer (recording, 0, recording.getNumSamples());
            std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
        }
    }
    return finite ? 0 : 1;
}
