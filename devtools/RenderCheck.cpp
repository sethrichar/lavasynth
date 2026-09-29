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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    double seconds = 12.0;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]).startsWith ("--seconds="))
            seconds = juce::String (argv[a]).fromFirstOccurrenceOf ("=", false, false).getDoubleValue();

    RotorAudioProcessor p;
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);

    bool plain = false;
    for (int a = 2; a < argc; ++a)
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
    }
    for (int a = 2; a < argc; ++a)
    {
        const juce::String arg (argv[a]);
        if (arg.contains ("=") && ! arg.startsWith ("--"))
            set (p, arg.upToFirstOccurrenceOf ("=", false, false), arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    }

    const int totalBlocks = (int) (seconds * sr / block);
    juce::AudioBuffer<float> buffer (2, block);
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
            for (int n : chords[c])
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
        }
        if (b % blocksPerChord == blocksPerChord / 2)
        {
            const int c = (b / blocksPerChord) % 4;
            for (int n : chords[c])
                midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        }
        // Second half: switch to Legato unison with the bandpass filter and heavier fold.
        if (b == totalBlocks / 2 && ! plain)
        {
            set (p, "voiceMode", 4);
            set (p, "filterMode", 1);
            set (p, "fold", 0.8f);
        }

        p.processBlock (buffer, midi);
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
