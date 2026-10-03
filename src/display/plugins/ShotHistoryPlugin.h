#ifndef SHOTHISTORYPLUGIN_H
#define SHOTHISTORYPLUGIN_H

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <display/core/Plugin.h>
#include <display/core/ShotCoach.h>
#include <display/core/utils.h>
#include <display/models/shot_log_format.h>

struct Event;

constexpr size_t SHOT_HISTORY_INTERVAL = 100;
constexpr size_t MIN_FREE_SPACE_BYTES = 500 * 1024;         // 500 KB reserved free space
constexpr unsigned long EXTENDED_RECORDING_DURATION = 3000; // 3 seconds
constexpr unsigned long WEIGHT_STABILIZATION_TIME = 1000;   // 1 second
constexpr float WEIGHT_STABILIZATION_THRESHOLD = 0.1f;      // 0.1g threshold

class ShotHistoryPlugin : public Plugin {
  public:
    ShotHistoryPlugin() = default;

    void setup(Controller *controller, PluginManager *pluginManager) override;
    void loop() override {};

    void record();

    void handleRequest(JsonDocument &request, JsonDocument &response);

    // Index management methods
    bool appendToIndex(const ShotIndexEntry &entry);
    void updateIndexMetadata(uint32_t shotId, uint8_t rating, uint16_t volume);
    void markIndexDeleted(uint32_t shotId);
    void rebuildIndex();
    void startAsyncRebuild();
    bool ensureIndexExists();

    // Read up to maxCount most recent non-deleted index entries, newest first.
    // Returns the number of entries written to outEntries.
    size_t readRecentEntries(ShotIndexEntry *outEntries, size_t maxCount);

  private:
    // Index helper functions
    bool readIndexHeader(File &indexFile, ShotIndexHeader &header);
    int findEntryPosition(File &indexFile, const ShotIndexHeader &header, uint32_t shotId);
    bool readEntryAtPosition(File &indexFile, size_t position, ShotIndexEntry &entry);
    bool writeEntryAtPosition(File &indexFile, size_t position, const ShotIndexEntry &entry);
    void saveNotes(const String &id, const JsonDocument &notes);
    void loadNotes(const String &id, JsonDocument &notes);
    // Shot Coach: publish `shot:facts` once the shot is persisted, store the taste/advice from `shot:feedback`.
    void publishShotFacts(bool aborted, uint32_t durationMs, float finalWeight);
    void lookupPreviousShot(const char *profileId, uint8_t &taste, uint8_t &advice, float &doseIn);
    void saveFeedback(Event const &event);
    void startRecording();

    uint16_t getSystemInfo(); // Helper to pack system state bits

    unsigned long getTime();

    void endRecording();
    void endExtendedRecording();
    void cleanupHistory();
    size_t getFreeSpace();

    void recordPhaseTransition(uint8_t phaseNumber, uint16_t sampleIndex,
                               uint8_t reason); // Helper for phase transitions

    Controller *controller = nullptr;
    PluginManager *pluginManager = nullptr;
    FS *fs = &LittleFS;
    String currentId = "";
    bool isFileOpen = false;
    File currentFile;
    ShotLogHeader header{};
    uint32_t sampleCount = 0;
    uint8_t ioBuffer[4096];
    size_t ioBufferPos = 0; // bytes used

    bool recording = false;
    bool extendedRecording = false;
    bool shotStartedVolumetric = false; // Track initial volumetric mode
    double currentBrewDelay = 0.0;      // Brew delay (ms) the active shot was started with
    unsigned long shotStart = 0;
    unsigned long extendedRecordingStart = 0;
    unsigned long lastWeightChangeTime = 0;
    float currentTemperature = 0.0f;
    float currentBluetoothWeight = 0.0f;
    float lastStableWeight = 0.0f;
    float lastBluetoothWeight = 0.0f;
    float currentBluetoothFlow = 0.0f;
    float currentEstimatedWeight = 0.0f;
    float currentPuckResistance = 0.0f;
    String currentProfileName;

    // Phase transition tracking (v5+)
    uint8_t lastRecordedPhase = 0xFF; // Invalid initial value to detect first phase
    uint8_t finalExitReason = 0;      // Reason the shot ended (PhaseExitReason); captured at brew end

    // Running aggregates for the current shot, used to populate the index
    // entry at completion without re-parsing the .slog file (see record()).
    uint32_t tempSumScaled = 0; // sum of sample.ct (°C * 10)
    uint32_t tempSampleCount = 0;
    uint16_t maxPressureScaled = 0; // max of sample.cp (bar * 10)
    uint32_t flowSumScaled = 0;     // sum of positive sample.fl (ml/s * 100)
    uint32_t positiveFlowCount = 0;

    // Shot Coach facts gathered while recording (see record()).
    uint32_t firstDripMs = 0;      // first sample with >= 1 g on the scale, 0 = none
    uint16_t channelingEvents = 0; // puck-flow spikes with a simultaneous pressure drop
    uint32_t lastChannelingMs = 0;
    uint16_t targetTempScaled = 0;              // target temperature at shot start (°C * 10)
    float coachTargetWeight = 0.0f;             // volumetric target of the shot, 0 when time based
    float coachProfilePressure = 0.0f;          // highest pressure target in the profile, 0 if flow only
    String coachRoast;                          // profile roast label
    static constexpr size_t CHANNEL_WINDOW = 5; // compare against the sample ~0.5 s earlier
    int16_t recentPuckFlow[CHANNEL_WINDOW] = {0};
    uint16_t recentPressure[CHANNEL_WINDOW] = {0};
    size_t recentPos = 0;
    size_t recentCount = 0;

    // Async rebuild state
    bool rebuildInProgress = false;

    xTaskHandle taskHandle;
    void flushBuffer();
    static void loopTask(void *arg);
};

extern ShotHistoryPlugin ShotHistory;

#endif // SHOTHISTORYPLUGIN_H
