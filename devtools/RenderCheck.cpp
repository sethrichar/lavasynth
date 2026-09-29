// Offline render of the full plugin: CPU cost, peak/RMS, NaN check, optional WAV.
// Build: cmake -DROTOR_BUILD_DEVTOOLS=ON …
// Run:   ./RotorRender [out.wav] [--plain] [paramId=value …]
//        --plain skips the busy demo patch (defaults only); key=value sets any parameter (real units).
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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
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
