#include "raylib.h"
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>
#include <atomic>

// --- Configuration ---
const int screenWidth = 1250;
const int screenHeight = 650;
const int sampleRate = 44100;
const int sidebarWidth = 250;

enum WaveType { SINE, SQUARE, TRIANGLE, SAW };
WaveType currentWave = TRIANGLE;

// --- Waveform Visualizer ---
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

// --- Audio Synthesis ---
float GenerateWave(float freq, float time, WaveType type) {
    if (freq <= 0) return 0.0f;
    float t = freq * time;
    switch (type) {
    case SINE:     return sinf(2.0f * PI * t);
    case SQUARE:   return (sinf(2.0f * PI * t) >= 0) ? 1.0f : -1.0f;
    case TRIANGLE: return (2.0f / PI) * asinf(sinf(2.0f * PI * t));
    case SAW:      return 2.0f * (t - floorf(0.5f + t));
    default:       return 0.0f;
    }
}

void MyAudioCallback(void* buffer, unsigned int frames) {
    float* fbuf = (float*)buffer;
    for (unsigned int i = 0; i < frames; i++) {
        float mixedSample = 0;
        float visualSample = 0;
        for (int v = 0; v < 15; v++) {
            if (voices[v].vol > 0.0001f) {
                float s = GenerateWave(voices[v].freq, voices[v].time, currentWave) * voices[v].vol;
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

// --- Training Logic ---
enum Difficulty { EASY, MEDIUM, HARD, HARDER };
Difficulty currentDiff = EASY;

struct Challenge {
    std::vector<int> notes;
    std::string name;
};

Challenge currentChallenge;
std::vector<int> lastChallengeNotes;
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
            c.notes = { root, root + GetRandomValue(1, 12) };
            c.name = "Interval";
        }
        else if (diff == HARD) {
            bool isMajor = GetRandomValue(0, 1) == 0;
            int third = isMajor ? 4 : 3;
            int inv = GetRandomValue(0, 2);
            if (inv == 0)      c.notes = { root, root + third, root + 7 };
            else if (inv == 1) c.notes = { root + third, root + 7, root + 12 };
            else               c.notes = { root + 7, root + 12, root + 12 + third };
            c.name = isMajor ? "Major Chord" : "Minor Chord";
        }
        else {
            int types = GetRandomValue(0, 3);
            if (types == 0)      c.notes = { root, root + 4, root + 7 };
            else if (types == 1) c.notes = { root, root + 3, root + 7 };
            else if (types == 2) c.notes = { root, root + 4, root + 8 };
            else                 c.notes = { root, root + 3, root + 6 };
            c.name = "Advanced Chord Type";
        }
        std::sort(c.notes.begin(), c.notes.end());
        if (c.notes != lastChallengeNotes) { lastChallengeNotes = c.notes; isSame = false; }
    }
    return c;
}

void StartNewChallenge(std::string& feedback, bool playImmediately) {
    currentChallenge = Generate(currentDiff);
    challengeSolved = false;
    waitingForNext = false;
    if (playImmediately) { for (int n : currentChallenge.notes) TriggerNote(n); }
    feedback = "Identify the " + currentChallenge.name;
}

int GetMidiFromKey(int key) {
    switch (key) {
    case KEY_Z: return 60; case KEY_S: return 61; case KEY_X: return 62; case KEY_D: return 63;
    case KEY_C: return 64; case KEY_V: return 65; case KEY_G: return 66; case KEY_B: return 67;
    case KEY_H: return 68; case KEY_N: return 69; case KEY_J: return 70; case KEY_M: return 71;
    case KEY_COMMA: return 72; case KEY_L: return 73; case KEY_PERIOD: return 74;
    case KEY_SEMICOLON: return 75; case KEY_SLASH: return 76;
    case KEY_Q: return 72; case KEY_TWO: return 73; case KEY_W: return 74; case KEY_THREE: return 75;
    case KEY_E: return 76; case KEY_R: return 77; case KEY_FIVE: return 78; case KEY_T: return 79;
    case KEY_SIX: return 80; case KEY_Y: return 81; case KEY_SEVEN: return 82; case KEY_U: return 83;
    case KEY_I: return 84; case KEY_NINE: return 85; case KEY_O: return 86; case KEY_ZERO: return 87;
    case KEY_P: return 88; case KEY_EQUAL: return 89;
    default: return -1;
    }
}

int main() {
    InitWindow(screenWidth, screenHeight, "Music Note Guesser");

    Font fontConsolas = LoadFont("C:\\Windows\\Fonts\\consola.ttf");
    SetTextureFilter(fontConsolas.texture, TEXTURE_FILTER_BILINEAR);

    Image icon = LoadImage("icon.png");
    SetWindowIcon(icon);
    UnloadImage(icon);

    InitAudioDevice();
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    SetTargetFPS(60);

    float successTimer = 0, postSuccessTimer = 0;
    int successStep = 0;
    std::string feedback;

    // Security States
    int lastMouseMidi = -1;
    bool wasMouseDownLastFrame = false;
    float lastKeyTime = 0;
    float keyboardDragBlockTimer = 0;

    StartNewChallenge(feedback, false);

    int whiteMidi[] = { 60,62,64,65,67,69,71,72,74,76,77,79,81,83,84,86,88 };
    int blackMidi[] = { 61,63,-1,66,68,70,-1,73,75,-1,78,80,82,-1,85,87 };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (successTimer > 0) successTimer -= dt;
        if (postSuccessTimer > 0) postSuccessTimer -= dt;
        if (keyboardDragBlockTimer > 0) keyboardDragBlockTimer -= dt;

        // Success Melody Logic
        if (challengeSolved && successStep > 0 && successTimer <= 0) {
            int seq[] = { 60, 64, 67, 72 };
            TriggerNote(seq[successStep - 1], 0.6f, false);
            successTimer = 0.12f; successStep++;
            if (successStep > 4) { successStep = 0; waitingForNext = true; postSuccessTimer = 1.0f; }
        }
        if (waitingForNext && postSuccessTimer <= 0) StartNewChallenge(feedback, true);

        Vector2 mouse = GetMousePosition();
        activeKeys.clear();

        // 1. Sidebar UI
        if (IsMouseButtonPressed(0) && mouse.x < sidebarWidth) {
            Difficulty oldD = currentDiff;
            if (CheckCollisionPointRec(mouse, { 20, 60, 210, 40 })) currentDiff = EASY;
            if (CheckCollisionPointRec(mouse, { 20, 110, 210, 40 })) currentDiff = MEDIUM;
            if (CheckCollisionPointRec(mouse, { 20, 160, 210, 40 })) currentDiff = HARD;
            if (CheckCollisionPointRec(mouse, { 20, 210, 210, 40 })) currentDiff = HARDER;
            if (currentDiff != oldD) StartNewChallenge(feedback, false);
            if (CheckCollisionPointRec(mouse, { 20, 280, 210, 40 })) currentWave = (WaveType)((currentWave + 1) % 4);
        }

        // 2. Keyboard Input With Slide Security
        for (int k = 32; k < 349; k++) {
            int midi = GetMidiFromKey(k);
            if (midi != -1) {
                if (IsKeyDown(k)) activeKeys.insert(midi);
                if (IsKeyPressed(k)) {
                    TriggerNote(midi);
                    float curT = (float)GetTime();
                    // Detect slide: if interval between keys is < 120ms
                    if (curT - lastKeyTime < 0.12f) keyboardDragBlockTimer = 0.5f;
                    lastKeyTime = curT;
                }
            }
        }

        // 3. Mouse Piano Input (With Glissando Security)
        float sX = sidebarWidth + 50;
        int currentMouseMidi = -1;
        for (int i = 0; i < 16; i++) {
            if (blackMidi[i] != -1) {
                Rectangle br = { sX + (i * 50) + 35, 400, 30, 100 };
                if (CheckCollisionPointRec(mouse, br)) { currentMouseMidi = blackMidi[i]; break; }
            }
        }
        if (currentMouseMidi == -1) {
            for (int i = 0; i < 17; i++) {
                Rectangle wr = { sX + (i * 50), 400, 48, 180 };
                if (CheckCollisionPointRec(mouse, wr)) { currentMouseMidi = whiteMidi[i]; break; }
            }
        }

        bool mouseTrigger = false;
        if (currentMouseMidi != -1 && IsMouseButtonDown(0)) {
            activeKeys.insert(currentMouseMidi);
            if (!wasMouseDownLastFrame || currentMouseMidi != lastMouseMidi) mouseTrigger = true;
        }
        if (mouseTrigger) TriggerNote(currentMouseMidi);

        // 4. Solve Detection
        if (!challengeSolved && !currentChallenge.notes.empty()) {
            bool possible = (activeKeys.size() == currentChallenge.notes.size());

            // Security Checks
            if (IsMouseButtonDown(0) && !IsMouseButtonPressed(0)) possible = false;
            if (keyboardDragBlockTimer > 0) possible = false;

            if (possible) {
                bool match = true;
                for (int n : currentChallenge.notes) if (activeKeys.find(n) == activeKeys.end()) match = false;
                if (match) { challengeSolved = true; successStep = 1; successTimer = 0.5f; feedback = "Correct!"; }
            }
        }

        if (IsKeyPressed(KEY_SPACE) && !challengeSolved) {
            for (int n : currentChallenge.notes) TriggerNote(n);
        }

        lastMouseMidi = currentMouseMidi;
        wasMouseDownLastFrame = IsMouseButtonDown(0);

        // 5. Drawing
        BeginDrawing();
        ClearBackground({ 18, 18, 18, 255 });

        // UI Sidebar
        DrawRectangle(0, 0, sidebarWidth, 650, { 35, 35, 35, 255 });
        auto drawBtn = [&](Rectangle r, const char* t, bool active, Color c = VIOLET) {
            DrawRectangleRec(r, active ? c : DARKGRAY);
            Vector2 textPos = { r.x + 20, r.y + 10 };
            DrawTextEx(fontConsolas, t, textPos, 20, 1.0f, WHITE);
        };
        drawBtn({ 20, 60, 210, 40 }, "EASY", currentDiff == EASY);
        drawBtn({ 20, 110, 210, 40 }, "MEDIUM", currentDiff == MEDIUM);
        drawBtn({ 20, 160, 210, 40 }, "HARD", currentDiff == HARD);
        drawBtn({ 20, 210, 210, 40 }, "HARDER", currentDiff == HARDER);
        const char* waveNames[] = { "SINE", "SQUARE", "TRIANGLE", "SAWTOOTH" };
        drawBtn({ 20, 280, 210, 40 }, TextFormat("WAVE: %s", waveNames[currentWave]), true, DARKBLUE);

        // Waveform Visualizer
        int boxX = sidebarWidth + 50;
        DrawRectangleLines(boxX, 80, 900, 220, DARKGRAY);
        int start = writeIdx.load();
        for (int i = 1; i < MAX_WAVE_SAMPLES; i++) {
            DrawLine(boxX + i - 1, 190 + (int)(safeWaveform[(start + i - 1) % MAX_WAVE_SAMPLES] * 80),
                boxX + i, 190 + (int)(safeWaveform[(start + i) % MAX_WAVE_SAMPLES] * 80), VIOLET);
        }
        DrawTextEx(fontConsolas, feedback.c_str(), { (float)boxX, 320 }, 25, 1.0f, WHITE);
        DrawTextEx(fontConsolas, "SPACE to Replay | Play chords clearly (no sliding!)", { (float)boxX, 355 }, 18, 1.0f, GRAY);

        // Keyboard Rendering
        for (int i = 0; i < 17; i++) {
            Rectangle r = { sX + (i * 50), 400, 48, 180 };
            bool pressed = activeKeys.count(whiteMidi[i]);
            DrawRectangleRec(r, pressed ? RED : RAYWHITE);
            DrawRectangleLinesEx(r, 1, BLACK);
            if (whiteMidi[i] % 12 == 0) {
                int oct = (whiteMidi[i] / 12) - 1;
                Rectangle lBox = { r.x + 4, r.y + 155, r.width - 8, 20 };
                DrawRectangleRounded(lBox, 0.4f, 8, pressed ? MAROON : LIGHTGRAY);
                Vector2 labelPos = {
                    lBox.x + lBox.width / 2 - MeasureTextEx(fontConsolas, TextFormat("C%d", oct), 15, 1.0f).x / 2,
                    lBox.y + 3
                };
                DrawTextEx(fontConsolas, TextFormat("C%d", oct), labelPos, 15, 1.0f, pressed ? WHITE : DARKGRAY);
            }
        }
        for (int i = 0; i < 16; i++) {
            if (blackMidi[i] != -1) {
                Rectangle br = { sX + (i * 50) + 35, 400, 30, 100 };
                DrawRectangleRec(br, activeKeys.count(blackMidi[i]) ? RED : BLACK);
                DrawRectangleLinesEx(br, 1, activeKeys.count(blackMidi[i]) ? BLACK : DARKGRAY);
            }
        }

        EndDrawing();
    }

    UnloadFont(fontConsolas);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}