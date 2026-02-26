#include "raylib.h"

int main(void)
{
    const int screenWidth = 1920,
          screenHeight = 1200,
          minScreenWidth = 810,
          minScreenHeight = 600;

    SetConfigFlags(
            FLAG_WINDOW_RESIZABLE | // enable resizable window
            FLAG_VSYNC_HINT |       // enable vsync
            FLAG_MSAA_4X_HINT);     // enable anti-alias

    InitWindow(screenWidth, screenHeight, "GAME TITLE");

    SetWindowMinSize(minScreenWidth, minScreenHeight);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(100, 250, 500, 500, LIME);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
