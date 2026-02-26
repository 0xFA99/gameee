// Standard include
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "raylib.h"

#define GAMEEE_DEFAULT_FPS 60
#define GAMEEE_MAX_CONTEXT_SWITCH 3

// Contoh kasar

enum GameeeContextFlags
{
    MainWindows,MenuWindows,None
};

struct GameeeContextMetadata
{
    bool windowsLifetime;
};

struct GameeeContextSwitching
{
    enum GameeeContextFlags(*switchWindows[GAMEEE_MAX_CONTEXT_SWITCH])(struct GameeeContextMetadata *);
};

// Main State

enum GameeeContextFlags MainWin(struct GameeeContextMetadata *metadata)
{
    // event handler
    if (IsKeyPressed(KEY_A))
    {
        return MenuWindows;
    }
    ClearBackground(GREEN);
    DrawText("INFO : hello from main windows",100,100,20,WHITE);
    return MainWindows;
}

// Menu State

enum GameeeContextFlags MenuWin(struct GameeeContextMetadata *metadata)
{
    if (IsKeyPressed(KEY_A))
    {
        return MainWindows;
    }
    ClearBackground(BLUE);
    DrawText("INFO : hello from menu windows",100,200,20,WHITE);
    return None;
}

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

    SetTargetFPS(GAMEEE_DEFAULT_FPS);

    struct GameeeContextMetadata mdata; // Metadata shared
    mdata.windowsLifetime = true;

    struct GameeeContextSwitching switchCtx;
    /*
     * fungsi struct switch context,menyimpan function to ptr
     */

    switchCtx.switchWindows[MainWindows] = MainWin;
    switchCtx.switchWindows[MenuWindows] = MenuWin;

    enum GameeeContextFlags flagsCtx = MainWindows;

    while (!WindowShouldClose())
    {
        BeginDrawing();
        switch (switchCtx.switchWindows[flagsCtx](&mdata))
        {
            case MainWindows:
                flagsCtx = MainWindows;
                break;
            case MenuWindows:
                flagsCtx = MenuWindows;
                break;
            case None:
                //fprintf(stderr,"ERROR : failed to change state,state not found\n");
                break;
            default:
                break;
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
