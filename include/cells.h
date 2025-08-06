// cells.h : Include file for standard system include files,
// or project specific include files.

#pragma once

// TODO: Reference additional headers your program requires here.




void Add_Rendered_Point(int, int);
void Clear_Rendered_Points();

void Update_Simulation();

bool Try_Paint_Point(int, int);

void PaintCells();

void Paint_Line(int, int, int, int);
void Paint_Line_Steep(int, int, int, int, int, int);
void Paint_Line_Shallow(int, int, int, int, int, int);
