/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
//                                                                           8 BALL POOL GAME
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

// Standard libraries
#include <stdlib.h>             
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <allegro.h>
#include <time.h>

// My ptask library
#include "ptask.h"              

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// PHYSIC CONSTANTS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define     N_BALLS     16          // number of balls in the game
#define     N_HOLES     6           // number of holes in the table
#define     DIAM        0.055       // radius of a ball [m]
#define     WLEN        100         // wake lenght for trail depiction

#define     LX          1.77        // width of field in x direction [m]
#define     LY          0.85        // width of field in y direction [m]
#define     HC          0.035       // parameter that defines holes position [m]
#define     HP          0.065       // parameter that defines the gap in the table bounds [m]

#define     B0SX        0.425       // white ball spawn coordinate x [m]
#define     B0SY        0.425       // white ball spawn coordinate y [m]
#define     B1SX        1.345       // ball 1 spawn coordinate x [m]
#define     B1SY        0.425       // ball 1 spawn coordinate y [m]
#define     B1EX        1.94        // ball 1 eliminated coordinate x [m]
#define     B1EY        0           // ball 1 eliminated coordinate y [m]

#define     PER         40          // ball task period [ms]

#define     D_VEL       0.01        // velocity variation in shot regulation [m/s]
#define     V_MAX       2           // maximum shot velocity [m/s]
#define     D_F         0.001       // friction factor variation for regulation
#define     F_MAX       0.1         // maximum friction factor
#define     D_DUMP      0.01        // dumping factor variation for regulation
#define     DUMP_MAX    1           // maximun dumping factor
#define     D_TS        0.1         // time scale factor variation
#define     TS_MAX      3           // maximum time scale factor

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// GRAPHIC CONSTANTS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
#define     XWIN        1024        // window x resolution
#define     YWIN        768         // window y resolution
#define     XTAB        800         // table x lenght
#define     YTAB        432         // table y lenght
#define     BANK        46          // distance from the side of the table to the field
#define     IN_WID      50          // shot power indicator width
#define     IN_HEI      200         // shot power indicator height
#define     DIAM_P      22          // radius of the ball in pixels
#define     CF          400         // m to pixels ratio

// Colors
#define     BLACK       0
#define     DARK_BLUE   4278190208
#define     BLUE        255
#define     CYAN        65535
#define     PURPLE      8388736  
#define     WHITE       16777215
#define     RED         16711680
#define     ORANGE      16753920
#define     YELLOW      16776960
#define     GREEN       65280
#define     BROWN       10824234


/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// STRUCTURES
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Ball status and features structure
struct  status {
        float   x, y;           // Position [m]
        float   vx, vy;         // Velocity [m/s]
        int     tcol;           // trail color
        BITMAP* bm;             // relative bitmap
        float   xo, yo;         // ball position when it gets eliminated [m]
        int     active_flag;    // determines if a ball is active (1) or not (0)
        int     type;           // determines the type of the ball: 0 = solid, 1 = striped
        int     el_ph;          // phase in which the ball was eliminated
};
struct status ball[N_BALLS]; // Array for ball states


// Holes structure
struct  holes {
        float   x, y;   // hole position [m]
};
struct  holes   hole[N_HOLES];


// Circular buffer that stores wake for trail depiction
struct  cbuf {              // circular buffer structure
        int     top;        // index of the current element
        float   x[WLEN];    // array of x coordinates
        float   y[WLEN];    // array of y coordinates 
};
struct  cbuf    wake[N_BALLS];  // wake array

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// GLOBAL VARIABLES
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

// Constants
const   int     x_tc = (int) (XWIN - XTAB) / 2;             // x coord. of top_left corner of the table wrt screen [pixels]
const   int     y_tc = (int) (YWIN - YTAB) / 2;             // y coord. of top_left corner of the table wrt screen [pixels]
const   int     x_or = (int) (XWIN - XTAB) / 2 + BANK;      // x coord. of game field origin wrt screen [pixels]
const   int     y_or = (int) (YWIN - YTAB) / 2 + BANK;      // y coord. of game field origin wrt screen [pixels]

BITMAP  *GameTable;         // table bitmap on which ball's bitmaps will be pasted
BITMAP  *CleanTable;        // empty table bitmap to delete past bitmaps
BITMAP  *Win1;              // player 1 victory message bitmap
BITMAP  *Win2;              // player 2 victory message bitmap

// Semaphores (used for ball structure fields x and y)
pthread_mutex_t     mux;
pthread_mutexattr_t matt;

// Game parameters
float   theta = 0;          // shot inclination [rad]
float   v = V_MAX;          // velocity after the shot [m/s]
float   f = 0.02 ;          // table friction factor
float   dump = 0.9;         // bounds bouncing dumping factor
float   T_scale = 1;        // time scale factor

// Game flags
int     end = 0;            // task termination flag
int     trail_flag = 0;     // show trail flag
int     player_flag = 0;    // indicates whose the turn: 0 = player 1, 1 = player 2
int     phase_flag = 0;     // indicates the phase of the game: 0 = break, 1 = open game, 2 = standard game
int     type_flag = 0;      // indicates who gets the solid balls: 0 = still no one, 1 = player 1, 2 = player 2
int     fb_flag = 1;        // deactivates after first bump and reactivates at the next turn, useful for counters
int     dsp_flag = 1;       // determines if a phase switch has already happened
int     dst_flag = 1;       // determines if a turn switch has already happened
int     foul_flag = 0;      // indicates if a foul has occured: 0 = no, 1 = yes
int     en81_flag = 0;      // determines if player 1 can take a winning shot to 8 ball
int     en82_flag = 0;      // determines if player 2 can take a winning shot to 8 ball
int     win_flag = 0;       // determines who won the game: 1 = player 1, 2 = player 2
int     dec_hole = 0;       // indicates the declared hole from the player who's taking the winning shot
int     show_game = 1;      // stops displaying the game when the winning tab is shown

// Counters
int     n0sol = 0;          // counter of white to solid balls collisions
int     n0str = 0;          // counter of white to striped balls collisions
int     n08 = 0;            // counter of white to 8 ball collision
int     nbb = 0;            // counter of bounces on the bounds during the break phase
int     nhsol = 0;          // counter of solid balls pocketed
int     nhstr = 0;          // counter of striped balls pocketed
int     nf = 0;             // counter of fouls

// Conditions
int     cond1[N_BALLS];     // shot phase activation condition
int     cond2[N_BALLS];     // standard game phase activation condition

// Previous value store variables
int     prev_cond1 = 1;     // saves the previous turn condition in order to guarantee correct turn switch
int     prev_n0sol = 0;     // stores n0f
int     prev_n0str = 0;     // stores n0h
int     prev_n08 = 0;       // stores n08
int     prev_nbb = 0;       // stores nbb
int     prev_nhsol = 0;     // stores nhsol
int     prev_nhstr = 0;     // stores nhstr
int     prev_nf = 0;        // stores nf

       
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// FUNCTIONS DEFINITIONS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Return the pressed key
char    get_scancode()
{
    if (keypressed()) return readkey() >> 8;
    else return 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Returns ball type
int     get_type(void) 
{
int     i;  // ball index
int     type = - 1;

        for (i = 0; i < N_BALLS; i++) {
            if (!ball[i].active_flag && ball[i].el_ph == 1) type = ball[i].type;
        }

        return type;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Initialize balls status structure with defining parameters
void    init_balls(void)
{
int     i;          // ball index
float   a, r, f;    // these parameters define the relative position of the balls

        f = 1.1;    // defines the distance among balls keeping the formation
        a = f * sqrt(3) * DIAM/2;
        r = f * DIAM/2;

        // White ball
        ball[0].x = B0SX;
        ball[0].y = B0SY;
        ball[0].tcol = WHITE;
        ball[0].bm = load_bitmap("ball0.bmp", NULL);

        // Ball 1
        ball[1].x = B1SX;
        ball[1].y = B1SY;
        ball[1].tcol = YELLOW;
        ball[1].bm = load_bitmap("ball1.bmp", NULL);
        ball[1].xo = B1EX;
        ball[1].yo = B1EY;

        // Ball 2
        ball[2].x = B1SX + a;
        ball[2].y = B1SY + r;
        ball[2].tcol = BLUE;
        ball[2].bm = load_bitmap("ball2.bmp", NULL);
        ball[2].xo = B1EX;
        ball[2].yo = B1EY + 2*DIAM;

        // Ball 3
        ball[3].x = B1SX + 2*a;
        ball[3].y = B1SY + 2*r;
        ball[3].tcol = RED;
        ball[3].bm = load_bitmap("ball3.bmp", NULL);
        ball[3].xo = B1EX;
        ball[3].yo = B1EY + 4*DIAM;

        // Ball 4
        ball[4].x = B1SX + 3*a;
        ball[4].y = B1SY + 3*r;
        ball[4].tcol = PURPLE;
        ball[4].bm = load_bitmap("ball4.bmp", NULL);
        ball[4].xo = B1EX;
        ball[4].yo = B1EY + 6*DIAM;

        // Ball 5
        ball[5].x = B1SX + 4*a;
        ball[5].y = B1SY - 4*r;
        ball[5].tcol = ORANGE;
        ball[5].bm = load_bitmap("ball5.bmp", NULL);
        ball[5].xo = B1EX;
        ball[5].yo = B1EY + 8*DIAM;

        // Ball 6
        ball[6].x = B1SX + 4*a;
        ball[6].y = B1SY;
        ball[6].tcol = GREEN;
        ball[6].bm = load_bitmap("ball6.bmp", NULL);
        ball[6].xo = B1EX;
        ball[6].yo = B1EY + 10*DIAM;

        // Ball 7
        ball[7].x = B1SX + 3*a;
        ball[7].y = B1SY - r;
        ball[7].tcol = BROWN;
        ball[7].bm = load_bitmap("ball7.bmp", NULL);
        ball[7].xo = B1EX;
        ball[7].yo = B1EY + 12*DIAM;

        // Ball 8
        ball[8].x = B1SX + 2*a;
        ball[8].y = B1SY;
        ball[8].tcol = BLACK;
        ball[8].bm = load_bitmap("ball8.bmp", NULL);
        ball[8].xo = B1EX;
        ball[8].yo = B1EY + 14*DIAM;

        // Ball 9
        ball[9].x = B1SX + a;
        ball[9].y = B1SY - r;
        ball[9].tcol = YELLOW;
        ball[9].bm = load_bitmap("ball9.bmp", NULL);
        ball[9].xo = B1EX + 2*DIAM;
        ball[9].yo = B1EY;

        // Ball 10
        ball[10].x = B1SX + 2*a;
        ball[10].y = B1SY - 2*r;
        ball[10].tcol = BLUE;
        ball[10].bm = load_bitmap("ball10.bmp", NULL);
        ball[10].xo = B1EX + 2*DIAM;
        ball[10].yo = B1EY + 2*DIAM;

        // Ball 11
        ball[11].x = B1SX + 3*a;
        ball[11].y = B1SY - 3*r;
        ball[11].tcol = RED;
        ball[11].bm = load_bitmap("ball11.bmp", NULL);
        ball[11].xo = B1EX + 2*DIAM;
        ball[11].yo = B1EY + 4*DIAM;

        // Ball 12
        ball[12].x = B1SX + 4*a;
        ball[12].y = B1SY + 4*r;
        ball[12].tcol = PURPLE;
        ball[12].bm = load_bitmap("ball12.bmp", NULL);
        ball[12].xo = B1EX + 2*DIAM;
        ball[12].yo = B1EY + 6*DIAM;

        // Ball 13
        ball[13].x = B1SX + 4*a;
        ball[13].y = B1SY - 2*r;
        ball[13].tcol = ORANGE;
        ball[13].bm = load_bitmap("ball13.bmp", NULL);
        ball[13].xo = B1EX + 2*DIAM;
        ball[13].yo = B1EY + 8*DIAM;

        // Ball 14
        ball[14].x = B1SX + 3*a;
        ball[14].y = B1SY + r;
        ball[14].tcol = GREEN;
        ball[14].bm = load_bitmap("ball14.bmp", NULL);
        ball[14].xo = B1EX + 2*DIAM;
        ball[14].yo = B1EY + 10*DIAM;

        // Ball 15
        ball[15].x = B1SX + 4*a;
        ball[15].y = B1SY + 2*r;
        ball[15].tcol = BROWN;
        ball[15].bm = load_bitmap("ball15.bmp", NULL);
        ball[15].xo = B1EX + 2*DIAM;  
        ball[15].yo = B1EY + 12*DIAM;

        // All balls start still and active
        for (i = 0; i < N_BALLS; i++) {
            ball[i].vx = 0;
            ball[i].vy = 0;
            ball[i].active_flag = 1;
            wake[i].top = 0; // initialize wake
            
        }

        // Balls from 1 to 7 are solid
        for (i = 1; i < 8; i++) {
            ball[i].type = 0;
            ball[i].el_ph = - 1;
        }
        // Balls from 9 to 15 are striped
        for (i = 9; i < N_BALLS; i++) {
            ball[i].type = 1;
            ball[i].el_ph = - 1;
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Initialize holes position
void    init_holes(void)
{
    // upper left hole
    hole[0].x = - HC;
    hole[0].y = - HC;

    // upper middle hole
    hole[1].x = LX/2;
    hole[1].y = - HC;

    // upper right hole
    hole[2].x = LX + HC;
    hole[2].y = - HC;

    // lower right hole
    hole[3].x = LX + HC;
    hole[3].y = LY + HC;

    // lower middle hole
    hole[4].x = LX/2;
    hole[4].y = LY + HC;

    // lower left hole
    hole[5].x = - HC;
    hole[5].y = LY + HC;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Initialize game environment, Allegro settings and scheduling policy
void    init(void) 
{
        allegro_init();

        install_keyboard();

        install_mouse();
        
        set_color_depth(32);

        if (set_gfx_mode(GFX_AUTODETECT_WINDOWED, XWIN, YWIN, 0, 0) != 0) {
            allegro_message("Failed to initialize graphics mode!\n");
            exit(-1);
        }

        clear_to_color(screen, DARK_BLUE);

        show_mouse(screen);

        GameTable = load_bitmap("table.bmp", NULL);
        CleanTable = load_bitmap("table.bmp", NULL);

        Win1 = load_bitmap("P1W.bmp", NULL);
        Win2 = load_bitmap("P2W.bmp", NULL);

        draw_sprite(screen, GameTable, x_tc, y_tc);

        init_balls();

        init_holes();

        ptask_init(SCHED_FIFO);
    }

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Update i-th ball status
void    update_status(int i, float f, float dt)
{  
        // Euler integration for position-velocity
        ball[i].x += ball[i].vx * dt;
        ball[i].y += ball[i].vy * dt;

        // Apply friction factor (NOTE: dependent on integration step but not directly)
        ball[i].vx *= (1 - f); 
        ball[i].vy *= (1 - f);
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Handle bounces of i-th ball on the table borders considering the dumping factor
void    handle_bounce(int i, float dump)
{
float   vx, vy;             // temporary velocity data copy
float   RAD = DIAM / 2;     // ball radius

        // bounce on left border
        if ((ball[i].x < RAD) && ((ball[i].y > HP - RAD) && (ball[i].y < LY - HP + RAD))) { // bound check

            ball[i].x = RAD;
            ball[i].vx = - dump * ball[i].vx;

            if (phase_flag == 0) nbb++; // count the bounce on the bounds to check if the break has to be repeated
        }

        // bounce on right border
        if ((ball[i].x > LX - RAD) && ((ball[i].y > HP - RAD) && (ball[i].y < LY - HP + RAD))) { 

            ball[i].x = LX - RAD;
            ball[i].vx = - dump * ball[i].vx;

            if (phase_flag == 0) nbb++;
        }

        // bounce on upper border
        if ((ball[i].y < RAD) && (((ball[i].x > HP - RAD) && (ball[i].x < LX/2 - HP + RAD)) || ((ball[i].x > LX/2 + HP - RAD) && (ball[i].x < LX - HP + RAD)))) {

            ball[i].y = RAD;
            ball[i].vy = - dump * ball[i].vy;

            if (phase_flag == 0) nbb++;
        }

        // bounce on lower border
        if ((ball[i].y > LY - RAD) && (((ball[i].x > HP - RAD) && (ball[i].x < LX/2 - HP + RAD)) || ((ball[i].x > LX/2 + HP - RAD) && (ball[i].x < LX - HP + RAD)))) {

            ball[i].y = LY - RAD;
            ball[i].vy = - dump * ball[i].vy;

            if (phase_flag == 0) nbb++;
        }

        // For each of the corner holes there is a short tunnel that leads to the hole

        // bounce inside left-upper corner
        // right wall of the tunnel (ball perspective)
        if ((ball[i].y < RAD) && (ball[i].x < HP) && (ball[i].x > ball[i].y + HP - RAD)) {
            vx = dump * ball[i].vy;
            vy = dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // left wall of the tunnel (ball perspective)
        if ((ball[i].x < RAD) && (ball[i].y < HP) && (ball[i].x < ball[i].y - HP + RAD)) {
            vx = dump * ball[i].vy;
            vy = dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // bounce inside left-lower corner
        // right wall of the tunnel (ball perspective)
        if ((ball[i].x < RAD) && (ball[i].y > LY - HP) && (ball[i].x < - ball[i].y + LY - HP + RAD)) {
            vx = - dump * ball[i].vy;
            vy = - dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // left wall of the tunnel (ball perspective)
        if ((ball[i].y > LX - RAD) && (ball[i].x < HP) && (ball[i].x > - ball[i].y + LY + HP - RAD)) {
            vx = - dump * ball[i].vy;
            vy = - dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // bounce inside right-upper corner
        // right wall of the tunnel (ball perspective)
        if ((ball[i].x > LX - RAD) && (ball[i].y < HP) && (ball[i].x > - ball[i].y + LX + HP - RAD)) {
            vx = - dump * ball[i].vy;
            vy = - dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // left wall of the tunnel (ball perspective)
        if ((ball[i].y < RAD) && (ball[i].x > LX - HP) && (ball[i].x < - ball[i].y + LX - HP + RAD)) {
            vx = - dump * ball[i].vy;
            vy = - dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // bounce inside right-lower corner
        // right wall of the tunnel (ball perspective)
        if ((ball[i].y > LY - RAD) && (ball[i].x > LX - HP) && (ball[i].x < ball[i].y + LX - LY - HP + RAD)) {
            vx = dump * ball[i].vy;
            vy = dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }

        // left wall of the tunnel (ball perspective)
        if ((ball[i].x > LX - RAD) && (ball[i].y > LY - HP) && (ball[i].x > ball[i].y + LX -LY + HP - RAD)) {
            vx = dump * ball[i].vy;
            vy = dump * ball[i].vx;
            ball[i].vx = vx;
            ball[i].vy = vy;
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Handle all the cases of each ball being pocketed
void    handle_holes(void)
{
int     i, j;       // ball and hole indexes
float   d[N_HOLES]; // every element is the distance from each hole

        for (i = 0; i < N_BALLS; i++) {
            for (j = 0; j < N_HOLES; j++) {

                // Compute the distance between each ball centre and each hole centre
                d[j] = sqrt((ball[i].x - hole[j].x)*(ball[i].x - hole[j].x) + (ball[i].y - hole[j].y)*(ball[i].y - hole[j].y));

                if (d[j] < HP) { // if a ball ends up in a hole

                    // Increases counter of eliminated solid and striped balls
                    if (!ball[i].type && i != 0 && i != 8) nhsol++;
                    if (ball[i].type && i != 0 && i != 8)  nhstr++;
                    
                    // white ball pocketed
                    if (i == 0) {
                        // if the white ball is pocketed after the 8, the other player wins
                        if (!ball[8].active_flag) { 
                            if (!player_flag) win_flag = 2;
                            if (player_flag) win_flag = 1;
                        }
                        // In other cases it's a foul
                        else {
                            ball[0].active_flag = 0;
                            nf++;
                        }
                    }
                    // 8 ball pocketed: it causes the win of the other player in all cases but the one in which the winning shot belongs to one of the players
                    else if (i == 8) {
                        ball[8].active_flag = 0;
                        ball[8].x = ball[i].xo;
                        ball[8].y = ball[i].yo;
                        if (!player_flag && !en81_flag) win_flag = 2;   // if during player 1 turn the ball is pocketed but it's not due, player 2 wins
                        if (player_flag && !en82_flag) win_flag = 1;    // if during player 2 turn the ball is pocketed but it's not due, player 1 wins
                        if (!player_flag && en81_flag) {                // if during player 1 turn the ball is pocketed and it's a winning shot
                            if (j == dec_hole) win_flag = 1;  // if it's pocketed in the declared hole player 1 wins
                            else               win_flag = 2;  // otherwise player 2 wins
                        }
                        if (player_flag && en82_flag) {                 // if during player 2 turn the ball is pocketed and it's a winning shot
                            if (j == dec_hole) win_flag = 2;  // if it's pocketed in the declared hole player 2 wins
                            else               win_flag = 1;  // otherwise player 1 wins
                        }
                    }
                    // in other cases the ball is deactivated and put outside the field
                    else {
                        ball[i].active_flag = 0;
                        ball[i].x = ball[i].xo;
                        ball[i].y = ball[i].yo;
                        ball[i].el_ph = phase_flag; // this is useful for type assignment
                    }                    
                }
            }
        }   
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Handle collisions between balls and update counters of collisions between certain balls
void    handle_collision(int i, int j, float dump)
{
float   xi, yi;             // coordinates of i-th ball position
float   xj, yj;             // coordinates of j-th ball position
float   vxi, vyi;           // velocity of i-th ball before collision
float   vxj, vyj;           // velocity of j-th ball before collision
float   d;                  // distance between balls
float   e;                  // ball intersection
float   nx, ny;             // normal versor components
float   tx, ty;             // tangent versor components
float   vni, vti;           // velocity module on normal and tangential direction of i-th ball before collision 
float   vnj, vtj;           // velocity module on normal and tangential direction of j-th ball before collision 
float   vni_new, vnj_new;   // normal velocities after collision
float   A;                  // this element prevents to do a square root of a negative number

        xi = ball[i].x;
        yi = ball[i].y;

        xj = ball[j].x;
        yj = ball[j].y;

        d = sqrt((xi - xj)*(xi - xj) + (yi - yj)*(yi - yj));

        if (d < DIAM) {
            
            // the counters increase just with the first bump, after that the counter is disabled
            if (fb_flag) {
                if (i == 0 && !ball[j].type && j != 8) {
                    n0sol++; // white touches solid
                    fb_flag = 0;
                }
                if (i == 0 && ball[j].type && j != 8) {
                    n0str++; // white touches striped
                    fb_flag = 0;
                }
                if (i == 0 && j == 8) {
                    n08++; // white touches 8
                    fb_flag = 0;
                }
            }
            // versors definition
            nx = (xi - xj) / d;
            ny = (yi - yj) / d;

            tx = - ny;
            ty = nx;

            e = DIAM - d;

            // Solve compenetration
            ball[i].x += (e/2) * nx;
            ball[i].y += (e/2) * ny;

            ball[j].x -= (e/2) * nx;
            ball[j].y -= (e/2) * ny;

            // Solve partially anelastic collision
            vxi = ball[i].vx;
            vyi = ball[i].vy;

            vxj = ball[j].vx;
            vyj = ball[j].vy;

            vni = vxi * nx + vyi * ny;
            vnj = vxj * nx + vyj * ny;

            vti = vxi * tx + vyi * ty;
            vtj = vxj * tx + vyj * ty;

            if ((2*dump*dump - 1)*(vni*vni + vnj*vnj) - 2*vni*vnj > 0) {
                A = 0.5*sqrt((2*dump*dump - 1)*(vni*vni + vnj*vnj) - 2*vni*vnj);
            }
            else A = 0;

            vni_new = 0.5*(vni + vnj) + A;
            vnj_new = 0.5*(vni + vnj) - A;

            ball[i].vx = vni_new * nx + vti * tx;
            ball[i].vy = vni_new * ny + vti * ty;

            ball[j].vx = vnj_new * nx + vtj * tx;
            ball[j].vy = vnj_new * ny + vtj * ty;
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Position the white ball after a foul
void    foul(void)
{
float   x_m, y_m;    // mouse position in the field

        foul_flag = 1;

        if (ball[0].active_flag) ball[0].active_flag  = 0; // deactivate white ball

        if (player_flag) player_flag = 0;
        else             player_flag = 1;

        do { // move the mouse to choose the position

            x_m = (((float) mouse_x - x_or) / CF);
            y_m = (((float) mouse_y - y_or) / CF);

            if (mouse_x > x_tc + BANK && mouse_x < x_tc + XTAB  - BANK && mouse_y > y_tc + BANK && mouse_y < y_tc + YTAB - BANK)
                circle(screen, mouse_x, mouse_y, DIAM_P/2, RED);

            if (x_m > 0 && x_m < LX) ball[0].x = x_m;
            if (y_m > 0 && y_m < LY) ball[0].y = y_m;

        } while (!key[KEY_TAB]); // press TAB to confirm the position

        ball[0].active_flag = 1; // reactivate the ball and put velocities to 0
        ball[0].vx = 0;
        ball[0].vy = 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Decree the winner and give the possibility to restart the game
void    endgame(void) 
{
int     i;  // ball index

        do {
            if (win_flag == 1) draw_sprite(screen, Win1, x_tc, y_tc);
            if (win_flag == 2) draw_sprite(screen, Win2, x_tc, y_tc);
        } while (!key[KEY_ENTER]);

        // In case of restart 
        rectfill(screen, x_tc, y_tc, x_tc + XTAB, y_tc + YTAB, DARK_BLUE);
        rectfill(screen, x_tc + XTAB + 1, y_tc, XWIN, y_tc + YTAB, DARK_BLUE);

        draw_sprite(screen, GameTable, x_tc, y_tc);

        init_balls();

        v = V_MAX;
        theta = 0;

        phase_flag = 0;
        type_flag = 0;
        fb_flag = 1;
        dsp_flag = 1;
        dst_flag = 1;
        foul_flag = 0;
        en81_flag = 0;
        en82_flag = 0;
        win_flag = 0;
        show_game = 1;

        n0sol = 0;
        n0str = 0;
        n08 = 0;
        nbb = 0;
        nhsol = 0;
        nhstr = 0;
        nf = 0;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Store the wake of the i-th ball
void    store_wake(int i)
{
int     k;  

        k = wake[i].top;           // new element
        k = (k + 1) % WLEN;        // put on top of wake
        wake[i].x[k] = ball[i].x;
        wake[i].y[k] = ball[i].y;
        wake[i].top = k;
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// DRAWING FUNCTIONS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

// Draw i-th ball trail with w past values
void    draw_trail(int i, int w, int tcol)
{
int     j, k;   // wake indexes
int     x, y;   // graphics coordinates

        for (j = 0; j < w; j++) {
            k = (wake[i].top + WLEN -j) % WLEN;
            x = BANK + CF * wake[i].x[k];
            y = BANK + CF * wake[i].y[k];
            putpixel(GameTable, x, y, tcol);
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Draw the ball
void    draw_ball(int i, float xm, float ym, BITMAP* btm)
{
int     x, y;   // coordinates of the ball (wrt to table)in pixels
        
            if (ball[i].active_flag || (i = 0 && !ball[i].active_flag)) { // the white ball and other active balls have to be pasted on the table bitmap
                x = (int) (BANK + (CF * xm) - DIAM_P/2);
                y = (int) (BANK + (CF * ym) - DIAM_P/2);
                draw_sprite(GameTable, btm, x, y);
            }
            else { // eliminated balls have to be pasted on the screen
                x = (int) (x_or + (CF * xm) - DIAM_P/2);
                y = (int) (y_or + (CF * ym) - DIAM_P/2);
                draw_sprite(screen, btm, x, y);
            }    
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Draw the trajectory of white ball during the shot and a little circle of the size of the ball to show what it is going to hit first
void    draw_traj(float theta, float xw, float yw, float xi[], float yi[])
{
int     x0, y0;     // coordinates of the white ball [pixels]
int     x, y;       // iterative coordinates for trajectory line [pixels]
float   xf, yf;     // iterative coordinates for trajectory line [m]
int     k;          // iteration index for trajectory
int     i;          // ball index
float   d[N_BALLS]; // distance between white ball and other balls [pixels]
int     cond;       // ball collision condition

        x0 = (int) (BANK + (CF * xw));
        y0 = (int) (BANK + (CF * yw));

        for (k = 1; k < XTAB; k++) {
            x = x0 + k * cos(theta);
            y = y0 + k * sin(theta);

            xf = (float) (x - BANK)/CF;
            yf = (float) (y - BANK)/CF;

            cond = 0;

            for (i = 1; i < N_BALLS; i++) {

                d[i] = sqrt((xi[i] - xf)*(xi[i] - xf) + (yi[i] - yf)*(yi[i] - yf));

                if (d[i] < DIAM) cond = 1;
            }

            if (xf > 0 && yf > 0 && xf < LX && yf < LY) {

                putpixel(GameTable, x, y, WHITE);

                if ((xf < DIAM/2 || yf < DIAM/2 || (LX - xf) < DIAM/2 || (LY - yf) < DIAM/2) || cond) {

                        circle(GameTable, x, y, DIAM_P/2, WHITE);
                        break;
                } 
            }
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Draw the shot power indicator with different colors for various powers
void    draw_pow_ind(float v)
{
int     ind;
char    s[20];

        sprintf(s, "v = %4.2f", v);
        textout_centre_ex(screen, font, "SHOT POWER", 56, 483 - IN_HEI - 25, WHITE, -1);
        textout_centre_ex(screen, font, s, 56, 483 - IN_HEI - 15, DARK_BLUE, WHITE);
        rectfill(screen, 31, 484, 31 + IN_WID, 484 - IN_HEI, BLACK);
        rect(screen, 30, 485, 32 + IN_WID, 483 - IN_HEI, WHITE);

        ind = (int) (IN_HEI * (v / V_MAX));

        if (v < 0.25 * V_MAX) rectfill(screen, 31, 484, 31 + IN_WID, 484 - ind, GREEN);
        else if (v >= 0.25 * V_MAX && v < 0.5 * V_MAX) rectfill(screen, 31, 484, 31 + IN_WID, 484 - ind, YELLOW);
        else if (v >= 0.5 * V_MAX && v < 0.75 * V_MAX) rectfill(screen, 31, 484, 31 + IN_WID, 484 - ind, ORANGE);
        else if (v >= 0.75 * V_MAX) rectfill(screen, 31, 484, 31 + IN_WID, 484 - ind, RED);
}
        
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Draw parameters indicators
void    draw_par_ind(float f, float dump, float T_scale)
{
int     ind_f;
int     ind_d;
int     ind_t;
int     a;

char    sf[25];
char    sd[25];
char    st[30];

        ind_f = (int) 200 * (f / F_MAX);
        ind_d = (int) 200 * (dump / DUMP_MAX);
        ind_t = (int) 200 * (T_scale / TS_MAX);

        a = 10;

        sprintf(sf, "friction factor = %5.3f", f);
        textout_ex(screen, font, sf, 20, (600 + 13 - 10) + a, DARK_BLUE, WHITE);
        rectfill(screen, 20, (600 + 13) + a, (20 + 200), (600 + 8 + 30) + a, BLACK);
        rect(screen, (20 - 1), (600 + 13 - 1) + a, (20 + 200 + 1), (600 + 8 + 30 + 1) + a, WHITE);
        rectfill(screen, 20, (600 + 13) + a, (20 + ind_f), (600 + 8 + 30) + a, CYAN);

        sprintf(sd, "dumping factor = %4.2f", dump);
        textout_ex(screen, font, sd, 20, (656 + 13 - 10) + a, DARK_BLUE, WHITE);
        rectfill(screen, 20, (656 + 13) + a, (20 + 200), (656 + 8 + 30) + a, BLACK);
        rect(screen, (20 - 1), (656 + 13 - 1) + a, (20 + 200 + 1), (656 + 8 + 30 + 1) + a, WHITE);
        rectfill(screen, 20, (656 + 13) + a, (20 + ind_d), (656 + 8 + 30) + a, CYAN);

        sprintf(st, "time scale factor = %4.2f", T_scale);
        textout_ex(screen, font, st, 20, (712 + 13 - 10) + a, DARK_BLUE, WHITE);
        rectfill(screen, 20, (712 + 13) + a, (20 + 200), (712 + 8 + 30) + a, BLACK);
        rect(screen, (20 - 1), (712 + 13 - 1) + a, (20 + 200 + 1), (712 + 8 + 30 + 1) + a, WHITE);
        rectfill(screen, 20, (712 + 13) + a, (20 + ind_t), (712 + 8 + 30) + a, CYAN);

}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Indicates which hole the player wants to declare for the winning shot
void    draw_dec_hole_ind(int dec_hole)
{
int     x, y, r;

        x = (int) x_or + CF * hole[dec_hole].x;
        y = (int) y_or + CF * hole[dec_hole].y;

        circlefill(screen, x, y, 5, RED);
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// TASK FUNCTIONS DEFINITIONS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

// Impose status update
void*   ball_task(void* arg) 
{
int     a;         // task index
int     i, j;      // ball index
float   dt;        // integration step

        a = get_task_index(arg);

        wait_for_activation(a);

        while (!end) {

            dt = T_scale*(float)task_period(a)/1000;

            pthread_mutex_lock(&mux);

            handle_holes();

            for (i = 0; i < N_BALLS; i++) {

                if (ball[i].active_flag) {

                    update_status(i, f, dt);

                    if (trail_flag) store_wake(i);

                    handle_bounce(i, dump);

                    for (j = 0; j < N_BALLS; j++) {
                        if (j != i && ball[j].active_flag) {
                            handle_collision(i, j, dump);
                        }
                    }
                }
            }

            pthread_mutex_unlock(&mux);

            deadline_miss(a);

            wait_for_period(a);
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Select shot direction and strenght (by imposing starting velocity)
void*   shot_task(void* arg) 
{
int     a;                   // task index
int     i;                   // ball index
float   x_m, y_m;            // mouse position on the field [m]
float   Delta_x, Delta_y;    // difference from mouse and white ball position on the field [m]
char    scan;                // indicate the pressed key

        a = get_task_index(arg);

        wait_for_activation(a);
        
        while (!end) {

            scan = get_scancode();

            if (ball[0].active_flag && cond1[N_BALLS - 1]) { // if the game is still

                // Press spacebar to shoot
                if (scan == KEY_SPACE) {
                    ball[0].vx = v * cos(theta);
                    ball[0].vy = v * sin(theta);
                }

                // When the mouse wheel is pressed the mouse will direct the shot
                if (mouse_b & 4) {     

                    x_m = (((float) mouse_x - x_or) / CF);
                    y_m = (((float) mouse_y - y_or) / CF);
                    Delta_x = x_m - ball[0].x;
                    Delta_y = y_m - ball[0].y;

                    theta = atan2(Delta_y, Delta_x);
                }

                // Press left button to increase power
                if (mouse_b & 1) {      
                    if (v < V_MAX) v += D_VEL;
                }

                // Press right button to decrease power
                if (mouse_b & 2) {
                    if (v > D_VEL) v -= D_VEL;
                }
            

             deadline_miss(a);
            }

            wait_for_period(a);
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Impose selection of friction and damping coefficient and integration step
void*   set_param_task(void* arg)
{
int     a;      // task index
char    scan;   // indicate key pressed

        a = get_task_index(arg);

        wait_for_activation(a);

        while (!end) {

            if (cond1[N_BALLS - 1]) { // if the game is still

                scan = get_scancode();

                switch (scan) {

                    case KEY_T: // press T to show or hide trail for all balls 
                        if (trail_flag) trail_flag = 0;
                        else            trail_flag = 1;
                        break;


                    case KEY_Q:
                        if (f < F_MAX) f += D_F;
                        break;

                    case KEY_A:
                        if (f > D_F) f -= D_F;
                        break;

                    case KEY_W:
                        if (dump < DUMP_MAX) dump += D_DUMP;
                        break;

                    case KEY_S:
                        if (dump > D_DUMP) dump -= D_DUMP;
                        break;

                    case KEY_E:
                        if (T_scale < TS_MAX) T_scale += D_TS;
                        break;

                    case KEY_D:
                        if (T_scale > D_TS) T_scale -= D_TS;
                        break;

                    default: break;
                }

                deadline_miss(a);
            }

            wait_for_period(a);
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// Manage the players turn and the phases of the game
void*   manage_task(void* arg)
{
int     a;              // task index
int     i;              // ball index
int     j;              // hole index
float   thres;          // velocity threshold
char    scan, scan1;    // gets pressed key
int     t;              // this element helps assign the ball type to a player

        a = get_task_index(arg);

        wait_for_activation(a);

        thres = 1e-3;

        while (!end) {

            scan = get_scancode();

            /**************TO USE IN TEST PHASE ONLY*********************/
            // Eliminate balls manually (do it only coherently with game development to avoid unexpected behaviour)
            if (scan == KEY_1) {
                if (ball[1].active_flag) {
                    ball[1].active_flag = 0;
                    ball[1].x = ball[1].xo;
                    ball[1].y = ball[1].yo;
                    ball[1].el_ph = phase_flag;
                    nhsol++; 
                    }
            }
            if (scan == KEY_2) {
                if (ball[2].active_flag) {
                    ball[2].active_flag = 0;
                    ball[2].x = ball[2].xo;
                    ball[2].y = ball[2].yo;
                    ball[2].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_3) {
                if (ball[3].active_flag) {
                    ball[3].active_flag = 0;
                    ball[3].x = ball[3].xo;
                    ball[3].y = ball[3].yo;
                    ball[3].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_4) {
                if (ball[4].active_flag) {
                    ball[4].active_flag = 0;
                    ball[4].x = ball[4].xo;
                    ball[4].y = ball[4].yo;
                    ball[4].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_5) {
                if (ball[5].active_flag) {
                    ball[5].active_flag = 0;
                    ball[5].x = ball[5].xo;
                    ball[5].y = ball[5].yo;
                    ball[5].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_6) {
                if (ball[6].active_flag) {
                    ball[6].active_flag = 0;
                    ball[6].x = ball[6].xo;
                    ball[6].y = ball[6].yo;
                    ball[6].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_7) {
                if (ball[7].active_flag) {
                    ball[7].active_flag = 0;
                    ball[7].x = ball[7].xo;
                    ball[7].y = ball[7].yo;
                    ball[7].el_ph = phase_flag;
                    nhsol++;
                }
            }
            if (scan == KEY_9) {
                if (ball[9].active_flag) {
                    ball[9].active_flag = 0;
                    ball[9].x = ball[9].xo;
                    ball[9].y = ball[9].yo;
                    ball[9].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_0) {
                if (ball[10].active_flag) {
                    ball[10].active_flag = 0;
                    ball[10].x = ball[10].xo;
                    ball[10].y = ball[10].yo;
                    ball[10].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_P) {
                if (ball[11].active_flag) {
                    ball[11].active_flag = 0;
                    ball[11].x = ball[11].xo;
                    ball[11].y = ball[11].yo;
                    ball[11].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_O) {
                if (ball[12].active_flag) {
                    ball[12].active_flag = 0;
                    ball[12].x = ball[12].xo;
                    ball[12].y = ball[12].yo;
                    ball[12].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_L) {
                if (ball[13].active_flag) {
                    ball[13].active_flag = 0;
                    ball[13].x = ball[13].xo;
                    ball[13].y = ball[13].yo;
                    ball[13].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_K) {
                if (ball[14].active_flag) {
                    ball[14].active_flag = 0;
                    ball[14].x = ball[14].xo;
                    ball[14].y = ball[14].yo;
                    ball[14].el_ph = phase_flag;
                    nhstr++;
                }
            }
            if (scan == KEY_M) {
                if (ball[15].active_flag) {
                    ball[15].active_flag = 0;
                    ball[15].x = ball[15].xo;
                    ball[15].y = ball[15].yo;
                    ball[15].el_ph = phase_flag;
                    nhstr++;
                }
            }
            /***********************************************************/
        
            // All balls have to be still (enough) to switch the turn
            for (i = 0; i < N_BALLS; i++) {
            
                if (ball[i].active_flag) {
                    if (fabs(ball[i].vx) < thres && fabs(ball[i].vy) < thres) cond1[i] = 1;
                    else cond1[i] = 0;
                }
                else cond1[i] = 1;

                if (i != 0) cond1[i] = cond1[i] * cond1[i - 1];
            }

            // Check if the ball is still and then enables shot and parameters change tasks
            if (cond1[N_BALLS - 1]) {

                unscare_mouse();

                // PHASE SWITCHING

                // Confront with previous value allows to switch the turn at when all the balls stop moving again
                if (cond1[N_BALLS - 1] != prev_cond1) {

                    fb_flag = 1; // enable the first bump counter

                    /***FOULS***/

                    // Check if a foul occured (open and standard game phases)

                    if (phase_flag == 1 || phase_flag == 2) {
                        // Deals with no ball being touched 
                        if (!en81_flag && !en82_flag) {
                            if (!((n0sol + n0str) > (prev_n0sol + prev_n0str))) nf++;
                        }
                        else if (en81_flag && !en82_flag) {
                            if (!player_flag && !(n08 > prev_n08) ||
                                player_flag && !((n0sol + n0str) > (prev_n0sol + prev_n0str))) nf++;
                        }
                        else if (!en81_flag && en82_flag) {
                            if (player_flag && !(n08 > prev_n08) ||
                                !player_flag && !((n0sol + n0str) > (prev_n0sol + prev_n0str))) nf++;
                        }
                        else if (en81_flag && en82_flag) {
                            if (!(n08 > prev_n08)) nf++;
                        }
                    }

                    
                    // Check if all the balls of the belonging type have been eliminated, in that case enable the possibility to hit the 8 ball
                    if (phase_flag == 2) {
                        
                        if (type_flag == 1) { // 1 solid, 2 striped

                            if (nhsol == 7) en81_flag = 1;
                            if (nhstr == 7) en82_flag = 1;
                        }

                        if (type_flag == 2) { // 1 striped, 2 solid

                            if (nhsol == 7) en82_flag = 1;
                            if (nhstr == 7) en81_flag = 1;
                        }
                    }                  

                    // Check if a foul occured (standard game phase)
                    if (phase_flag == 2) {
                        // Deals with each player touching the wrong ball
                        if (((!player_flag) && (type_flag == 1) && (n0str > prev_n0str)) || // player 1 has solids but hits a striped
                            ((!player_flag) && (type_flag == 2) && (n0sol > prev_n0sol)) || // player 1 has stripeds but hits a solid
                            ((player_flag)  && (type_flag == 2) && (n0str > prev_n0str)) || // player 2 has solids but hits a striped
                            ((player_flag)  && (type_flag == 1) && (n0sol > prev_n0sol))    // player 2 has stripeds but hits a solid
                        ) nf++;
                    }

                    if (nf > prev_nf && !win_flag) foul(); // Activates the foul if any occured

                    // All of the following happens if no foul occured
                    if (!foul_flag) {

                        // Assignes the type of the ball
                        if (type_flag == 0 && phase_flag == 1) {

                                t = get_type();

                                if (t != - 1) {

                                    if (!t && !player_flag)  type_flag = 1;  // solids to player 1
                                    if (t  && !player_flag)  type_flag = 2;  // stripeds to player 1
                                    if (!t &&  player_flag)  type_flag = 2;  // solids to player 2
                                    if (t  &&  player_flag)  type_flag = 1;  // stripeds to player 2
                                }
                        }

                        if (dsp_flag) {
                            
                            // Condition that searches for the first deactivate ball to move on to standard game phase
                            if (phase_flag == 1) {
                                
                                for (i = 0; i < N_BALLS; i++) {
                                
                                    if (ball[i].active_flag) cond2[i] = 1;
                                    else if (!ball[i].active_flag && ball[i].el_ph != 1) cond2[i] = 1;
                                    else cond2[i] = 0;

                                    if (i != 0) cond2[i] = cond2[i] * cond2[i - 1];
                                }

                                if (!cond2[N_BALLS - 1]) phase_flag = 2;

                                dsp_flag = 0;
                            }

                            // In the break phase at least 4 balls have to touch the bounds, otherwise it has to be repeated
                            if (phase_flag == 0) {
                                if (nbb >= 4) phase_flag = 1;
                                else {
                                    init_balls();
                                    nbb = 0;
                                }
                                dsp_flag = 0;
                            }

                        }
                        
                        /***TURN KEEPING OR SWITCHING***/

                        if (dst_flag) {

                            // In standard game the player eliminates its assigned type of ball keeps playing
                            if (phase_flag == 2) {
                                
                                if (!player_flag) { // player 1
                                    if ((type_flag == 1 && !(nhsol > prev_nhsol)) ||
                                        (type_flag == 2 && !(nhstr > prev_nhstr))) 
                                        player_flag = 1; 
                                }
                                else { // player 2
                                    if ((type_flag == 2 && !(nhsol > prev_nhsol)) ||
                                        (type_flag == 1 && !(nhstr > prev_nhstr))) 
                                        player_flag = 0;
                                }

                                dst_flag = 0;
                            }

                            // In break phase if the player eliminates any ball keeps playing
                            if (phase_flag == 0 || phase_flag == 1) {

                                if (!((nhsol + nhstr) > (prev_nhsol + prev_nhstr))) {
                                    if (player_flag) player_flag = 0;
                                    else             player_flag = 1;
                                }
                                dst_flag = 0;
                            }
                        }
                    }

                    // If the 8 ball is eliminated the winner has to be decreed
                    if (!ball[8].active_flag) {
                        show_game = 0;
                        endgame();
                        
                    }

                    /***HOLE DECLARATION***/

                    // When either of the player has to take a winning shot the hole in which he wants to pocket the ball
                    if ((en81_flag && !player_flag) || (en82_flag && player_flag)) {

                        while (!key[KEY_ENTER]) {

                            scan1 = get_scancode();

                            switch (scan1 ) {

                                case KEY_RIGHT:
                                    if (dec_hole < (N_HOLES - 1)) dec_hole++;
                                    else dec_hole = 0;
                                    break;

                                case KEY_LEFT:
                                    if (dec_hole > 0) dec_hole--;
                                    else dec_hole = 5;
                                    break;
                                
                                default: break;
                            }

                            draw_dec_hole_ind(dec_hole);
                        }
                    }

                    // Previous value storage
                    prev_n0sol = n0sol;
                    prev_n0str = n0str;
                    prev_n08 = n08;
                    prev_nhsol = nhsol;
                    prev_nhstr = nhstr;
                    prev_nf = nf;
                }
            }
            else { // when the balls are moving we can't see the shot power indicator

                scare_mouse();

                rectfill(screen, 0, 490, 111, 168, DARK_BLUE); // cover shot power indicator

                dsp_flag = 1;
                dst_flag = 1;
                foul_flag = 0;
            }

            prev_cond1 = cond1[N_BALLS - 1]; // stores condition

            deadline_miss(a);
                
            wait_for_period(a); 
        }
}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// DYSPLAY TASK FUNCTIONS
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

// Display ball on screen and restore background when it changes position
void*   display_task(void* arg)
{
int     a;      // task index
int     i;      // ball index
float   x[N_BALLS], y[N_BALLS]; // copy variables

        a = get_task_index(arg);

        wait_for_activation(a);

        while (!end) {

            pthread_mutex_lock(&mux);
            for (i = 0; i < N_BALLS; i++) {
                x[i] = ball[i].x;
                y[i] = ball[i].y;
            }
            pthread_mutex_unlock(&mux);

            if (show_game) {
                
                draw_sprite(GameTable, CleanTable, 0, 0); // draw table to delete old bitmaps

                for (i = 0; i < N_BALLS; i++) { // Draw all bitmaps and trail on table bitmap
                    if (trail_flag && ball[i].active_flag) draw_trail(i, WLEN, ball[i].tcol);
                    draw_ball(i, x[i], y[i], ball[i].bm);
                }

                // Display white ball trajectory for shot and shot power indicator
                if (ball[0].active_flag && cond1[N_BALLS - 1]) {
                    draw_traj(theta, x[0], y[0], x, y);
                    draw_pow_ind(v);
                }

                draw_sprite(screen, GameTable, x_tc, y_tc); // paste table on screen

                draw_par_ind(f, dump, T_scale); // draw parameters indicator

                // Shows whose the turn and, if determined, which type of balls belong to who
                if (!player_flag) {

                    rectfill(screen, 10, 520, x_tc - 10, 520 + 40, BLUE);
                    rect(screen, 10, 519, x_tc - 10, 520 + 41, WHITE);
                    textout_centre_ex(screen, font, "PLAYER 1", x_tc/2 , 530, WHITE, - 1);

                    if (type_flag == 1) textout_centre_ex(screen, font, "solid", x_tc/2 , 550, WHITE, - 1);
                    if (type_flag == 2) textout_centre_ex(screen, font, "striped", x_tc/2 , 550, WHITE, - 1);
                }
                else {
    
                    rectfill(screen, 10, 520, x_tc - 10, 520 + 40, RED);
                    rect(screen, 10, 519, x_tc - 10, 520 + 41, WHITE);
                    textout_centre_ex(screen, font, "PLAYER 2", x_tc/2, 530, WHITE, - 1);

                    if (type_flag == 2) textout_centre_ex(screen, font, "solid", x_tc/2 , 550, WHITE, - 1);
                    if (type_flag == 1) textout_centre_ex(screen, font, "striped", x_tc/2 , 550, WHITE, - 1);
                }

                // Show if the ball trail is being displayed or not
                rectfill(screen, 5, 580, x_tc - 5, 580 + 20, BLACK);
                rect(screen, 5, 580, x_tc - 5, 580 + 20, WHITE);
                if (trail_flag) textout_centre_ex(screen, font, "trail = ON", x_tc/2, 587, WHITE, - 1);
                else            textout_centre_ex(screen, font, "trail = OFF", x_tc/2, 587, WHITE, - 1);
            }

            deadline_miss(a);

            wait_for_period(a);
        }

}

/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// MAIN
/*--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

int main() 
{
int     i;          // ball index
int     a, b;       // parameters for graphic

char    s0[30];     // ball task deadline misses string
char    s1[30];     // shot task deadline misses string
char    s2[30];     // display task deadline misses string
char    s3[30];     // set param task deadline misses string
char    s4[30];     // manage task deadline misses string

        init();     // initialize game

        // Initialize semaphore
        pthread_mutex_init(&mux, NULL);

        // Create tasks
        task_create(ball_task, 0, PER, PER, 90, ACT);

        task_create(shot_task, 1, 20, 20, 70, ACT);
            
        task_create(display_task, 2, 50, 50, 50, ACT);

        task_create(set_param_task, 3, 20, 20, 60, ACT);

        task_create(manage_task, 4, 200, 200, 80, ACT);

        while (!key[KEY_ESC]) { // Press esc to exit
            
            rect(screen, 10, 10, XWIN - 10, y_tc - 10, WHITE);

            textout_ex(screen, font,
            "SHOT: Move the mouse while pressing the wheel to direct, left button to increase power, right to decrease, SPACE to shoot",
            30, 30, WHITE, - 1);

            textout_ex(screen, font,
            "- If a foul occurs move the mouse on the field to move the ball on the field and press TAB to position",
            30, 55, WHITE, - 1);

            textout_ex(screen, font,
            "- Before a shot for the win change the declared hole with LEFT or RIGHT arrow and choose one with ENTER",
            30, 80, WHITE, - 1);

            textout_ex(screen, font,
            "- Press T to activate or deactivate balls trail display",
            30, 105, WHITE, - 1);

            textout_ex(screen, font,
            "- Press ESC in any moment to turn the game off",
            30, 130, WHITE, - 1);

            sprintf(s0, "ball task = %3.1d", task_dmiss(0));
            sprintf(s1, "shot task = %3.1d", task_dmiss(1));
            sprintf(s2, "display task = %3.1d", task_dmiss(2));
            sprintf(s3, "set param task = %3.1d", task_dmiss(3));
            sprintf(s4, "manage task = %3.1d", task_dmiss(4));

            a = 820;
            b = 613;

            textout_ex(screen, font, "Task deadline misses:", a, b, RED, YELLOW);
            textout_ex(screen, font, s0, a, b + 20, RED, YELLOW);
            textout_ex(screen, font, s1, a, b + 40, RED, YELLOW);
            textout_ex(screen, font, s2, a, b + 60, RED, YELLOW);
            textout_ex(screen, font, s3, a, b + 80, RED, YELLOW);
            textout_ex(screen, font, s4, a, b + 100, RED, YELLOW);

            textout_ex(screen, font,
            "Press Q to increase friction, A to decrease",
            240, 633, WHITE, - 1);

            textout_ex(screen, font,
            "Press W to increase dumping factor (- loss), S to decrease (+ loss)",
            240, 633 + 56, WHITE, - 1);

            textout_ex(screen, font,
            "Press E to increase time scale factor, D to decrease",
            240, 633 + 56 + 56, WHITE, - 1);

        }

        // Free memory and cleanup
        destroy_bitmap(GameTable);
        for (i = 0; i < N_BALLS; i++) {
            if (ball[i].bm) {
                destroy_bitmap(ball[i].bm);
            }
        }
        allegro_exit();
        return 0;
}