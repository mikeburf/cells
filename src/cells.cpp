/* points.c ... */

/*
 * This example creates an SDL window and renderer, and then draws some points
 * to it every frame.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */


#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <algorithm>
#include "cells.h"

 /* We will use this renderer to draw into this window every frame. */
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

#define MAX_STEPS_PER_SECOND 20
static Uint64 last_step_time = 0;
static float steps_per_second = 0;

static bool mouseDown;
static float mouseX;
static float mouseY;

static int lastMouseCellX;
static int lastMouseCellY;
static bool wasPainting = false;


#define SIM_WIDTH 480
#define SIM_HEIGHT 270

#define RENDER_SCALE 4

constexpr int WINDOW_WIDTH = SIM_WIDTH * RENDER_SCALE;
constexpr int WINDOW_HEIGHT = SIM_HEIGHT * RENDER_SCALE;

static bool currentState[SIM_WIDTH][SIM_HEIGHT];
static bool nextState[SIM_WIDTH][SIM_HEIGHT];

static SDL_FPoint renderPoints[SIM_WIDTH * SIM_HEIGHT];
static int renderPointCount = 0;
static bool needs_new_render = true;
//static float point_speeds[NUM_POINTS];

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    SDL_SetAppMetadata("Example Renderer Points", "1.0", "com.example.renderer-points");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("examples/renderer/points", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderScale(renderer, RENDER_SCALE, RENDER_SCALE);

    last_step_time = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    // the mouse wheel changes the simulation speed
    else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        float delta = event->wheel.y;
        steps_per_second = steps_per_second + delta;
        if (steps_per_second < 0) steps_per_second = 0;
        if (steps_per_second > MAX_STEPS_PER_SECOND) steps_per_second = MAX_STEPS_PER_SECOND;
    }

    // clicking the left mouse button begins painting
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event->button.button == 1) {

            mouseDown = true;

            mouseX = event->button.x;
            mouseY = event->button.y;
        }
    }

    // this updates the simulation-space mouse coordinates while painting
    else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        if (mouseDown) {

            mouseX = event->button.x;
            mouseY = event->button.y;
        }
    }

    // releasing the left mouse button stops painting
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event->button.button == 1) {
            mouseDown = false;
        }
    }

    return SDL_APP_CONTINUE;
}

void Paint_Line(int x0, int y0, int x1, int y1) {

    const int dx = std::abs(x1 - x0);
    const int dy = std::abs(y1 - y0);

    if (dy <= dx) {
        if (x0 <= x1) Paint_Line_Shallow(x0, y0, x1, y1, dx, dy);
        else Paint_Line_Shallow(x1, y1, x0, y0, dx, dy);
    }
    else {
        if (y0 <= y1) Paint_Line_Steep(x0, y0, x1, y1, dx, dy);
        else Paint_Line_Steep(x1, y1, x0, y0, dx, dy);
    }
}

bool Try_Paint_Point(int x, int y) {
    if (x >= 0 && y >= 0 && x < SIM_WIDTH && y < SIM_HEIGHT) {
        if (currentState[x][y] == false) {
            currentState[x][y] = true;
            Add_Rendered_Point(x, y);
        }
        return true;
    }
    else return false;
}

// Uses the Bresenham algorithm to draw a rasterized line of live cells on the simulation
// works for all lines with a slope between -1 and 1, inclusive
// assumes x0 < x1; dx = abs(x1 - x0); and dy = abs(y1 - y0)
void Paint_Line_Shallow(int x0, int y0, int x1, int y1, int dx, int dy) {
    const int step = (y1 < y0 ? -1 : 1);
    int error = 2 * dy - dx;

    int y = y0;

    for (int x = x0; x <= x1; x++) {

        Try_Paint_Point(x, y);

        if (error > 0) {
            y += step;
            error -= 2 * dx;
        }
        error += 2 * dy;
    }
}

// Uses the Bresenham algorithm to draw a rasterized line of live cells on the simulation
// works for all lines with a slope outside -1 to 1
// assumes y0 < y1; dx = abs(x1 - x0); and dy = abs(y1 - y0)
void Paint_Line_Steep(int x0, int y0, int x1, int y1, int dx, int dy) {
    const int step = (x1 < x0 ? -1 : 1);
    int error = 2 * dx - dy;

    int x = x0;

    for (int y = y0; y <= y1; y++) {

        Try_Paint_Point(x, y);

        if (error > 0) {
            x += step;
            error -= 2 * dy;
        }
        error += 2 * dx;
    }
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    PaintCells();


    // if the simulation isn't paused
    if (steps_per_second > 0) {

        // find the elapsed time since the last frame and the number of seconds per simulation update
        const Uint64 now = SDL_GetTicks();
        const float elapsed = ((float)(now - last_step_time)) / 1000.0f;
        const float seconds_per_step = 1 / steps_per_second;

        // if the elapsed time is greater than the seconds per step, update the simulation
        if (elapsed >= seconds_per_step) {

            Update_Simulation();
            last_step_time = now;
        }
    }

    // if any points were drawn, rerender the screen
    if (needs_new_render) {

        // first make everything black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // then render all the points white
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
        SDL_RenderPoints(renderer, renderPoints, renderPointCount);

        // update the screen
        SDL_RenderPresent(renderer);

        needs_new_render = false;
    }

    return SDL_APP_CONTINUE;
}

void PaintCells()
{
    // if the mouse is down and on the screen, paint live pixels
    if (mouseDown) {

        int mouseCellX = (int)(mouseX / RENDER_SCALE);
        int mouseCellY = (int)(mouseY / RENDER_SCALE);

        // tries to interpolate between frames using Bresenham's algorithm
        if (wasPainting) {
            Paint_Line(lastMouseCellX, lastMouseCellY, mouseCellX, mouseCellY);
        }
        else Try_Paint_Point(mouseCellX, mouseCellY);

        lastMouseCellX = mouseCellX;
        lastMouseCellY = mouseCellY;
    }
    wasPainting = mouseDown;
}

// stop any points from being rendered
// this happens by setting the number of points that will be passed to the renderer to 0
void Clear_Rendered_Points() {
    renderPointCount = 0;
}

//sets the next point in the render buffer to the given xy value 
// and increments the render output count by 1
void Add_Rendered_Point(int x, int y) {
    renderPoints[renderPointCount].x = (float)x;
    renderPoints[renderPointCount].y = (float)y;
    renderPointCount++;

    needs_new_render = true;
}

// updates the game of life simulation according to the standard rules
void Update_Simulation()
{
    int x, y; // current xy position

    // current and absoluate position being checked relative to x and y above
    // these are seperated because the simulation wraps around on both axes
    // so dx might be -1 or +1 but nx will be on the other side of the simulation from x
    int dx, dy;
    int nx, ny;

    int neighbors; // neighbor count

    Clear_Rendered_Points(); // clear all points from being rendered

    for (x = 0; x < SIM_WIDTH; x++) {
        for (y = 0; y < SIM_HEIGHT; y++) {

            neighbors = 0;
            for (dx = -1; dx <= 1; dx++) {

                nx = x + dx;
                if (nx < 0) nx = SIM_WIDTH - 1;
                if (nx == SIM_WIDTH) nx = 0;

                for (dy = -1; dy <= 1; dy++) {

                    if (dx == 0 && dy == 0) continue;
                    ny = y + dy;
                    if (ny < 0) ny = SIM_HEIGHT - 1;
                    if (ny == SIM_HEIGHT) ny = 0;

                    if (currentState[nx][ny]) neighbors++;
                    if (neighbors > 3) break;
                }
            }

            /*
            *
            Game of life rules:
                1. dead squares with exactly 3 neighbors come alive
                2. living squares with 0, 1, or 4+ neighbors die
                3. all other squares remain the same

            This implementation uses two 2D arrays, one for reading the current state
            and one for writing the next state, and then swaps them once the step is complete.
            To avoid weird persistent cells from previous states, every coordinate in the output array
            should be set to a value each simulation step, even if it isnt changing.
            */
            if (currentState[x][y]) {
                if (neighbors < 2 || neighbors > 3) {
                    nextState[x][y] = false;
                }
                else {
                    nextState[x][y] = true;
                    Add_Rendered_Point(x, y);
                }
            }
            else {
                if (neighbors == 3) {
                    nextState[x][y] = true;
                    Add_Rendered_Point(x, y);
                }
                else nextState[x][y] = false;
            }
        }
    }
    // swaps the two arrays, so currentState will point to this steps output
    // and nextState will point to the old state (and should be totally overwritten next sim step)
    std::swap(currentState, nextState);
}

// i think this is necessary to leave here?
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{

}
