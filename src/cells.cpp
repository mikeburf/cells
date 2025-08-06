// some of this code is adapted from the SDL3 example renderer/points
// https://examples.libsdl.org/SDL3/renderer/04-points/

#define SDL_MAIN_USE_CALLBACKS 1


#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <algorithm>
#include "cells.h"

// the width and height of the simulation
constexpr int SIM_WIDTH = 480;
constexpr int SIM_HEIGHT = 270;

// the number of pixels per simulation cell
constexpr int RENDER_SCALE = 4;

// the width and height of the window in pixels
constexpr int WINDOW_WIDTH = SIM_WIDTH * RENDER_SCALE;
constexpr int WINDOW_HEIGHT = SIM_HEIGHT * RENDER_SCALE;

constexpr int MAX_STEPS_PER_SECOND = 20; //maximum number of simulation steps per second

//the window and renderer
static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;

static unsigned long long last_step_time = 0; //the timestamp of the last simulation step
static float steps_per_second = 0; //the current simulation speed in steps per second

static bool mouseDown; // if the left mouse button is down

// the x,y position of the mouse in window space
static float mouseX; 
static float mouseY;

// the last x,y position of the mouse in simulation space
static int lastMouseCellX;
static int lastMouseCellY;

// if the left mouse button was down last frame
static bool mouseWasDown = false;

// the current and next state of the simulation
static bool currentState[SIM_WIDTH][SIM_HEIGHT];
static bool nextState[SIM_WIDTH][SIM_HEIGHT];

// a buffer of points to render
static SDL_FPoint renderPoints[SIM_WIDTH * SIM_HEIGHT];

// the number of points in the render buffer that should be rendered.
static int renderPointCount = 0;

// whether the screen needs to be redrawn
static bool needs_new_render = true;

// runs on startup
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    SDL_SetAppMetadata("Game of Life", "1.0", NULL);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("cells", WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderScale(renderer, RENDER_SCALE, RENDER_SCALE);

    last_step_time = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

// runs on an input event
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

// runs every frame
SDL_AppResult SDL_AppIterate(void* appstate)
{
    // handles mouse cell painting
    PaintCells();

    // if the simulation isn't paused
    if (steps_per_second > 0) {

        // find the elapsed time since the last frame, and the number of seconds per simulation update
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

// handles painting for a frame
void PaintCells()
{
    // if the mouse is down and on the screen, paint live pixels
    if (mouseDown) {

        int mouseCellX = (int)(mouseX / RENDER_SCALE);
        int mouseCellY = (int)(mouseY / RENDER_SCALE);

        // interpolates between previous and current mouse position using Bresenham's algorithm
        if (mouseWasDown) {
            Paint_Line(lastMouseCellX, lastMouseCellY, mouseCellX, mouseCellY);
        }
        else Try_Paint_Point(mouseCellX, mouseCellY);

        lastMouseCellX = mouseCellX;
        lastMouseCellY = mouseCellY;
    }
    mouseWasDown = mouseDown;
}



// paints a straight line of live cells between two points
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

// tries to paint a single live cell at the given x,y position
// returns true if the point was inside the window, false otherwise
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

// updates the game of life simulation according to the standard rules
void Update_Simulation()
{
    int x, y; // current xy position

    // current and absolute position being checked for a neighbor, relative to x and y above
    // these are seperate variables because the simulation wraps around on both axes
	// e.g. -1 <= dx <= 1 always, but nx might wrap around to 0 if x = SIM_WIDTH - 1
    int dx, dy;
    int nx, ny;

    int neighbors; // neighbor count for the current cell

    Clear_Rendered_Points(); // clear all points from being rendered

    for (x = 0; x < SIM_WIDTH; x++) {
        for (y = 0; y < SIM_HEIGHT; y++) {

            neighbors = 0;
            for (dx = -1; dx <= 1; dx++) {

                // find the x coordinate of the adjacent cell
                nx = x + dx;
                if (nx < 0) nx = SIM_WIDTH - 1;
                if (nx == SIM_WIDTH) nx = 0;

                for (dy = -1; dy <= 1; dy++) {

					// don't check the current cell
                    if (dx == 0 && dy == 0) continue;

					// find the y coordinate of the adjacent cell
                    ny = y + dy;
                    if (ny < 0) ny = SIM_HEIGHT - 1;
                    if (ny == SIM_HEIGHT) ny = 0;

					// if the adjacent cell is alive, increment the neighbor count
                    if (currentState[nx][ny]) neighbors++;

					// no need to check for more neighbors if we already have 4
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
            should be explicitly set each simulation step, even if it isnt changing.
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
    // swaps the two arrays, so currentState will point to this step's output
    // and nextState will point to the old state (and should be totally overwritten next sim step)
    std::swap(currentState, nextState);
}

// sets the number of points that will be passed to the renderer to 0
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

// i think this is necessary to leave here?
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{

}
