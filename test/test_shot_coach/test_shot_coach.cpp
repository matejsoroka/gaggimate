// Unit tests for the Shot Coach dial-in rules (src/display/core/ShotCoach.cpp).
// Host-side, no ESP32/Arduino runtime: pio test -e native -f test_shot_coach.

#include <unity.h>

#include <cstring>

#include "display/core/ShotCoach.cpp"

using namespace shotcoach;

static Facts normalShot() {
    Facts f;
    f.durationMs = 34000;
    f.firstDripMs = 6000; // 28 s of extraction
    f.finalWeight = 36.0f;
    f.doseIn = 18.0f; // ratio 1:2
    f.maxPressure = 9.0f;
    f.avgFlow = 1.5f;
    f.avgTemp = 93.0f;
    f.targetTemp = 93.0f;
    return f;
}

void setUp() {}
void tearDown() {}

void test_aborted_shot_gives_no_advice() {
    Facts f = normalShot();
    f.aborted = true;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::NO_DATA), static_cast<int>(a.change));
}

void test_unknown_taste_asks_for_feedback() {
    Advice a = advise(normalShot(), Taste::UNKNOWN);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::NO_DATA), static_cast<int>(a.change));
    TEST_ASSERT_NOT_NULL(strstr(a.headline, "Tap"));
}

void test_channeling_beats_taste() {
    Facts f = normalShot();
    f.durationMs = 18000; // would otherwise be "fast + sour -> finer"
    f.firstDripMs = 0;
    f.channelingEvents = 2;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::PUCK_PREP), static_cast<int>(a.change));
    // Balanced cups do not get the channeling warning
    a = advise(f, Taste::BALANCED);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
}

void test_choked_puck_is_coarser_large() {
    Facts f = normalShot();
    f.firstDripMs = 18000;
    f.durationMs = 50000;
    f.maxPressure = 9.5f;
    Advice a = advise(f, Taste::SOUR); // even a sour cup: the puck choked
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::LARGE), static_cast<int>(a.size));
    TEST_ASSERT_NOT_NULL(strstr(a.headline, "2 steps"));
}

void test_sour_fast_is_finer_with_size_boundary() {
    Facts f = normalShot();
    f.firstDripMs = 0;
    f.durationMs = 19000;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::MEDIUM), static_cast<int>(a.size));
    f.durationMs = 14000;
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Size::LARGE), static_cast<int>(a.size));
}

void test_extraction_time_counts_from_first_drip() {
    Facts f = normalShot();
    f.durationMs = 26000;
    f.firstDripMs = 8000; // 18 s extraction -> fast
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.0f, extractionSeconds(f));
}

void test_sour_short_ratio_is_yield_up() {
    Facts f = normalShot();
    f.finalWeight = 28.0f; // 1:1.56
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::YIELD_UP), static_cast<int>(a.change));
}

void test_sour_cool_is_temp_up() {
    Facts f = normalShot();
    f.avgTemp = 89.0f;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::TEMP_UP), static_cast<int>(a.change));
}

void test_sour_in_range_after_finer_switches_to_dose() {
    Facts f = normalShot();
    f.prevAdvice = Change::GRIND_FINER;
    f.prevTaste = Taste::SOUR;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::DOSE_DOWN), static_cast<int>(a.change));
    f.prevAdvice = Change::NONE;
    f.prevTaste = Taste::UNKNOWN;
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::SMALL), static_cast<int>(a.size));
}

void test_bitter_slow_is_coarser() {
    Facts f = normalShot();
    f.firstDripMs = 0;
    f.durationMs = 40000;
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::MEDIUM), static_cast<int>(a.size));
    f.durationMs = 50000;
    a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Size::LARGE), static_cast<int>(a.size));
}

void test_bitter_long_ratio_is_yield_down() {
    Facts f = normalShot();
    f.finalWeight = 50.0f; // 1:2.78
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::YIELD_DOWN), static_cast<int>(a.change));
}

void test_bitter_hot_is_temp_down() {
    Facts f = normalShot();
    f.avgTemp = 96.0f;
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::TEMP_DOWN), static_cast<int>(a.change));
}

void test_bitter_in_range_after_coarser_switches_to_dose() {
    Facts f = normalShot();
    f.prevAdvice = Change::GRIND_COARSER;
    f.prevTaste = Taste::BITTER;
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::DOSE_UP), static_cast<int>(a.change));
}

void test_balanced_is_hold() {
    Advice a = advise(normalShot(), Taste::BALANCED);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "1:2.0"));
}

void test_balanced_low_pressure_hint() {
    Facts f = normalShot();
    f.maxPressure = 5.0f;
    f.firstDripMs = 0;
    f.durationMs = 18000;
    Advice a = advise(f, Taste::BALANCED);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "low pressure"));
}

void test_oscillation_guard_reduces_size() {
    Facts f = normalShot();
    f.firstDripMs = 0;
    f.durationMs = 40000;               // bitter + slow -> coarser MEDIUM ...
    f.prevAdvice = Change::GRIND_FINER; // ... but last time we went finer because it was sour
    f.prevTaste = Taste::SOUR;
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::SMALL), static_cast<int>(a.size));
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "overshot"));
    TEST_ASSERT_NOT_NULL(strstr(a.headline, "half step"));
}

void test_no_scale_still_advises_on_time() {
    Facts f;
    f.durationMs = 17000;
    f.maxPressure = 9.0f;
    f.avgTemp = 93.0f;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, brewRatio(f));
    a = advise(f, Taste::BALANCED);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
    TEST_ASSERT_NULL(strstr(a.reason, "1:"));
}

void test_light_roast_widens_ranges() {
    Facts f = normalShot(); // 28 s, 1:2.0, 93 C sits exactly on the light-roast lower bounds: in range
    f.roast = Roast::LIGHT;
    Advice a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    f.firstDripMs = 0;
    f.durationMs = 22000; // 22 s is fast for light (< 24) but fine for medium (>= 20)
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    f.roast = Roast::MEDIUM;
    a = advise(f, Taste::SOUR); // medium: 22 s ok, ratio 2.0 ok, 93 C ok -> small finer
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_FINER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::SMALL), static_cast<int>(a.size));
    f.roast = Roast::LIGHT;
    f.durationMs = 30000;
    f.finalWeight = 34.0f; // 1:1.9 is short for light
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::YIELD_UP), static_cast<int>(a.change));
    f.finalWeight = 45.0f; // 1:2.5 is fine for light ...
    f.avgTemp = 92.0f;     // ... but 92 C is cool for light
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::TEMP_UP), static_cast<int>(a.change));
}

void test_dark_roast_tightens_ranges() {
    Facts f = normalShot();
    f.roast = Roast::DARK;
    f.firstDripMs = 0;
    f.durationMs = 34000; // 34 s is slow for dark (> 32) but fine for medium
    Advice a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::MEDIUM), static_cast<int>(a.size));
    f.durationMs = 25000;
    f.finalWeight = 40.0f; // 1:2.2 is long for dark
    a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::YIELD_DOWN), static_cast<int>(a.change));
    f.finalWeight = 30.0f; // 1:1.7 ok for dark ...
    f.avgTemp = 93.0f;     // ... but 93 C is hot for dark
    a = advise(f, Taste::BITTER);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::TEMP_DOWN), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Roast::DARK), static_cast<int>(roastFromLabel("dark")));
    TEST_ASSERT_EQUAL(static_cast<int>(Roast::UNKNOWN), static_cast<int>(roastFromLabel("")));
}

void test_time_cap_short_of_target_is_too_fine() {
    Facts f = normalShot();
    f.hitTimeCap = true;
    f.targetWeight = 36.0f;
    f.finalWeight = 24.0f; // 67 % of target when the cap hit
    f.durationMs = 60000;
    Advice a = advise(f, Taste::BALANCED); // even "balanced": the shot did not finish
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_EQUAL(static_cast<int>(Size::LARGE), static_cast<int>(a.size));
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "Timed out"));
    f.finalWeight = 34.0f; // 94 %: close enough, normal rules apply
    a = advise(f, Taste::BALANCED);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
}

void test_profile_pressure_scales_pressure_rules() {
    Facts f = normalShot();
    f.profilePressure = 6.0f; // a 6 bar profile
    f.maxPressure = 5.5f;
    f.firstDripMs = 0;
    f.durationMs = 18000;
    Advice a = advise(f, Taste::BALANCED); // 6 bar is what was asked for: no low-pressure remark
    TEST_ASSERT_EQUAL(static_cast<int>(Change::HOLD), static_cast<int>(a.change));
    TEST_ASSERT_NULL(strstr(a.reason, "low pressure"));
    f.profilePressure = 0.0f; // unknown profile pressure: a 6 bar peak on a fast shot is low
    a = advise(f, Taste::BALANCED);
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "low pressure"));
    // Choke detection follows the profile pressure too
    f = normalShot();
    f.profilePressure = 6.0f;
    f.maxPressure = 5.8f;
    f.firstDripMs = 18000;
    f.durationMs = 50000;
    a = advise(f, Taste::SOUR);
    TEST_ASSERT_EQUAL(static_cast<int>(Change::GRIND_COARSER), static_cast<int>(a.change));
    TEST_ASSERT_NOT_NULL(strstr(a.reason, "choked"));
}

void test_labels_round_trip() {
    TEST_ASSERT_EQUAL_STRING("grind_finer", changeLabel(Change::GRIND_FINER));
    TEST_ASSERT_EQUAL_STRING("sour", tasteLabel(Taste::SOUR));
    TEST_ASSERT_EQUAL_STRING("", tasteLabel(Taste::UNKNOWN));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_aborted_shot_gives_no_advice);
    RUN_TEST(test_unknown_taste_asks_for_feedback);
    RUN_TEST(test_channeling_beats_taste);
    RUN_TEST(test_choked_puck_is_coarser_large);
    RUN_TEST(test_sour_fast_is_finer_with_size_boundary);
    RUN_TEST(test_extraction_time_counts_from_first_drip);
    RUN_TEST(test_sour_short_ratio_is_yield_up);
    RUN_TEST(test_sour_cool_is_temp_up);
    RUN_TEST(test_sour_in_range_after_finer_switches_to_dose);
    RUN_TEST(test_bitter_slow_is_coarser);
    RUN_TEST(test_bitter_long_ratio_is_yield_down);
    RUN_TEST(test_bitter_hot_is_temp_down);
    RUN_TEST(test_bitter_in_range_after_coarser_switches_to_dose);
    RUN_TEST(test_balanced_is_hold);
    RUN_TEST(test_balanced_low_pressure_hint);
    RUN_TEST(test_oscillation_guard_reduces_size);
    RUN_TEST(test_no_scale_still_advises_on_time);
    RUN_TEST(test_light_roast_widens_ranges);
    RUN_TEST(test_dark_roast_tightens_ranges);
    RUN_TEST(test_time_cap_short_of_target_is_too_fine);
    RUN_TEST(test_profile_pressure_scales_pressure_rules);
    RUN_TEST(test_labels_round_trip);
    return UNITY_END();
}
