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


extern void block_markBuf( UThread* ut, UBuffer* buf );
extern int boron_doVoid( UThread* ut, const UCell* blkC );


//#define REPORT_LAYOUT   1

#define MAX_DIM     0x7fff


#define IS_ENABLED(wp)  (! (wp->flags & GW_DISABLED))

#define EACH_CHILD(parent,it)   for(it = parent->child; it; it = it->next) {

#define EACH_SHOWN_CHILD(parent,it) \
    EACH_CHILD(parent,it) \
        if( it->flags & GW_HIDDEN ) \
            continue;

#define EACH_END    }

#define MIN(x,y)    ((x) < (y) ? (x) : (y))


#ifdef KR_TODO
#define EACH_ROOT(it) \
    for(it.id = ui->rootList; IS_VALID(it.id); it.id = it->next) { \
        it = WPTR(it.id);

#define EACH_SHOWN_ROOT(it) \
    EACH_ROOT(it) \
        if( it->flags & GW_HIDDEN ) \
            continue;


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


/*
  Set cell to coord! holding widget area.
*/
void gui_initRectCoord( UCell* cell, GWidget* w, UAtom what )
{
    ur_setId(cell, UT_COORD);
    if( what == UR_ATOM_SIZE )
    {
        cell->coord.len = 2;
        cell->coord.n[0] = w->area.w;
        cell->coord.n[1] = w->area.h;
    }
    else if( what == UR_ATOM_POS )
    {
        cell->coord.len = 2;
        cell->coord.n[0] = w->area.x;
        cell->coord.n[1] = w->area.y;
    }
    else
    {
        cell->coord.len = 4;
        cell->coord.n[ 0 ] = w->area.x;
        cell->coord.n[ 1 ] = w->area.y;
        cell->coord.n[ 2 ] = w->area.w;
        cell->coord.n[ 3 ] = w->area.h;
    }
}


//----------------------------------------------------------------------------


#define no_mark     0
#define no_render   0


static void no_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    (void) ut;
    (void) wp;
    (void) ev;
}


int gui_areaSelect( GWidget* wp, UAtom atom, UCell* result )
{
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
    ((wp->area.x != rect->x) || (wp->area.y != rect->y) || \
     (wp->area.w != rect->w) || (wp->area.h != rect->h))

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


#if 0
//----------------------------------------------------------------------------


static void expand_init( GUI* ui, GWidget* wp, const UCell* arg, int argc )
{
    (void) ui;
    (void) arg;
    (void) argc;

    wp->area.w = 0;
    wp->area.h = 0;
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
#endif


//----------------------------------------------------------------------------


#define BOX_MEMBERS \
    uint16_t marginL; \
    uint16_t marginT; \
    uint16_t marginR; \
    uint16_t marginB; \
    uint16_t spacing; \
    uint8_t  rows; \
    uint8_t  cols;

typedef struct
{
    GWidget wid;
    BOX_MEMBERS
}
Box;

#define EX_PTR  Box* ep = (Box*) wp


/*
  mc must be a coord!
*/
static void setBoxMargins( Box* wd, const UCell* mc )
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


GWidgetClass wclass_hbox;
GWidgetClass wclass_vbox;

// box [margin coord!] block
static GWidget* box_make( UThread* ut, UBlockIter* bi )
{
    Box* ep;
    (void) ut;

    ep = (Box*) gui_allocWidget( sizeof(Box), &wclass_hbox );

    ep->wid.flags |= GW_DISABLED;   // Does not handle input. 

    ep->marginL = 0;
    ep->marginT = 0;
    ep->marginR = 0;
    ep->marginB = 0;
    ep->spacing = 8;

    /*
    if( argc == 3 )
    {
        setBoxMargins( ep, arg + 1 );
    }
    */

    ++bi->it;
    return 0;
}


static void box_free( GWidget* wp )
{
    memFree( wp );
}


static void box_mark( UThread* ut, GWidget* wp )
{
    GMarkFunc func;
    GWidget* it;

    EACH_CHILD( wp, it )
        if( (func = it->wclass->mark) )
            func( ut, it );
    EACH_END
}


static void hbox_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    GWidget* it;
    GSizeHint cs;
    int count = 0;

    size->minW    = ep->marginL + ep->marginR;
    size->minH    = ep->marginT + ep->marginB;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_FIXED;
    size->policyY = GW_FIXED;

    EACH_SHOWN_CHILD( wp, it )
        ++count;
        it->wclass->sizeHint( it, &cs );
        size->minW += cs.minW;
        if( size->minH < cs.minH )
            size->minH = cs.minH;
    EACH_END

    if( count > 1 )
        size->minW += (count - 1) * ep->spacing;
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


static void layout_query( GWidget* wp, LayoutData* lo )
{
    GWidget* it;
    GSizeHint* hint = lo->hint;

    lo->count = 0;

    EACH_SHOWN_CHILD( wp, it )
        assert( lo->count < MAX_LO_WIDGETS );
        it->wclass->sizeHint( it, hint );
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


static void hbox_layout( UThread* ut, GWidget* wp /*, GRect* rect*/ )
{
    EX_PTR;
    GWidget* it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;

    layout_query( wp, &lo );
    layout_stats( &lo, 'x' );

    room = wp->area.w - ep->marginL - ep->marginR -
           (ep->spacing * (lo.count - 1));
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

    dim = wp->area.x + ep->marginL;
    room = wp->area.h - ep->marginT - ep->marginB;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = dim;
        cr.y = wp->area.y + ep->marginB;
        cr.w = hint->minW;
        cr.h = MIN( hint->maxH, room );

        dim += cr.w + ep->spacing;

#ifdef REPORT_LAYOUT
        printf( "KR hbox layout  id %d  class %d  %d,%d,%d,%d\n",
                it.id, it->classId, cr.x, cr.y, cr.w, cr.h );
#endif
        //if( areaUpdate( it, &cr ) )
        areaUpdate( it, &cr );        // Always call layout to recompile DL.
            it->wclass->layout( ut, it );
        ++hint;
    EACH_END
}


static void box_render( GUI* ui, GWidget* wp )
{
    GWidget* it;

    EACH_SHOWN_CHILD( wp, it )
        it->wclass->render( ui, it );
    EACH_END
}


//----------------------------------------------------------------------------


static void vbox_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    GWidget* it;
    GSizeHint cs;
    int count = 0;

    size->minW    = ep->marginL + ep->marginR;
    size->minH    = ep->marginT + ep->marginB;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_WEIGHTED;
    size->policyY = GW_WEIGHTED;

    EACH_SHOWN_CHILD( wp, it )
        ++count;
        it->wclass->sizeHint( it, &cs );
        size->minH += cs.minH;
        if( size->minW < cs.minW )
            size->minW = cs.minW;
    EACH_END

    if( count > 1 )
        size->minH += (count - 1) * ep->spacing;
}


static void vbox_layout( UThread* ut, GWidget* wp /*, GRect* rect*/ )
{
    EX_PTR;
    GWidget* it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;

    layout_query( wp, &lo );
    layout_stats( &lo, 'y' );

    room = wp->area.h - ep->marginT - ep->marginB -
           (ep->spacing * (lo.count - 1));
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

    dim = wp->area.y + wp->area.h - ep->marginT;
    room = wp->area.w - ep->marginL - ep->marginR;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = wp->area.x + ep->marginL;
        cr.w = MIN( hint->maxW, room );
        cr.h = hint->minH;
        cr.y = dim - cr.h;

        dim -= cr.h + ep->spacing;

#ifdef REPORT_LAYOUT
        printf( "KR vbox layout  id %d  class %d  %d,%d,%d,%d\n",
                it.id, it->classId, cr.x, cr.y, cr.w, cr.h );
#endif
        //if( areaUpdate( it, &cr ) )
        areaUpdate( it, &cr );        // Always call layout to recompile DL.
            it->wclass->layout( ut, it );
        ++hint;
    EACH_END
}


//----------------------------------------------------------------------------


typedef struct
{
    GWidget wid;
    BOX_MEMBERS
    UIndex   dp[2];
    UIndex   titleN;
    UIndex   eventN;        // Event handler block
    /*
    GWidget* keyFocus;
    GWidget* mouseFocus;
    char     mouseGrabbed;
    */
}
GWindow;

#undef EX_PTR
#define EX_PTR  GWindow* ep = (GWindow*) wp

#define WINDOW_HBOX     GW_FLAG_USER1


GWidgetClass wclass_window;

static GWidget* window_make( UThread* ut, UBlockIter* bi )
{
    GWindow* ep;
    const UCell* arg = bi->it;

    ep = (GWindow*) gui_allocWidget( sizeof(GWindow), &wclass_window );

    ep->marginL = 8;
    ep->marginT = 8;
    ep->marginR = 8;
    ep->marginB = 8;
    ep->spacing = 0;

    ep->dp[0] = ur_makeDrawProg( ut );


    /* TODO: Support path?
    if( ur_sel(arg) == ui->atom_hbox )
        wp->flags |= WINDOW_HBOX;
    */

    if( ++arg == bi->end )
        goto arg_end;

    if( ur_is(arg, UT_STRING) )
    {
        ep->titleN = arg->series.buf;
        glv_setTitle( glEnv.view, boron_cstr( ut, arg, 0 ) );
        if( ++arg == bi->end )
            goto arg_end;
    }

    if( ur_is(arg, UT_BLOCK) )
    {
        ep->eventN = arg->series.buf;
        ++arg;
    }

arg_end:

    bi->it = arg;
    return (GWidget*) ep;
}


static void window_free( GWidget* wp )
{
    memFree( wp );
}


static void window_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBuffer( ut, ep->dp[0] );
    if( ep->titleN > UR_INVALID_BUF )   // Also acts as (! ur_isShared(n))
        ur_markBuffer( ut, ep->titleN );
    ur_markBlkN( ut, ep->eventN );

    box_mark( ut, wp );
}


static void window_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    UBlockIter bi;
    UAtom name;

    if( ep->eventN )
    {
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

        bi.buf = ur_buffer( ep->eventN );
        bi.it  = bi.buf->ptr.cell;
        bi.end = bi.it + bi.buf->used;
        if( bi.buf->used & 1 )
            --bi.end;
        while( bi.it != bi.end )
        {
            if( ur_is(bi.it, UT_WORD) )
            {
                if( ur_atom(bi.it) == name )
                {
                    ++bi.it;
                    if( ur_is(bi.it, UT_BLOCK) )
                    {
                        if( ! boron_doVoid( ut, bi.it ) )
                        {
                            UR_GUI_THROW;
                        }
                    }
                    return;
                }
            }
            bi.it += 2;
        }
    }
}


static void window_sizeHint( GWidget* wp, GSizeHint* size )
{
    if( wp->flags & WINDOW_HBOX )
        hbox_sizeHint( wp, size );
    else
        vbox_sizeHint( wp, size );
}


static void window_layout( UThread* ut, GWidget* wp )
{
    EX_PTR;
    DPCompiler* save;
    DPCompiler dpc;
    UCell* style;
    UCell* rc;


    save = ur_beginDP( &dpc );

    style = gui_style( ut );

    rc = style + CI_STYLE_START_DL;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );


    rc = style + CI_STYLE_WINDOW_MARGIN;
    if( ur_is(rc, UT_COORD) )
        setBoxMargins( (Box*) ep, rc );

    // Set draw list variables.
    rc = style + CI_STYLE_LABEL;
    ur_setId( rc, UT_STRING );
    ur_setSeries( rc, ep->titleN, 0 );

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );

    // Compile draw list.

    rc = style + CI_STYLE_WINDOW;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );

    if( wp->flags & WINDOW_HBOX )
        hbox_layout( ut, wp );
    else
        vbox_layout( ut, wp );

    ur_endDP( ut, ur_buffer(ep->dp[0]), save );
}


static void window_render( GUI* ui, GWidget* wp )
{
    EX_PTR;
    DPState ds;

    ur_initDrawState( &ds );
    ur_runDrawProg( ui->ut, &ds, ep->dp[0] );

    box_render( ui, wp );
}


GWidgetClass wclass_hbox =
{
    "hbox",
    box_make,        box_free,        box_mark,
    no_dispatch,     hbox_sizeHint,   hbox_layout,
    box_render,      gui_areaSelect,
    0, 0
};


GWidgetClass wclass_vbox =
{
    "vbox",
    box_make,        box_free,        box_mark,
    no_dispatch,     vbox_sizeHint,   vbox_layout,
    box_render,      gui_areaSelect,
    0, 0
};


GWidgetClass wclass_window =
{
    "window",
    window_make,       window_free,       window_mark,
    window_dispatch,   window_sizeHint,   window_layout,
    window_render,     gui_areaSelect,
    0, 0
};


/*
  Return draw-prog buffer index or UR_INVALID_BUF.
*/
UIndex gui_parentDrawProg( GWidget* wp )
{
    while( wp->parent )
    {
        wp = wp->parent;
        if( wp->wclass == &wclass_window )
            return ((GWindow*) wp)->dp[0];
    }
    return UR_INVALID_BUF;
}


#ifdef KR_TODO
//----------------------------------------------------------------------------


static GWidgetClass wclass[ WCLASS_MAX ] =
{
  { "expand",
    expand_init,      no_mark,        no_event,
    expand_sizeHint,  base_layout,    no_render,      gui_areaSelect,
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
    GWidget* it;
    EACH_SHOWN_CHILD( wp, it )
        if( gui_widgetContains( it.w, ev->x, ev->y ) )
            return childAt( ui, it.w, ev );
    EACH_END
    return wp;
}


static GWidgetId widgetAt( GUI* ui, const GLViewEvent* ev )
{
    GWidget* it;
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
            GWidget* it;
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
        GWidget* it;
        EACH_ROOT( it )
            it->flags |= GW_UPDATE_LAYOUT;
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


void gui_doBlockN( UThread* ut, UIndex blkN )
{
    if( ! boron_doBlockN( ut, blkN, boron_result(ut) ) )
    {
        UR_GUI_THROW;
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


UCell* gui_style( UThread* ut )
{
    UBuffer* ctx = ur_threadContext( ut );
    int n = ur_ctxLookup( ctx, UR_ATOM_GUI_STYLE );
    if( n > -1 )
    {
        UCell* cc = ur_ctxCell( ctx, n );
        if( ur_is(cc, UT_CONTEXT) )
        {
            ctx = ur_bufferSerM(cc);
            return ur_ctxCell( ctx, 0 );
        }
    }
    return 0;
}


#ifdef KR_TODO
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
    if( ! (ui->style = gui_style( ui->ut )) )
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
        EACH_END
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
        gui_dumpWidget( ui, it, 1 );
    EACH_END
    printf( "]\n" );
}
*/
#endif


// EOF
