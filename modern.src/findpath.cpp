/*
**	Command & Conquer(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header:   F:\projects\c&c\vcs\code\findpath.cpv   2.17   16 Oct 1995 16:51:04   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : FINDPATH.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : September 10, 1993                                           *
 *                                                                                             *
 *                  Last Update : May 25, 1995   [PWG]                                         *
 *                                                                                             *
 * The path algorithm works by following a LOS path to the target. If it                       *
 * collides with an impassable spot, it uses an Edge following routine to                      *
 * get around it. The edge follower moves along the edge in a clockwise or                     *
 * counter clockwise fashion until finding the destination spot. The                           *
 * destination is determined by Find_Path. It is the first passable that                       *
 * can be reached (so it will handle the doughnut case, where there is                         *
 * a passable in the center of an unreachable area).                                           *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Clear_Path_Overlap -- clears the path overlap list                                        *
 *   Find_Path -- Find a path from point a to point b.                                         *
 *   Find_Path_Cell -- Finds a given cell on a specified path                                  *
 *   Follow_Edge -- Follow an edge to get around an impassable spot.                           *
 *   FootClass::Unravel_Loop -- Unravels a loop in the movement path                           *
 *   Get_New_XY -- Get the new x,y based on current position and direction.                    *
 *   Optimize_Moves -- Optimize the move list.                                                 *
 *   Set_Path_Overlap -- Sets the overlap bit for given cell                                   *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"
//#include	<string.h>

/*
**	When an edge search is started, it can be performed CLOCKwise or
**	COUNTERCLOCKwise direction.
*/
#define	CLOCK				(FacingType)1	// Clockwise.
#define	COUNTERCLOCK	(FacingType)-1	// Counterclockwise.

/*
**	If defined, diagonal moves are allowed, else no diagonals.
*/
#define	DIAGONAL

/*
**	This is the marker to signify the end of the path list.
*/
#define	END			FACING_NONE

/*
**	If memory is more important than speed, set this define to
**	true. It will then perform intermediate optimizations to get the most
**	milage out of a limited movement list staging area. If this value
**	is true then it figures paths a bit more intelligently.
*/
#define	SAVEMEM		true

/*
**	Modify this macro so that given two cell values, it will return
**	a value between 0 and 7, with 0 being North and moving
**	clockwise (just like map degrees).
*/
#define	CELL_FACING(a,b)		Dir_Facing(::Direction((a),(b)))


/*-------------------------------------------------------------------------*/
/*
**	Cells values are really indexes into the 'map'. The following value is
**	the X width of the map.
*/
#define	MODULO	MAP_CELL_W

/*
**	Maximum lookahead cells. Twice this value in bytes will be
**	reserved on the stack. The smaller this number, the faster the processing.
*/
#define MAX_MLIST_SIZE		300
#define THREAT_THRESHOLD	5

#ifdef NEVER
typedef enum {
	FACING_N,			// North
	FACING_NE,			// North-East
	FACING_E,			// East
	FACING_SE,			// South-East
	FACING_S,			// South
	FACING_SW,			// South-West
	FACING_W,			// West
	FACING_NW,			// North-West

	FACING_COUNT			// Total of 8 directions (0..7).
} FacingType;
#endif


/*-------------------------------------------------------------------------*/
static bool DrawPath;

inline FacingType Opposite(FacingType face)
{
	return( (FacingType) (face ^ 4));
}

static inline void Draw_Cell_Point(CELL cell, bool passable, int threat_stage, int overide = 0)
{
	if (DrawPath) {
		if (!Debug_Find_Path) {
			int x, y;

			if (Map.Coord_To_Pixel(Cell_Coord(cell), x, y)) {
				if (threat_stage>2) {
					SeenBuff.Put_Pixel(x, y, (passable) ? LTGREEN : RED);
				} else {
					SeenBuff.Put_Pixel(x, y, (passable) ? 9+threat_stage : RED);
				}
			}
		} else {
			int x = cell & 63;
			int y = cell / 64;
			if (!overide) {
				SeenBuff.Put_Pixel(64 + (x * 3) + 1, 8 + (y * 3) + 1, (passable) ? WHITE : BLACK);
			} else {
				SeenBuff.Put_Pixel(64 + (x * 3) + 1, 8 + (y * 3) + 1, overide);
			}
		}
	}
}

inline static FacingType Next_Direction(FacingType facing, FacingType dir)
{
	facing = facing + dir;
	#ifndef DIAGONAL
		facing = (FacingType)(facing & 0x06);
	#endif
	return(facing);
}

/*=========================================================================*/
/* Define a couple of variables which are private to the module they are   */
/*      declared in.                                                       */
/*=========================================================================*/
static uint32_t MainOverlap[MAP_CELL_TOTAL/32];		// kept for PathType.Overlap field compatibility

static bool Infantry_Path_Can_Share_Cell(FootClass const *foot, CELL cell)
{
	if (!foot || foot->What_Am_I() != RTTI_INFANTRY) {
		return(false);
	}

	if ((unsigned)cell >= MAP_CELL_TOTAL) {
		return(false);
	}

	CellClass const &cellptr = Map[cell];

	if (cellptr.InfType == HOUSE_NONE || !foot->House->Is_Ally(cellptr.InfType)) {
		return(false);
	}

	if ((cellptr.Flag.Composite & 0x1F) != 0x1F) {
		return(false);
	}

	if (cellptr.Flag.Occupy.Vehicle || cellptr.Flag.Occupy.Monolith) {
		return(false);
	}

	if (!Ground[cellptr.Land_Type()].Cost[SPEED_FOOT]) {
		return(false);
	}

	return(true);
}


//static CELL MoveMask = 0;
static CELL DestLocation;
static CELL StartLocation;

/*
**	Legacy edge-following code — replaced by A* in Find_Path.
**	Kept as reference; not compiled.
*/
#ifdef LEGACY_EDGE_FOLLOW
/***************************************************************************
 * FootClass::Unravel_Loop -- Unravels a loop in the movement path         *
 *                                                                         *
 * While in the midst of the Follow Edge logic, it is possible (due to the *
 * fact that we support diagonal movement) to begin looping around a       *
 * column of some type.  The Unravel loop function will scan backward      *
 * through the list and fixup the path to try to prevent the loop.         *
 *                                                                         *
 * INPUT:      path   -   pointer to the generated path so we can pull the *
 *                         commands out of it.                             *
 *               cell   -   the cell we tried to enter that generated the  *
 *                        double overlap condition.                        *
 *               dir    -   the direction we tried to enter from when we   *
 *                        generated the double overlap condition           *
 *               startx -   the start x position of this path segment      *
 *               starty - the start y position of this path segment        *
 *               destx    - the dest x position for this path segment      *
 *               desty    - the dest y position for this path segment      *
 *                                                                         *
 * OUTPUT:      TRUE    - loop has been sucessfully unravelled             *
 *               FALSE  - loop can not be unravelled so abort follow edge  *
 *                                                                         *
 * WARNINGS:   none                                                        *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/25/1995 PWG : Created.                                             *
 *=========================================================================*/
bool FootClass::Unravel_Loop(PathType *path, CELL &cell, FacingType &dir, int sx, int sy, int dx, int dy, MoveType threshhold)
{
	/*
	** Walk back to the actual cell before we advanced our position
	*/
	FacingType	curr_dir	= dir;
	CELL			curr_pos = Adjacent_Cell(cell, Opposite(curr_dir));
	int			idx		= path->Length;			// start at the last position
	FacingType	*list		= &path->Command[idx-1];	// point to the last command
	int			checkx;
	int			checky;
	int			last_was_line	= false;

	/*
	** loop backward through the list searching for a point that is
	** on the line.  If the point was a diagonal move then adjust
	** it.
	*/
	while (idx) {
		checkx		= Cell_X(curr_pos);
		checky		= Cell_Y(curr_pos);

		if (!Point_Relative_To_Line(checkx, checky, sx, sy, dx, dy) || last_was_line) {

			/*
			** We have now found a point on the line.  Now we must check to see
			** if we left the line on a diagonal.  If we did then we need to fix
			** it up.
			*/
			if (curr_dir & 1 && curr_pos != path->LastFixup) {
				cell 				 = curr_pos;
				dir  				 = *(list-1);
				path->Length	 = idx;
				path->LastFixup = curr_pos;
				Draw_Cell_Point(curr_pos, true, -1, CYAN);
				return(true);
			}

			last_was_line = !last_was_line;
		}

		/*
		** Since this cell will not be in the list, then pull out its cost
		*/
		path->Cost -= Passable_Cell(curr_pos, *list, -1, threshhold);

		/*
		** Remove this cells flag from the overlap list for the path
		*/
		path->Overlap[curr_pos >> 5] &= ~(1 << (curr_pos & 31));

		/*
		** Mark cell on the map
		*/
		Draw_Cell_Point(curr_pos, true, -1, LTCYAN);

		/*
		** Adjust to the next list position and direction.
		*/
		curr_dir = *list--;
		curr_pos	= Adjacent_Cell(curr_pos, Opposite(curr_dir));
		idx--;
	}

	/*
	** If we can't modify the list to eliminate the problem, then we have
	** a larger problem in that we have deleted all of the cells in the
	** list.
	*/
	return(false);
}


/***************************************************************************
 * Register_Cell -- registers a cell on our path and check for backtrack   *
 *                                                                         *
 * This function adds a new cell to our path.  If the cell has already     *
 * been recorded as part of our path, then this function moves back down   *
 * the list truncating it at the point we registered that cell.  This      *
 * function will elliminate all backtracking from the list.                *
 *                                                                         *
 * INPUT:      long   * list - the list to set the overlap bit for         *
 *               CELL  cell    - the cell to mark on the overlap list      *
 *                                                                         *
 * OUTPUT:     BOOL - TRUE if bit has been set, FALSE if bit already set   *
 *                                                                         *
 * HISTORY:                                                                *
 *   05/23/1995 PWG : Created.                                             *
 *=========================================================================*/
bool FootClass::Register_Cell(PathType *path, CELL cell, FacingType dir, int cost, MoveType threshhold)
{
	FacingType  *list;
	int 	pos  = cell >> 5;
	int	bit  = (cell & 31); /* LP64: was (cell & 31) - 1, but shift by -1 is UB */

	/*
	** See if this point has already been registered as on the list.  If so
	** we need to truncate the list back to this point and register the
	** new direction.
	*/
	if (path->Overlap[pos] & (1 << bit)) {
		/*
		** If this is not a case of immediate back tracking then handle
		** by searching the list to see what we find.  However is this is
		** an immediate back track, then pop of the last direction
		** and unflag the cell we are in (not the cell we are moving to).
		** Note: That we do not check for a zero length cell because we
		** could not have a duplicate unless there are cells in the list.
		*/

		if (path->Command[path->Length - 1] == Opposite(dir)) {
			CELL pos = Adjacent_Cell(cell, Opposite(dir));
			path->Overlap[pos >> 5] &= ~(1 << (pos & 31));
			path->Length--;
			Draw_Cell_Point(pos, true, -1, BLUE);
		} else {
			/*
			** If this overlap is in the same place as we had our last overlap
			** then we are in a loop condition.  We need to signify that we
			** cannot register this cell.
			*/
			if (path->LastOverlap == cell) {
				return(false);
			} else {
				path->LastOverlap = cell;
			}

			CELL pos 	  	= path->Start;
			int newlen		= 0;
			int idx 		   = 0;
			list		      = path->Command;

			/*
			** Note that the cell has to be in this list, so theres no sense
			** in checking whether we found it (famous last words).
			**
			** PWG 8/16/95 - However there is no sense searching the list if
			**               the cell we have overlapped on is the cell we
			**               started in.
			*/

			if (pos != cell) {
				while (idx < path->Length) {
					pos = Adjacent_Cell(pos, *list);
			  		if (pos == cell) {
						idx++;
						list++;
						break;
					}
					idx++;
					list++;
				}
				newlen = idx;
			}

			/*
			** Now we are pointing at the next command in the list.  From here on
			** out we need to unmark the fact that we have entered these cells and
			** adjust the cost of our path to reflect that we have not entered
			** then.
			*/
			while (idx < path->Length) {
				pos			= Adjacent_Cell(pos, *list);
				path->Cost -= Passable_Cell(pos, *list, -1, threshhold);
				path->Overlap[pos >> 5] &= ~(1 << (pos & 31));
				Draw_Cell_Point(pos, true, -1, LTBLUE);
				idx++;
				list++;
			}
			path->Length = newlen;
		}
	} else {
		/*
		** Now we need to register the new direction, updating the cell structure
		** and the cost.
		*/
		int cpos 				= path->Length++;
		path->Command[cpos]	= dir;			// save of the direction we moved
		path->Cost 			  += cost;			// figure new cost for cell
		path->Overlap[pos]  |= (1 << bit);	// mark the we have entered point
	}
	return(true);
}
#ifdef OBSOLETE
bool FootClass::Register_Cell(PathType *path, CELL cell, FacingType dir, int cost, MoveType threshhold)
{
	FacingType  *list;
	int 	pos  = cell >> 5;
	int	bit  = (cell & 31); /* LP64: was (cell & 31) - 1, but shift by -1 is UB */
	int	idx;

	/* LP64 safety: bounds check */
	if ((unsigned)cell >= MAP_CELL_TOTAL || pos >= (MAP_CELL_TOTAL/32)) return false;

	/*
	** See if this point has already been registered as on the list.  If so
	** we need to truncate the list back to this point and register the
	** new direction.
	*/
	if (path->Overlap[pos] & (1 << bit)) {
		/*
		** If this is not a case of immediate back tracking then handle
		** by searching the list to see what we find.  However is this is
		** an immediate back track, then pop of the last direction
		** and unflag the cell we are in (not the cell we are moving to).
		** Note: That we do not check for a zero length cell because we
		** could not have a duplicate unless there are cells in the list.
		*/

		if (path->Command[path->Length - 1] == Opposite(dir)) {
			CELL pos = Adjacent_Cell(cell, Opposite(dir));
			path->Overlap[pos >> 5] &= ~(1 << (pos & 31));
			path->Length--;
			Draw_Cell_Point(pos, true, -1, BLUE);
		} else {
			/*
			** If this overlap is in the same place as we had our last overlap
			** then we are in a loop condition.  We need to signify that we
			** cannot register this cell.
			*/
			if (path->LastOverlap == cell) {
				return(false);
			} else {
				path->LastOverlap = cell;
			}

			CELL pos 	  	= path->Start;
			int newlen		= 0;

			/*
			** Note that the cell has to be in this list, so theres no sense
			** in checking whether we found it (famous last words)
			*/
			for (idx = 0, list = path->Command; idx < path->Length; idx++, list++) {
				pos = Adjacent_Cell(pos, *list);
			  	if (pos == cell) {
					idx++;
					list++;
					break;
				}
			}
			newlen = idx;

			/*
			** Now we are pointing at the next command in the list.  From here on
			** out we need to unmark the fact that we have entered these cells and
			** adjust the cost of our path to reflect that we have not entered
			** then.
			*/
			while (idx < path->Length) {
				pos			= Adjacent_Cell(pos, *list);
				path->Cost -= Passable_Cell(pos, *list, -1, threshhold);
				path->Overlap[pos >> 5] &= ~(1 << (pos & 31));
				Draw_Cell_Point(pos, true, -1, LTBLUE);
				idx++;
				list++;
			}
			path->Length = newlen;
		}
	} else {
		/*
		** Now we need to register the new direction, updating the cell structure
		** and the cost.
		*/
		int cpos 				= path->Length++;
		path->Command[cpos]	= dir;			// save of the direction we moved
		path->Cost 			  += cost;			// figure new cost for cell
		path->Overlap[pos]  |= (1 << bit);	// mark the we have entered point
	}
	return(true);
}
#endif
#endif /* LEGACY_EDGE_FOLLOW */


/***********************************************************************************************
 * Find_Path -- Find a path from point a to point b.                                           *
 *                                                                                             *
 * INPUT:      int source x,y, int destination x,y, char *final moves                          *
 *             array to store moves, int maximum moves we may attempt                          *
 *                                                                                             *
 * OUTPUT:     int number of moves it took (IMPOSSIBLE_MOVES if we could                       *
 *             not reach the destination                                                       *
 *                                                                                             *
 * WARNINGS:   This algorithm assumes that the target is NOT situated                          *
 *             inside an impassable. If this case may arise, the do-while                      *
 *             statement inside the inner while (true) must be changed                         *
 *             to include a check to se if the next_x,y is equal to the                        *
 *             dest_x,y. If it is, then return(IMPOSSIBLE_MOVES).                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/08/1991  CY : Created.                                                                 *
 *=============================================================================================*/
PathType * FootClass::Find_Path(CELL dest, FacingType *final_moves, int maxlen, MoveType threshhold)
{
	CELL source = Coord_Cell(Coord);
	if ((unsigned)source >= MAP_CELL_TOTAL || (unsigned)dest >= MAP_CELL_TOTAL) return NULL;
	if (!final_moves) return(NULL);

	static PathType path;

	/*
	**	Set up debug drawing.
	*/
	if (!Debug_Find_Path) {
		DrawPath = IsSelected && Special.IsShowPath;
	} else {
		DrawPath = IsSelected;
	}
	Debug_Draw_Map("A* Search", source, dest, false);

	/*
	**	Threat handling for team AI.
	*/
	int unit_threat, threat;
	if (Team && Team->Class->IsRoundAbout) {
		unit_threat = (Team) ? Team->Risk : Risk();
		threat = 0;
	} else {
		unit_threat = threat = -1;
	}

	StartLocation = source;
	DestLocation = dest;

	/*
	**	Initialize path structure.
	*/
	path.Start       = source;
	path.Cost        = 0;
	path.Length       = 0;
	path.Command     = final_moves;
	path.Command[0]  = END;
	path.Overlap     = MainOverlap;
	path.LastOverlap = -1;
	path.LastFixup   = -1;

	if (source == dest) return(&path);

	/*
	**	A* data structures.  Static because only one pathfind runs at a time
	**	and these are too large for the stack.
	*/
	static int      g_score[MAP_CELL_TOTAL];
	static CELL     came_from[MAP_CELL_TOTAL];
	static uint32_t closed[MAP_CELL_TOTAL / 32];

	/*
	**	Binary min-heap for the open set.  With lazy deletion (duplicates
	**	allowed, stale entries skipped when popped), the heap can grow
	**	larger than MAP_CELL_TOTAL but is bounded in practice.
	*/
	struct HeapNode { int f; CELL cell; };
	static HeapNode heap[MAP_CELL_TOTAL * 2];
	int heap_size = 0;

	memset(g_score, 0x7F, sizeof(g_score));   // ~2 billion = infinity
	memset(closed, 0, sizeof(closed));

	#define ASTAR_CLOSED(c) (closed[(c) >> 5] & (1u << ((c) & 31)))
	#define ASTAR_CLOSE(c)  (closed[(c) >> 5] |= (1u << ((c) & 31)))

	/*
	**	Chebyshev distance heuristic (diagonal moves cost 1, same as cardinal).
	*/
	#define ASTAR_H(a, b) (MAX(abs(Cell_X(a) - Cell_X(b)), abs(Cell_Y(a) - Cell_Y(b))))

	/*
	**	Heap push (sift up).
	*/
	#define ASTAR_PUSH(fscore, c) do { \
		int _i = ++heap_size; \
		heap[_i].f = (fscore); heap[_i].cell = (c); \
		while (_i > 1 && heap[_i].f < heap[_i/2].f) { \
			HeapNode _t = heap[_i]; heap[_i] = heap[_i/2]; heap[_i/2] = _t; \
			_i /= 2; \
		} \
	} while(0)

	/*
	**	Start A*.
	*/
	g_score[source] = 0;
	ASTAR_PUSH(ASTAR_H(source, dest), source);

	bool found = false;
	int threat_stage = 0;

astar_retry:
	while (heap_size > 0) {
		/*
		**	Pop minimum f-score node (sift down).
		*/
		HeapNode cur_node = heap[1];
		heap[1] = heap[heap_size--];
		int i = 1;
		for (;;) {
			int sm = i;
			if (2*i   <= heap_size && heap[2*i].f   < heap[sm].f) sm = 2*i;
			if (2*i+1 <= heap_size && heap[2*i+1].f < heap[sm].f) sm = 2*i+1;
			if (sm == i) break;
			HeapNode t = heap[i]; heap[i] = heap[sm]; heap[sm] = t;
			i = sm;
		}

		CELL cur = cur_node.cell;

		/*
		**	Skip stale entries (cell already processed with a better g).
		*/
		if (ASTAR_CLOSED(cur)) continue;
		ASTAR_CLOSE(cur);

		Draw_Cell_Point(cur, true, threat_stage);

		if (cur == dest) {
			found = true;
			break;
		}

		/*
		**	Explore all 8 neighbors.
		*/
		for (FacingType face = FACING_N; face < FACING_COUNT; face++) {
			CELL nb = Adjacent_Cell(cur, face);

			if ((unsigned)nb >= MAP_CELL_TOTAL) continue;
			if (ASTAR_CLOSED(nb)) continue;

			/*
			**	Check if the neighbor is within the valid map area.
			*/
			if (!Map.In_Radar(nb) && nb != dest) continue;

			int cost = Passable_Cell(nb, face, threat, threshhold);
			if (!cost) {
				Draw_Cell_Point(nb, false, threat_stage);
				continue;
			}

			int tentative_g = g_score[cur] + cost;
			if (tentative_g < g_score[nb]) {
				g_score[nb]   = tentative_g;
				came_from[nb] = cur;
				int f = tentative_g + ASTAR_H(nb, dest);
				if (heap_size < (MAP_CELL_TOTAL * 2) - 1) {
					ASTAR_PUSH(f, nb);
				}
			}
		}
	}

	/*
	**	If not found and using threat-aware routing, escalate threat
	**	tolerance and retry.
	*/
	if (!found && threat != -1) {
		switch (threat_stage++) {
			case 0: threat = unit_threat >> 1; break;
			case 1: threat += unit_threat;     break;
			case 2: threat = -1;               break;
		}
		// Reset A* state for retry
		heap_size = 0;
		memset(g_score, 0x7F, sizeof(g_score));
		memset(closed, 0, sizeof(closed));
		g_score[source] = 0;
		ASTAR_PUSH(ASTAR_H(source, dest), source);
		goto astar_retry;
	}

	if (!found) {
		/*
		**	No path exists.  Find the closest cell we reached to the
		**	destination and path there instead (partial path).
		*/
		CELL best = source;
		int  best_dist = ASTAR_H(source, dest);
		for (int c = 0; c < MAP_CELL_TOTAL; c++) {
			if (g_score[c] < 0x7F000000) {
				int d = ASTAR_H((CELL)c, dest);
				if (d < best_dist) {
					best_dist = d;
					best = (CELL)c;
				}
			}
		}
		if (best == source) {
			return(NULL);
		}
		dest = best;   // Path to closest reachable cell
	}

	/*
	**	Reconstruct path: walk came_from[] from dest back to source.
	*/
	static CELL cell_path[MAP_CELL_TOTAL];
	int path_len = 0;
	{
		CELL c = dest;
		while (c != source && path_len < MAP_CELL_TOTAL) {
			cell_path[path_len++] = c;
			c = came_from[c];
		}
	}

	/*
	**	Reverse into forward FacingType direction list.
	**	Reserve one slot for the trailing END marker.
	*/
	int dir_count = 0;
	CELL cur = source;
	for (int i = path_len - 1; i >= 0 && dir_count < maxlen - 1; i--) {
		FacingType dir = CELL_FACING(cur, cell_path[i]);
		final_moves[dir_count++] = dir;
		cur = cell_path[i];
	}

	path.Length = dir_count;
	path.Cost  = g_score[dest];

	/*
	**	Poke in the stop command.
	*/
	if (path.Length < maxlen) {
		final_moves[path.Length++] = END;
	}

	#ifdef DIAGONAL
		Optimize_Moves(&path, threshhold);
	#endif

	if (Debug_Find_Path && DrawPath) {
		Debug_Draw_Map("A* Result", source, dest, false);
		Debug_Draw_Path(&path);
		Get_Key_Num();
	}

	#undef ASTAR_CLOSED
	#undef ASTAR_CLOSE
	#undef ASTAR_H
	#undef ASTAR_PUSH

	return(&path);
}


#ifdef LEGACY_EDGE_FOLLOW
/***********************************************************************************************
 * Follow_Edge -- Follow an edge to get around an impassable spot.                             *
 *                                                                                             *
 * INPUT:   start    -- cell to head from                                                      *
 *                                                                                             *
 *            target   -- Target cell to head to.                                              *
 *                                                                                             *
 *          path     -- Pointer to path list structure.                                        *
 *                                                                                             *
 *          search   -- Direction of search (1=clock, -1=counterclock).                        *
 *                                                                                             *
 *          olddir   -- Facing impassible direction from start.                                *
 *                                                                                             *
 *          callback -- Function pointer for determining if a cell is                          *
 *                      passable or not.                                                       *
 *                                                                                             *
 * OUTPUT:  bool: Could a path be found to the desired cell?                                   *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/08/1991  CY : Created.                                                                 *
 *   06/01/1992  JLB : Optimized & commented.                                                  *
 *=============================================================================================*/
bool FootClass::Follow_Edge(CELL start, CELL target, PathType *path, FacingType search, FacingType olddir, int threat, int threat_stage, int max_cells, MoveType threshhold)
{
	FacingType	newdir;			// Direction of facing before surrounding cell check.
	CELL			oldcell,		// Current cell.
					newcell;		// Tentative new cell.
	int			cost;				// Working cost value.
	int			startx;
	int			starty;
	int			online=true;
	int			targetx;
	int			targety;
	int			oldval = 0;
	int			cellcount=0;
	int			forceout = false;
	FacingType	firstdir = (FacingType)-1;
	CELL			firstcell = -1;
	bool			stepped_off_line = false;
	startx 	= Cell_X(start);
	starty	= Cell_Y(start);
	targetx  = Cell_X(target);
	targety	= Cell_Y(target);

	if (!path) return(false);
	path->LastOverlap = -1;
	path->LastFixup	= -1;

	#ifndef DIAGONAL
		/*
		**	The edge following algorithm doesn't "do" diagonals. Force initial facing
		**	to be an even 90 degree value. Adjust it in the direction it should be
		**	rotating.
		*/
		if (olddir & 0x01) {
			olddir = Next_Direction(olddir, search);
		}
	#endif

	newdir		= Next_Direction(olddir, search);
	oldcell 		= start;
	newcell 		= Adjacent_Cell(oldcell, newdir);

	/*
	**	Continue until we find our target, find our original starting spot,
	**	or run out of moves.
	*/
	while (path->Length < max_cells) {

		/*
		**	Look in all the adjacent cells to determine a passable one that
		**	most closely matches the desired direction (working in the specified
		**	direction).
		*/
		newdir = olddir;
		for (;;) {
			bool	forcefail;		// Is failure forced?

			forcefail = false;

			#ifdef DIAGONAL
				/*
				**	Rotate 45/90 degrees in desired direction.
				*/
				newdir = Next_Direction(newdir, search);

				/*
				**	If facing a diagonal we must check the next 90 degree location
				**	to make sure that we don't walk right by the destination. This
				**	will happen if the destination it is at the corner edge of an
				**	impassable that we are moving around.
				*/
				if (newdir & FACING_NE) {
					CELL	checkcell;		// Non-diagonal check cell.
					//int	x,y;

					checkcell = Adjacent_Cell(oldcell, Next_Direction(newdir, search));

					if (checkcell == target) {

						/*
						**	This only works if in fact, it is possible to move to the
						**	cell from the current location.
						*/
						cost = Passable_Cell(checkcell, Next_Direction(newdir, search), threat, threshhold);
						if (cost) {
							Draw_Cell_Point(checkcell, true, threat_stage);

							/*
							**	YES! The destination is at the corner of an impassable, so
							**	set the direction to point directly at it and then the
							**	scanning will terminate later.
							*/
							newdir = Next_Direction(newdir, search);
							newcell = Adjacent_Cell(oldcell, newdir);
							break;
						} else {
							Draw_Cell_Point(checkcell, false, threat_stage);
						}
					}

					/*
					**	Perform special diagonal check. If the edge follower would cross the
					**	diagonal or fall on the diagonal line from the source, then consider
					**	that cell impassible. Otherwise, the find path algorithm will fail
					**	when there are two impassible locations located on a diagonal
					**	that is lined up between the source and destination location.
					**
					** P.S. It might help if you check the right cell rather than using
					**      the value that just happened to be in checkcell.
					*/

					checkcell = Adjacent_Cell(oldcell, newdir);

					int checkx		= Cell_X(checkcell);
					int checky		= Cell_Y(checkcell);
					int checkval	= Point_Relative_To_Line(checkx, checky, startx, starty, targetx, targety);
					if (checkval && !online) {
						forcefail = ((checkval ^ oldval) < 0);
					} else {
			 			forcefail = false;
					}
					/*
					** The only exception to the above is when we are directly backtracking
					** because we could be trying to escape from a culdesack!
					*/
					if (forcefail && path->Length > 0 && (FacingType)(newdir ^ 4) == path->Command[path->Length - 1]) {
// ST - 12/18/96 5:15PM		if (forcefail && (FacingType)(newdir ^ 4) == path->Command[path->Length - 1]) {
						forcefail = false;
					}
				}

			#else
				newdir = Next_Direction(newdir, search*2);
			#endif

			/*
			**	If we have just checked the same heading we started with,
			**	we are surrounded by impassable characters and we exit.
			*/
			if (newdir == olddir) {
				return(false);
			}

			/*
			**	Get the new cell.
			*/
			newcell = Adjacent_Cell(oldcell, newdir);

			/*
			**	If we found a passable position, this is where we should move.
			*/
			if (!forcefail && ((cost = Passable_Cell(newcell, newdir, threat, threshhold)) != 0)) {
				Draw_Cell_Point(newcell, true, threat_stage);
				break;
			} else {
				Draw_Cell_Point(newcell, false, threat_stage, (forcefail) ? BROWN : 0);
				if (newcell == target) {
					forceout = true;
					break;
				}
			}

		}

		/*
		**	Record the direction.
		*/
		if (!forceout) {
			/*
			** Mark the cell because this is where we need to be.  If register
			** cell fails then the list has been shortened and we need to adjust
			** the new direction.
			*/
			if (!Register_Cell(path, newcell, newdir, cost, threshhold)) {
				/*
				** The only reason we could not register a cell is that we are in
				** a looping situation.  So we need to try and unravel the loop if
				** we can.
				*/
				if (!Unravel_Loop(path, newcell, newdir, startx, starty, targetx, targety, threshhold)) {
					return(false);
				}
				/*
				** Since we need to eliminate a diagonal we must pretend the upon
				** attaining this square, we were moving turned farther in the
				** search direction then we really were.
				*/
				newdir = Next_Direction(newdir, (FacingType)(search*2));
			}
			/*
			** Find out which side of the line this cell is on.  If it is on
			** a side, then store off that side.
			*/
			int newx	= Cell_X(newcell);
			int newy	= Cell_Y(newcell);
			int val	= Point_Relative_To_Line(newx, newy, startx, starty, targetx, targety);
			if (val) {
				oldval = val;
				online = false;
			} else {
				online = true;
			}
			cellcount++;
			if (cellcount==200) {
				return(false);
			}
		}

		/*
		**	If we have found the target spot, we are done.
		*/
		if (newcell == target) {
			path->Command[path->Length] = END;
			return(true);
		}

		/*
		**	If we make a full circle back to our original spot, get out.
		*/
		if (newcell == firstcell && newdir == firstdir) {
			return(false);
		}

		if (firstcell == -1) {
			firstcell = newcell;
			firstdir  = newdir;
		}

		/*
		**	Because we moved, our facing is now incorrect. We want to face toward
		**	the impassable edge we are following (well, not actually toward, but
		**	a little past so that we can turn corners). We have to turn 45/90 degrees
		**	more than expected in anticipation of the pending 45/90 degree turn at
		**	the start of this loop.
		*/
		#ifdef DIAGONAL
			olddir = Next_Direction(newdir, (FacingType)(-(int)search*3));
		#else
			olddir = Next_Direction(newdir, (FacingType)(-(int)search*4));
		#endif
		oldcell = newcell;
	}

	/*
	**	The maximum search path is exhausted... abort with a failure.
	*/
	return(false);
}
#endif /* LEGACY_EDGE_FOLLOW */


/***********************************************************************************************
 * Optimize_Moves -- Optimize the move list.                                                   *
 *                                                                                             *
 * INPUT:      char *moves to optimize                                                         *
 *                                                                                             *
 * OUTPUT:     none (list is optimized)                                                        *
 *                                                                                             *
 * WARNINGS:   EMPTY moves are used to hold the place of eliminated                            *
 *             commands. Also, NEVER call this routine with a list that                        *
 *             contains illegal commands. The list MUST be terminated                          *
 *             with a EOL command                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07/08/1991  CY : Created.                                                                 *
 *   06/01/1992  JLB : Optimized and commented.                                                *
 *=============================================================================================*/
#define	EMPTY		(FacingType)-2
int FootClass::Optimize_Moves(PathType *path, MoveType threshhold)
//int Optimize_Moves(PathType *path, int (*callback)(CELL, FacingType), int threshold)
{
	/*
	**	Facing command pair adjustment table. Compare the facing difference between
	**	the two commands. 0 means no optimization is possible. 3 means backtracking
	**	so eliminate both commands. Any other value adjusts the first command facing.
	*/
#ifdef DIAGONAL
	static FacingType _trans[FACING_COUNT] = {(FacingType)0, (FacingType)0, (FacingType)1, (FacingType)2, (FacingType)3, (FacingType)-2, (FacingType)-1, (FacingType)0};	// Smoothing.
#else
	static FacingType _trans[FACING_COUNT] = {(FacingType)0, (FacingType)0, (FacingType)0, (FacingType)2, (FacingType)3, (FacingType)-2, (FacingType)0, (FacingType)0};
#endif
	FacingType	*cmd1,		// Floating first command pointer.
					*cmd2,		// Floating second command pointer.
					newcmd;		// Calculated new optimized command.
	FacingType	newdir;		// Tentative new direction for smoothing.
	CELL			cell;			// Working cell (as it moves along path).

	/*
	**	Abort if there is any illegal parameter.
	*/
	if (!path || !path->Command) return(0);

	/*
	**	Optimization loop -- start scanning with the
	**	first pair of commands (if there are at least two
	**	in the command list).
	*/
	path->Command[path->Length] = END;		// Force end of list.
	cell = path->Start;
	if (path->Length > 1) {
		cmd2 = path->Command + 1;
		while (*cmd2 != END) {

			/*
			**	Set the cmd1 pointer to point to the valid command closest, but
			**	previous to cmd2. Be sure not to go previous to the head of the
			**	command list.
			*/
			cmd1 = cmd2-1;
			while (*cmd1 == EMPTY && cmd1 != path->Command) {
				cmd1--;
			}

			/*
			**	If there isn't any valid previous command, then bump the
			**	cmd pointers to the next command pair and continue...
			*/
			if (*cmd1 == EMPTY) {
				cmd2++;
				continue;
			}

			/*
			**	Fetch precalculated command change value. 0 means leave
			**	command set alone, 3 means backtrack and eliminate two
			**	commands. Any other value is new direction and eliminate
			**	one command.
			*/
			newcmd = (FacingType)(*cmd2 - *cmd1);
			if (newcmd < FACING_N) newcmd = (FacingType)(newcmd + FACING_COUNT);
			newcmd = _trans[newcmd];

			/*
			**	Check for backtracking. If this occurs, then eliminate the
			**	two commands. This is the easiest optimization.
			*/
			if (newcmd == FACING_SE) {
				*cmd1 = EMPTY;
				*cmd2++ = EMPTY;
				continue;
			}

			/*
			**	If an optimization code was found the process it. The command is a facing
			**	offset to more directly travel toward the immediate destination cell.
			*/
			if (newcmd) {

				/*
				**	Optimizations differ when dealing with diagonals. Especially when dealing
				**	with diagonals of 90 degrees. In such a case, 90 degree optimizations can
				**	only be optimized if the intervening cell is passable. The distance travelled
				**	is the same, but the path is less circuitous.
				*/
				if (*cmd1 & FACING_NE) {

					/*
					**	Diagonal optimizations are always only 45
					**	degree adjustments.
					*/
					newdir = Next_Direction(*cmd1, (newcmd < FACING_N) ? (FacingType)-1 : (FacingType)1);

					/*
					**	Diagonal 90 degree changes can be smoothed, although
					**	the path isn't any shorter.
					*/
					if (ABS((int)newcmd) == 1) {
						if (Passable_Cell(Adjacent_Cell(cell, newdir), newdir, -1, threshhold)) {
							*cmd2 = newdir;
							*cmd1 = newdir;
						}
						// BOB 16.12.92
						cell = Adjacent_Cell(cell, *cmd1);
						cmd2++;
						continue;
					}
				} else {
					newdir = Next_Direction(*cmd1, newcmd);
				}

				/*
				**	Allow shortening turn only on right angle moves that are based on
				**	90 degrees. Always allow 135 degree optimizations.
				*/
				*cmd2 = newdir;
				*cmd1 = EMPTY;

				/*
				**	Backup what it thinks is the current cell.
				*/
				while (*cmd1 == EMPTY && cmd1 != path->Command) {
					cmd1--;
				}
				if (*cmd1 != EMPTY) {
					cell = Adjacent_Cell(cell, Next_Direction(*cmd1, FACING_S));
				} else {
					cell = path->Start;
				}
				continue;
			}

			/*
			**	Since we could not make an optimization, we move our
			**	head pointer forward.
			*/
			cell = Adjacent_Cell(cell, *cmd1);
			cmd2++;
		}
	}

	/*
	**	Pack the command list to remove any EMPTY command entries.
	*/
	cmd1 = path->Command;
	cmd2 = path->Command;
	cell = path->Start;
	path->Cost = 0;
	path->Length = 0;
	while (*cmd2 != END) {
		if (*cmd2 != EMPTY) {

#ifdef NEVER
			if (Debug_ShowPath) {
				int	x,y,x1,y1;

				if (Map.Coord_To_Pixel(Cell_Coord(cell), x, y)) {
					Map.Coord_To_Pixel(Cell_Coord(Adjacent_Cell(cell, *cmd2)), x1, y1);
					Set_Logic_Page(SeenBuff);
					LogicPage->Draw_Line(x, y+8, x1, y1+8, DKGREY);
				}
			}
#endif

			cell = Adjacent_Cell(cell, *cmd2);
			path->Cost+= Passable_Cell(cell, *cmd2, -1, threshhold);
			path->Length++;
			*cmd1++ = *cmd2;
		}
		cmd2++;
	}
	path->Length++;
	*cmd1 = END;
	return(path->Length);
}


CELL FootClass::Safety_Point(CELL src, CELL dst, int start, int max)
{
	FacingType dir;
	CELL		  next;
	int 		  lp;

	dir = (FacingType)(CELL_FACING(src, dst) ^ 4) - 1;

	/*
	** Loop through the different acceptable distances.
	*/
	for (int dist = start; dist < max; dist ++) {

		/*
		** Move to the starting location.
		*/
		next = dst;

		for (lp = 0; lp < dist; lp ++) {
			next = Adjacent_Cell(next, dir);
		}

		if (dir & 1) {
			/*
			** If our direction is diagonal than we need to check
			** only one side which is as long as both of the old sides
			** together.
			*/
			for (lp = 0; lp < dist << 1; lp ++) {
				next = Adjacent_Cell(next, dir + 3);
				if (!Can_Enter_Cell(next)) {
					return(next);
				}
			}
		} else {
			/*
			** If our direction is not diagonal than we need to check two
			** sides so that we are checking a corner like location.
			*/
			for (lp = 0; lp < dist; lp ++) {
				next = Adjacent_Cell(next, dir + 2);
				if (!Can_Enter_Cell(next)) {
					return(next);
				}
			}

			for (lp = 0; lp < dist; lp ++) {
				next = Adjacent_Cell(next, dir + 4);
				if (!Can_Enter_Cell(next)) {
					return(next);
				}
			}
		}
	}
	return(-1);
}




int FootClass::Passable_Cell(CELL cell, FacingType face, int threat, MoveType threshhold)
{
	MoveType move = Can_Enter_Cell(cell, face);

	/*
	**	Infantry reserve sub-spots inside a cell, but A* only searches at cell granularity.
	**	Treat a fully occupied allied infantry cell as a moving blockage so infantry columns
	**	can still plan through shared cells and let the runtime sub-spot reservation arbitrate.
	*/
	if (move == MOVE_NO && Infantry_Path_Can_Share_Cell(this, cell)) {
		move = MOVE_MOVING_BLOCK;
	}

	if (move > threshhold) return(0);

	if (GameToPlay == GAME_NORMAL) {
		if (threat != -1) {
			if (Map.Cell_Distance(cell, DestLocation) > THREAT_THRESHOLD) {
				if (Map.Cell_Threat(cell, Owner()) > threat)
					return(0);
			}
		}
	}

	static int _value[MOVE_COUNT] = {
		1,			//	MOVE_OK
		1,			//	MOVE_CLOAK
		3,			//	MOVE_MOVING_BLOCK
		8,			//	MOVE_DESTROYABLE
		10,		//	MOVE_TEMP
		0			//	MOVE_NO
	};
	return(_value[move]);

#ifdef NEVER
	int can;
	int retval;

	int temp_move_mask = MoveMask;

	if (!House->IsHuman) {
		temp_move_mask &= ~MOVEF_TEMP;
	}

#ifdef NEVER
	if ((!(MoveMask & MOVEF_MOVING_BLOCK)) && Map.Cell_Distance(StartLocation, cell) > 2) {
		temp_move_mask |= MOVEF_MOVING_BLOCK;
	}
#endif

	can = (temp_move_mask & Can_Enter_Cell(cell, face));
	if (can & MOVEF_NO) return(0);

	retval = 1;
	if (can & MOVEF_MOVING_BLOCK) retval += 3;
	if (can & MOVEF_DESTROYABLE) retval += 10;
	if (can & MOVEF_TEMP) retval += 10;

	if (threat != -1) {
		if (Map.Cell_Distance(cell, DestLocation) > THREAT_THRESHOLD) {
			if (Map.Cell_Threat(cell, Owner()) > threat)
				return(0);
		}
	}

	return(retval);
#endif
}


void FootClass::Debug_Draw_Map(char *txt, CELL start, CELL dest, bool pause)
{
	if ((!Debug_Find_Path) || (!DrawPath)) return;

	if (pause) Get_Key_Num();
	GraphicViewPortClass * page = Set_Logic_Page(SeenBuff);

	VisiblePage.Clear();
	Fancy_Text_Print(txt, 160, 0, WHITE, BLACK, TPF_8POINT|TPF_CENTER);
	for (int x = 0; x < 64; x++) {
 		for (int y = 0; y < 64; y++) {
			int color = 0;

			switch (Can_Enter_Cell( (CELL)((y << 6) + x))) {
				case MOVE_OK:
					color = GREEN;
					break;
				case MOVE_MOVING_BLOCK:
					color = LTGREEN;
					break;

				case MOVE_DESTROYABLE:
					color = YELLOW;
					break;
				case MOVE_TEMP:
					color = BROWN;
					break;
				default:
					color = RED;
					break;
			}
			if ((CELL)((y << 6) + x) == start)
				color = LTBLUE;
			if ((CELL)((y << 6) + x) == dest)
				color = BLUE;
			Fat_Put_Pixel(64 + (x*3), 8 + (y*3), color, 3, SeenBuff);
		}
	}
	Set_Logic_Page(page);
}

void FootClass::Debug_Draw_Path(PathType *path)
{
	if (!path) return;

	FacingType	*list	= path->Command;
	CELL 			pos	= path->Start;

	for (int idx = 0; idx < path->Length; idx++) {
		pos = Adjacent_Cell(pos, *list++);
		Draw_Cell_Point(pos, true,	-1, 0);
	}
}
