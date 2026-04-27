#include "raylib.h"
#include <string>
#include <cmath>
#include <vector>

const int screenWidth = 1000;
const int screenHeight = 650;
const int sampleRate = 44100;

// --- Audio Engine ---
float frequency = 0.0f;
float audioTime = 0.0f;
float volume = 0.0f;
float visualVolume = 0.0f; // Separate volume for the spectrogram

float GenerateTriangle(float freq, float time) {
    if (freq <= 0) return 0.0f;
    return (2.0f / PI) * asinf(sinf(2.0f * PI * freq * time));
}

void MyAudioCallback(void* buffer, unsigned int frames) {
    float* fbuf = (float*)buffer;
    for (unsigned int i = 0; i < frames; i++) {
        fbuf[i] = GenerateTriangle(frequency, audioTime) * volume;
        audioTime += 1.0f / sampleRate;
        volume *= 0.99994f;
        visualVolume *= 0.99994f; // Decay the visualizer gain too
    }
}

// Added 'showInSpectrogram' flag
void TriggerNote(int midi, float vol = 0.4f, bool showInSpectrogram = true) {
    if (midi < 0) return;
    frequency = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    volume = vol;
    visualVolume = showInSpectrogram ? vol : 0.0f;
    audioTime = 0.0f;
}

int GetMidiFromKey(int key) {
    switch (key) {
    case KEY_Z: return 60; case KEY_X: return 62; case KEY_C: return 64; case KEY_V: return 65;
    case KEY_B: return 67; case KEY_N: return 69; case KEY_M: return 71; case KEY_COMMA: return 72;
    case KEY_PERIOD: return 74; case KEY_SLASH: return 76;
    case KEY_Q: return 72; case KEY_W: return 74; case KEY_E: return 76; case KEY_R: return 77;
    case KEY_T: return 79; case KEY_Y: return 81; case KEY_U: return 83; case KEY_I: return 84;
    case KEY_O: return 86; case KEY_P: return 88;
    case KEY_S: return 61; case KEY_D: return 63; case KEY_G: return 66; case KEY_H: return 68;
    case KEY_J: return 70; case KEY_L: return 73; case KEY_SEMICOLON: return 75;
    case KEY_TWO: return 73; case KEY_THREE: return 75; case KEY_FIVE: return 78;
    case KEY_SIX: return 80; case KEY_SEVEN: return 82; case KEY_NINE: return 85;
    case KEY_ZERO: return 87; case KEY_EQUAL: return 90;
    default: return -1;
    }
}

int main() {
    InitWindow(screenWidth, screenHeight, "Ear Trainer Pro");
    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(1024);
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    int targetNote = -1;
    int lastPlayedMidi = -1;
    float timer = 0.0f;
    int sequenceStep = 0; // 0: Idle, 1: Wrong Seq, 2: Correct Seq
    int subStep = 0;
    std::string feedback = "Press ENTER to Start";

    SetTargetFPS(60);

    // Keyboard geometry
    float startX = 75.0f;
    float kw = 50.0f, kh = 180.0f;
    int whiteMidiTable[] = { 60,62,64,65,67,69,71,72,74,76,77,79,81,83,84,86,88 };
    int blackMidiTable[] = { 61,63,-1,66,68,70,-1,73,75,-1,78,80,82,-1,85,87 };

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (timer > 0) timer -= dt;

        // --- Sequence State Machine ---
        if (sequenceStep == 1 && timer <= 0) { // WRONG SEQUENCE
            TriggerNote(targetNote); // Replay Target (Visible)
            sequenceStep = 0;
            feedback = "Listen again...";
        }
        else if (sequenceStep == 2 && timer <= 0) { // CORRECT SEQUENCE
            if (subStep == 0) {
                TriggerNote(64, 0.3f, false); // E4 (Invisible)
                timer = 0.2f;
                subStep++;
            }
            else {
                TriggerNote(72, 0.3f, false); // C5 (Invisible)
                sequenceStep = 0;
                subStep = 0;
                feedback = "Correct! ENTER for next.";
            }
        }

        // --- Input Handling ---
        int clickedMidi = -1;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mPos = GetMousePosition();
            // Check Black Keys first (since they are on top)
            for (int i = 0; i < 16; i++) {
                if (blackMidiTable[i] != -1) {
                    Rectangle br = { startX + (i * kw) + (kw * 0.7f), 400, 32, 105 };
                    if (CheckCollisionPointRec(mPos, br)) { clickedMidi = blackMidiTable[i]; break; }
                }
            }
            // Check White Keys if no black key was hit
            if (clickedMidi == -1) {
                for (int i = 0; i < 17; i++) {
                    Rectangle wr = { startX + (i * kw), 400, kw - 2, kh };
                    if (CheckCollisionPointRec(mPos, wr)) { clickedMidi = whiteMidiTable[i]; break; }
                }
            }
        }

        if (IsKeyPressed(KEY_ENTER) && sequenceStep == 0) {
            targetNote = GetRandomValue(60, 88);
            TriggerNote(targetNote);
            feedback = "Identify the note";
            lastPlayedMidi = -1;
        }

        int keyboardMidi = GetMidiFromKey(GetKeyPressed());
        int finalMidi = (clickedMidi != -1) ? clickedMidi : keyboardMidi;

        if (finalMidi != -1 && sequenceStep == 0 && targetNote != -1) {
            lastPlayedMidi = finalMidi;
            TriggerNote(finalMidi);

            if (finalMidi == targetNote) {
                sequenceStep = 2; timer = 0.6f; subStep = 0;
            }
            else {
                feedback = "Incorrect...";
                sequenceStep = 1; timer = 1.2f; subStep = 0;
            }
        }

        // --- Drawing ---
        BeginDrawing();
        ClearBackground(GetColor(0x111111FF));

        // Spectrogram (Uses visualVolume)
        DrawRectangleLines(50, 80, 900, 220, DARKGRAY);
        for (int i = 0; i < 898; i++) {
            float wave = GenerateTriangle(frequency * 0.02f, (float)i * 0.01f + (float)GetTime() * 5.0f);
            DrawPixel(51 + i, 190 + (int)(wave * visualVolume * 150.0f), VIOLET);
        }

        DrawText(feedback.c_str(), 50, 320, 25, RAYWHITE);

        // Render Piano
        for (int i = 0; i < 17; i++) {
            Rectangle r = { startX + (i * kw), 400, kw - 2, kh };
            DrawRectangleRec(r, (whiteMidiTable[i] == lastPlayedMidi) ? RED : RAYWHITE);
            DrawRectangleLinesEx(r, 1, BLACK);
            if (whiteMidiTable[i] % 12 == 0) {
                DrawText(TextFormat("C%d", (whiteMidiTable[i] / 12) - 1), r.x + 10, r.y + kh - 25, 18, DARKGRAY);
            }
        }
        for (int i = 0; i < 16; i++) {
            if (blackMidiTable[i] != -1) {
                Rectangle br = { startX + (i * kw) + (kw * 0.7f), 400, 32, 105 };
                DrawRectangleRec(br, (blackMidiTable[i] == lastPlayedMidi) ? RED : BLACK);
            }
        }

        EndDrawing();
    }
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}