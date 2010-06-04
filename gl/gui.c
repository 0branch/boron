/*
  Boron OpenGL GUI
  Copyright 2008-2010 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
  TODO

  [ ] Layout dialect, way to make widgets
     [ ] Use 'parse or C code?
     [x] Need to support custom widget classes
  [x] class-specific data (inherited structure)
  [x] Rendering
     [x] styles
  [x] Layout
  [/] Input
  [ ] Handle GUIs in viewports (interfaces on objects in simulation world).
*/

/*
  Each widget is linked to one of these lists:

    GUI free list
    GUI root list
    GWidget child list
*/


#include "glh.h"
#include <GL/glv_keys.h>
#include "boron.h"
#include "boron-gl.h"
#include "gl_atoms.h"
#include "gui.h"
#include "os.h"
#include "draw_prog.h"


extern DPCompiler* gDPC;
extern void block_markBuf( UThread* ut, UBuffer* buf );
extern int boron_doVoid( UThread* ut, const UCell* blkC );
extern int boron_doBlock( UThread* ut, const UCell* ec, UCell* res );


//#define REPORT_LAYOUT   1

#define GScalar     int16_t
#define MAX_DIM     0x7fff


#define IS_ENABLED(wp)  (! (wp->flags & GW_DISABLED))

#define EACH_CHILD(parent,it)   for(it = parent->child; it; it = it->next)

#ifdef KR_TODO
#define EACH_SHOWN_CHILD(parent,it) \
    EACH_CHILD(parent,it) \
        if( it.w->flags & GW_HIDDEN ) \
            continue;

#define EACH_ROOT(it) \
    for(it.id = ui->rootList; IS_VALID(it.id); it.id = it.w->next) { \
        it.w = WPTR(it.id);

#define EACH_SHOWN_ROOT(it) \
    EACH_ROOT(it) \
        if( it.w->flags & GW_HIDDEN ) \
            continue;

#define EACH_END    }

#define MIN(x,y)    ((x) < (y) ? (x) : (y))


#define INIT_EVENT(ms,et,ec,es,ex,ey) \
    ms.type  = et; \
    ms.code  = ec; \
    ms.state = es; \
    ms.x     = ex; \
    ms.y     = ey


void gui_removeFocus( GUI* ui, GWidget* wp )
{
    if( ui->mouseFocus == wp )
    {
        ui->mouseFocus = 0;
        if( ui->mouseGrabbed )
            ui->mouseGrabbed = 0;
    }
    if( ui->keyFocus == wp )
        ui->keyFocus = 0;
}
#endif


GWidget* gui_allocWidget( int size, GWidgetClass* wclass )
{
    GWidget* wp = (GWidget*) memAlloc( size );
    memSet( wp, 0, size );
    wp->wclass = wclass;
    wp->flags  = GW_UPDATE_LAYOUT;
    return wp;
}


void gui_freeWidget( GWidget* wp )
{
    GWidget* it;
    GWidget* next;

    //gui_removeFocus( ui, wp );

    for( it = wp->child; it; it = next )
    {
        next = it->next;
        gui_freeWidget( it );
    }
    wp->wclass->free( wp );
}


/*
  Returns non-zero if widget contains point x,y.
*/
int gui_widgetContains( const GWidget* wp, int x, int y )
{
    if( (x < wp->area.x) ||
        (y < wp->area.y) ||
        (x >= (wp->area.x + wp->area.w)) ||
        (y >= (wp->area.y + wp->area.h)) )
        return 0;
    return 1;
}


#ifdef KR_TODO
/*
  Set cell to coord! holding widget area.
*/
static void gui_initRectCoord( UCell* cell, GWidget* w, UAtom what )
{
    ur_setId(cell, UT_COORD);
    if( what == UR_ATOM_SIZE )
    {
        cell->coord.len = 2;
        cell->coord.n[0] = w->w;
        cell->coord.n[1] = w->h;
    }
    else if( what == UR_ATOM_POS )
    {
        cell->coord.len = 2;
        cell->coord.n[0] = w->x;
        cell->coord.n[1] = w->y;
    }
    else
    {
        cell->coord.len = 4;
        cell->coord.n[ 0 ] = w->x;
        cell->coord.n[ 1 ] = w->y;
        cell->coord.n[ 2 ] = w->w;
        cell->coord.n[ 3 ] = w->h;
    }
}


//----------------------------------------------------------------------------


#define no_mark     0
#define no_render   0


static void no_event( GUI* ui, GWidget* wp, const GLViewEvent* ev )
{
    (void) ui;
    (void) wp;
    (void) ev;
}


static void base_layout( GUI* ui, GWidget* wp )
{
    (void) ui;
    (void) wp;
}


static int base_select( GUI* ui, GWidget* wp, UAtom atom, UCell* result )
{
    (void) ui;
    switch( atom )
    {
        case UR_ATOM_RECT:
        case UR_ATOM_SIZE:
        case UR_ATOM_POS:
            gui_initRectCoord( result, wp, atom );
            return 1;
    }
    return 0;
}


#define gui_areaChanged(wp,rect) \
    ((wp->x != rect->x) || (wp->y != rect->y) || \
     (wp->w != rect->w) || (wp->h != rect->h))

/*
  Returns non-zero if area changed.
*/
static int areaUpdate( GWidget* wp, GRect* rect )
{
    if( gui_areaChanged( wp, rect ) )
    {
        wp->area = *rect;
        return 1;
    }
    return 0;
}


//----------------------------------------------------------------------------


static void expand_init( GUI* ui, GWidget* wp, const UCell* arg, int argc )
{
    (void) ui;
    (void) arg;
    (void) argc;

    wp->w = 0;
    wp->h = 0;
}


static void expand_sizeHint( GUI* ui, GWidget* wp, GSizeHint* size )
{
    (void) ui;
    (void) wp;

    size->minW    = 0;
    size->minH    = 0;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_EXPANDING;
    size->policyY = GW_EXPANDING;
}


//----------------------------------------------------------------------------


typedef struct
{
    GWidget wid;

    uint16_t marginL;
    uint16_t marginT;
    uint16_t marginR;
    uint16_t marginB;
    uint16_t spacing;
    uint16_t _pad;

    // For window or group classes
    UIndex   dp[2];
    UIndex   titleN;
    UIndex   eventN;        // Event handler block

    // Future...
    uint8_t  layoutType;
    uint8_t  rows;
    uint8_t  cols;
}
BoxWidget;

#define BOX_DATA(wp)    ((BoxData*) (wp)->cell)


/*
  mc must be a coord!
*/
static void setBoxMargins( BoxData* wd, const UCell* mc )
{
    wd->marginL = mc->coord.n[0];
    wd->marginT = mc->coord.n[1];

    if( mc->coord.len < 4 )
    {
        wd->marginR = wd->marginL;
        wd->marginB = wd->marginT;
        if( mc->coord.len == 3 )
            wd->spacing = mc->coord.n[2];
    }
    else
    {
        wd->marginR = mc->coord.n[2];
        wd->marginB = mc->coord.n[3];
        if( mc->coord.len == 5 )
            wd->spacing = mc->coord.n[4];
    }
}


// box [margin coord!] block
static void box_init( GUI* ui, GWidget* wp, const UCell* arg, int argc )
{
    BoxData* wd = BOX_DATA(wp);
    (void) ui;
    (void) arg;
    (void) argc;

    wp->flags |= GW_DISABLED;   // Does not handle input. 

    wd->marginL = 0;
    wd->marginT = 0;
    wd->marginR = 0;
    wd->marginB = 0;
    wd->spacing = 8;

    if( argc == 3 )
    {
        setBoxMargins( wd, arg + 1 );
    }
}


static void box_mark( GUI* ui, GWidget* wp )
{
    GMarkFunc func;
    GWidgetIt it;
    EACH_CHILD( wp, it )
        func = CLASS( it.w ).mark;
        if( func )
            func( ui, it.w );
    EACH_END
}


static void hbox_sizeHint( GUI* ui, GWidget* wp, GSizeHint* size )
{
    GWidgetIt it;
    GSizeHint cs;
    int count = 0;
    BoxData* wd = BOX_DATA(wp);

    size->minW    = wd->marginL + wd->marginR;
    size->minH    = wd->marginT + wd->marginB;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_FIXED;
    size->policyY = GW_FIXED;

    EACH_SHOWN_CHILD( wp, it )
        ++count;
        CLASS( it.w ).sizeHint( ui, it.w, &cs );
        size->minW += cs.minW;
        if( size->minH < cs.minH )
            size->minH = cs.minH;
    EACH_END

    if( count > 1 )
        size->minW += (count - 1) * wd->spacing;
}


#define MAX_LO_WIDGETS 16

typedef struct
{
    int count;
    int required;
    int widgetWeight;
    int expandWeight;
    GSizeHint hint[ MAX_LO_WIDGETS ];
}
LayoutData;


static void layout_query( GUI* ui, GWidget* wp, LayoutData* lo )
{
    GWidgetIt it;
    GSizeHint* hint = lo->hint;

    lo->count = 0;

    EACH_SHOWN_CHILD( wp, it )
        assert( lo->count < MAX_LO_WIDGETS );
        CLASS( it.w ).sizeHint( ui, it.w, hint );
        ++hint;
        ++lo->count;
    EACH_END
}


static void layout_stats( LayoutData* lo, int axis )
{
    int used = 0;
    int weight = 0;
    int ew = 0;
    GSizeHint* it  = lo->hint;
    GSizeHint* end = it + lo->count;
    if( axis == 'y' )
    {
        while( it != end )
        {
            if( it->policyY == GW_EXPANDING )
                ew += it->weightY;
            else
                weight += it->weightY;
            used   += it->minH;
            ++it;
        }
    }
    else
    {
        while( it != end )
        {
            if( it->policyX == GW_EXPANDING )
                ew += it->weightX;
            else
                weight += it->weightX;
            used   += it->minW;
            ++it;
        }
    }
    lo->required = used;
    lo->widgetWeight = weight;
    lo->expandWeight = ew;
}


static void hbox_layout( GUI* ui, GWidget* wp /*, GRect* rect*/ )
{
    GWidgetIt it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;
    BoxData* wd = BOX_DATA(wp);

    layout_query( ui, wp, &lo );
    layout_stats( &lo, 'x' );

    room = wp->w - wd->marginL - wd->marginR -
           (wd->spacing * (lo.count - 1));
    if( room > lo.required )
    {
        //int excess = room - lo.required;
        int avail = room;
        hint = lo.hint;
        hend = hint + lo.count;
        while( hint != hend )
        {
            if( hint->policyX == GW_WEIGHTED )
            {
                dim = (room * hint->weightX) / lo.widgetWeight;
                if( dim > hint->maxW )
                    hint->minW = hint->maxW;
                else if( dim > hint->minW )
                    hint->minW = dim;
                avail -= hint->minW;
            }
            else if( hint->policyX == GW_FIXED )
            {
                avail -= hint->minW;
            }
            ++hint;
        }

        if( avail > 0 )
        {
            // Allocate unused space to expanders.
            hint = lo.hint;
            while( hint != hend )
            {
                if( hint->policyX == GW_EXPANDING )
                {
                    hint->minW = (avail * hint->weightX) / lo.expandWeight;
                }
                ++hint;
            }
        }
    }

    //gui_setArea( wp, rect );

    dim = wp->x + wd->marginL;
    room = wp->h - wd->marginT - wd->marginB;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = dim;
        cr.y = wp->y + wd->marginB;
        cr.w = hint->minW;
        cr.h = MIN( hint->maxH, room );

        dim += cr.w + wd->spacing;

#ifdef REPORT_LAYOUT
        printf( "KR hbox layout  id %d  class %d  %d,%d,%d,%d\n",
                it.id, it.w->classId, cr.x, cr.y, cr.w, cr.h );
#endif
        //if( areaUpdate( it.w, &cr ) )
        areaUpdate( it.w, &cr );        // Always call layout to recompile DL.
            CLASS( it.w ).layout( ui, it.w );
        ++hint;
    EACH_END
}


static void box_render( GUI* ui, GWidget* wp )
{
    GWidgetIt it;
    GRenderFunc rfunc;

    EACH_SHOWN_CHILD( wp, it )
        rfunc = CLASS( it.w ).render;
        if( rfunc )
            rfunc( ui, it.w );
    EACH_END
}


//----------------------------------------------------------------------------


static void vbox_sizeHint( GUI* ui, GWidget* wp, GSizeHint* size )
{
    GWidgetIt it;
    GSizeHint cs;
    int count = 0;
    BoxData* wd = BOX_DATA(wp);

    size->minW    = wd->marginL + wd->marginR;
    size->minH    = wd->marginT + wd->marginB;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_WEIGHTED;
    size->policyY = GW_WEIGHTED;

    EACH_SHOWN_CHILD( wp, it )
        ++count;
        CLASS( it.w ).sizeHint( ui, it.w, &cs );
        size->minH += cs.minH;
        if( size->minW < cs.minW )
            size->minW = cs.minW;
    EACH_END

    if( count > 1 )
        size->minH += (count - 1) * wd->spacing;
}


static void vbox_layout( GUI* ui, GWidget* wp /*, GRect* rect*/ )
{
    GWidgetIt it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;
    BoxData* wd = BOX_DATA(wp);

    layout_query( ui, wp, &lo );
    layout_stats( &lo, 'y' );

    room = wp->h - wd->marginT - wd->marginB -
           (wd->spacing * (lo.count - 1));
    if( room > lo.required )
    {
        //int excess = room - lo.required;
        int avail = room;
        hint = lo.hint;
        hend = hint + lo.count;
        while( hint != hend )
        {
            if( hint->policyY == GW_WEIGHTED )
            {
                dim = (room * hint->weightY) / lo.widgetWeight;
                if( dim > hint->maxH )
                    hint->minH = hint->maxH;
                else if( dim > hint->minH )
                    hint->minH = dim;
                avail -= hint->minH;
            }
            else if( hint->policyY == GW_FIXED )
            {
                avail -= hint->minH;
            }
            ++hint;
        }

        if( avail > 0 )
        {
            // Allocate unused space to expanders.
            hint = lo.hint;
            while( hint != hend )
            {
                if( hint->policyY == GW_EXPANDING )
                {
                    hint->minH = (avail * hint->weightY) / lo.expandWeight;
                }
                ++hint;
            }
        }
    }

    //gui_setArea( wp, rect );

    dim = wp->y + wp->h - wd->marginT;
    room = wp->w - wd->marginL - wd->marginR;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = wp->x + wd->marginL;
        cr.w = MIN( hint->maxW, room );
        cr.h = hint->minH;
        cr.y = dim - cr.h;

        dim -= cr.h + wd->spacing;

#ifdef REPORT_LAYOUT
        printf( "KR vbox layout  id %d  class %d  %d,%d,%d,%d\n",
                it.id, it.w->classId, cr.x, cr.y, cr.w, cr.h );
#endif
        //if( areaUpdate( it.w, &cr ) )
        areaUpdate( it.w, &cr );        // Always call layout to recompile DL.
            CLASS( it.w ).layout( ui, it.w );
        ++hint;
    EACH_END
}


//----------------------------------------------------------------------------


#define WINDOW_HBOX     GW_FLAG_USER1


static void window_init( GUI* ui, GWidget* wp, const UCell* arg, int argc )
{
    BoxData* wd = BOX_DATA(wp);
    //UThread* ut = ui->ut;

    wd->marginL = 8;
    wd->marginT = 8;
    wd->marginR = 8;
    wd->marginB = 8;
    wd->spacing = 0;

    wd->dp[0]  = gx_makeDrawProg( ui->ut );

    if( argc == 4 )
    {
        /* TODO: Support path?
        if( ur_sel(arg) == ui->atom_hbox )
            wp->flags |= WINDOW_HBOX;
        */

        wd->titleN = arg[1].series.buf;
        glv_setTitle( glEnv.view, boron_cstr( ui->ut, arg + 1, 0 ) );

        wd->eventN = arg[2].series.buf;
    }
    else
    {
        wd->titleN = 0;
        wd->eventN = 0;
    }
}


static inline void _markBufN( UThread* ut, UIndex n )
{
    if( n > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
        ur_markBuffer( ut, n );
}


static inline void _markCell( UThread* ut, UCell* cell )
{
    int t = ur_type(cell);
    if( t >= UT_REFERENCE_BUF )
        (ut->types[ t ])->mark( ut, cell );
}


void _markBlockN( UThread* ut, UIndex n )
{
    if( n > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
    {
        if( ur_markBuffer( ut, n ) )
            block_markBuf( ut, ur_buffer(n) );
    }
}


static void window_mark( GUI* ui, GWidget* wp )
{
    BoxData* wd = BOX_DATA(wp);
    UThread* ut = ui->ut;

    ur_markBuffer( ut, wd->dp[0] );
    _markBufN( ut, wd->titleN );
    _markBlockN( ut, wd->eventN );

    box_mark( ui, wp );
}


static void window_event( GUI* ui, GWidget* wp, const GLViewEvent* ev )
{
    BoxData* wd = BOX_DATA(wp);

    if( wd->eventN )
    {
        UThread* ut = ui->ut;
        UBuffer* blk;
        UCell* it;
        UCell* end;
        UAtom name;

        switch( ev->type )
        {
            case GLV_EVENT_CLOSE:
                name = UR_ATOM_CLOSE;
                break;
            case GLV_EVENT_RESIZE:
                name = UR_ATOM_RESIZE;
                break;
            default:
                return;
        }

        blk = ur_buffer( wd->eventN );
        it  = blk->ptr.cell;
        end = it + blk->used;
        if( blk->used & 1 )
            --end;
        while( it != end )
        {
            if( ur_is(it, UT_WORD) )
            {
                if( ur_atom(it) == name )
                {
                    ++it;
                    if( ur_is(it, UT_BLOCK) )
                    {
                        if( ! boron_doVoid( ut, it ) )
                        {
                            UR_GUI_THROW;
                        }
                    }
                    return;
                }
            }
            it += 2;
        }
    }
}


static void window_sizeHint( GUI* ui, GWidget* wp, GSizeHint* size )
{
    if( wp->flags & WINDOW_HBOX )
        hbox_sizeHint( ui, wp, size );
    else
        vbox_sizeHint( ui, wp, size );
}


static void window_layout( GUI* ui, GWidget* wp )
{
    DPCompiler* save;
    DPCompiler dpc;
    UCell* rc;
    UThread* ut = ui->ut;
    BoxData* wd = BOX_DATA(wp);


    save = gx_beginDP( &dpc );

    rc = gui_styleCell( CI_STYLE_START_DL );
    if( ur_is(rc, UT_BLOCK) )
        gx_compileDP( ut, rc, 1 );


    rc = gui_styleCell( CI_STYLE_WINDOW_MARGIN );
    if( ur_is(rc, UT_COORD) )
        setBoxMargins( wd, rc );

    // Set draw list variables.
    rc = gui_styleCell( CI_STYLE_LABEL );
    ur_setId( rc, UT_STRING );
    ur_setSeries( rc, wd->titleN, 0 );

    rc = gui_styleCell( CI_STYLE_AREA );
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );

    // Compile draw list.

    rc = gui_styleCell( CI_STYLE_WINDOW );
    if( ur_is(rc, UT_BLOCK) )
        gx_compileDP( ut, rc, 1 );

    if( wp->flags & WINDOW_HBOX )
        hbox_layout( ui, wp );
    else
        vbox_layout( ui, wp );

    gx_endDP( ut, ur_buffer(wd->dp[0]), save );
}


static void window_render( GUI* ui, GWidget* wp )
{
    DPState ds;
    BoxData* wd = BOX_DATA(wp);
    gx_initDrawState( &ds );
    gx_runDrawProg( ui->ut, &ds, wd->dp[0] );
    box_render( ui, wp );
}


/*
  Return draw-prog resource index or -1 if invalid.
*/
UIndex gui_rootDP( const GUI* ui, GWidgetId parent )
{
#define CLASS_WINDOW    3
    GWidget* root = gui_root( ui, parent );
    if( root && (root->classId == CLASS_WINDOW) )
    {
        BoxData* wd = BOX_DATA(root);
        return wd->dp[0];
    }
    return -1;
}


//----------------------------------------------------------------------------


#define _bitIsSet(array,n)    (array[(n)>>3] & 1<<((n)&7))

#include "widget_button.c"
#include "widget_label.c"
#include "widget_twidget.c"
#include "widget_lineedit.c"
#include "widget_list.c"


static int wclassCount = 10;

static GWidgetClass wclass[ WCLASS_MAX ] =
{
  { "expand",
    expand_init,      no_mark,        no_event,
    expand_sizeHint,  base_layout,    no_render,      base_select,
    0, 0, 0 },

  { "hbox",
    box_init,         box_mark,       no_event,
    hbox_sizeHint,    hbox_layout,    box_render,     base_select,
    0, 0, 0 },

  { "vbox",
    box_init,         box_mark,       no_event,
    vbox_sizeHint,    vbox_layout,    box_render,     base_select,
    0, 0, 0 },

  { "window",   // CLASS_WINDOW
    window_init,      window_mark,    window_event,
    window_sizeHint,  window_layout,  window_render,  base_select,
    0, 0, 0 },

  { "button",   // Change ButtonClass in buttonw.c if this moves.
    button_init,      button_mark,    button_event,
    button_sizeHint,  button_layout,  button_render,  button_select,
    0, 0, 0 },

  { "checkbox",  // Change ButtonClass in buttonw.c if this moves.
    button_init,      button_mark,    checkbox_event,
    button_sizeHint,  button_layout,  button_render,  button_select,
    0, 0, 0 },

  { "label",
    label_init,       label_mark,     no_event,
    label_sizeHint,   label_layout,   label_render,   label_select,
    0, 0, 0 },

  { "line-edit",
    ledit_init,       ledit_mark,     ledit_event,
    ledit_sizeHint,   ledit_layout,   ledit_render,   ledit_select,
    0, 0, 0 },

  { "list",     // "block! block!"
    listw_init,       listw_mark,     listw_event,
    listw_sizeHint,   listw_layout,   listw_render,   listw_select,
    0, 0, 0 },
  /*
    "console",
    "option",
    "choice",
    "menu",
    "data",         // label
    "data-edit",    // spin box, line editor
  */
};


static inline void initClass( UThread* ut, GWidgetClass* cl )
{
    const char* name = cl->name;

    cl->id       = cl - wclass;
    cl->nameAtom = ur_internAtom( ut, name, name + strLen(name) );
}


void gui_init( GUI* ui, UThread* ut )
{
    ui->ut         = ut;
    ui->style      = 0;
    ui->keyFocus   = 0;
    ui->mouseFocus = 0;
    ui->firstFree  = 0;
    ui->rootList   = 0;
    ui->rootW      = 0;
    ui->rootH      = 0;
    ui->mouseGrabbed = 0;

    ur_internAtoms( ut, "gui-style hbox action text selection",
                    &ui->atom_gui_style );
}


void gui_cleanup( GUI* ui )
{
    ur_arrFree( &ui->widgets );
}


void gui_setKeyFocus( GUI* ui, GWidget* wp )
{
    ui->keyFocus = wp;
}


static void gui_callFocus( GUI* ui, GWidget* wp, int eventType )
{
    if( IS_ENABLED(wp) )
    {
        GLViewEvent me;
        INIT_EVENT( me, eventType, 0, 0, 0, 0 );
        wp->wclass->dispatch( ui, w, &me );
    }
}


void gui_setMouseFocus( GUI* ui, GWidget* wp )
{
    if( ui->mouseFocus != wp )
    {
        gui_callFocus( ui, ui->mouseFocus, GLV_EVENT_FOCUS_OUT );
        ui->mouseFocus = id;
        gui_callFocus( ui, id, GLV_EVENT_FOCUS_IN );
    }
}


void gui_grabMouse( GUI* ui, GWidget* wp )
{
    ui->mouseFocus = wp;
    ui->mouseGrabbed = 1;
}


void gui_ungrabMouse( GUI* ui, GWidget* wp )
{
    if( ui->mouseFocus == wp )
        ui->mouseGrabbed = 0;
}


/*
  Returns child of wp under event x,y, or wp if no child contains the point.
*/
static GWidget* childAt( GUI* ui, GWidget* wp, const GLViewEvent* ev )
{
    GWidgetIt it;
    EACH_SHOWN_CHILD( wp, it )
        if( gui_widgetContains( it.w, ev->x, ev->y ) )
            return childAt( ui, it.w, ev );
    EACH_END
    return wp;
}


static GWidgetId widgetAt( GUI* ui, const GLViewEvent* ev )
{
    GWidgetIt it;
    EACH_SHOWN_ROOT( it )
        if( gui_widgetContains( it.w, ev->x, ev->y ) )
        {
            it.w = childAt( ui, it.w, ev );
            return it.w ? WID(it.w) : 0;
        }
    EACH_END
    return 0;
}


void gui_dispatch( GUI* ui, GLViewEvent* ev )
{
    GWidget* w;

#if 0
    printf( "KR dispatch %d  mouseFocus %d  keyFocus %d\n",
            ev->type, ui->mouseFocus, ui->keyFocus );
#endif

    switch( ev->type )
    {
        case GLV_EVENT_CLOSE:
        {
            GWidgetIt it;
            EACH_SHOWN_ROOT( it )
                w = it.w;
                goto dispatch;
            EACH_END
        }
            return;

        case GLV_EVENT_BUTTON_DOWN:
        case GLV_EVENT_BUTTON_UP:
            // Convert window system origin from top of window to the bottom.
            ev->y = ui->rootH - ev->y;
            // Fall through...

        case GLV_EVENT_WHEEL:
            if( IS_VALID(ui->mouseFocus) )
            {
                w = WPTR(ui->mouseFocus);
                goto dispatch;
            }
            return;

        case GLV_EVENT_MOTION:
            // Convert window system origin from top of window to the bottom.
            ev->y = ui->rootH - ev->y;

            if( IS_VALID(ui->mouseFocus) )
            {
                w = WPTR(ui->mouseFocus);
                if( ui->mouseGrabbed )
                {
                    goto dispatch;
                }
                else if( gui_widgetContains( w, ev->x, ev->y ) )
                {
                    GWidget* cw = childAt( ui, w, ev );
                    if( cw != w )
                    {
                        gui_setMouseFocus( ui, WID(cw) );
                        w = cw;
                    }
                    goto dispatch;
                }
            }

            gui_setMouseFocus( ui, widgetAt( ui, ev ) );
            if( IS_VALID(ui->mouseFocus) )
            {
                w = WPTR(ui->mouseFocus);
                goto dispatch;
            }
            break;

        case GLV_EVENT_KEY_DOWN:
        case GLV_EVENT_KEY_UP:
            if( IS_VALID(ui->keyFocus) )
            {
                w = WPTR(ui->keyFocus);
                goto dispatch;
            }
            break;

        case GLV_EVENT_FOCUS_IN:
            ui->mouseFocus = widgetAt( ui, ev );
            // Fall through...

        case GLV_EVENT_FOCUS_OUT:
            if( IS_VALID(ui->mouseFocus) )
            {
                w = WPTR(ui->mouseFocus);
                goto dispatch;
            }
            break;
    }
    return;

dispatch:

    CLASS( w ).dispatch( ui, w, ev );
}


void gui_resizeRootArea( GUI* ui, int width, int height )
{
    if( (width != ui->rootW) || (height != ui->rootH) )
    {
        GWidgetIt it;
        EACH_ROOT( it )
            it.w->flags |= GW_UPDATE_LAYOUT;
        EACH_END

        ui->rootW = width;
        ui->rootH = height;
    }
}
#endif


/*
  Do block, report any exception, and restore stacks.
*/
void gui_doBlock( UThread* ut, const UCell* blkC )
{
    /*
    UBuffer* buf = ur_errorBlock(ut);
    UIndex errUsed = buf->used;
    */

    if( ! boron_doBlock( ut, blkC, boron_result(ut) ) )
    {
#if 1
        UR_GUI_THROW;
#else
        // Report any exception, and restore stacks.
        UBuffer* buf;
        UCell* ex = boron_exception( ut );
        if( ur_is(ex, UT_ERROR) )
        {
            UBuffer* str = &glEnv.tmpStr;
            str->used = 0;
            ur_toText( ut, ex, str );
            ur_strTermNull( str );
            fprintf( stderr, str->ptr.c );
        }

        buf = ur_errorBlock(ut);
        buf->used = 0;
#endif
    }
}


/*
  Append child to end of parent list.
  Child must be valid and unlinked.
*/
void gui_appendChild( GWidget* parent, GWidget* child )
{
    GWidget* it = parent->child;
    if( it )
    {
        while( it->next )
            it = it->next;
        it->next = child; 
    }
    else
    {
        parent->child = child;
    }
    child->parent = parent;
}


/*
  Remove widget from parent list.
*/
void gui_unlink( GWidget* wp )
{
    GWidget* it;
    GWidget* prev;
    GWidget* parent = wp->parent;
    if( parent )
    {
        wp->parent = 0;
        prev = 0;
        for( it = parent->child; it; it = it->next )
        {
            if( it == wp )
            {
                if( prev )
                    prev->next = wp->next;
                else
                    parent->child = wp->next;
                break;
            }
        }
    }
}


/**
  Returns pointer to root parent widget.
*/
GWidget* gui_root( GWidget* wp )
{
    while( wp->parent )
        wp = wp->parent;
    return wp;
}


void gui_enable( GWidget* wp, int active )
{
    int disabled = wp->flags & GW_DISABLED;
    if( active )
    {
        if( disabled )
            wp->flags &= ~GW_DISABLED;
    }
    else if( ! disabled )
    {
        wp->flags |= GW_DISABLED;
    }
}


static void markLayoutDirty( GWidget* wp )
{
    wp->flags |= GW_UPDATE_LAYOUT;
    if( wp->parent )
        markLayoutDirty( wp->parent );
}


void gui_show( GWidget* wp, int show )
{
    int hidden = wp->flags & GW_HIDDEN;
    if( show )
    {
        if( hidden )
        {
            wp->flags &= ~GW_HIDDEN;
            if( wp->parent )
                markLayoutDirty( wp->parent );
        }
    }
    else if( ! hidden )
    {
        //KR_TODO gui_removeFocus( ui, wp );
        wp->flags |= GW_HIDDEN;
        if( wp->parent )
            markLayoutDirty( wp->parent );
    }
}


#ifdef KR_TODO
static UCell* setStyleCells( GUI* ui )
{
    UThread* ut = ui->ut;
    UBuffer* ctx = ur_threadContext(ut);
    int n = ur_ctxLookup( ctx, ui->atom_gui_style );
    if( n > -1 )
    {
        UCell* cc = ur_ctxCell( ctx, n );
        if( ur_is(cc, UT_CONTEXT) )
        {
            ctx = ur_bufferSerM(cc);
            return ui->style = ur_ctxCell( ctx, 0 );
        }
    }
    return 0;
}


/*
  Render a root widget.

  Note that compiliation of draw lists will happen inside gui_render,
  usually during layout.
*/
void gui_render( GUI* ui, GWidgetId id )
{
    GWidget* w;
    GWidgetClass* wc;
    GRenderFunc rfunc;

    if( id >= ur_avail(&ui->widgets) )
        return;
    if( ! setStyleCells( ui ) )
        return;

    w = WPTR(id);
    wc = &CLASS( w );

    if( w->flags & GW_UPDATE_LAYOUT )
    {
        GRect rect;

        rect.x = 0;
        rect.y = 0;
        rect.w = ui->rootW;
        rect.h = ui->rootH;

        w->flags &= ~GW_UPDATE_LAYOUT;
        if( areaUpdate( w, &rect ) )
        {
            GLViewEvent me;
            INIT_EVENT( me, GLV_EVENT_RESIZE, 0, 0, rect.w, rect.h );
            wc->dispatch( ui, w, &me );

            wc->layout( ui, w );
        }
    }

    rfunc = wc->render;
    if( rfunc )
        rfunc( ui, w );
}


/*
  The select method returns non-zero if result is set, or zero if error.
  The select method must not throw errors.
*/
int gui_selectAtom( GUI* ui, GWidget* wp, UAtom atom, UCell* result )
{
    return CLASS( wp ).select( ui, wp, atom, result );
}


static inline void linkRoot( GUI* ui, GWidget* wp, GWidgetId id )
{
    wp->next = ui->rootList;
    ui->rootList = id;
}


/*-cf-
   make-widget
        parent  widget!/none!
        block   block!
*/
CFUNC_PUB( make_widget )
{
    UBlockIter bi;
    GUI* ui = &glEnv.gui;
    GWidgetId id;
    GWidget* parent = 0;


    if( ur_is(a1, UT_WIDGET) )
        parent = gui_widgetPtr( ui, ur_int(a1) );

    ur_blkSlice( ut, &bi, a1 + 1 );
    if( bi.it != bi.end )
    {
        GWidget* wp;
        UAtom atom;
        GWidgetClass* cl   = wclass;
        GWidgetClass* cend = wclass + wclassCount;

        atom = ur_atom(bi.it);
        while( cl != cend )
        {
            if( cl->nameAtom == atom )
                break;
            ++cl;
        }
        if( cl == cend )
            return ur_error( ut, UR_ERR_SCRIPT, "unknown widget class" );

        wp = gui_allocWidget( ui, cl->id, bi.it, bi.end - bi.it );
        if( ur_thrown( ut ) )
            return UR_THROW;
        if( ! wp )
            return ur_error( ut, UR_ERR_SCRIPT, "allocWidget failed" );
        id = WID(wp);

        if( parent )
        {
            gui_link( ui, parent, id );
        }
        else
        {
            linkRoot( ui, wp, id );
            if( ! IS_VALID(ui->keyFocus) )
                ui->keyFocus = id;
        }

        ur_setId( res, UT_WIDGET );
        ur_int(res) = id;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_SCRIPT, "widget! make expected block!" );
}
#endif


#ifdef DEBUG
static void dumpIndent( int indent )
{
    while( indent-- )
        printf( "    " );
}


void gui_dumpWidget( const GWidget* wp, int indent )
{
    dumpIndent( indent );
    printf( "%p %s %d,%d,%d,%d 0x%02X", wp, wp->wclass->name,
            wp->area.x, wp->area.y, wp->area.w, wp->area.h, wp->flags );
    if( wp->child )
    {
        GWidget* it;
        printf( " [\n" );
        ++indent;
        EACH_CHILD( wp, it )
            gui_dumpWidget( it, indent );
        --indent;
        dumpIndent( indent );
        printf( "]\n" );
    }
    else
    {
        printf( "\n" );
    }
}


/*
void gui_dump( const GUI* ui )
{
    GWidget* it;

    printf( "gui [\n" );
    EACH_ROOT( it )
        gui_dumpWidget( ui, it.w, 1 );
    EACH_END
    printf( "]\n" );
}
*/
#endif


// EOF
