/* xscreensaver, Copyright (c) 1992-2008 Jamie Zawinski <jwz@jwz.org>
 * MineSweep, Copyright (c) 2017 Albert Andrew Spencer <cowboycoderaas@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

/*
 * Albert Andrew Spencer
 * 10/10/17
 * This will play minesweep, based off of the greynetic created by Jamie Zawinski
 *
 */ 

#include "screenhack.h"

#ifndef HAVE_JWXYZ
# define DO_STIPPLE
#endif

#define NBITS 12

/* On some systems (notably MacOS X) these files are messed up.
 * They're tiny, so we might as well just inline them here.
 *
 * # include <X11/bitmaps/stipple>
 * # include <X11/bitmaps/cross_weave>
 * # include <X11/bitmaps/dimple1>
 * # include <X11/bitmaps/dimple3>
 * # include <X11/bitmaps/flipped_gray>
 * # include <X11/bitmaps/gray1>
 * # include <X11/bitmaps/gray3>
 * # include <X11/bitmaps/hlines2>
 * # include <X11/bitmaps/light_gray>
 * # include <X11/bitmaps/root_weave>
 * # include <X11/bitmaps/vlines2>
 * # include <X11/bitmaps/vlines3>
*/

/*
#ifdef DO_STIPPLE
#define stipple_width  16
#define stipple_height 4
static unsigned char stipple_bits[] = {
  0x55, 0x55, 0xee, 0xee, 0x55, 0x55, 0xba, 0xbb};

#define cross_weave_width  16
#define cross_weave_height 16
static unsigned char cross_weave_bits[] = {
   0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22, 0x55, 0x55, 0x88, 0x88,
   0x55, 0x55, 0x22, 0x22, 0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22,
   0x55, 0x55, 0x88, 0x88, 0x55, 0x55, 0x22, 0x22};

#define dimple1_width 16
#define dimple1_height 16
static unsigned char dimple1_bits[] = {
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00,
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00,
   0x55, 0x55, 0x00, 0x00, 0x55, 0x55, 0x00, 0x00};

#define dimple3_width 16
#define dimple3_height 16
static unsigned char dimple3_bits[] = {
   0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
*/
/* might need these later or the basic idea to create effects and bevles */


/*
#define flipped_gray_width  4
#define flipped_gray_height 2
static char flipped_gray_bits[] = { 0x07, 0x0d};
#define gray1_width  2
#define gray1_height 2
static char gray1_bits[] = { 0x01, 0x02};
#define gray3_width  4
#define gray3_height 4
static char gray3_bits[] = { 0x01, 0x00, 0x04, 0x00};
#define hlines2_width  1
#define hlines2_height 2
static char hlines2_bits[] = { 0x01, 0x00};
#define light_gray_width  4
#define light_gray_height 2
static char light_gray_bits[] = { 0x08, 0x02};
#define root_weave_width  4
#define root_weave_height 4
static char root_weave_bits[] = { 0x07, 0x0d, 0x0b, 0x0e};
#define vlines2_width  2
#define vlines2_height 1
static char vlines2_bits[] = { 0x01};
#define vlines3_width  3
#define vlines3_height 1
static char vlines3_bits[] = { 0x02};
*/
/*#endif*/ /* DO_STIPPLE */

#define LOAD_FACTOR .5
#define INITAL_SIZE 250
#define DEBUG 1

/* MACROS */

#define POS(y, x, w) y*w+x
#define GETPOS(y, x, w, p) 	{ \
	y = (int)p/w; \
	x = p%w; \
							}

static enum STATE {
	NONE = 0, /* nothing has happend */
	CLICKED,
	FLAGGED,
	BOARDER /* if I am not in the game and just a boarder */
} state;

static enum MOVE { /* describes the type of move availabe */
	CANT = 0,
	FLAG, /* I can be flaged */
	CLICK 
} move;

static struct tile {
	Bool isBomb;
	int bombNumber;
	int state; /* if its clicked flaged or not  */
	int move; /* if I am capable of doing a move */
	
	int y; /* the position it is in the grid */
	int x;
	int w;
	
	/* the Q flags structure, only becomes relevent when tile is clicked */
	int flagCount; 
	int unclicked; 
} tile;

/* used by the hash set to contain the links */
static struct bucket {
	struct tile *data;
	struct bucket *next;
} bucket;

/* a hash set used for tiles */
static struct hashset {
	struct bucket **list;
	size_t size;
	size_t elements;
	size_t startElement;
} hashset;

/*
 * holds everything I have an interest in playing the minesweep game besides drawing it
 * using the old trick of tileWidth+2 and tileHeight+2 to create a boarder
 */
static struct board {
	int buffer1; /* XXX corruption occurs unless I have these 2 variables here */
	int buffer2;
	int tileWidth; /* the actual tiles that I am working with */
	int tileHeight;
	int bwidth; /* this is tileWidth+2 */
	int bheight; 
	int bombCount;
	int foundCount;
	
	struct tile *grid;
	
	struct hashset *moveSet; /* holds the moves that I am able to make in the game */

	int firstY, firstX; /* the first moves that will be made,-1 if I have already made a first move*/
} board;

struct state { /* this puppy holds all the info that will be moved from frame to frame */
  Display *dsp;
  Window window;

  Pixmap pixmaps [NBITS];

  GC gc;
  int speed; /* how fast I am playing the game */
  unsigned long fg, bg, pixels [512];
  int npixels;
  int xlim, ylim;
  Colormap cmap;

  struct board game;
};

/* the options */
static const char *minesweep_defaults [] = {
	".width: 		30",
	".height: 		16",
	".bombcount: 	99",
	".speed: 		1000",
#ifdef HAVE_MOBILE
	"*ignoreRotation: True",
#endif
	0
};

static XrmOptionDescRec minesweep_options [] = {
	{"-width",	".width", XrmoptionSepArg, 0},
	{"-height", ".height", XrmoptionSepArg, 0},
	{"-bombcount", ".bombcount", XrmoptionSepArg, 0},
	{"-speed", ".speed", XrmoptionSepArg, 0},
	{ 0, 0, 0, 0 } /* to terminate the list */
};

/*** hashset functions ***/

static int
hashset_empty(struct hashset *set)
{
	return set->elements==0;
}

/* basic hash function I gots from stack overflow */
static unsigned int 
hash(unsigned int n)
{
	n = ((n>>16)^n)*0x45d9f3b;
	n = ((n>>16)^n)*0x45d9f3b;
	n = (n>>16)^n;
	return n;
}

/*
 * will pop the first element on the set and remove it
 * returns null if the set is empty
 */
static struct tile *
hashset_pop(struct hashset *set)
{
	int i;
	struct tile *result;
	if(hashset_empty(set)) return NULL;
	result = NULL;
	for(i=0; i<set->size; i++)
	{
		struct bucket *temp = set->list[(i+set->startElement)%set->size];
		if(temp)
		{
			result = temp->data;
			set->list[(i+set->startElement)%set->size] = temp->next;
			free(temp);
			set->startElement = (i+set->startElement)%set->size;
			break;
		}
	}
	return result;
}

static struct hashset *
hashset_init(size_t startSize)
{
	struct hashset *set = (struct hashset *) malloc(sizeof(hashset));
	set->size = startSize;
	set->startElement = startSize-1;
	set->elements = 0;
	set->list = (struct bucket **) malloc(sizeof(struct bucket *)*(set->size+1));
	memset(set->list, 0, sizeof(struct bucket *)*(set->size+1));
	return set;
}

static void
hashset_free(struct hashset *set)
{
	while(!hashset_empty(set)) hashset_pop(set);
	free(set->list);
	free(set);
}

static struct hashset *hashset_add(struct hashset *set, struct tile *value);

static struct hashset * /* XXX could be more effenent with malloc and free's */
hashset_rehash(struct hashset *old)
{
	struct hashset *set = hashset_init(old->size*2);
	while(!hashset_empty(old)) hashset_add(set, hashset_pop(set));
	return set;
}

static struct hashset *
hashset_add(struct hashset *set, struct tile *value)
{
	int n = POS(value->y, value->x, value->w);
	size_t place = (size_t) hash(n)%set->size;
	int noDupe = 1; /* I assume there is no duplicate, if there is I switch this off */
	struct bucket *ted = set->list[place];
	while(ted) /* iterate until I find a duplicate or run out */
	{
		if(n==POS(ted->data->y, ted->data->x, ted->data->w))
		{ /* there was a duplicate based off of there position value */
			noDupe = 0;
			break;
		}
		ted = ted->next;
	}
	if(noDupe)
	{
		struct bucket *holder = (struct bucket *) malloc(sizeof(bucket));
		holder->data = value;
		holder->next = set->list[place];
		set->list[place] = holder;
		set->elements++;
		if(place<set->startElement) set->startElement = place;
		if((double)(set->elements/(double)set->size)>=LOAD_FACTOR) return hashset_rehash(set);
	}
	return set;
}

/* returns 0 if it does not contain, 1 if it does contain */
static int
hashset_contains(struct hashset *set, struct tile *value)
{
	int n = POS(value->y, value->x, value->w);
	size_t place = (size_t) hash(n)%set->size;
	struct bucket *ted = set->list[place];
	while(ted)
		if(n==POS(ted->data->y, ted->data->x, ted->data->w)) return 1; /* found */
	return 0;
}

/* *** initalizing functions ***/

static void
init_tile(struct tile *t, int y, int x, int w, int state)
{
	t->y = y;
	t->x = x;
	t->w = w;
	t->state = state;
}

static void
board_init_grid(struct board *game)
{
	int i;
	int j;
	game->grid = malloc(sizeof(struct tile)*(game->bwidth*game->bheight));
	memset(game->grid, 0, sizeof(struct tile)*(game->bwidth*game->bheight));
	/* setting up the boarder tiles */
	for(i=0; i<game->bwidth; i++) 
	{
		init_tile(&game->grid[POS(0, i, game->bwidth)], 0, i, game->bwidth,  BOARDER);
		init_tile(&game->grid[POS(game->bheight-1, i, game->bwidth)], 
				game->bheight-1, i, game->bwidth, BOARDER);
	}
	for(i=0; i<game->bheight; i++)
	{
		init_tile(&game->grid[POS(i, 0, game->bwidth)], i, 0, game->bwidth, BOARDER);
		init_tile(&game->grid[POS(i, game->bwidth-1, game->bwidth)], 
				i, game->bwidth-1, game->bwidth, BOARDER);
	}
	/* initing the full grid now */
	for(i=1; i<=game->tileHeight; i++) for(j=1; j<=game->tileWidth; j++)
	{
		init_tile(&game->grid[POS(i, j, game->bwidth)], i, j, game->bwidth, NONE);
	}
}

/*
 * initalizes the first click
 * this will be used when initing the bombs
 * also adds the first click to the move set
 */
static void
board_init_firstClick(struct board *game)
{
	int position;
	struct tile *temp;
	int fullLength = game->bwidth*game->bheight;
	do
	{
		position = random() % fullLength;
		game->firstX = position%game->bwidth;
		game->firstY = (int)position/game->bwidth;
	} while(game->grid[position].state!=BOARDER);
	game->moveSet = hashset_init(INITAL_SIZE);
	temp = &game->grid[POS(game->firstY, game->firstX, game->bwidth)];
	temp->move = CLICK;
	hashset_add(game->moveSet, temp);
}

static void
board_init_bombs(struct board *game)
{
	int i;
	int j;
	int position;
	int fullLength = game->bwidth*game->bheight;
	int madeCount = 0;
	struct hashset *used = hashset_init(INITAL_SIZE); /* what I have already placed a bomb in */
	for(i=-1; i<=1; i++) for(j=-1; j<=1; j++)
	{
		hashset_add(used, &game->grid[POS(game->firstY+i, game->firstX+j, game->bwidth)]);
	}
	/* now go through and start adding bombs */
	while(madeCount < game->bombCount)
	{
		position = random() % fullLength;
		if(!hashset_contains(used, &game->grid[position]) && game->grid[position].state!=BOARDER
				&& !game->grid[position].isBomb)
		{
			game->grid[position].isBomb = True;
			madeCount++;
		}
	}
	hashset_free(used);
}

static void
board_init_bombNumber(struct board *game)
{
	int i;
	int j;
	int y;
	int x;
	int count;
	for(i=1; i<=game->tileHeight; i++) for(j=1; j<=game->tileWidth; j++)
	{
		if(game->grid[POS(i, j, game->bwidth)].isBomb) continue; /* don't count bombs */
		count = 0;
		for(y=-1; y<=1; y++) for(x=-1; x<=1; x++)
		{
			if(game->grid[POS(i+y, j+x, game->bwidth)].isBomb) count++;
		}
		game->grid[POS(i, j, game->bwidth)].bombNumber = count;
	}
}

/*
 * will be used to create a new board
 * will randomize were the bombs are
 * thought I'd try somthing different and made a bunch of individule functions
 * instead of making a bloated function with all of it inside
 */
static void 
board_init(struct board *game)
{
	game->tileWidth = game->bwidth-2;
	game->tileHeight = game->bheight-2;
	/*printf("th %d tw %d\n", game->tileWidth, game->tileHeight); */ 
	board_init_grid(game);
	board_init_firstClick(game);
	board_init_bombs(game);
	board_init_bombNumber(game);
}

/*
 * Called in between games
 * will re-initalize the board
 * clearing flags and stuff
 * 
 * calls board init aftarwards
 */
static void
board_clear(struct board *game)
{
	hashset_free(game->moveSet);
	free(game->grid);
	game->foundCount = 0;
	game->firstY = 0;
	game->firstX = 0;
	board_init(game);
}

static void *
minesweep_init(Display *dpy, Window window)
{
	XWindowAttributes atter;
	struct state *lore = (struct state *) calloc(1, sizeof(*lore));
	
	lore->dsp = dpy;
	lore->window = window;

	XGetWindowAttributes(lore->dsp, lore->window, &atter);
	
	lore->xlim = atter.width;
	lore->ylim = atter.height;
	lore->cmap = atter.colormap;
	
	lore->game.tileWidth = get_integer_resource(lore->dsp, "width", "Integer");
	lore->game.tileHeight = get_integer_resource(lore->dsp, "height", "Integer");
	/*printf("tileWidth %d tileHeight %d\n", lore->game.tileWidth, lore->game.tileHeight); */
	lore->game.bwidth = lore->game.tileWidth+2;
	lore->game.bheight = lore->game.tileHeight+2;
	lore->game.bombCount = get_integer_resource(lore->dsp, "bombcount", "Integer");
	lore->speed = get_integer_resource(lore->dsp, "speed", "Integer");

	board_init(&lore->game);

	return lore;
}

/*** GAME PLAY ***/



/*** DRAWING ***/



static unsigned long
minesweep_draw(Display *dpy, Window window, void *closure)
{
	struct state *lore = (struct state *) closure;
	return lore->speed;
}



static void
minesweep_reshape (Display *dsp, Window window, void *closure, unsigned int w, unsigned int h)
{
	struct state *lore = (struct state *) closure;
	lore->xlim = w;
	lore->ylim = h;
}

static Bool
minesweep_event (Display *dpy, Window window, void *closure, XEvent *event)
{
	state=0; /* I really wanted to suppress some warnings so I'm doing some odd jobs here */
	move=0;
	board.firstY = -1;
	return False;
}

static void
minesweep_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC (st->dsp, st->gc);
  free (st);
}

XSCREENSAVER_MODULE ("MineSweep", minesweep)
