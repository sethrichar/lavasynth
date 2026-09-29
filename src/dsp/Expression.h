#pragma once

#include <algorithm>
#include <array>

namespace rotor
{

// What a voice needs from the performer's expression, per note.
struct Expression
{
    double bendSemitones = 0.0; // pitch bend (manager/global + per-note in MPE)
    double pressure = 0.0;      // aftertouch 0..1
    double timbre = 0.5;        // MPE timbre (CC74) 0..1, 0.5 = neutral
};

// Tracks pitch bend, pressure, timbre and the mod wheel per MIDI channel. Pure logic.
//
// Normal MIDI: every note gets the (any-channel) bend, channel pressure and CC74.
// MPE (lower zone): channel 1 is the manager (its bend/pressure apply to all notes);
//   channels 2–16 carry per-note bend (default ±48 semitones), pressure and timbre.
class MidiExpression
{
public:
    // OPEN: the hardware's pitch-bend range in normal MIDI mode (assumed ±2 semitones).
    static constexpr double defaultBendRange = 2.0;
    static constexpr double defaultMpeNoteBendRange = 48.0;

    void setMpe (bool enabled)
    {
        if (enabled != mpe)
            reset();
        mpe = enabled;
    }
    bool isMpe() const { return mpe; }

    void setBendRange (double semitones) { bendRange = semitones; }
    void setMpeNoteBendRange (double semitones) { noteBendRange = semitones; }

    void reset()
    {
        bend.fill (0.0);
        pressure.fill (0.0);
        timbre.fill (0.5);
        wheel = 0.0;
    }

    // channel is 1..16. value14 is the raw 14-bit bend (0..16383, 8192 = centre).
    // In normal MIDI mode every channel writes the same (shared) slot: the latest message wins.
    void pitchBend (int channel, int value14)
    {
        bend[slot (channel)] = std::clamp ((value14 - 8192) / 8192.0, -1.0, 1.0);
    }
    void channelPressure (int channel, int value7) { pressure[slot (channel)] = value7 / 127.0; }
    void controller (int channel, int number, int value7)
    {
        if (number == 1)
            wheel = value7 / 127.0;
        else if (number == 74)
            timbre[slot (channel)] = value7 / 127.0;
    }

    // A new note on `channel` starts from neutral per-note expression (MPE convention).
    void noteOn (int channel)
    {
        if (mpe && channel != managerChannel)
        {
            bend[index (channel)] = 0.0;
            pressure[index (channel)] = 0.0;
            timbre[index (channel)] = 0.5;
        }
    }

    double getWheel() const { return wheel; }

    Expression forChannel (int channel) const
    {
        Expression e;
        if (! mpe)
        {
            e.bendSemitones = bend[0] * bendRange;
            e.pressure = pressure[0];
            e.timbre = timbre[0];
            return e;
        }
        const size_t m = index (managerChannel), c = index (channel);
        e.bendSemitones = bend[m] * bendRange + (channel == managerChannel ? 0.0 : bend[c] * noteBendRange);
        e.pressure = std::max (pressure[m], channel == managerChannel ? 0.0 : pressure[c]);
        e.timbre = channel == managerChannel ? timbre[m] : timbre[c];
        return e;
    }

private:
    static constexpr int managerChannel = 1;
    static size_t index (int channel) { return (size_t) std::clamp (channel, 1, 16) - 1; }

    size_t slot (int channel) const { return mpe ? index (channel) : 0; }

    bool mpe = false;
    double bendRange = defaultBendRange;
    double noteBendRange = defaultMpeNoteBendRange;
    std::array<double, 16> bend {}, pressure {}, timbre {};
    double wheel = 0.0;
};

// External (sidechain) input routing to the five voices, emulating the hardware's inputs:
//   mono (input 1 only) → every voice; stereo pair → voice 1 gets left, voice 2 right, and
//   voices 3–5 get both (the mono sum).
// OPEN: exactly how the stereo pair is "hard-panned, and also distributed to voices 3, 4, 5".
enum class ExtSource
{
    mono,
    left,
    right
};

inline ExtSource extSourceFor (int voice, bool stereoInput)
{
    if (! stereoInput)
        return ExtSource::mono;
    if (voice == 0) return ExtSource::left;
    if (voice == 1) return ExtSource::right;
    return ExtSource::mono;
}

} // namespace rotor
