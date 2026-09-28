#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rotor
{

constexpr int numVoices = 5;

// Assigns notes to the five fixed voices. Pure logic — no audio — so every mode is unit-tested.
// v1.2: Round-Robin Forward / Backward / Random. v1.3 adds the unison modes.
class VoiceAllocator
{
public:
    enum class Mode
    {
        forward = 0,
        backward,
        random
    };

    // Bit i set = voice i (0-based; voice 1 on the panel is index 0).
    using VoiceMask = std::uint8_t;

    VoiceAllocator() { reset(); }

    void reset()
    {
        for (auto& v : voices)
            v = {};
        keysDown = {};
        lastVoice = -1;
        numKeysDown = 0;
    }

    void setMode (Mode newMode) { mode = newMode; }
    Mode getMode() const { return mode; }

    void setRoundRobinReset (bool shouldReset) { roundRobinReset = shouldReset; }
    void setRandomSeed (std::uint32_t seed) { rng = seed != 0 ? seed : 1u; }

    // Returns the voices that must be released because they were just disabled.
    VoiceMask setVoiceEnabled (int voice, bool enabled)
    {
        if (! isValid (voice) || voices[(std::size_t) voice].enabled == enabled)
            return 0;

        auto& v = voices[(std::size_t) voice];
        v.enabled = enabled;
        if (enabled || ! v.gate)
            return 0;

        // OPEN: disabling a sounding voice — assumed it releases normally (no hard cut).
        v.gate = false;
        return maskOf (voice);
    }

    bool isVoiceEnabled (int voice) const { return isValid (voice) && voices[(std::size_t) voice].enabled; }

    // Returns the voice that should play this note, or -1 when every voice is disabled.
    // Rotation steals: the next voice takes the note even if it is still sounding.
    int noteOn (int note)
    {
        setKeyDown (note, true);

        const int voice = mode == Mode::random ? pickRandom() : pickNextInSequence();
        if (voice < 0)
            return -1;

        auto& v = voices[(std::size_t) voice];
        v.note = note;
        v.gate = true;
        lastVoice = voice;
        return voice;
    }

    // Returns the voices whose key was lifted (gate off → release).
    VoiceMask noteOff (int note)
    {
        setKeyDown (note, false);

        VoiceMask released = 0;
        for (int i = 0; i < numVoices; ++i)
        {
            auto& v = voices[(std::size_t) i];
            if (v.gate && v.note == note)
            {
                v.gate = false;
                released |= maskOf (i);
            }
        }

        if (numKeysDown == 0 && roundRobinReset)
            lastVoice = -1; // next note starts again from voice 1

        return released;
    }

    VoiceMask allNotesOff()
    {
        VoiceMask released = 0;
        for (int i = 0; i < numVoices; ++i)
        {
            if (voices[(std::size_t) i].gate)
                released |= maskOf (i);
            voices[(std::size_t) i].gate = false;
        }
        keysDown = {};
        numKeysDown = 0;
        if (roundRobinReset)
            lastVoice = -1;
        return released;
    }

    int getVoiceNote (int voice) const { return isValid (voice) ? voices[(std::size_t) voice].note : -1; }
    bool isVoiceGateOn (int voice) const { return isValid (voice) && voices[(std::size_t) voice].gate; }
    int getLastVoice() const { return lastVoice; }
    int getNumKeysDown() const { return numKeysDown; }

    static VoiceMask maskOf (int voice) { return (VoiceMask) (1u << voice); }

private:
    struct VoiceState
    {
        bool enabled = true;
        bool gate = false;
        int note = -1;
    };

    static bool isValid (int voice) { return voice >= 0 && voice < numVoices; }

    void setKeyDown (int note, bool down)
    {
        if (note < 0 || note >= (int) keysDown.size() || keysDown[(std::size_t) note] == down)
            return;
        keysDown[(std::size_t) note] = down;
        numKeysDown += down ? 1 : -1;
    }

    int pickNextInSequence() const
    {
        const int step = mode == Mode::backward ? numVoices - 1 : 1;

        // After a reset (or at start) the sequence begins at voice 1.
        // OPEN: for Backward, assumed Round-Robin Reset also returns to voice 1 (then 5, 4, …).
        int candidate = lastVoice < 0 ? 0 : (lastVoice + step) % numVoices;
        for (int tries = 0; tries < numVoices; ++tries)
        {
            if (voices[(std::size_t) candidate].enabled)
                return candidate;
            candidate = (candidate + step) % numVoices;
        }
        return -1;
    }

    // OPEN: assumed no immediate repeat (unless only one voice is enabled).
    int pickRandom()
    {
        std::array<int, numVoices> choices {};
        int count = 0;
        for (int i = 0; i < numVoices; ++i)
            if (voices[(std::size_t) i].enabled && i != lastVoice)
                choices[(std::size_t) count++] = i;

        if (count == 0)
            return isVoiceEnabled (lastVoice) ? lastVoice : -1;

        return choices[(std::size_t) (nextRandom() % (std::uint32_t) count)];
    }

    std::uint32_t nextRandom()
    {
        // xorshift32: deterministic, allocation-free, fine for voice picking.
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    }

    std::array<VoiceState, numVoices> voices {};
    std::array<bool, 128> keysDown {};
    Mode mode = Mode::forward;
    bool roundRobinReset = false;
    int lastVoice = -1;
    int numKeysDown = 0;
    std::uint32_t rng = 0x9E3779B9u;
};

} // namespace rotor
