#include "raylib.h"
#include <vector>
#include <string>
#include <cmath>

// --- Configuration ---
const int screenWidth = 800;
const int screenHeight = 600;
const int sampleRate = 44100;

// Key mapping
int GetMidiFromKey(int key) {
    switch (key) {
        // --- Bottom Row ---
        case KEY_Z: return 60; case KEY_S: return 61; case KEY_X: return 62;
        case KEY_D: return 63; case KEY_C: return 64; case KEY_V: return 65;
        case KEY_G: return 66; case KEY_B: return 67; case KEY_H: return 68;
        case KEY_N: return 69; case KEY_J: return 70; case KEY_M: return 71;

        // --- Top Row ---
        case KEY_Q: return 72; case KEY_TWO: return 73; case KEY_W: return 74;
        case KEY_THREE: return 75; case KEY_E: return 76; case KEY_R: return 77;
        case KEY_FIVE: return 78; case KEY_T: return 79; case KEY_SIX: return 80;
        case KEY_Y: return 81; case KEY_SEVEN: return 82; case KEY_U: return 83;

        default: return -1;
    }
}

std::string GetNoteName(int midi) {
    std::string names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[midi % 12] + std::to_string((midi / 12) - 1);
}

// --- Audio State ---
float frequency = 0.0f;
float audioTime = 0.0f;
float volume = 0.0f;

void MyAudioCallback(void* buffer, unsigned int frames) {
    float* out = (float*)buffer;
    for (unsigned int i = 0; i < frames; i++) {
        out[i] = sinf(2.0f * PI * frequency * audioTime) * volume;
        audioTime += 1.0f / sampleRate;
        volume *= 0.9995f; 
    }
}

int main() {
    InitWindow(screenWidth, screenHeight, "Ear Trainer - Raylib");
    InitAudioDevice();

    SetAudioStreamBufferSizeDefault(1024);
    AudioStream stream = LoadAudioStream(sampleRate, 32, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    int targetNote = 60 + GetRandomValue(0, 12);
    int userGuess = -1;
    bool showResult = false;
    std::string feedback = "Listen to the note...";

    // First play
    frequency = 440.0f * powf(2.0f, (targetNote - 69) / 12.0f);
    volume = 0.5f;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Input Handling
        int key = GetKeyPressed();
        int pressedMidi = GetMidiFromKey(key);

        if (pressedMidi != -1) {
            userGuess = pressedMidi;
            frequency = 440.0f * powf(2.0f, (userGuess - 69) / 12.0f);
            volume = 0.5f;

            if (userGuess == targetNote) {
                feedback = "CORRECT! Press SPACE for next.";
                showResult = true;
            }
            else {
                feedback = "WRONG! Try again.";
            }
        }

        if (IsKeyPressed(KEY_SPACE) && showResult) {
            targetNote = 60 + GetRandomValue(0, 12);
            frequency = 440.0f * powf(2.0f, (targetNote - 69) / 12.0f);
            volume = 0.5f;
            showResult = false;
            userGuess = -1;
            feedback = "Guess the note!";
        }

        // 2. Drawing
        BeginDrawing();
        ClearBackground(GetColor(0x181818FF));

        // Previewer (Top)
        DrawText("CURRENT NOTE", 320, 50, 20, GRAY);
        if (userGuess != -1) {
            DrawText(GetNoteName(userGuess).c_str(), 360, 80, 50, GOLD);
        }

        // Spectrogram Box (Center)
        DrawRectangleLines(100, 150, 600, 250, RAYWHITE);
        for (int i = 0; i < 598; i++) {
            float y = sinf((i * 0.1f) + (float)GetTime() * 10.0f) * (volume * 100.0f);
            DrawPixel(101 + i, 275 + (int)y, BLUE);
        }

        // Feedback Text
        DrawText(feedback.c_str(), 100, 420, 20, LIGHTGRAY);

        // Keyboard (Bottom)
        for (int i = 0; i < 12; i++) {
            int midi = 60 + i;
            bool isSharp = (midi % 12 == 1 || midi % 12 == 3 || midi % 12 == 6 || midi % 12 == 8 || midi % 12 == 10);
            Rectangle r = { 100.0f + (i * 50.0f), 480.0f, 45.0f, 100.0f };
            DrawRectangleRec(r, isSharp ? BLACK : RAYWHITE);
            if (userGuess == midi) DrawRectangleLinesEx(r, 3, RED);
        }

        EndDrawing();
    }

    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}