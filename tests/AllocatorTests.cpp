#include "dsp/VoiceAllocator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <set>
#include <vector>

using rotor::VoiceAllocator;
using Mode = VoiceAllocator::Mode;

namespace
{
    // Plays `count` staccato notes (on then off) and returns the voices used.
    std::vector<int> playStaccato (VoiceAllocator& a, int count, int firstNote = 60)
    {
        std::vector<int> used;
        for (int i = 0; i < count; ++i)
        {
            used.push_back (a.noteOn (firstNote + i));
            a.noteOff (firstNote + i);
        }
        return used;
    }

    // Plays `count` notes without releasing them.
    std::vector<int> playHeld (VoiceAllocator& a, int count, int firstNote = 60)
    {
        std::vector<int> used;
        for (int i = 0; i < count; ++i)
            used.push_back (a.noteOn (firstNote + i));
        return used;
    }
} // namespace

TEST_CASE ("Forward rotates 1→5 and wraps", "[allocator]")
{
    VoiceAllocator a;
    a.setMode (Mode::forward);
    CHECK (playStaccato (a, 7) == std::vector<int> { 0, 1, 2, 3, 4, 0, 1 });
}

TEST_CASE ("Backward rotates 5→1 and wraps", "[allocator]")
{
    VoiceAllocator a;
    a.setMode (Mode::backward);
    // Starts at voice 1 (see OPEN note in VoiceAllocator), then 5, 4, …
    CHECK (playStaccato (a, 7) == std::vector<int> { 0, 4, 3, 2, 1, 0, 4 });
}

TEST_CASE ("Disabled voices are skipped", "[allocator]")
{
    SECTION ("forward")
    {
        VoiceAllocator a;
        a.setVoiceEnabled (1, false);
        a.setVoiceEnabled (3, false);
        CHECK (playStaccato (a, 5) == std::vector<int> { 0, 2, 4, 0, 2 });
    }
    SECTION ("backward")
    {
        VoiceAllocator a;
        a.setMode (Mode::backward);
        a.setVoiceEnabled (0, false);
        a.setVoiceEnabled (4, false);
        CHECK (playStaccato (a, 5) == std::vector<int> { 3, 2, 1, 3, 2 });
    }
    SECTION ("random never picks a disabled voice")
    {
        VoiceAllocator a;
        a.setMode (Mode::random);
        a.setVoiceEnabled (2, false);
        for (int v : playStaccato (a, 200))
            CHECK (v != 2);
    }
}

TEST_CASE ("All voices disabled → note is dropped", "[allocator]")
{
    const auto mode = GENERATE (Mode::forward, Mode::backward, Mode::random);
    VoiceAllocator a;
    a.setMode (mode);
    for (int i = 0; i < rotor::numVoices; ++i)
        a.setVoiceEnabled (i, false);
    CHECK (a.noteOn (60) == -1);
    CHECK (a.noteOff (60) == 0);
}

TEST_CASE ("Rotation steals: the sixth held note takes voice 1", "[allocator]")
{
    VoiceAllocator a;
    CHECK (playHeld (a, 6) == std::vector<int> { 0, 1, 2, 3, 4, 0 });
    CHECK (a.getVoiceNote (0) == 65);

    // Lifting the stolen key (60) releases nothing: voice 1 now belongs to 65.
    CHECK (a.noteOff (60) == 0);
    CHECK (a.isVoiceGateOn (0));

    // Lifting 65 releases voice 1.
    CHECK (a.noteOff (65) == VoiceAllocator::maskOf (0));
}

TEST_CASE ("Note off releases only the voice holding that key", "[allocator]")
{
    VoiceAllocator a;
    playHeld (a, 3);
    CHECK (a.noteOff (61) == VoiceAllocator::maskOf (1));
    CHECK (a.isVoiceGateOn (0));
    CHECK_FALSE (a.isVoiceGateOn (1));
    CHECK (a.isVoiceGateOn (2));
    CHECK (a.noteOff (99) == 0); // never pressed
}

TEST_CASE ("Round-Robin Reset", "[allocator]")
{
    SECTION ("off: continues from the last-used voice after all keys are released")
    {
        VoiceAllocator a;
        a.setRoundRobinReset (false);
        playStaccato (a, 3); // voices 1, 2, 3
        CHECK (a.noteOn (70) == 3);
    }
    SECTION ("on: returns to voice 1 when all keys are released")
    {
        VoiceAllocator a;
        a.setRoundRobinReset (true);
        playStaccato (a, 3);
        CHECK (a.noteOn (70) == 0);
    }
    SECTION ("on: does not reset while any key is still held")
    {
        VoiceAllocator a;
        a.setRoundRobinReset (true);
        a.noteOn (60);         // voice 1, held
        a.noteOn (61);         // voice 2
        a.noteOff (61);        // 60 still held
        CHECK (a.noteOn (62) == 2);
    }
    SECTION ("on: resets to the first enabled voice when voice 1 is off")
    {
        VoiceAllocator a;
        a.setRoundRobinReset (true);
        a.setVoiceEnabled (0, false);
        playStaccato (a, 2);
        CHECK (a.noteOn (70) == 1);
    }
    SECTION ("on, backward: resets to voice 1, then continues 5, 4, …")
    {
        VoiceAllocator a;
        a.setMode (Mode::backward);
        a.setRoundRobinReset (true);
        playStaccato (a, 3);
        CHECK (playStaccato (a, 3) == std::vector<int> { 0, 0, 0 }); // every note is after a full release
        a.noteOn (80);
        CHECK (a.noteOn (81) == 4);
    }
    SECTION ("on: allNotesOff also resets")
    {
        VoiceAllocator a;
        a.setRoundRobinReset (true);
        playHeld (a, 3);
        a.allNotesOff();
        CHECK (a.noteOn (70) == 0);
    }
}

TEST_CASE ("Random: no immediate repeats and every voice gets used", "[allocator]")
{
    VoiceAllocator a;
    a.setMode (Mode::random);
    a.setRandomSeed (1234);
    const auto used = playStaccato (a, 500);
    std::set<int> seen (used.begin(), used.end());
    CHECK (seen == std::set<int> { 0, 1, 2, 3, 4 });
    for (size_t i = 1; i < used.size(); ++i)
        CHECK (used[i] != used[i - 1]);
}

TEST_CASE ("Random with one enabled voice keeps using it", "[allocator]")
{
    VoiceAllocator a;
    a.setMode (Mode::random);
    for (int i = 0; i < rotor::numVoices; ++i)
        a.setVoiceEnabled (i, i == 3);
    CHECK (playStaccato (a, 4) == std::vector<int> { 3, 3, 3, 3 });
}

TEST_CASE ("Random is reproducible from a seed", "[allocator]")
{
    VoiceAllocator a, b;
    a.setMode (Mode::random);
    b.setMode (Mode::random);
    a.setRandomSeed (42);
    b.setRandomSeed (42);
    CHECK (playStaccato (a, 50) == playStaccato (b, 50));
}

TEST_CASE ("Disabling a voice mid-note releases it; enabling mid-note does not trigger", "[allocator]")
{
    VoiceAllocator a;
    playHeld (a, 3); // voices 1..3 hold 60..62

    CHECK (a.setVoiceEnabled (1, false) == VoiceAllocator::maskOf (1));
    CHECK_FALSE (a.isVoiceGateOn (1));
    CHECK (a.setVoiceEnabled (1, false) == 0);   // already off: no change
    CHECK (a.noteOff (61) == 0);                 // its key lifts later: nothing left to release

    CHECK (a.setVoiceEnabled (1, true) == 0);
    CHECK_FALSE (a.isVoiceGateOn (1));

    // Disabling an idle voice releases nothing.
    CHECK (a.setVoiceEnabled (4, false) == 0);

    // Rotation continues from voice 3 and skips the disabled voice 5.
    CHECK (a.noteOn (70) == 3);
    CHECK (a.noteOn (71) == 0);
}

TEST_CASE ("Disabling the last-used voice keeps the rotation going", "[allocator]")
{
    VoiceAllocator a;
    playStaccato (a, 2);          // last used: voice 2
    a.setVoiceEnabled (1, false);
    CHECK (a.noteOn (70) == 2);
}

TEST_CASE ("Switching mode mid-stream continues from the last-used voice", "[allocator]")
{
    VoiceAllocator a;
    playStaccato (a, 3);          // last used: voice 3
    a.setMode (Mode::backward);
    CHECK (a.noteOn (70) == 1);
}

TEST_CASE ("Repeated note-on of a held key is counted once for Round-Robin Reset", "[allocator]")
{
    VoiceAllocator a;
    a.setRoundRobinReset (true);
    a.noteOn (60);
    a.noteOn (60);                // same key again (e.g. from a sequencer)
    CHECK (a.getNumKeysDown() == 1);
    CHECK (a.noteOff (60) == (VoiceAllocator::maskOf (0) | VoiceAllocator::maskOf (1)));
    CHECK (a.getNumKeysDown() == 0);
    CHECK (a.noteOn (61) == 0);
}
