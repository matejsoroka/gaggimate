#ifndef SHOTCOACH_H
#define SHOTCOACH_H

#include <cstdint>

// Rule-based dial-in advice ("Shot Coach"). Pure C++: no Arduino or LVGL dependencies so the
// rules compile and run in the native unit test environment (test/test_shot_coach).
namespace shotcoach {

enum class Taste : uint8_t { UNKNOWN = 0, SOUR = 1, BALANCED = 2, BITTER = 3 };

enum class Change : uint8_t {
    NONE = 0,
    HOLD,
    GRIND_FINER,
    GRIND_COARSER,
    DOSE_UP,
    DOSE_DOWN,
    YIELD_UP,
    YIELD_DOWN,
    TEMP_UP,
    TEMP_DOWN,
    PUCK_PREP,
    NO_DATA,
};

enum class Size : uint8_t { SMALL = 1, MEDIUM = 2, LARGE = 3 };

enum class Roast : uint8_t { UNKNOWN = 0, LIGHT = 1, MEDIUM = 2, DARK = 3 };

// Everything the rules look at. Mirrors the `shot:facts` event published by ShotHistoryPlugin.
struct Facts {
    bool aborted = false;
    uint32_t durationMs = 0;
    uint32_t firstDripMs = 0;  // 0 = unknown (no scale)
    float finalWeight = 0.0f;  // g, 0 = unknown
    float targetWeight = 0.0f; // g, 0 = time based shot
    float doseIn = 0.0f;       // g, 0 = unknown
    float maxPressure = 0.0f;  // bar
    float avgFlow = 0.0f;      // ml/s
    float avgTemp = 0.0f;      // °C
    float targetTemp = 0.0f;   // °C
    uint16_t channelingEvents = 0;
    bool hitTimeCap = false;          // brew-by-weight shot ended on its duration cap before reaching targetWeight
    float profilePressure = 0.0f;     // highest pressure target in the profile, 0 = flow-only / unknown
    Roast roast = Roast::UNKNOWN;     // from the profile; UNKNOWN behaves like MEDIUM
    Taste prevTaste = Taste::UNKNOWN; // previous shot on the same profile
    Change prevAdvice = Change::NONE; // advice given for that shot
};

struct Advice {
    Change change = Change::NONE;
    Size size = Size::SMALL;
    char headline[48] = {0};
    char reason[96] = {0};
};

// Roast-dependent ranges. Extraction time is measured from first drip when a scale is present.
// Light roasts are denser and less soluble: they want hotter water, longer ratios and longer shots.
// Dark roasts are brittle and very soluble: cooler water, shorter ratios and shorter shots.
struct Thresholds {
    float fastS;     // extraction shorter than this = fast
    float veryFastS; // ... and this = very fast (bigger grind step)
    float slowS;     // longer than this = slow
    float verySlowS;
    float ratioLow;  // brew ratio (yield / dose) below this = short
    float ratioHigh; // above this = long
    float tempLowC;  // average brew temperature below this = cool for the roast
    float tempHighC; // above this = hot for the roast
};
constexpr Thresholds THRESHOLDS_LIGHT{24.0f, 18.0f, 42.0f, 50.0f, 2.0f, 3.0f, 93.0f, 96.0f};
constexpr Thresholds THRESHOLDS_MEDIUM{20.0f, 15.0f, 38.0f, 45.0f, 1.7f, 2.6f, 91.0f, 95.0f};
constexpr Thresholds THRESHOLDS_DARK{17.0f, 13.0f, 32.0f, 38.0f, 1.3f, 2.0f, 88.0f, 92.0f};
const Thresholds &thresholdsFor(Roast roast);

// Roast independent.
constexpr float CHOKE_S = 15.0f;           // first drip later than this ...
constexpr float CHOKE_PRESSURE_BAR = 9.0f; // ... at this pressure or more = choked puck
constexpr float LOW_PRESSURE_BAR = 6.0f;   // peak below this (or well below the profile's target) = no puck resistance
constexpr float TIME_CAP_SHORTFALL = 0.9f; // timed out with less than this share of the target weight = too fine
constexpr uint16_t CHANNELING_EVENTS = 2;

Advice advise(const Facts &facts, Taste taste);

const char *changeLabel(Change change);
const char *tasteLabel(Taste taste);
Roast roastFromLabel(const char *label); // "light" | "medium" | "dark", anything else = UNKNOWN

// Seconds of extraction and brew ratio as the rules see them (exposed for tests and the UI).
float extractionSeconds(const Facts &facts);
float brewRatio(const Facts &facts);

} // namespace shotcoach

#endif // SHOTCOACH_H
