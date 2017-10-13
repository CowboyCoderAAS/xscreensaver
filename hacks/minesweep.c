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
/* might need these later or the basic idea to create effects and bevles */

#define LOAD_FACTOR .5
#define INITAL_SIZE 250

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

#endif /* DO_STIPPLE */

/* MACROS */

#define POS(y, x, w) y*w+x

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
	int width; /* needed for calculating stuff */
	
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
	int tileWidth;
	int tileHeight;
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

/* The functions */

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
	memset(set->list, 0, sizeof(tile)*(set->size+1));
	return set;
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
	size_t place = (size_t) hash(POS(value->y, value->x, value->width));
	int noDupe = 1; /* I assume there is no duplicate, if there is I switch this off */
	struct bucket *ted = set->list[place];
	int n = POS(value->y, value->x, value->width);
	while(ted) /* iterate until I find a duplicate or run out */
	{
		if(n==POS(ted->data->y, ted->data->x, ted->data->width))
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

/*
 * will be used to create a new board
 * will randomize were the bombs are
 */
static void 
board_init(struct board *game)
{
	game->grid = malloc(sizeof(tile)*((game->tileWidth+2)*(game->tileHeight+2)));
	
	
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
	/* TODO write code here */
	
	board_init(game);
}

static void *
minesweep_init (Display *dpy, Window window)
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
	lore->game.bombCount = get_integer_resource(lore->dsp, "bombcount", "Integer");
	lore->speed = get_integer_resource(lore->dsp, "speed", "Integer");

	board_init(&lore->game);

	return lore;
}

static unsigned long
minesweep_draw(Display *dpy, Window window, void *closure)
{
	struct state *lore = (struct state *) closure;

	return lore->speed;
}



static void
minesweep_reshape (Display *dpy, Window window, void *closure, unsigned int w, unsigned int h)
{

}

static Bool
minesweep_event (Display *dpy, Window window, void *closure, XEvent *event)
{
	size_t filler;
	state=0;
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

