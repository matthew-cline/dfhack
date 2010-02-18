#include <integers.h>
#include <string.h> // for memset
#include <string>
#include <iostream>
#include <vector>
#include <map>
using namespace std;

#include <DFTypes.h>
#include <DFTileTypes.h>
#include <DFHackAPI.h>
#include <DFProcess.h>
using namespace DFHack;
#include <curses.h>
#include <stdlib.h>
#include <signal.h>

string error;

static void finish(int sig);

int gotoxy(int x, int y)
{
    wmove(stdscr, y , x );
    return 0;
}

int putch(int x, int y, int znak, int color)
{
    attron(COLOR_PAIR(color));
    mvwaddch(stdscr, y, x, znak);
    attroff(COLOR_PAIR(color));
}
/*
    enum TileClass
    {
        EMPTY,
        
        WALL,
        PILLAR,
        FORTIFICATION,
        
        STAIR_UP,
        STAIR_DOWN,
        STAIR_UPDOWN,
        
        RAMP,
        
        FLOOR,
        TREE_DEAD,
        TREE_OK,
        SAPLING_DEAD,
        SAPLING_OK,
        SHRUB_DEAD,
        SHRUB_OK,
        BOULDER,
        PEBBLES
    };*/

int puttile(int x, int y, int tiletype, int color)
{
    unsigned int znak;
    switch(tileTypeTable[tiletype].c)
    {
        case EMPTY:
            znak = ' ';
            break;
        case WALL:
        case FORTIFICATION:
            znak = '#';
            break;
        case PILLAR:
            znak = 'O';
            break;
        case STAIR_DOWN:
            znak = '>';
            break;
        case STAIR_UP:
            znak = '<';
            break;
        case STAIR_UPDOWN:
            znak = '=';
            break;
        case RAMP:
            znak = '^';
            break;
        case FLOOR:
            znak = '.';
            break;
        case TREE_DEAD:
        case TREE_OK:
            znak= 'Y';
            break;
        case SAPLING_DEAD:
        case SAPLING_OK:
            znak= 'i';
            break;
        case SHRUB_DEAD:
        case SHRUB_OK:
            znak= 'o';
            break;
        case BOULDER:
        case PEBBLES:
            znak= '*';
            break;
    }
    attron(COLOR_PAIR(color));
    mvwaddch(stdscr, y, x, znak);
    attroff(COLOR_PAIR(color));
}

int cprintf(char *fmt, ...)
{
    va_list ap; 
    va_start(ap, fmt);
    int i = vwprintw(stdscr,fmt, ap);
    va_end(ap);
    return i;
}

void clrscr()
{
    wbkgd(stdscr, COLOR_PAIR(COLOR_BLACK));
    wclear(stdscr);
}

/*
    enum TileMaterial
    {
        AIR,
        SOIL,
        STONE,
        FEATSTONE, // whatever it is
        OBSIDIAN,
        
        VEIN,
        ICE,
        GRASS,
        GRASS2,
        GRASS_DEAD,
        GRASS_DRY,
#include <DFProcess.h>
#include <DFProcess.h>
        DRIFTWOOD,
        HFS,
        MAGMA,
        CAMPFIRE,
        FIRE,
        ASHES,
        CONSTRUCTED
    };
*/
int pickColor(int tiletype)
{
    switch(tileTypeTable[tiletype].m)
    {
        case AIR:
            return COLOR_BLACK;
        case STONE:
        case FEATSTONE:
        case OBSIDIAN:
        case CONSTRUCTED:
        case ASHES:
        default:
            return COLOR_WHITE;
        case SOIL:
        case GRASS_DEAD:
        case GRASS_DRY:
        case DRIFTWOOD:
            return COLOR_YELLOW;
        case ICE:
            return COLOR_CYAN;
        case VEIN:
            return COLOR_MAGENTA;
        case GRASS:
        case GRASS2:
            return COLOR_GREEN;
        case HFS:
        case MAGMA:
        case CAMPFIRE:
        case FIRE:
            return COLOR_RED;
    }
}

string getGCCClassName (Process * p, uint32_t vptr)
{
    int typeinfo = p->readDWord(vptr - 4);
    int typestring = p->readDWord(typeinfo + 4);
    return p->readCString(typestring);
}

main(int argc, char *argv[])
{
    /* initialize your non-curses data structures here */

    signal(SIGINT, finish);      /* arrange interrupts to terminate */

    initscr();      /* initialize the curses library */
    keypad(stdscr, TRUE);  /* enable keyboard mapping */
    nonl();         /* tell curses not to do NL->CR/NL on output */
    cbreak();       /* take input chars one at a time, no wait for \n */
    noecho();       /* don't echo input */
    //nodelay(stdscr, true); 
    int wxMax = getmaxx(stdscr);
    int wyMax = getmaxy(stdscr);

    keypad(stdscr, TRUE);
    scrollok(stdscr, TRUE);

    if (has_colors())
    {
        start_color();

        /*
         * Simple color assignment, often all we need.
         */
        init_pair(COLOR_BLACK, COLOR_BLACK, COLOR_BLACK);
        init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
        init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);

        init_color(COLOR_CYAN, 700, 700, 700); // lt grey
        init_color(COLOR_MAGENTA, 500, 500, 500); // dk grey
        init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
        init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    }
    
    int x_max,y_max,z_max;
    uint32_t x_max_a,y_max_a,z_max_a;
    uint16_t tiletypes[16][16];
    DFHack::t_designation designations[16][16];
    uint8_t regionoffsets[16];
    map <int16_t, uint32_t> materials;
    materials.clear();
    vector<DFHack::t_matgloss> stonetypes;
    vector< vector <uint16_t> > layerassign;
    vector<t_vein> veinVector;
    
    // init the API
    DFHack::API DF("Memory.xml");
    
    // attach
    if(!DF.Attach())
    {
        error = "Can't find DF.";
        finish(0);
    }
    
    Process* p = DF.getProcess();
    // init the map
    DF.InitMap();
    
    DF.getSize(x_max_a,y_max_a,z_max_a);
    x_max = x_max_a;
    y_max = y_max_a;
    z_max = z_max_a;
    
    // get stone matgloss mapping
    if(!DF.ReadStoneMatgloss(stonetypes))
    {
        error = "Can't read stone types.";
        finish(0);
    }
    
    // get region geology
    if(!DF.ReadGeology( layerassign ))
    {
        error = "Can't read local geology.";
        finish(0);
    }

    // FIXME: could fail on small forts
    int cursorX = x_max/2 - 1;
    int cursorY = y_max/2 - 1;
    int cursorZ = z_max/2 - 1;
    
    
    
    int vein = 0;
    // walk the map!
    for (;;)
    {
        int c = getch();     /* refresh, accept single keystroke of input */
        clrscr();
        /* process the command keystroke */
        switch(c)
        {
            case KEY_DOWN:
                cursorY ++;
                break;
            case KEY_UP:
                cursorY --;
                break;
            case KEY_LEFT:
                cursorX --;
                break;
            case KEY_RIGHT:
                cursorX ++;
                break;
            case KEY_NPAGE:
                cursorZ --;
                break;
            case KEY_PPAGE:
                cursorZ ++;
                break;
            case '+':
                vein ++;
                break;
            case '-':
                vein --;
                break;
            default:
                break;
        }
        cursorX = max(cursorX, 0);
        cursorY = max(cursorY, 0);
        cursorZ = max(cursorZ, 0);
        
        cursorX = min(cursorX, x_max - 1);
        cursorY = min(cursorY, y_max - 1);
        cursorZ = min(cursorZ, z_max - 1);        

        DF.Suspend();
        for(int i = -1; i <= 1; i++)
        {
            for(int j = -1; j <= 1; j++)
            {
                if(DF.isValidBlock(cursorX+i,cursorY+j,cursorZ))
                {
                    // read data
                    DF.ReadTileTypes(cursorX+i,cursorY+j,cursorZ, (uint16_t *) tiletypes);
                    DF.ReadDesignations(cursorX+i,cursorY+j,cursorZ, (uint32_t *) designations);
                    for(int x = 0; x < 16; x++)
                    {
                        for(int y = 0; y < 16; y++)
                        {
                            int color = COLOR_BLACK;
                            color = pickColor(tiletypes[x][y]);
                            if(designations[x][y].bits.hidden)
                            {
                                puttile(x+(i+1)*16,y+(j+1)*16,tiletypes[x][y], color);
                            }
                            else
                            {
                                attron(A_STANDOUT);
                                puttile(x+(i+1)*16,y+(j+1)*16,tiletypes[x][y], color);
                                attroff(A_STANDOUT);
                            }
                        }
                    }
                    if(i == 0 && j == 0)
                    {
                        veinVector.clear();
                        DF.ReadVeins(cursorX+i,cursorY+j,cursorZ,veinVector);
                    }
                }
            }
        }
        gotoxy(0,48);
        cprintf("arrow keys, PGUP, PGDN = navigate");
        gotoxy(0,49);
        cprintf("+,-                    = switch vein");
        gotoxy(0,50);
        if(vein == veinVector.size()) vein = veinVector.size() - 1;
        if(vein < -1) vein = -1;
        cprintf("X %d/%d, Y %d/%d, Z %d/%d. Vein %d of %d",cursorX+1,x_max,cursorY+1,y_max,cursorZ,z_max,vein+1,veinVector.size());
        if(!veinVector.empty())
        {
            if(vein != -1 && vein < veinVector.size())
            {
                string str = getGCCClassName(p, veinVector[vein].vtable);
                if(str == "34block_square_event_frozen_liquidst")
                {
                    t_frozenliquidvein frozen;
                    uint32_t size = sizeof(t_frozenliquidvein);
                    p->read(veinVector[vein].address_of,size,(uint8_t *)&frozen);
                    for(uint32_t i = 0;i<16;i++)
                    {
                        for (uint32_t j = 0; j< 16;j++)
                        {
                            int color = COLOR_BLACK;
                            int tile = frozen.tiles[i][j];
                            color = pickColor(tile);
                            
                            attron(A_STANDOUT);
                            puttile(i+16,j+16,tile, color);
                            attroff(A_STANDOUT);
                        }
                    }
                }
                else if (str == "28block_square_event_mineralst")
                {
                    //iterate through vein rows
                    for(uint32_t j = 0;j<16;j++)
                    {
                        //iterate through the bits
                        for (uint32_t k = 0; k< 16;k++)
                        {
                            // and the bit array with a one-bit mask, check if the bit is set
                            bool set = !!(((1 << k) & veinVector[vein].assignment[j]) >> k);
                            if(set)
                            {
                                putch(k+16,j+16,'$',COLOR_RED);
                            }
                        }
                    }
                    gotoxy(0,52);
                    cprintf("%s",stonetypes[veinVector[vein].type].name);
                }
                gotoxy(0,51);
                cprintf("%s, address 0x%x",str.c_str(),veinVector[vein].address_of);
            }
        }
        DF.Resume();
        wrefresh(stdscr);
    }
    finish(0);
}

static void finish(int sig)
{
    endwin();
    if(!error.empty())
    {
        cerr << error << endl;
    }
    exit(0);
}