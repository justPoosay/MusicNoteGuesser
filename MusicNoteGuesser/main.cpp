#include "raylib.h"
#include <string>
#include <cmath>
#include <vector>

// --- Configuration ---
const int screenWidth = 1000;
const int screenHeight = 650;
const int sampleRate = 44100;

// --- Synth Logic ---
float frequency = 0.0f;
float audioTime = 0.0f;
float volume = 0.0f;

// Triangle wave formula: 2/PI * asin(sin(2*PI * f * t))
float GenerateTriangle(float freq, float time) {
    if (freq <= 0) return 0.0f;
    return (2.0f / PI) * asinf(sinf(2.0f * PI * freq * time));
}

void MyAudioCallback(void* buffer, unsigned int frames) {
    float* fbuf = (float*)buffer;
    for (unsigned int i = 0; i < frames; i++) {
        fbuf[i] = GenerateTriangle(frequency, audioTime) * volume;
        audioTime += 1.0f / sampleRate;
        volume *= 0.99994f; // Decay over time
    }
}

void TriggerNote(int midi) {
    frequency = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    volume = 0.4f;
    audioTime = 0.0f; // Reset phase to avoid clicks
}

// --- Key Mapping (FL Studio Style) ---
int GetMidiFromKey(int key) {
    switch (key) {
        // Bottom Row: C4 - E5 (White)
    case KEY_Z: return 60; case KEY_X: return 62; case KEY_C: return 64; case KEY_V: return 65;
    case KEY_B: return 67; case KEY_N: return 69; case KEY_M: return 71; case KEY_COMMA: return 72;
    case KEY_PERIOD: return 74; case KEY_SLASH: return 76;
        // Top Row: C5 - E6 (White)
    case KEY_Q: return 72; case KEY_W: return 74; case KEY_E: return 76; case KEY_R: return 77;
    case KEY_T: return 79; case KEY_Y: return 81; case KEY_U: return 83; case KEY_I: return 84;
    case KEY_O: return 86; case KEY_P: return 88;
        // Black Keys: (Patterns mapping to Sharps)
    case KEY_S: return 61; case KEY_D: return 63; case KEY_G: return 66; case KEY_H: return 68;
    case KEY_J: return 70; case KEY_L: return 73; case KEY_SEMICOLON: return 75;
    case KEY_TWO: return 73; case KEY_THREE: return 75; case KEY_FIVE: return 78;
    case KEY_SIX: return 80; case KEY_SEVEN: return 82; case KEY_NINE: return 85;
    case KEY_ZERO: return 87; case KEY_EQUAL: return 90;
    default: return -1;
    }
}

std::string GetNoteName(int midi) {
    if (midi < 0) return "";
    std::string names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[midi % 12] + std::to_string((midi / 12) - 1);
}

int main() {
    // 1. Initialization
    InitWindow(screenWidth, screenHeight, "Ear Trainer - C4 to E6");
    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(1024);
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    // 2. Game State
    int targetNote = -1;
    int userGuess = -1;
    bool gameStarted = false;
    std::string feedback = "Press SPACE to generate a random note";

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // --- Input Logic ---
        if (IsKeyPressed(KEY_SPACE)) {
            targetNote = GetRandomValue(60, 88); // Range C4 (60) to E6 (88)
            TriggerNote(targetNote);
            gameStarted = true;
            feedback = "Guess the note!";
            userGuess = -1;
        }

        int pressedKey = GetKeyPressed();
        int pressedMidi = GetMidiFromKey(pressedKey);

        if (pressedMidi != -1 && gameStarted) {
            userGuess = pressedMidi;
            if (userGuess == targetNote) {
                TriggerNote(userGuess);
                feedback = "CORRECT! Press SPACE for new note.";
            }
            else {
                // Play target note again to help the user
                TriggerNote(targetNote);
                feedback = "WRONG! Listen again and try.";
            }
        }

        // --- Drawing ---
        BeginDrawing();
        ClearBackground(GetColor(0x121212FF));

        // Note Previewer (Note Symbol)
        if (userGuess != -1) {
            DrawText(GetNoteName(userGuess).c_str(), 470, 40, 60, GOLD);
        }
        else {
            DrawText("?", 485, 40, 60, GRAY);
        }

        // Spectrogram/Visualizer Box (Center)
        DrawRectangleLines(50, 120, 900, 250, DARKGRAY);
        for (int i = 0; i < 898; i++) {
            // Visual oscillation based on frequency/volume
            float wave = GenerateTriangle(frequency * 0.02f, (float)i * 0.01f + (float)GetTime() * 5.0f);
            DrawPixel(51 + i, 245 + (int)(wave * volume * 150.0f), VIOLET);
        }

        DrawText(feedback.c_str(), 50, 390, 20, LIGHTGRAY);

        // --- Keyboard Rendering ---
        float startX = 75.0f;
        float whiteWidth = 50.0f;
        float whiteHeight = 180.0f;
        float blackWidth = 32.0f;
        float blackHeight = 100.0f;

        // Pass 1: White Keys (17 keys for C4-E6 range)
        for (int i = 0; i < 17; i++) {
            Rectangle r = { startX + (i * whiteWidth), 440, whiteWidth - 2, whiteHeight };
            DrawRectangleRec(r, RAYWHITE);
            DrawRectangleLinesEx(r, 1, BLACK);
        }

        // Pass 2: Black Keys (Thinner and Shorter)
        int sharpsPattern[] = { 0, 1, 3, 4, 5 }; // Index of white key that has a sharp after it
        for (int i = 0; i < 16; i++) {
            int noteInOctave = i % 7;
            bool needsSharp = false;
            for (int p : sharpsPattern) if (noteInOctave == p) needsSharp = true;

            if (needsSharp) {
                Rectangle br = {
                    startX + (i * whiteWidth) + (whiteWidth * 0.68f),
                    440,
                    blackWidth,
                    blackHeight
                };
                DrawRectangleRec(br, BLACK);
            }
        }

        EndDrawing();
    }

    // Cleanup
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}