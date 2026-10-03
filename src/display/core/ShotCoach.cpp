#include "ShotCoach.h"

#include <cstdio>
#include <cstring>

namespace shotcoach {

namespace {

const char *stepHint(Size size) {
    switch (size) {
    case Size::SMALL:
        return "half step";
    case Size::MEDIUM:
        return "1 step";
    case Size::LARGE:
        return "2 steps";
    }
    return "";
}

bool isGrindChange(Change c) { return c == Change::GRIND_FINER || c == Change::GRIND_COARSER; }

// FINER <-> COARSER, UP <-> DOWN
Change opposite(Change c) {
    switch (c) {
    case Change::GRIND_FINER:
        return Change::GRIND_COARSER;
    case Change::GRIND_COARSER:
        return Change::GRIND_FINER;
    case Change::DOSE_UP:
        return Change::DOSE_DOWN;
    case Change::DOSE_DOWN:
        return Change::DOSE_UP;
    case Change::YIELD_UP:
        return Change::YIELD_DOWN;
    case Change::YIELD_DOWN:
        return Change::YIELD_UP;
    case Change::TEMP_UP:
        return Change::TEMP_DOWN;
    case Change::TEMP_DOWN:
        return Change::TEMP_UP;
    default:
        return Change::NONE;
    }
}

Taste oppositeTaste(Taste t) {
    if (t == Taste::SOUR)
        return Taste::BITTER;
    if (t == Taste::BITTER)
        return Taste::SOUR;
    return Taste::UNKNOWN;
}

Size smaller(Size s) { return s == Size::SMALL ? Size::SMALL : static_cast<Size>(static_cast<uint8_t>(s) - 1); }

void setText(Advice &a, const char *headline, const char *reason) {
    snprintf(a.headline, sizeof(a.headline), "%s", headline);
    snprintf(a.reason, sizeof(a.reason), "%s", reason);
}

void appendStep(Advice &a) {
    if (!isGrindChange(a.change))
        return;
    char buf[sizeof(a.headline) + 16];
    snprintf(buf, sizeof(buf), "%s (%s)", a.headline, stepHint(a.size));
    snprintf(a.headline, sizeof(a.headline), "%.47s", buf);
}

} // namespace

float extractionSeconds(const Facts &facts) {
    if (facts.firstDripMs > 0 && facts.firstDripMs < facts.durationMs)
        return (facts.durationMs - facts.firstDripMs) / 1000.0f;
    return facts.durationMs / 1000.0f;
}

float brewRatio(const Facts &facts) {
    if (facts.doseIn > 0.0f && facts.finalWeight > 0.0f)
        return facts.finalWeight / facts.doseIn;
    return 0.0f;
}

const Thresholds &thresholdsFor(Roast roast) {
    switch (roast) {
    case Roast::LIGHT:
        return THRESHOLDS_LIGHT;
    case Roast::DARK:
        return THRESHOLDS_DARK;
    default:
        return THRESHOLDS_MEDIUM;
    }
}

Advice advise(const Facts &facts, Taste taste) {
    Advice a;
    const Thresholds &t = thresholdsFor(facts.roast);
    const float seconds = extractionSeconds(facts);
    const float ratio = brewRatio(facts);
    char reason[sizeof(a.reason)];

    if (facts.aborted) {
        a.change = Change::NO_DATA;
        setText(a, "Shot was stopped early", "No advice for an aborted shot.");
        return a;
    }
    if (taste == Taste::UNKNOWN) {
        a.change = Change::NO_DATA;
        setText(a, "Tap how it tasted", "Sour, balanced or bitter.");
        return a;
    }

    if (facts.channelingEvents >= CHANNELING_EVENTS && taste != Taste::BALANCED) {
        a.change = Change::PUCK_PREP;
        a.size = Size::MEDIUM;
        setText(a, "Fix puck prep", "Flow spiked while pressure dropped: channeling, not grind.");
        return a;
    }

    // A brew-by-weight shot that ran into its time cap well short of the target is too fine, whatever it tasted like.
    if (facts.hitTimeCap && facts.targetWeight > 0.0f && facts.finalWeight < facts.targetWeight * TIME_CAP_SHORTFALL) {
        a.change = Change::GRIND_COARSER;
        a.size = Size::LARGE;
        snprintf(reason, sizeof(reason), "Timed out at %.0f s with %.0f of %.0f g: too fine.", facts.durationMs / 1000.0f,
                 facts.finalWeight, facts.targetWeight);
        setText(a, "Grind coarser", reason);
        appendStep(a);
        return a;
    }

    // Choked puck: first drip very late while the pump sat at the profile's pressure (or 9 bar when unknown).
    const float fullPressure = facts.profilePressure > 0.0f ? facts.profilePressure - 0.5f : CHOKE_PRESSURE_BAR;
    const float firstDripS = facts.firstDripMs / 1000.0f;
    if (facts.firstDripMs > 0 && firstDripS > CHOKE_S && facts.maxPressure >= fullPressure) {
        a.change = Change::GRIND_COARSER;
        a.size = Size::LARGE;
        snprintf(reason, sizeof(reason), "First drip after %.0f s at full pressure: choked.", firstDripS);
        setText(a, "Grind coarser", reason);
        appendStep(a);
        return a;
    }

    if (taste == Taste::SOUR) {
        if (seconds < t.fastS) {
            a.change = Change::GRIND_FINER;
            a.size = seconds < t.veryFastS ? Size::LARGE : Size::MEDIUM;
            snprintf(reason, sizeof(reason), "Ran %.0f s, sour: under-extracted.", seconds);
            setText(a, "Grind finer", reason);
        } else if (ratio > 0.0f && ratio < t.ratioLow) {
            a.change = Change::YIELD_UP;
            a.size = Size::MEDIUM;
            snprintf(reason, sizeof(reason), "Ratio 1:%.1f is short for a sour cup.", ratio);
            setText(a, "Pull a longer shot", reason);
        } else if (facts.avgTemp > 0.0f && facts.avgTemp < t.tempLowC) {
            a.change = Change::TEMP_UP;
            a.size = Size::SMALL;
            snprintf(reason, sizeof(reason), "Brewed at %.0f C, sour: try hotter.", facts.avgTemp);
            setText(a, "Raise temperature 1-2 C", reason);
        } else if (facts.prevAdvice == Change::GRIND_FINER && facts.prevTaste == Taste::SOUR) {
            a.change = Change::DOSE_DOWN;
            a.size = Size::SMALL;
            setText(a, "Lower dose 0.5 g", "Still sour after going finer: change dose instead.");
        } else {
            a.change = Change::GRIND_FINER;
            a.size = Size::SMALL;
            snprintf(reason, sizeof(reason), "Time and ratio look fine (%.0f s), still sour.", seconds);
            setText(a, "Grind finer", reason);
        }
    } else if (taste == Taste::BITTER) {
        if (seconds > t.slowS) {
            a.change = Change::GRIND_COARSER;
            a.size = seconds > t.verySlowS ? Size::LARGE : Size::MEDIUM;
            snprintf(reason, sizeof(reason), "Ran %.0f s, bitter: over-extracted.", seconds);
            setText(a, "Grind coarser", reason);
        } else if (ratio > t.ratioHigh) {
            a.change = Change::YIELD_DOWN;
            a.size = Size::MEDIUM;
            snprintf(reason, sizeof(reason), "Ratio 1:%.1f is long for a bitter cup.", ratio);
            setText(a, "Stop the shot earlier", reason);
        } else if (facts.avgTemp > t.tempHighC) {
            a.change = Change::TEMP_DOWN;
            a.size = Size::SMALL;
            snprintf(reason, sizeof(reason), "Brewed at %.0f C, bitter: try cooler.", facts.avgTemp);
            setText(a, "Lower temperature 1-2 C", reason);
        } else if (facts.prevAdvice == Change::GRIND_COARSER && facts.prevTaste == Taste::BITTER) {
            a.change = Change::DOSE_UP;
            a.size = Size::SMALL;
            setText(a, "Raise dose 0.5 g", "Still bitter after going coarser: change dose instead.");
        } else {
            a.change = Change::GRIND_COARSER;
            a.size = Size::SMALL;
            snprintf(reason, sizeof(reason), "Time and ratio look fine (%.0f s), still bitter.", seconds);
            setText(a, "Grind coarser", reason);
        }
    } else { // BALANCED
        a.change = Change::HOLD;
        a.size = Size::SMALL;
        // "Low" is relative to what the profile asked for: a 6 bar profile peaking at 6 bar is fine.
        const float lowPressure = facts.profilePressure > 0.0f ? facts.profilePressure - 2.0f : LOW_PRESSURE_BAR;
        if (facts.maxPressure > 0.0f && facts.maxPressure < lowPressure && seconds < t.fastS) {
            setText(a, "Keep it", "Tastes right, but low pressure: a touch finer would add body.");
        } else if (ratio > 0.0f) {
            snprintf(reason, sizeof(reason), "%.0f s, 1:%.1f. Nice.", seconds, ratio);
            setText(a, "Keep these settings", reason);
        } else {
            snprintf(reason, sizeof(reason), "%.0f s. Nice.", seconds);
            setText(a, "Keep these settings", reason);
        }
        return a;
    }

    // Oscillation guard: last time we pushed the other way and the taste flipped, so take a smaller step.
    if (facts.prevAdvice != Change::NONE && facts.prevAdvice == opposite(a.change) && facts.prevTaste == oppositeTaste(taste)) {
        a.size = smaller(a.size);
        const size_t len = strlen(a.reason);
        snprintf(a.reason + len, sizeof(a.reason) - len, " (you overshot last time)");
    }
    appendStep(a);
    return a;
}

const char *changeLabel(Change change) {
    switch (change) {
    case Change::NONE:
        return "none";
    case Change::HOLD:
        return "hold";
    case Change::GRIND_FINER:
        return "grind_finer";
    case Change::GRIND_COARSER:
        return "grind_coarser";
    case Change::DOSE_UP:
        return "dose_up";
    case Change::DOSE_DOWN:
        return "dose_down";
    case Change::YIELD_UP:
        return "yield_up";
    case Change::YIELD_DOWN:
        return "yield_down";
    case Change::TEMP_UP:
        return "temp_up";
    case Change::TEMP_DOWN:
        return "temp_down";
    case Change::PUCK_PREP:
        return "puck_prep";
    case Change::NO_DATA:
        return "no_data";
    }
    return "none";
}

Roast roastFromLabel(const char *label) {
    if (label == nullptr)
        return Roast::UNKNOWN;
    if (strcmp(label, "light") == 0)
        return Roast::LIGHT;
    if (strcmp(label, "medium") == 0)
        return Roast::MEDIUM;
    if (strcmp(label, "dark") == 0)
        return Roast::DARK;
    return Roast::UNKNOWN;
}

const char *tasteLabel(Taste taste) {
    switch (taste) {
    case Taste::SOUR:
        return "sour";
    case Taste::BALANCED:
        return "balanced";
    case Taste::BITTER:
        return "bitter";
    default:
        return "";
    }
}

} // namespace shotcoach
