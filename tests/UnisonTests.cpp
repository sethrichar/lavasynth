#include "dsp/VoiceAllocator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <vector>

using rotor::VoiceAllocator;
using Mode = VoiceAllocator::Mode;
using Priority = VoiceAllocator::MonoPriority;

namespace
{
    constexpr VoiceAllocator::VoiceMask allVoices = 0b11111;

    std::vector<int> notesOf (const VoiceAllocator& a)
    {
        std::vector<int> notes;
        for (int i = 0; i < rotor::numVoices; ++i)
            notes.push_back (a.isVoiceGateOn (i) ? a.getVoiceNote (i) : -1);
        return notes;
    }

    VoiceAllocator make (Mode mode)
    {
        VoiceAllocator a;
        a.setMode (mode);
        return a;
    }
} // namespace

TEST_CASE ("Unison: one note takes all five voices", "[unison]")
{
    const auto mode = GENERATE (Mode::staccato, Mode::legato, Mode::mono);
    auto a = make (mode);
    CHECK (a.noteOn (60).triggered() == allVoices);
    CHECK (notesOf (a) == std::vector<int> { 60, 60, 60, 60, 60 });
}

TEST_CASE ("Unison: voices spread over held notes and redistribute as keys change", "[unison]")
{
    const auto mode = GENERATE (Mode::staccato, Mode::legato);
    auto a = make (mode);
    a.noteOn (60);
    a.noteOn (64);
    CHECK (notesOf (a) == std::vector<int> { 60, 64, 60, 64, 60 }); // 3 + 2
    a.noteOn (67);
    CHECK (notesOf (a) == std::vector<int> { 60, 64, 67, 60, 64 }); // 2 + 2 + 1
    a.noteOn (71);
    a.noteOn (74);
    CHECK (notesOf (a) == std::vector<int> { 60, 64, 67, 71, 74 }); // one each
}

TEST_CASE ("Unison: at most 5 voices — with 6 keys the newest 5 notes play", "[unison]")
{
    auto a = make (Mode::legato);
    for (int n : { 60, 62, 64, 65, 67, 69 })
        a.noteOn (n);
    CHECK (notesOf (a) == std::vector<int> { 62, 64, 65, 67, 69 });
}

TEST_CASE ("Unison skips disabled voices", "[unison]")
{
    auto a = make (Mode::staccato);
    a.setVoiceEnabled (0, false);
    a.noteOn (60);
    a.noteOn (64);
    CHECK (notesOf (a) == std::vector<int> { -1, 60, 64, 60, 64 }); // 2 + 2
}

TEST_CASE ("Staccato retriggers every voice on every note trigger", "[unison]")
{
    auto a = make (Mode::staccato);
    a.noteOn (60);
    auto r = a.noteOn (64);
    CHECK (r.triggered() == allVoices);
    CHECK (r.moved() == 0);
}

TEST_CASE ("Legato only moves voices while keys are held", "[unison]")
{
    auto a = make (Mode::legato);
    a.noteOn (60);
    auto r = a.noteOn (64);
    CHECK (r.triggered() == 0);
    CHECK (r.moved() == (VoiceAllocator::maskOf (1) | VoiceAllocator::maskOf (3)));

    // Release everything, then a new note retriggers all voices.
    a.noteOff (60);
    CHECK (a.noteOff (64).released() == allVoices);
    CHECK (a.noteOn (67).triggered() == allVoices);
}

TEST_CASE ("Releasing a key while others are held re-spreads without retriggering", "[unison]")
{
    const auto mode = GENERATE (Mode::staccato, Mode::legato);
    auto a = make (mode);
    a.noteOn (60);
    a.noteOn (64);
    auto r = a.noteOff (60);
    CHECK (r.triggered() == 0);
    CHECK (r.released() == 0);
    CHECK (notesOf (a) == std::vector<int> { 64, 64, 64, 64, 64 });
}

TEST_CASE ("No grace: releasing a chord leaves the last-lifted note on all voices", "[unison]")
{
    auto a = make (Mode::legato);
    a.setGracePeriod (false);
    a.noteOn (60);
    a.noteOn (64);
    a.noteOn (67);
    a.noteOff (60);
    a.noteOff (64);
    CHECK (notesOf (a) == std::vector<int> { 67, 67, 67, 67, 67 });
    CHECK (a.noteOff (67).released() == allVoices);
}

TEST_CASE ("Grace period: voices fade out on their own notes when a chord is released", "[unison]")
{
    auto a = make (Mode::legato);
    a.setGracePeriod (true);
    a.setGraceSamples (100);
    a.noteOn (60);
    a.noteOn (64);
    a.noteOn (67); // 60, 64, 67, 60, 64

    auto r = a.noteOff (60);
    CHECK (r.released() == (VoiceAllocator::maskOf (0) | VoiceAllocator::maskOf (3)));
    CHECK (r.moved() == 0);
    CHECK (a.isGraceActive());

    a.advance (40);
    r = a.noteOff (64);
    CHECK (r.released() == (VoiceAllocator::maskOf (1) | VoiceAllocator::maskOf (4)));
    CHECK (r.moved() == 0);

    a.advance (40);
    r = a.noteOff (67);
    CHECK (r.released() == VoiceAllocator::maskOf (2));
    CHECK_FALSE (a.isGraceActive());
    CHECK (a.advance (1000).triggered() == 0); // nothing left to re-spread
}

TEST_CASE ("Grace period: if keys are still held when it expires, voices re-spread", "[unison]")
{
    auto a = make (Mode::legato);
    a.setGracePeriod (true);
    a.setGraceSamples (100);
    a.noteOn (60);
    a.noteOn (64);
    a.noteOff (60); // voices 1, 3, 5 fade out on 60

    CHECK (a.advance (99).triggered() == 0);
    auto r = a.advance (1);
    CHECK (r.triggered() == (VoiceAllocator::maskOf (0) | VoiceAllocator::maskOf (2) | VoiceAllocator::maskOf (4)));
    CHECK (notesOf (a) == std::vector<int> { 64, 64, 64, 64, 64 });
}

TEST_CASE ("Grace period: a new note cancels it and re-spreads immediately", "[unison]")
{
    auto a = make (Mode::legato);
    a.setGracePeriod (true);
    a.setGraceSamples (100);
    a.noteOn (60);
    a.noteOn (64);
    a.noteOff (60);
    a.noteOn (67);
    CHECK_FALSE (a.isGraceActive());
    CHECK (notesOf (a) == std::vector<int> { 64, 67, 64, 67, 64 });
}

TEST_CASE ("Mono: all voices on one note chosen by priority", "[unison][mono]")
{
    SECTION ("last (default)")
    {
        auto a = make (Mode::mono);
        a.noteOn (60);
        a.noteOn (72);
        a.noteOn (64);
        CHECK (notesOf (a) == std::vector<int> { 64, 64, 64, 64, 64 });
        a.noteOff (64);
        CHECK (notesOf (a) == std::vector<int> { 72, 72, 72, 72, 72 });
    }
    SECTION ("lowest")
    {
        auto a = make (Mode::mono);
        a.setMonoPriority (Priority::lowest);
        a.noteOn (64);
        a.noteOn (60);
        a.noteOn (72);
        CHECK (a.getVoiceNote (0) == 60);
        a.noteOff (60);
        CHECK (notesOf (a) == std::vector<int> { 64, 64, 64, 64, 64 });
    }
    SECTION ("highest")
    {
        auto a = make (Mode::mono);
        a.setMonoPriority (Priority::highest);
        a.noteOn (64);
        a.noteOn (72);
        a.noteOn (60);
        CHECK (a.getVoiceNote (0) == 72);
        a.noteOff (72);
        CHECK (notesOf (a) == std::vector<int> { 64, 64, 64, 64, 64 });
    }
}

TEST_CASE ("Mono: retriggers only when the sounding note changes", "[unison][mono]")
{
    auto a = make (Mode::mono);
    a.setMonoPriority (Priority::highest);
    a.noteOn (64);
    auto r = a.noteOn (60); // lower key: highest priority keeps 64
    CHECK (r.triggered() == 0);
    CHECK (r.moved() == 0);
    r = a.noteOn (67);
    CHECK (r.triggered() == allVoices);
    r = a.noteOff (67); // back to 64 without retrigger
    CHECK (r.triggered() == 0);
    CHECK (r.moved() == allVoices);
}

TEST_CASE ("Unison: disabling a voice mid-note releases it and re-spreads the rest", "[unison]")
{
    auto a = make (Mode::legato);
    a.noteOn (60);
    a.noteOn (64); // 60, 64, 60, 64, 60
    auto r = a.setVoiceEnabled (1, false);
    CHECK (r.released() == VoiceAllocator::maskOf (1));
    CHECK (notesOf (a) == std::vector<int> { 60, -1, 64, 60, 64 });
}

TEST_CASE ("Unison: enabling a voice mid-note brings it in on a held note", "[unison]")
{
    auto a = make (Mode::legato);
    a.setVoiceEnabled (4, false);
    a.noteOn (60);
    auto r = a.setVoiceEnabled (4, true);
    CHECK (r.triggered() == VoiceAllocator::maskOf (4));
    CHECK (a.getVoiceNote (4) == 60);
}

TEST_CASE ("Unison: with every voice disabled, notes do nothing", "[unison]")
{
    auto a = make (Mode::staccato);
    for (int i = 0; i < rotor::numVoices; ++i)
        a.setVoiceEnabled (i, false);
    CHECK (a.noteOn (60).triggered() == 0);
    CHECK (a.noteOff (60).released() == 0);
}

TEST_CASE ("Changing mode releases sounding voices", "[unison]")
{
    auto a = make (Mode::forward);
    a.noteOn (60);
    a.noteOn (62);
    CHECK (a.setMode (Mode::legato).released() == (VoiceAllocator::maskOf (0) | VoiceAllocator::maskOf (1)));
    CHECK (a.setMode (Mode::legato).released() == 0); // same mode: no-op
    // The next key in unison takes every voice.
    CHECK (a.noteOn (64).triggered() == allVoices);
}

TEST_CASE ("Unison: all notes off releases everything", "[unison]")
{
    auto a = make (Mode::staccato);
    a.noteOn (60);
    a.noteOn (64);
    CHECK (a.allNotesOff().released() == allVoices);
    CHECK (a.getNumKeysDown() == 0);
}
