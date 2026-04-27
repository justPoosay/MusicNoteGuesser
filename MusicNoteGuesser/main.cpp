#include "raylib.h"
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>
#include <atomic>

const int screenWidth = 1250;
const int screenHeight = 650;
const int sampleRate = 44100;
const int sidebarWidth = 250;

// --- Thread-Safe Waveform ---
const int MAX_WAVE_SAMPLES = 900;
float safeWaveform[MAX_WAVE_SAMPLES] = { 0 };
std::atomic<int> writeIdx{ 0 };

struct Voice {
    float freq = 0;
    float vol = 0;
    float time = 0;
    bool showVisual = true;
};

Voice voices[15];

float GenerateTriangle(float freq, float time) {
    if (freq <= 0) return 0.0f;
    return (2.0f / PI) * asinf(sinf(2.0f * PI * freq * time));
}

void MyAudioCallback(void* buffer, unsigned int frames) {
    float* fbuf = (float*)buffer;
    for (unsigned int i = 0; i < frames; i++) {
        float mixedSample = 0;
        float visualSample = 0;
        for (int v = 0; v < 15; v++) {
            if (voices[v].vol > 0.0001f) {
                float s = GenerateTriangle(voices[v].freq, voices[v].time) * voices[v].vol;
                mixedSample += s;
                if (voices[v].showVisual) visualSample += s;
                voices[v].time += 1.0f / sampleRate;
                voices[v].vol *= 0.99994f;
            }
        }
        float limiter = 1.0f;
        if (std::abs(mixedSample) > 1.0f) limiter = 1.0f / std::abs(mixedSample);
        fbuf[i] = mixedSample * limiter;

        if (i % 64 == 0) {
            float vLim = 1.0f;
            if (std::abs(visualSample) > 1.0f) vLim = 1.0f / std::abs(visualSample);
            int idx = writeIdx.load();
            safeWaveform[idx] = visualSample * vLim;
            writeIdx.store((idx + 1) % MAX_WAVE_SAMPLES);
        }
    }
}

void TriggerNote(int midi, float vol = 0.3f, bool visual = true) {
    for (int v = 0; v < 15; v++) {
        if (voices[v].vol < 0.01f) {
            voices[v].freq = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
            voices[v].vol = vol;
            voices[v].time = 0;
            voices[v].showVisual = visual;
            break;
        }
    }
}

enum Difficulty { EASY, MEDIUM, HARD, HARDER };
Difficulty currentDiff = EASY;

struct Challenge {
    std::vector<int> notes;
    std::string name;
};

struct TimedNote {
    int midi;
    float time;
};
std::vector<TimedNote> inputBuffer;
float chordGracePeriod = 0.5f;

Challenge currentChallenge;
std::vector<int> lastChallengeNotes;
std::vector<int> userPicks; // Used primarily for EASY mode single-note tracking
std::set<int> activeKeys;
bool challengeSolved = false;
bool waitingForNext = false;

Challenge Generate(Difficulty diff) {
    Challenge c;
    int whiteMidi[] = { 60,62,64,65,67,69,71,72,74,76,77,79,81,83,84,86,88 };
    bool isSame = true;

    while (isSame) {
        int root = GetRandomValue(60, 72);

        if (diff == EASY) {
            c.notes = { whiteMidi[GetRandomValue(0, 16)] };
            c.name = "Single White Note";
        }
        else if (diff == MEDIUM) {
            int intervals[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
            c.notes = { root, root + intervals[GetRandomValue(0, 11)] };
            c.name = "Interval";
        }
        else if (diff == HARD) {
            bool isMajor = GetRandomValue(0, 1) == 0;
            int third = isMajor ? 4 : 3;
            int fifth = 7;

            // FIXED: Now includes 0 (Root Position)
            int inversion = GetRandomValue(0, 2);
            if (inversion == 0)      c.notes = { root, root + third, root + fifth };
            else if (inversion == 1) c.notes = { root + third, root + fifth, root + 12 };
            else                     c.notes = { root + fifth, root + 12, root + 12 + third };

            c.name = isMajor ? "Major Chord" : "Minor Chord";
        }
        else { // HARDER
            int types = GetRandomValue(0, 3);
            if (types == 0)      c.notes = { root, root + 4, root + 7 };
            else if (types == 1) c.notes = { root, root + 3, root + 7 };
            else if (types == 2) c.notes = { root, root + 4, root + 8 };
            else                 c.notes = { root, root + 3, root + 6 };
            c.name = "Advanced Chord Type";
        }

        std::vector<int> currentSorted = c.notes;
        std::sort(currentSorted.begin(), currentSorted.end());
        if (currentSorted != lastChallengeNotes) {
            lastChallengeNotes = currentSorted;
            isSame = false;
        }
    }
    return c;
}

void StartNewChallenge(std::string& feedback, bool playImmediately) {
    currentChallenge = Generate(currentDiff);
    userPicks.clear();
    challengeSolved = false;
    waitingForNext = false;
    if (playImmediately) {
        for (int n : currentChallenge.notes) TriggerNote(n);
    }
    feedback = "Identify the " + currentChallenge.name;
}

int GetMidiFromKey(int key) {
    switch (key) {
        // Lower Row (Starts C4)
    case KEY_Z: return 60; case KEY_S: return 61; case KEY_X: return 62; case KEY_D: return 63;
    case KEY_C: return 64; case KEY_V: return 65; case KEY_G: return 66; case KEY_B: return 67;
    case KEY_H: return 68; case KEY_N: return 69; case KEY_J: return 70; case KEY_M: return 71;
    case KEY_COMMA: return 72; case KEY_L: return 73; case KEY_PERIOD: return 74;
    case KEY_SEMICOLON: return 75; case KEY_SLASH: return 76;

        // Upper Row (Starts C5)
    case KEY_Q: return 72; case KEY_TWO: return 73; case KEY_W: return 74; case KEY_THREE: return 75;
    case KEY_E: return 76; case KEY_R: return 77; case KEY_FIVE: return 78; case KEY_T: return 79;
    case KEY_SIX: return 80; case KEY_Y: return 81; case KEY_SEVEN: return 82; case KEY_U: return 83;
    case KEY_I: return 84; case KEY_NINE: return 85; case KEY_O: return 86; case KEY_ZERO: return 87;
    case KEY_P: return 88; case KEY_EQUAL: return 89; // Fixed B4 and beyond
    default: return -1;
    }
}

int main() {
    InitWindow(screenWidth, screenHeight, "Ear Trainer - Chord Precision");
    InitAudioDevice();
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);
    SetTargetFPS(60);

    float successTimer = 0;
    float postSuccessTimer = 0;
    int successStep = 0;
    std::string feedback;

    StartNewChallenge(feedback, false);

    int whiteMidi[] = { 60,62,64,65,67,69,71,72,74,76,77,79,81,83,84,86,88 };
    int blackMidi[] = { 61,63,-1,66,68,70,-1,73,75,-1,78,80,82,-1,85,87 };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // 1. TIMERS & AUTO-ADVANCE
        if (successTimer > 0) successTimer -= dt;
        if (postSuccessTimer > 0) postSuccessTimer -= dt;

        if (challengeSolved && successStep > 0 && successTimer <= 0) {
            float loud = 0.6f;
            if (successStep == 1) { TriggerNote(60, loud, false); successTimer = 0.12f; successStep++; }
            else if (successStep == 2) { TriggerNote(64, loud, false); successTimer = 0.12f; successStep++; }
            else if (successStep == 3) { TriggerNote(67, loud, false); successTimer = 0.12f; successStep++; }
            else if (successStep == 4) {
                TriggerNote(72, loud, false);
                successStep = 0;
                waitingForNext = true;
                postSuccessTimer = 1.0f;
            }
        }

        if (waitingForNext && postSuccessTimer <= 0) {
            StartNewChallenge(feedback, true);
        }

        // 2. SIDEBAR & DIFFICULTY
        Vector2 m = GetMousePosition();
        if (IsMouseButtonPressed(0) && m.x < sidebarWidth) {
            Difficulty oldDiff = currentDiff;
            if (CheckCollisionPointRec(m, { 20, 60, 210, 40 })) currentDiff = EASY;
            if (CheckCollisionPointRec(m, { 20, 110, 210, 40 })) currentDiff = MEDIUM;
            if (CheckCollisionPointRec(m, { 20, 160, 210, 40 })) currentDiff = HARD;
            if (CheckCollisionPointRec(m, { 20, 210, 210, 40 })) currentDiff = HARDER;

            if (currentDiff != oldDiff) StartNewChallenge(feedback, false);
        }

        // 3. REPLAY
        if (IsKeyPressed(KEY_SPACE) && !currentChallenge.notes.empty() && !challengeSolved) {
            for (int n : currentChallenge.notes) TriggerNote(n);
        }

        // 4. INPUT HANDLING
        activeKeys.clear();
        for (int k = 32; k < 349; k++) {
            int midi = GetMidiFromKey(k);
            if (midi != -1) {
                if (IsKeyDown(k)) activeKeys.insert(midi); // Capture currently held keys
                if (IsKeyPressed(k)) TriggerNote(midi);    // Play sound on initial hit
            }
        }

        // 5. SOLVE LOGIC (Held-Key Comparison)
        if (!challengeSolved && !currentChallenge.notes.empty()) {
            bool isCorrect = false;

            if (currentDiff == EASY) {
                // For easy, we check if the ONLY key being held is the correct one
                if (activeKeys.size() == 1 && activeKeys.count(currentChallenge.notes[0])) {
                    isCorrect = true;
                }
            }
            else {
                // MEDIUM/HARD/HARDER: Check if the set of HELD keys matches the challenge
                if (activeKeys.size() == currentChallenge.notes.size()) {
                    bool match = true;
                    for (int target : currentChallenge.notes) {
                        if (activeKeys.find(target) == activeKeys.end()) {
                            match = false;
                            break;
                        }
                    }
                    if (match) isCorrect = true;
                }
            }

            if (isCorrect) {
                challengeSolved = true;
                successStep = 1;
                successTimer = 1.0f;
                feedback = "Correct!";
            }
        }

        // 6. RENDERING
        BeginDrawing();
        ClearBackground({ 18, 18, 18, 255 });

        // Sidebar
        DrawRectangle(0, 0, sidebarWidth, 650, { 35, 35, 35, 255 });
        auto drawBtn = [&](Rectangle r, const char* t, bool active) {
            DrawRectangleRec(r, active ? VIOLET : DARKGRAY);
            DrawText(t, (int)r.x + 40, (int)r.y + 10, 20, WHITE);
            };
        drawBtn({ 20, 60, 210, 40 }, "EASY", currentDiff == EASY);
        drawBtn({ 20, 110, 210, 40 }, "MEDIUM", currentDiff == MEDIUM);
        drawBtn({ 20, 160, 210, 40 }, "HARD", currentDiff == HARD);
        drawBtn({ 20, 210, 210, 40 }, "HARDER", currentDiff == HARDER);

        // Visualizer
        int boxX = sidebarWidth + 50;
        DrawRectangleLines(boxX, 80, 900, 220, DARKGRAY);
        int start = writeIdx.load();
        for (int i = 1; i < MAX_WAVE_SAMPLES; i++) {
            int idx1 = (start + i - 1) % MAX_WAVE_SAMPLES;
            int idx2 = (start + i) % MAX_WAVE_SAMPLES;
            DrawLine(boxX + i - 1, 190 + (int)(safeWaveform[idx1] * 80), boxX + i, 190 + (int)(safeWaveform[idx2] * 80), VIOLET);
        }
        DrawText(feedback.c_str(), boxX, 320, 25, WHITE);
        DrawText("SPACE to Replay | Hold all notes of the chord to identify", boxX, 355, 18, GRAY);

        // Piano Keyboard
        float sX = (float)sidebarWidth + 50;
        for (int i = 0; i < 17; i++) {
            Rectangle r = { sX + (i * 50), 400, 48, 180 };
            bool pressed = activeKeys.count(whiteMidi[i]);
            DrawRectangleRec(r, pressed ? RED : RAYWHITE);
            DrawRectangleLinesEx(r, 1, BLACK);

            if (whiteMidi[i] % 12 == 0) {
                int oct = (whiteMidi[i] / 12) - 1;
                Rectangle lBox = { r.x + 4, r.y + 150, r.width - 8, 22 };
                DrawRectangleRounded(lBox, 0.4f, 8, pressed ? MAROON : LIGHTGRAY);
                int tw = MeasureText(TextFormat("C%d", oct), 15);
                DrawText(TextFormat("C%d", oct), (int)(lBox.x + lBox.width / 2 - tw / 2), (int)lBox.y + 4, 15, pressed ? WHITE : DARKGRAY);
            }
        }
        for (int i = 0; i < 16; i++) {
            if (blackMidi[i] != -1) {
                bool pressed = activeKeys.count(blackMidi[i]);
                Rectangle br = { sX + (i * 50) + 35, 400, 30, 100 };
                DrawRectangleRec(br, pressed ? RED : BLACK);
                DrawRectangleLinesEx(br, 1, pressed ? BLACK : DARKGRAY);
            }
        }

        EndDrawing();
    }
    CloseAudioDevice();
    CloseWindow();
    return 0;
}