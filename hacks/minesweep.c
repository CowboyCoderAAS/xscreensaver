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
#define GRID_BOARDER 0
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

static enum GRID_COLOR {
	GRID_ZERO = 0,
	GRID_ONE,
	GRID_TWO,
	GRID_THREE,
	GRID_FOUR,
	GRID_FIVE,
	GRID_SIX,
	GRID_SEVEN,
	GRID_EIGHT,
	GRID_UNCLICKED,
	GRID_FLAG,
	GRID_BOMB	
} grid_color;

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
	int minY, minX, maxY, maxX;
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

static struct queue {
	struct bucket *front;
	size_t size;
} queue;

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
	int unclicked;
	int gameOver;
	Bool alwaysWin;
	
	struct tile *grid;
	
	struct hashset *moveSet; /* holds the moves that I am able to make in the game */
	struct hashset *pmove; /* holds potential moves that will need testing */
	struct hashset *flagSet; /* to update the qflags */
	struct queue *displayQueue; /* lets me know what tiles need updating */

	int firstY, firstX; /* the first moves that will be made,-1 if I have already made a first move*/

} board;

struct state { /* this puppy holds all the info that will be moved from frame to frame */
	Display *dsp;
	Window window;
	Window gridWindow;

	GC gc;
	Colormap cmap;
	int speed; /* how fast I am playing the game */
	int xlim, ylim;
	int wh;
	struct board game;
};

/* the options */
static const char *minesweep_defaults [] = {
	".background:	black",
	".foreground:	white",
	".width: 		50",
	".height: 		50",
	".bombcount: 	525",
	"*speed: 		5000",
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
	{"-alwayswin", ".alwayswin", XrmoptionNoArg},
	{ 0, 0, 0, 0 } /* to terminate the list */
};

/*** queue functions ***/
static struct queue *
queue_init(void)
{
	struct queue *result = (struct queue *) malloc(sizeof(struct queue));
	memset(result, 0, sizeof(struct queue));
	result->size = 0;
	return result;
}

static void
queue_add(struct queue *qu, struct tile *value)
{
	struct bucket *ted = (struct bucket *) malloc(sizeof(struct bucket));
	ted->data = value;
	ted->next = qu->front;
	qu->front = ted;
	qu->size++;
}

static struct tile *
queue_pop(struct queue *qu)
{
	struct tile *result;
	struct bucket *temp;
	if(!qu->size) return NULL;
	result = qu->front->data;
	temp = qu->front;
	qu->front = qu->front->next;
	free(temp);
	qu->size--;
	return result;
}

static void
queue_free(struct queue *qu)
{
	while(qu->size) queue_pop(qu);
	free(qu);
}

/*
 * returns true if queue is empty false otherwise
 */
static Bool
queue_empty(struct queue *qu)
{
	return !qu->size;
}

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
			set->elements--;
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
	while(!hashset_empty(old)) hashset_add(set, hashset_pop(old));
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
		if((double)(set->elements/(double)set->size)>=LOAD_FACTOR) 
			return hashset_rehash(set);
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
	{
		if(n==POS(ted->data->y, ted->data->x, ted->data->w)) 
		{
			return 1; /* found */
		}
		ted = ted->next;
	}
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
		init_tile(&game->grid[POS((game->bheight-1), i, game->bwidth)], 
				game->bheight-1, i, game->bwidth, BOARDER);
	}
	for(i=0; i<game->bheight; i++)
	{
		init_tile(&game->grid[POS(i, 0, game->bwidth)], i, 0, game->bwidth, BOARDER);
		init_tile(&game->grid[POS(i, (game->bwidth-1), game->bwidth)], 
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
	} while(game->grid[position].state==BOARDER);
	game->moveSet = hashset_init(INITAL_SIZE);
	temp = &game->grid[POS(game->firstY, game->firstX, game->bwidth)];
	temp->move = CLICK;
	hashset_add(game->moveSet, temp);
	game->unclicked = game->tileWidth*game->tileHeight;
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
		hashset_add(used, &game->grid[POS((game->firstY+i), (game->firstX+j), game->bwidth)]);
	}
	/* now go through and start adding bombs */
	while(madeCount < game->bombCount)
	{
		position = random() % fullLength;
		if(!hashset_contains(used, &game->grid[position]) 
				&& game->grid[position].state!=BOARDER
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
		if(game->grid[POS(i, j, game->bwidth)].isBomb) 
		{
			continue; /* don't count bombs */
		}
		count = 0;
		for(y=-1; y<=1; y++) for(x=-1; x<=1; x++)
		{
			if(game->grid[POS((i+y), (j+x), game->bwidth)].isBomb) count++;
		}
		game->grid[POS(i, j, game->bwidth)].bombNumber = count;
	}
}

#if DEBUG
static void
board_clickAll(struct board *game)
{
	int i, j;
	for(i=1; i<=game->tileHeight; i++) for(j=1; j<=game->tileWidth; j++)
	{
		game->grid[POS(i, j, game->bwidth)].state = CLICKED;
	}
}
#endif

/*
 * will be used to create a new board
 * will randomize were the bombs are
 * thought I'd try somthing different and made a bunch of individule functions
 * instead of making a bloated function with all of it inside
 */
static void 
board_init(struct board *game)
{
	game->tileWidth = game->bwidth-2; /* TODO test if I need this any more */
	game->tileHeight = game->bheight-2;
	board_init_grid(game);
	printf("finished init grid\n");
	board_init_firstClick(game);
	printf("finished first click\n");
	board_init_bombs(game);
	printf("finisehd init bobm\n");
	board_init_bombNumber(game);
	printf("finishedBombNumber\n");
	game->displayQueue = queue_init();
	game->pmove = hashset_init(INITAL_SIZE);
	game->flagSet = hashset_init(INITAL_SIZE);
#if DEBUG
	/*board_clickAll(game); */
#endif
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
	hashset_free(game->pmove);
	hashset_free(game->flagSet);
	queue_free(game->displayQueue);
	free(game->grid);
	game->foundCount = 0;
	game->firstY = 0;
	game->firstX = 0;
	game->gameOver = 0;
	board_init(game);
}

static void *
minesweep_init(Display *dpy, Window window)
{
	int heightOffset, widthOffset;
	XWindowAttributes atter;
	XGCValues value;
	struct state *lore = (struct state *) calloc(1, sizeof(*lore));
	
	lore->dsp = dpy;
	lore->window = window;

	XGetWindowAttributes(lore->dsp, lore->window, &atter);
	
	lore->xlim = atter.width;
	lore->ylim = atter.height;
	lore->cmap = atter.colormap;
	
	lore->game.tileWidth = get_integer_resource(lore->dsp, "width", "Integer");
	lore->game.tileHeight = get_integer_resource(lore->dsp, "height", "Integer");
	lore->game.bwidth = lore->game.tileWidth+2;
	lore->game.bheight = lore->game.tileHeight+2;
	lore->game.bombCount = get_integer_resource(lore->dsp, "bombcount", "Integer");
	lore->speed = get_integer_resource(lore->dsp, "speed", "Integer");
	printf("tWidth %d tHeight %d bwidth %d bheight %d bombCount %d speed %d",
			lore->game.tileWidth, lore->game.tileHeight, lore->game.bwidth, 
			lore->game.bheight, lore->game.bombCount, lore->speed);

	lore->game.alwaysWin = get_boolean_resource(lore->dsp, "alwayswin", "Boolean");

	board_init(&lore->game);
	
	lore->wh = (lore->xlim)/lore->game.tileWidth; 
	if(lore->wh>(lore->ylim)/lore->game.tileHeight) 
		lore->wh = (lore->ylim)/lore->game.tileHeight;
	heightOffset = (lore->ylim-lore->wh*lore->game.tileHeight)/2;
	widthOffset = (lore->xlim-lore->wh*lore->game.tileWidth)/2;
	lore->gridWindow = XCreateWindow(lore->dsp, lore->window, widthOffset, heightOffset, 
			lore->xlim*lore->game.tileWidth, lore->ylim*lore->game.tileHeight,
			GRID_BOARDER, CopyFromParent, CopyFromParent, CopyFromParent, 0, 0);
	XMapWindow(lore->dsp, lore->gridWindow);

	if(lore->game.tileWidth*lore->game.tileHeight>=lore->game.bombCount-9)
	{ /* not enough space for bombs exiting XXX make it revert to default */
		/*fprintf(stderr, "exiting cause I can\n");
		exit(1);*/
		printf("tileWidth %d tileHight %d\n", lore->game.tileWidth, lore->game.tileHeight);
	}
	value.foreground = get_pixel_resource(lore->dsp, lore->cmap, "foreground", "Foreground");
	value.background = get_pixel_resource(lore->dsp, lore->cmap, "background", "Background");
	value.fill_style = FillSolid;
	lore->gc = XCreateGC(lore->dsp, lore->window, GCForeground|GCBackground|GCFillStyle, &value);

	printf("finished init\n");
	return lore;
}

/*** GAME PLAY ***/

static void
update_qflag(struct board *game, struct tile *data)
{
	int i, j;
	if(data->state!=CLICKED) return;
	data->flagCount = 0;
	data->unclicked = 0;
	data->minY = data->y;
	data->minX = data->x;
	data->maxY = data->y;
	data->maxX = data->x;
	for(i=-1; i<=1; i++) for(j=-1; j<=1; j++)
	{
		struct tile *place = &game->grid[POS((data->y+i), (data->x+j), data->w)];
		if(place->state==FLAGGED) data->flagCount++;
		if(place->state==NONE) 
		{
			data->unclicked++;
			if(place->y < data->minY) data->minY = place->y;
			else if(place->y > data->maxY) data->maxY = place->y;
			if(place->x < data->minX) data->minX = place->x;
			else if(place->x > data->maxX) data->maxX = place->x;
		}
	}
}

static void
checkArea(struct board *game, struct tile *data)
{
	int i, j;
	struct tile *other;
	for(i=-1; i<=1; i++) for(j=-1; j<=1; j++)
	{
		other = &game->grid[POS((data->y+i), (data->x+j), data->w)];
		if(other->state == NONE && !hashset_contains(game->moveSet, other)) 
		{
			hashset_add(game->pmove, other);
		}
		else if(other->bombNumber > 0 && other->state==CLICKED) 
		{
			hashset_add(game->flagSet, other);
		}
	}
}

static void
clickMove(struct board *game, int y, int x)
{
	int i, j;
	int place = POS(y, x, game->bwidth);
	int type = game->grid[place].state;
	if(type == BOARDER || type == CLICKED || type == FLAGGED) return;
	game->grid[place].state = CLICKED;
	game->unclicked--;
	queue_add(game->displayQueue, &game->grid[place]);
	if(game->grid[place].bombNumber==0 && !game->grid[place].isBomb)
	{
		for(i=-1; i<=1; i++) for(j=-1; j<=1; j++)
		{
			clickMove(game, y+i, x+j);
		}
		checkArea(game, &game->grid[place]);
	} 
	else 
	{
		if(game->grid[place].isBomb) 
		{
			printf("BOOOM\n");
			game->gameOver = 1;
			return;
		}
		checkArea(game, &game->grid[place]);
	}
}

static void 
flagMove(struct board *game, struct tile *data)
{
	data->state = FLAGGED;
	game->foundCount++;
	checkArea(game, data);
}

/*
 * test if I can make a 1 1 or a 1 2 move
 * returns true if I can so I no longer have to move 
 * else returns false
 */
static Bool 
couldPattern(struct board *game, struct tile *previous, struct tile *current, struct tile *move)
{
	if(current->x>=move->x-1 && current->x<=move->x+1 
			&&current->y>=move->y-1 && current->y<=move->y+1) return False;
	if(current->maxX-previous->minX>=3  /* make sure I am within range of the liberty */
			|| previous->maxX-current->minX >=3 
			|| current->maxY-previous->minY >=3
			|| previous->maxY-current->minY >=3) return False;


	if(current->bombNumber-current->flagCount == current->unclicked-1 && current->unclicked == 2 
			&& previous->bombNumber-previous->flagCount == previous->unclicked-2 
			&& previous->unclicked == 3)
	{
		move->move = CLICK;
		hashset_add(game->moveSet, move);
		return True;
	}
	if(current->bombNumber-current->flagCount == current->unclicked-1 && current->unclicked == 2
			&& previous->bombNumber-previous->flagCount == previous->unclicked-1 
			&& previous->unclicked == 3)
	{
		move->move = FLAG;
		hashset_add(game->moveSet, move);
		return True;
	}
	return False;
}

/*
 * If I could make a move then adds it to the move set
 * returns true if it added it to the set
 * false otherwise
 */ 
static Bool
couldMove(struct board *game, struct tile *place)
{
	int i, j, y, x;
	struct tile *other;
	struct tile *current;
	if(place->state != NONE) return False;
	for(i=-1; i<=1; i++) for(j=-1; j<=1; j++)
	{
		other = &game->grid[POS((place->y+i), (place->x+j), game->bwidth)];
		if(other->state != CLICKED || other == place || other->bombNumber==0) continue;
		if(other->unclicked == other->bombNumber-other->flagCount)
		{
			place->move = FLAG;
			hashset_add(game->moveSet, place);
			return True;
		}
		else if(other->bombNumber - other->flagCount == 0)
		{
			place->move = CLICK;
			hashset_add(game->moveSet, place);
			return True;
		}
		for(y=-1; y<=1; y++) for(x=-1; x<=1; x++)
		{
			if(abs(y)==abs(x)) continue;
			current = &game->grid[POS((other->y+y), (other->x+x), other->w)];
			if(current->state != CLICKED || other == current || current->bombNumber==0) continue;
			if(couldPattern(game, other, current, place)) return True;
		}
	}
	return False;
}

/*
 * attempts to make a move from the set
 * will return True if it was able to make a move
 * false otherwise
 */
static Bool
move_fromSet(struct board *game)
{
	struct tile *move;
	if(hashset_empty(game->moveSet)) return False;
	move = hashset_pop(game->moveSet);
	if(move->move == CLICK) clickMove(game, move->y, move->x);
	else flagMove(game, move);
	move->move = CANT;
	return True;
}

static void
move_updateAll(struct board *game)
{
	int y, x;
	for(y=1; y<=game->tileHeight; y++) for(x=1; x<=game->tileWidth; x++)
	{
		update_qflag(game, &game->grid[POS(y, x, game->bwidth)]);
	}
}

static void
move_checkAll(struct board *game)
{
	int y, x;
	for(y=1; y<=game->tileHeight; y++) for(x=1; x<=game->tileWidth; x++)
	{
		couldMove(game, &game->grid[POS(y, x, game->bwidth)]);
	}
}

/*
 * will update first the q flag from the set
 * then will check move posibilitys from the pmove set
 */
static void
repopulate_area(struct board *game)
{
	struct tile *move;
	/*struct queue *store = queue_init(); */
	
	while(!hashset_empty(game->flagSet)) 
	{
		update_qflag(game, hashset_pop(game->flagSet));
	}
	while(!hashset_empty(game->pmove)) 
	{
		move = hashset_pop(game->pmove);
		/*if(!*/couldMove(game, move)/*) queue_add(store, move)*/;
	}
	/*while(!queue_empty(store)) hashset_add(game->pmove, queue_pop(store));*/
	if(hashset_empty(game->moveSet))
	{
		printf("checking all\n");
		move_updateAll(game);
		move_checkAll(game);
	}
}

static void
playGame(struct board *game)
{
	int y, x;
	int fullLength;
	int position;
	if(!move_fromSet(game))
	{
		repopulate_area(game);
		if(!move_fromSet(game)) /* trying again */
		{
			printf("time to guess\n");
			fullLength = game->bwidth*game->bheight;
			do
			{
				BACK:
				position = random() % fullLength;
				x = position%game->bwidth;
				y = (int) position/game->bwidth;
				if(game->alwaysWin && game->grid[position].isBomb) goto BACK;
				/* first time using a goto... I feel dirty */
			} while(game->grid[position].state != NONE);
			printf("guess click move of %d %d\n", y, x);
			clickMove(game, y, x); /* just guessing here and clicking */
		}
	}
}

/*** DRAWING ***/

static void
updateGC(Display *dsp, GC gc, int type)
{
	XGCValues value;
	/*value.foreground = 0xFFFFFF;*/
	switch(type)
	{
		case GRID_UNCLICKED:
			value.foreground = 0x0000FF;
			break;
		case GRID_FLAG:
			value.foreground = 0xFF0000;
			break;
		case GRID_BOMB:
			value.foreground = 0x800000;
			break;
		case GRID_ZERO:
			value.foreground = 0xFFFFFF;
			break;
		case GRID_ONE:
			value.foreground = 0x1E90FF;
			break;
		case GRID_TWO:
			value.foreground = 0x008000;
			break;
		case GRID_THREE:
			value.foreground = 0xFA8072;
			break;
		case GRID_FOUR:
			value.foreground = 0xFF8C00;
			break;
		case GRID_FIVE:
			value.foreground = 0x8B4513;
			break;
		case GRID_SIX:
			value.foreground = 0xFF00FF;
			break;
		case GRID_SEVEN:
			value.foreground = 0x00FFFF;
			break;
		case GRID_EIGHT:
			value.foreground = 0x696969;
			break;
	}
	XChangeGC(dsp, gc, GCBackground|GCForeground, &value);
}

static void 
draw_boarder(Display *dsp, Window window, GC gc, unsigned wh, struct tile *data)
{
	XGCValues value;
	value.foreground = 0x000000;
	XChangeGC(dsp, gc, GCForeground, &value);
	XDrawRectangle(dsp, window, gc, (data->x-1)*wh, (data->y-1)*wh, wh, wh);
}

static void
draw_tile(Display *dsp, Window window, GC gc, 
		unsigned wh, struct tile *data)
{
	if(data->state == BOARDER) return;
	if(data->state == NONE)
	{
		updateGC(dsp, gc, GRID_UNCLICKED);
		XFillRectangle(dsp, window, gc, (data->x-1)*wh, (data->y-1)*wh, wh, wh);
	} 
	else if(data->state == CLICKED)
	{
		if(data->isBomb)
		{
			updateGC(dsp, gc, GRID_BOMB);
			XFillRectangle(dsp, window, gc, (data->x-1)*wh, (data->y-1)*wh, wh, wh);
		} 
		else 
		{
			updateGC(dsp, gc, data->bombNumber);
			XFillRectangle(dsp, window, gc, (data->x-1)*wh, (data->y-1)*wh, wh, wh);
		}
	} 
	else /* I have a flagged tile */
	{
		updateGC(dsp, gc, GRID_FLAG);
		XFillRectangle(dsp, window, gc, (data->x-1)*wh, (data->y-1)*wh, wh, wh);
	}
	draw_boarder(dsp, window, gc, wh, data);
}

static void
draw_grid(struct state *lore)
{
	int i;
	int totalLength = lore->game.bwidth*lore->game.bheight;
	if(queue_empty(lore->game.displayQueue)) /* empty queue display all */
	{
		for(i=0; i<totalLength; i++) /* TODO could be more effetient */
		{
			draw_tile(lore->dsp, lore->gridWindow, lore->gc, lore->wh, 
					&lore->game.grid[i]);
		}
	} 
	else while(!queue_empty(lore->game.displayQueue))
	{
		struct tile *data = queue_pop(lore->game.displayQueue);
		draw_tile(lore->dsp, lore->gridWindow, lore->gc, lore->wh, data);
	}
}

static unsigned long
minesweep_draw(Display *dsp, Window window, void *closure)
{
	struct state *lore = (struct state *) closure;
	int temp=0;
	if(lore->game.firstY >=0)
	{
		lore->game.firstY = -1;
		draw_grid(lore);
		return lore->speed;
	}
	if(lore->game.gameOver || lore->game.unclicked-lore->game.foundCount==0)
	{
		if(lore->game.gameOver)
			temp=1;
		draw_grid(lore);
		board_clear(&lore->game);
		if(temp) return lore->speed*100;
		return lore->speed*3;
	}

	playGame(&lore->game);
	draw_grid(lore);

	return lore->speed;
}



static void
minesweep_reshape(Display *dsp, Window window, void *closure, unsigned int w, unsigned int h)
{
	/* TODO add more to this */
	struct state *lore = (struct state *) closure;
	lore->xlim = w;
	lore->ylim = h;
}

static Bool
minesweep_event(Display *dpy, Window window, void *closure, XEvent *event)
{
	return False;
}

static void
minesweep_free (Display *dpy, Window window, void *closure)
{
  struct state *st = (struct state *) closure;
  XFreeGC(st->dsp, st->gc);
  free(st);
}

XSCREENSAVER_MODULE ("MineSweep", minesweep)
