#pragma once

#include <cmath>

namespace rotor
{

// Portamento in the pitch domain (semitones), at a constant rate, so the glide time
// scales with the interval: a 2-octave leap takes 24× longer than a half step.
// Descending glides take ~10% longer than ascending (per the manual).
class Glide
{
public:
    static constexpr double descendingFactor = 1.1;

    void setSampleRate (double newSampleRate) { sampleRate = newSampleRate; updateRate(); }

    // Time to glide one octave upward. 0 = glide off.
    // OPEN: glide curve (linear in pitch assumed) and how time relates to the knob.
    void setTimePerOctave (double seconds)
    {
        timePerOctave = seconds;
        updateRate();
    }

    // Jump straight to a pitch (first note, or glide off).
    void jumpTo (double semitones)
    {
        current = target = semitones;
        hasPitch = true;
    }

    void setTarget (double semitones)
    {
        if (! hasPitch || stepUp <= 0.0)
            jumpTo (semitones);
        else
            target = semitones;
    }

    bool isGliding() const { return current < target || current > target; }
    double getCurrent() const { return current; }
    double getTarget() const { return target; }

    double process()
    {
        if (current < target)
        {
            current += stepUp;
            if (current > target) current = target;
        }
        else if (current > target)
        {
            current -= stepDown;
            if (current < target) current = target;
        }
        return current;
    }

    void reset() { hasPitch = false; current = target = 0.0; }

private:
    void updateRate()
    {
        if (timePerOctave <= 0.0)
        {
            stepUp = stepDown = 0.0;
            current = target;
            return;
        }
        stepUp = 12.0 / (timePerOctave * sampleRate);
        stepDown = stepUp / descendingFactor;
    }

    double sampleRate = 44100.0;
    double timePerOctave = 0.0;
    double stepUp = 0.0, stepDown = 0.0;
    double current = 0.0, target = 0.0;
    bool hasPitch = false;
};

} // namespace rotor
