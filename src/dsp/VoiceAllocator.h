#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rotor
{

constexpr int numVoices = 5;

// Assigns notes to the five fixed voices. Pure logic — no audio — so every mode is unit-tested.
//
// Round-Robin (Forward / Backward / Random): each note goes to the next voice in sequence.
// Unison (Staccato / Legato / Mono): all enabled voices are spread over the held notes and
// redistributed whenever the key count changes.
//
// Every call returns a Result: at most one action per voice (trigger, move, or release).
class VoiceAllocator
{
public:
    enum class Mode
    {
        forward = 0,
        backward,
        random,
        staccato,
        legato,
        mono
    };

    enum class MonoPriority
    {
        last = 0,
        lowest,
        highest
    };

    // Bit i set = voice i (0-based; voice 1 on the panel is index 0).
    using VoiceMask = std::uint8_t;

    struct Action
    {
        enum class Type
        {
            none,
            trigger, // start (or retrigger) the envelopes on `note`
            move,    // change pitch to `note`, envelopes keep running
            release  // key lifted → release stage
        };

        Type type = Type::none;
        int note = -1;
    };

    struct Result
    {
        std::array<Action, numVoices> actions {};

        VoiceMask maskOf (Action::Type type) const
        {
            VoiceMask m = 0;
            for (int i = 0; i < numVoices; ++i)
                if (actions[(std::size_t) i].type == type)
                    m |= VoiceAllocator::maskOf (i);
            return m;
        }

        VoiceMask triggered() const { return maskOf (Action::Type::trigger); }
        VoiceMask moved() const { return maskOf (Action::Type::move); }
        VoiceMask released() const { return maskOf (Action::Type::release); }

        int firstTriggered() const
        {
            for (int i = 0; i < numVoices; ++i)
                if (actions[(std::size_t) i].type == Action::Type::trigger)
                    return i;
            return -1;
        }
    };

    // OPEN: grace-period length. Long enough to cover a human "lifting a chord" spread.
    static constexpr double defaultGraceSeconds = 0.08;

    VoiceAllocator() { reset(); }

    void reset()
    {
        for (auto& v : voices)
            v = {};
        held = {};
        numHeld = 0;
        lastVoice = -1;
        graceRemaining = -1;
    }

    static bool isUnison (Mode m) { return m == Mode::staccato || m == Mode::legato || m == Mode::mono; }

    // Changing mode releases every sounding voice; held keys are remembered.
    Result setMode (Mode newMode)
    {
        Result r;
        if (newMode == mode)
            return r;
        mode = newMode;
        graceRemaining = -1;
        for (int i = 0; i < numVoices; ++i)
            releaseVoice (r, i);
        return r;
    }

    Mode getMode() const { return mode; }

    void setRoundRobinReset (bool shouldReset) { roundRobinReset = shouldReset; }
    void setMonoPriority (MonoPriority p) { monoPriority = p; }
    void setGracePeriod (bool enabled) { graceEnabled = enabled; }
    void setGraceSamples (int samples) { graceSamples = samples > 0 ? samples : 1; }
    void setRandomSeed (std::uint32_t seed) { rng = seed != 0 ? seed : 1u; }

    Result setVoiceEnabled (int voice, bool enabled)
    {
        Result r;
        if (! isValid (voice) || voices[(std::size_t) voice].enabled == enabled)
            return r;

        voices[(std::size_t) voice].enabled = enabled;

        // OPEN: disabling a sounding voice — assumed it releases normally (no hard cut).
        if (! enabled)
            releaseVoice (r, voice);

        // In unison the remaining voices re-spread over the held notes (an enabled voice joins in).
        if (isUnison (mode) && numHeld > 0 && graceRemaining < 0)
            redistribute (r, false);

        return r;
    }

    bool isVoiceEnabled (int voice) const { return isValid (voice) && voices[(std::size_t) voice].enabled; }

    Result noteOn (int note)
    {
        Result r;
        if (note < 0 || note > 127)
            return r;

        const int previousMonoNote = monoNote();
        removeHeld (note); // a re-pressed key becomes the newest
        held[(std::size_t) numHeld++] = note;

        if (! isUnison (mode))
        {
            const int voice = mode == Mode::random ? pickRandom() : pickNextInSequence();
            if (voice >= 0)
            {
                setAction (r, voice, Action::Type::trigger, note);
                lastVoice = voice;
            }
            return r;
        }

        graceRemaining = -1;
        const bool retrigger = mode == Mode::staccato || (mode == Mode::mono && monoNote() != previousMonoNote);
        redistribute (r, retrigger);
        return r;
    }

    Result noteOff (int note)
    {
        Result r;
        if (! removeHeld (note))
            return r;

        if (! isUnison (mode))
        {
            for (int i = 0; i < numVoices; ++i)
                if (voices[(std::size_t) i].gate && voices[(std::size_t) i].note == note)
                    releaseVoice (r, i);

            if (numHeld == 0 && roundRobinReset)
                lastVoice = -1; // next note starts again from voice 1
            return r;
        }

        if (numHeld == 0)
        {
            graceRemaining = -1;
            for (int i = 0; i < numVoices; ++i)
                releaseVoice (r, i);
            return r;
        }

        if (graceEnabled)
        {
            // Voices on the lifted key fade out on their own note; the rest wait out the
            // grace period before re-spreading, in case the whole chord is being released.
            for (int i = 0; i < numVoices; ++i)
                if (voices[(std::size_t) i].gate && voices[(std::size_t) i].note == note)
                    releaseVoice (r, i);
            graceRemaining = graceSamples;
            return r;
        }

        // No grace: the remaining keys grab the voices straight away (monophonic release).
        redistribute (r, false);
        return r;
    }

    Result allNotesOff()
    {
        Result r;
        held = {};
        numHeld = 0;
        graceRemaining = -1;
        for (int i = 0; i < numVoices; ++i)
            releaseVoice (r, i);
        if (roundRobinReset)
            lastVoice = -1;
        return r;
    }

    // Advance time (for the grace period).
    Result advance (int numSamples)
    {
        Result r;
        if (graceRemaining < 0)
            return r;

        graceRemaining -= numSamples;
        if (graceRemaining <= 0)
        {
            graceRemaining = -1;
            if (isUnison (mode) && numHeld > 0)
                redistribute (r, false);
        }
        return r;
    }

    int getVoiceNote (int voice) const { return isValid (voice) ? voices[(std::size_t) voice].note : -1; }
    bool isVoiceGateOn (int voice) const { return isValid (voice) && voices[(std::size_t) voice].gate; }
    int getLastVoice() const { return lastVoice; }
    int getNumKeysDown() const { return numHeld; }
    bool isGraceActive() const { return graceRemaining >= 0; }

    static VoiceMask maskOf (int voice) { return (VoiceMask) (1u << voice); }

private:
    struct VoiceState
    {
        bool enabled = true;
        bool gate = false;
        int note = -1;
    };

    static bool isValid (int voice) { return voice >= 0 && voice < numVoices; }

    void setAction (Result& r, int voice, Action::Type type, int note)
    {
        auto& v = voices[(std::size_t) voice];
        if (type == Action::Type::release)
        {
            v.gate = false;
        }
        else
        {
            v.gate = true;
            v.note = note;
        }
        r.actions[(std::size_t) voice] = { type, note };
    }

    void releaseVoice (Result& r, int voice)
    {
        if (voices[(std::size_t) voice].gate)
            setAction (r, voice, Action::Type::release, voices[(std::size_t) voice].note);
    }

    bool removeHeld (int note)
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[(std::size_t) i] != note)
                continue;
            for (int j = i; j < numHeld - 1; ++j)
                held[(std::size_t) j] = held[(std::size_t) j + 1];
            --numHeld;
            return true;
        }
        return false;
    }

    // The note Mono mode plays, by priority; -1 when no keys are held.
    int monoNote() const
    {
        if (numHeld == 0)
            return -1;
        int pick = held[(std::size_t) numHeld - 1];
        if (monoPriority == MonoPriority::last)
            return pick;
        for (int i = 0; i < numHeld; ++i)
        {
            const int n = held[(std::size_t) i];
            if ((monoPriority == MonoPriority::lowest && n < pick) || (monoPriority == MonoPriority::highest && n > pick))
                pick = n;
        }
        return pick;
    }

    // Spread the enabled voices over the held notes.
    // OPEN: how the hardware splits voices over 2–4 notes. Assumed: enabled voices, in panel
    // order, take the held notes in press order cyclically (2 notes → 3 + 2, the older note
    // gets the extra voice). With more notes than voices, the newest notes win.
    void redistribute (Result& r, bool retriggerAll)
    {
        std::array<int, numVoices> enabled {};
        int numEnabled = 0;
        for (int i = 0; i < numVoices; ++i)
            if (voices[(std::size_t) i].enabled)
                enabled[(std::size_t) numEnabled++] = i;

        std::array<int, numVoices> notes {};
        int numNotes = 0;
        if (mode == Mode::mono)
        {
            if (numHeld > 0)
                notes[(std::size_t) numNotes++] = monoNote();
        }
        else
        {
            const int count = numHeld < numEnabled ? numHeld : numEnabled;
            for (int i = numHeld - count; i < numHeld; ++i)
                notes[(std::size_t) numNotes++] = held[(std::size_t) i];
        }

        if (numNotes == 0 || numEnabled == 0)
        {
            for (int i = 0; i < numVoices; ++i)
                releaseVoice (r, i);
            return;
        }

        for (int k = 0; k < numEnabled; ++k)
        {
            const int voice = enabled[(std::size_t) k];
            const int note = notes[(std::size_t) (k % numNotes)];
            const auto& v = voices[(std::size_t) voice];

            if (! v.gate || retriggerAll)
                setAction (r, voice, Action::Type::trigger, note);
            else if (v.note != note)
                setAction (r, voice, Action::Type::move, note);
        }
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
    std::array<int, 128> held {}; // held keys, oldest first
    int numHeld = 0;

    Mode mode = Mode::forward;
    MonoPriority monoPriority = MonoPriority::last;
    bool roundRobinReset = false;
    bool graceEnabled = false;
    int graceSamples = 3528;   // 80 ms at 44.1 kHz until the processor sets it
    int graceRemaining = -1;   // -1 = no grace period running
    int lastVoice = -1;
    std::uint32_t rng = 0x9E3779B9u;
};

} // namespace rotor
