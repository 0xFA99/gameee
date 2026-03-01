// Standard include
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "mmapalloc.h"

#define GAMEEE "gameee"
#define GAMEEE_DEFAULT_FPS 60
#define GAMEEE_MAX_CONTEXT_SWITCH 3

// Contoh kasar

enum GameeeContextFlags { MainWindows, MenuWindows, None };

struct GameeeContextMetadata {
  bool windowsLifetime;
};

struct GameeeContextAssets
{
  Vector2 vec;
};

struct GameeeContextSwitching {
  enum GameeeContextFlags (*switchWindows[GAMEEE_MAX_CONTEXT_SWITCH])(
      struct GameeeContextMetadata *,struct GameeeContextAssets *);
};

// Main State

enum GameeeContextFlags MainWin(struct GameeeContextMetadata *metadata,struct GameeeContextAssets *shared) {
  Vector2 *vec = &shared[MainWindows].vec;
  // event handler
  if (IsKeyPressed(KEY_A)) {
    return MenuWindows;
  }
  if (IsKeyDown(KEY_SPACE))
  {
    vec->y += 10;
  }
  ClearBackground(GREEN);
  DrawText("INFO : hello from main windows", 100, 100, 20, WHITE);
  DrawRectangle(100,100,(int)vec->x,(int)vec->y,BLACK);
  return MainWindows;
}

// Menu State

enum GameeeContextFlags MenuWin(struct GameeeContextMetadata *metadata,struct GameeeContextAssets *shared) {
  Vector2 *vec = &shared[MenuWindows].vec;
  if (IsKeyPressed(KEY_A)) {
    return MainWindows;
  }
  if (IsKeyDown(KEY_SPACE))
  {
    vec->y += 10;
  }
  ClearBackground(BLUE);
  DrawText("INFO : hello from menu windows", 100, 200, 20, WHITE);
  DrawRectangle(100,100,(int)vec->x,(int)vec->y,BLACK);
  return None;
}

int main(void) {
  const int screenWidth = 1920, screenHeight = 1200, minScreenWidth = 810,
            minScreenHeight = 600;

  char *memory = (char *)mmapalloc(1024);
  printf("INFO : initialize first arena\n");
  char *second_memory = (char *)mmapalloc(32);
  printf("PTR : %p\n",second_memory);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | // enable resizable window
                 FLAG_VSYNC_HINT |       // enable vsync
                 FLAG_MSAA_4X_HINT);     // enable anti-alias

  InitWindow(screenWidth, screenHeight, GAMEEE);
  SetWindowMinSize(minScreenWidth, minScreenHeight);

  SetTargetFPS(GAMEEE_DEFAULT_FPS);

  struct GameeeContextMetadata mdata; // Metadata shared
  struct GameeeContextAssets *context_assets = mmapalloc(sizeof(struct GameeeContextAssets) * GAMEEE_MAX_CONTEXT_SWITCH);
  if (!context_assets)
  {
    fprintf(stderr,GAMEEE "failed to allocate struct\n");
    return EXIT_FAILURE;
  }
  mdata.windowsLifetime = true;

  context_assets[MainWindows].vec = (Vector2){10,20};
  context_assets[MenuWindows].vec = (Vector2){100,100};

  struct GameeeContextSwitching switchCtx;
  /*
   * fungsi struct switch context,menyimpan function to ptr
   */

  switchCtx.switchWindows[MainWindows] = MainWin;
  switchCtx.switchWindows[MenuWindows] = MenuWin;

  enum GameeeContextFlags flagsCtx = MainWindows;

  while (!WindowShouldClose()) {
    BeginDrawing();
    switch (switchCtx.switchWindows[flagsCtx](&mdata,context_assets)) {
    case MainWindows:
      flagsCtx = MainWindows;
      break;
    case MenuWindows:
      flagsCtx = MenuWindows;
      break;
    case None:
      // fprintf(stderr,"ERROR : failed to change state,state not found\n");
      break;
    default:
      break;
    }
    EndDrawing();
  }

  CloseWindow();
  fprintf(stdout,"INFO : Block active %d\n",mmapalloc_destroy());
  return 0;
}
