/*
  Boron OpenGL GUI
  Copyright 2008-2012 Karl Robillard

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

#include "glh.h"
#include <glv_keys.h>
#include "boron.h"
#include "boron-gl.h"
#include "gl_atoms.h"
#include "gui.h"
#include "os.h"
#include "draw_prog.h"

// Send debug printfs to Android log.
#ifdef __ANDROID__
#include <android/log.h>
#define printf(...) __android_log_print(ANDROID_LOG_INFO,"gui",__VA_ARGS__)
#endif


extern void block_markBuf( UThread* ut, UBuffer* buf );
extern UStatus boron_doVoid( UThread* ut, const UCell* blkC );
//void gui_dumpWidget( const GWidget* wp, int indent );
extern int itemview_parse( UThread* ut, UBlockIter* bi, GWidget* wp );


//#define REPORT_LAYOUT   1
//#define REPORT_HINT     1
#define EVAL_PATH       1


#define IGNORES_INPUT(wp)   (wp->flags & (GW_DISABLED | GW_NO_INPUT))

#define EACH_CHILD(parent,it)   for(it = parent->child; it; it = it->next) {

#define EACH_SHOWN_CHILD(parent,it) \
    EACH_CHILD(parent,it) \
        if( it->flags & GW_HIDDEN ) \
            continue;

#define EACH_END    }

#define MIN(x,y)    ((x) < (y) ? (x) : (y))

#define INIT_EVENT(ms,et,ec,es,ex,ey) \
    ms.type  = et; \
    ms.code  = ec; \
    ms.state = es; \
    ms.x     = ex; \
    ms.y     = ey


static void ur_arrAppendPtr( UBuffer* arr, void* ptr )
{
    ur_arrReserve( arr, arr->used + 1 );
    ((void**) arr->ptr.v)[ arr->used ] = ptr;
    ++arr->used;
}


GWidget* gui_allocWidget( int size, const GWidgetClass* wclass,
                          GWidget* parent )
{
    GWidget* wp = (GWidget*) memAlloc( size );
    if( wp )
    {
        memSet( wp, 0, size );
        wp->wclass = wclass;
        wp->flags  = wclass->flags;

        // Link to parent immediately so any recycle that occurs during
        // make will call the child's mark method.
        if( parent )
        {
            gui_appendChild( parent, wp );
        }
        else
        {
            ur_arrAppendPtr( &glEnv.rootWidgets, wp );
            wp->flags |= GW_CONSTRUCT;
        }
    }
    return wp;
}


void gui_unlink( GWidget* );

// Pass pointer returned from gui_allocWidget().
GWidget* gui_makeFail( GWidget* wp )
{
    if( wp )
    {
        // Undo setup of gui_allocWidget.
        if( wp->parent )
        {
            gui_unlink( wp );
            wp->wclass->free( wp );
        }
        else
        {
            // Leave cleanup to widget_recycle().
            wp->flags = (wp->flags & ~GW_CONSTRUCT) |
                        GW_HIDDEN | GW_DISABLED | GW_DESTRUCT;
        }
    }
    return NULL;
}


void widget_free( GWidget* wp )
{
    memFree( wp );
}


void widget_markChildren( UThread* ut, GWidget* wp )
{
    GMarkFunc func;
    GWidget* it;

    EACH_CHILD( wp, it )
        if( (func = it->wclass->mark) )
            func( ut, it );
    EACH_END
}


static void _freeWidget( GWidget* wp )
{
    GWidget* it;
    GWidget* next;
    for( it = wp->child; it; it = next )
    {
        next = it->next;
        _freeWidget( it );
    }
    wp->wclass->free( wp );
}


static void gui_removeFocus( GWidget* );

void gui_freeWidget( GWidget* wp )
{
    if( wp )
    {
        //printf( "KR freeWidget %p\n", wp );
        gui_removeFocus( wp );
        _freeWidget( wp );
    }
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
    ur_initCoord(cell, 2);
    if( what == UR_ATOM_SIZE )
    {
        cell->coord.n[0] = w->area.w;
        cell->coord.n[1] = w->area.h;
    }
    else if( what == UR_ATOM_POS )
    {
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


int gui_limitX( int x, const GRect* area )
{
    int right;
    if( x < area->x )
        return area->x;
    right = area->x + area->w - 1;
    return (x > right) ? right : x;
}


int gui_limitY( int y, const GRect* area )
{
    int top;
    if( y < area->y )
        return area->y;
    top = area->y + area->h - 1;
    return (y > top) ? top : y;
}


/*
  Return non-zero if cell matches types.
*/
static int _matchMultTypes( const uint8_t* pc, const UCell* cell )
{
    const uint8_t* end = pc + 2 + pc[1];
    for( pc += 2; pc != end; ++pc )
    {
        if( ur_is(cell, *pc) )
            return 1;
    }
    return 0;
}


/*
  Return UR_OK/UR_THROW.
*/
UStatus gui_parseArgs( UThread* ut, UBlockIter* bi, const GWidgetClass* wc,
                       const uint8_t* pc, const UCell** args )
{
    const UCell* command = bi->it++;
#ifdef EVAL_PATH
    UBuffer* pathArg;

    assert( glEnv.guiArgBlkN != UR_INVALID_BUF );
    pathArg = ur_buffer( glEnv.guiArgBlkN );
    pathArg->used = 0;
#endif

    while( 1 )
    {
        switch( *pc )
        {
            case GUIA_ARG:
            case GUIA_OPT:
                if( bi->it != bi->end && ur_is(bi->it, pc[1]) )
                    *args++ = bi->it++;
                else if( *pc == GUIA_OPT )
                    *args++ = 0;
                else
                    goto fail;
                pc += 2;
                break;

            case GUIA_ARGM:
            case GUIA_OPTM:
                if( bi->it != bi->end && _matchMultTypes( pc, bi->it ) )
                    *args++ = bi->it++;
                else if( *pc == GUIA_OPTM )
                    *args++ = 0;
                else
                    goto fail_mult;
                pc += 2 + pc[1];
                break;

            case GUIA_ARGW:
            //case GUIA_OPTW:
                if( bi->it == bi->end )
                    goto fail_end;
                {
                    const UCell* cell = bi->it;
                    if( ur_is(cell, UT_WORD) )
                    {
                        if( ! (cell = ur_wordCell( ut, cell )) )
                            return UR_THROW;
                    }
#ifdef EVAL_PATH
                    else if( ur_is(cell, UT_PATH) )
                    {
                        UCell* persist = ur_blkAppendNew( pathArg, UT_NONE );
                        if( ! ur_pathCell( ut, cell, persist ) )
                            return UR_THROW;
                        cell = persist;
                    }
                    else if( ur_is(cell, UT_PAREN) )
                    {
                        UCell* persist = ur_blkAppendNew( pathArg, UT_NONE );
                        if( ! boron_doBlock(ut, cell, persist) )
                            return UR_THROW;
                        cell = persist;
                    }
#endif
                    if( ! _matchMultTypes( pc, cell ) )
                        goto fail_mult;
                    *args++ = cell;
                    ++bi->it;
                }
                pc += 2 + pc[1];
                break;

            case GUIA_ANY:
                if( bi->it == bi->end )
                    goto fail_end;
                *args++ = bi->it++;
                ++pc;
                break;

            case GUIA_END:
                return UR_OK;

            default:
                assert( 0 && "Invalid GWidget argument program" );
                return UR_OK;
        }
    }

fail:
    return ur_error( ut, UR_ERR_TYPE, "Widget %s expected %s for argument %d",
                     wc->name, ur_atomCStr(ut, pc[1]), bi->it - command );

fail_mult:
    return ur_error( ut, UR_ERR_TYPE, "Widget %s argument %d is invalid",
                     wc->name, bi->it - command );

fail_end:
    return ur_error( ut, UR_ERR_SCRIPT, "Widget %s missing argument %d",
                     wc->name, bi->it - command );
}


//----------------------------------------------------------------------------


static void _propagateWindowCreated( UThread* ut, GWidget* wp,
                                     const GLViewEvent* ev )
{
    GWidget* cw;
    wp->flags |= GW_UPDATE_LAYOUT;
    EACH_CHILD( wp, cw )
        cw->wclass->dispatch( ut, cw, ev );
    EACH_END
}


void widget_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    if( ev->type == GUI_EVENT_WINDOW_CREATED )
        _propagateWindowCreated( ut, wp, ev );
    else
        gui_ignoreEvent( ev );
}


void widget_dispatchNul( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    (void) ut;
    (void) wp;
    gui_ignoreEvent( ev );
}


void widget_renderNul( GWidget* wp )
{
    (void) wp;
}


UStatus widget_setNul( UThread* ut, GWidget* wp, const UCell* val )
{
    (void) ut;
    (void) wp;
    (void) val;
    return UR_OK;
}


void widget_renderChildren( GWidget* wp )
{
    GWidget* it;

    EACH_SHOWN_CHILD( wp, it )
        it->wclass->render( it );
    EACH_END
}


/*
  Propagate widget area to children.
*/
static void widget_fullAreaLayout( GWidget* wp )
{
    GWidget* it;

#ifdef REPORT_LAYOUT
    printf( "----------------\n"
            "fullAreaLayout %p %s\t%d,%d,%d,%d\n",
            wp, wp->wclass->name,
            wp->area.x, wp->area.y, wp->area.w, wp->area.h );
#endif

    EACH_SHOWN_CHILD( wp, it )
        if( it->flags & GW_SELF_LAYOUT )
            continue;
        it->area = wp->area;
        it->wclass->layout( it );
        //wp->flags |= GW_UPDATE_LAYOUT;
    EACH_END
}


static void markLayoutDirty( GWidget* wp )
{
    while( wp )
    {
        wp->flags |= GW_UPDATE_LAYOUT;
        wp = wp->parent;
    }
}


int gui_areaSelect( GWidget* wp, UAtom atom, UCell* result )
{
    switch( atom )
    {
        case UR_ATOM_RECT:
        case UR_ATOM_SIZE:
        case UR_ATOM_POS:
            gui_initRectCoord( result, wp, atom );
            return UR_OK;
    }
    return ur_error( glEnv.guiUT, UR_ERR_SCRIPT, "Invalid widget selector '%s",
                     ur_atomCStr(glEnv.guiUT, atom) );
}


#if 0
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
#endif


extern CFUNC_PUB(cfunc_key_code);

#define keyModMask     number.id.ext
#define KEY_MOD_COUNT  3

static uint16_t glvModifierMask[ KEY_MOD_COUNT ] =
{
    GLV_MASK_ALT,
    GLV_MASK_CTRL,
    GLV_MASK_SHIFT
};

/*
   Return non-zero if cell is a valid key handler block.
*/
static int _remapKeys( UThread* ut, const UCell* cell, const UAtom* modAtom )
{
    if( ur_is(cell, UT_BLOCK) && (! ur_isShared(cell->series.buf)) )
    {
        UBlockIterM bi;
        UCell* out;
        int i;
        int mod = 0;

        ur_blkSliceM( ut, &bi, cell );
        out = bi.it;

        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_BLOCK) || ur_is(bi.it, UT_INT) )
            {
                // Skip code blocks and previously mapped keys (ints).
                if( bi.it != out )
                    *out = *bi.it;
                ++out;
                continue;
            }

            if( ur_is(bi.it, UT_WORD) )
            {
                UAtom atom = ur_atom(bi.it);
                for( i = 0; i < KEY_MOD_COUNT; ++i )
                {
                    if( atom == modAtom[i] )
                    {
                        mod |= glvModifierMask[i];
                        break;
                    }
                }
                if( i != KEY_MOD_COUNT )
                    continue;
            }

            cfunc_key_code( ut, bi.it, out );
            out->keyModMask = mod;
            //printf( "KR code %5d mod 0x%x\n", ur_int(out), mod );
            mod = 0;
            ++out;
        }
        bi.buf->used = out - bi.buf->ptr.cell;
        return 1;
    }
    return 0;
}


//----------------------------------------------------------------------------


#define expandMin   user16


/*-wid-
    expand  [minimum]
            int!

    Fills as much space as possible to push other widgets against parent
    edges.
*/
static GWidget* expand_make( UThread* ut, UBlockIter* bi,
                             const GWidgetClass* wclass, GWidget* parent )
{
    GWidget* wp;
    (void) ut;

    ++bi->it;
    wp = gui_allocWidget( sizeof(GWidget), wclass, parent );
    //wp->expandMin = 0;

    if( bi->it != bi->end && ur_is(bi->it, UT_INT) )
    {
        wp->expandMin = ur_int(bi->it);
        ++bi->it;
    }

    if( ! wp->expandMin )
        wp->flags |= GW_NO_SPACE;

    return wp;
}


static void expand_sizeHint( GWidget* wp, GSizeHint* size )
{
    size->minW    = size->minH    = wp->expandMin;
    size->maxW    = size->maxH    = GW_MAX_DIM;
    size->weightX = size->weightY = GW_WEIGHT_STD / 2;
    size->policyX = size->policyY = GW_POL_EXPANDING;
}


//----------------------------------------------------------------------------


#define spaceWeight     user16
#define spaceDim        user32


/*-wid-
    space   [weight minimum maximum]
            int!/coord!

    Fill some space to leave a gap.
*/
static GWidget* space_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass, GWidget* parent )
{
    GWidget* wp;
    int dmin = 8;
    int dmax = 24;
    (void) ut;

    ++bi->it;
    wp = gui_allocWidget( sizeof(GWidget), wclass, parent );
    wp->spaceWeight = GW_WEIGHT_STD;

    if( bi->it != bi->end )
    {
        const UCell* arg = bi->it;
        if( ur_is(arg, UT_INT) )
        {
            wp->spaceWeight = ur_int(arg);
            ++bi->it;
        }
        else if( ur_is(arg, UT_COORD) )
        {
            wp->spaceWeight = arg->coord.n[0];
            dmin = arg->coord.n[1];
            if( arg->coord.len > 2 )
                dmax = arg->coord.n[2];
            else
                dmax = dmin * 3;
            ++bi->it;
        }
    }

    wp->spaceDim = (dmax << 16) | dmin;

    return wp;
}


static void space_sizeHint( GWidget* wp, GSizeHint* size )
{
    size->minW    = size->minH    = wp->spaceDim & 0xffff;
    size->maxW    = size->maxH    = wp->spaceDim >> 16;
    size->weightX = size->weightY = wp->spaceWeight;
    size->policyX = size->policyY = GW_POL_WEIGHTED;
}


//----------------------------------------------------------------------------


enum EventContextIndex
{
    EI_EVENT,
    EI_KEY_DOWN,
    EI_KEY_UP,
    EI_MOUSE_MOVE,
    EI_MOUSE_DOWN,
    EI_MOUSE_UP,
    EI_MOUSE_WHEEL,
    EI_CLOSE,
    EI_RESIZE,
    EI_JOYSTICK,
    EI_DRAW,
    EI_COUNT
};


#define eventCtxN       user32


static int _installEventContext( UThread* ut, GWidget* wp, const UCell* blkC,
                                 int* eventMask )
{
    UAtom atoms[ EI_COUNT + KEY_MOD_COUNT ];
    UBuffer* blk;
    UBuffer* ctx;
    int i;
    int mask = 0;


    ur_internAtoms( ut, "event key-down key-up\n"
                    "mouse-move mouse-down mouse-up mouse-wheel\n"
                    "close resize joystick draw\n"
                    "alt ctrl shift",
                    atoms );

    wp->eventCtxN = ur_makeContext( ut, EI_COUNT );

    ctx = ur_buffer( wp->eventCtxN );
    for( i = 0; i < EI_COUNT; ++i )
        ur_ctxAppendWord( ctx, atoms[i] );
    ur_ctxSort( ctx );

    blk = ur_bufferSerM( blkC );
    if( ! blk )
        return UR_THROW;
    ur_bind( ut, blk, ctx, UR_BIND_THREAD );

    if( boron_doVoid( ut, blkC ) != UR_OK )
        return UR_THROW;

    ctx = ur_buffer( wp->eventCtxN );   // Re-aquire
    if( _remapKeys( ut, ur_ctxCell( ctx, EI_KEY_DOWN ), atoms + EI_COUNT ) )
        mask |= 1 << EI_KEY_DOWN;
    if( _remapKeys( ut, ur_ctxCell( ctx, EI_KEY_UP ), atoms + EI_COUNT ) )
        mask |= 1 << EI_KEY_UP;

    if( eventMask )
    {
        if( ur_is( ur_ctxCell(ctx, EI_MOUSE_MOVE), UT_BLOCK ) )
            mask |= 1 << EI_MOUSE_MOVE;
        *eventMask = mask;
    }

    return UR_OK;
}


static int eventContextSelect( GWidget* wp, UAtom atom, UCell* result )
{
    UIndex ctxN = wp->eventCtxN;

    if( atom == UR_ATOM_CONTEXT )
    {
        if( ctxN )
        {
            ur_setId( result, UT_CONTEXT );
            ur_setSeries( result, ctxN, 0 );
        }
        else
        {
            ur_setId( result, UT_NONE );
        }
        return UR_OK;
    }
    else if( ctxN )
    {
        UThread* ut = glEnv.guiUT;
        const UBuffer* ctx = ur_buffer( ctxN );
        int i = ur_ctxLookup( ctx, atom );
        if( i > -1 )
        {
            *result = *ur_ctxCell( ctx, i );
            return UR_OK;
        }
    }
    return gui_areaSelect( wp, atom, result );
}


static int mapButton( int code )
{
    switch( code )
    {
        case GLV_BUTTON_LEFT:   return 1;
        case GLV_BUTTON_RIGHT:  return 2;
        case GLV_BUTTON_MIDDLE: return 3;
    }
    return 0;
}


/*
  Select event handler from block created by _remapKeys().
*/
static const UCell* selectKeyHandler( UThread* ut, const UCell* cell,
                                      int code, int state )
{
    if( ur_is(cell, UT_BLOCK) )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, cell );
        if( (bi.end - bi.it) & 1 )
            --bi.end;

        // TODO: Sort key-handler block so a binary search can be used here.
        while( bi.it != bi.end )
        {
            if( ur_int(bi.it) == code )
            {
                int mod = bi.it->keyModMask;
                if( ! mod || ((mod & state) == mod) )
                    return bi.it + 1;
            }
            bi.it += 2;
        }
    }
    return 0;
}


static void eventContextDispatch( UThread* ut, GWidget* wp,
                                  const GLViewEvent* event )
{
    UCell* val;
    const UCell* cell;
    const UBuffer* ctx;

#define CCELL(N)     ur_ctxCell(ctx,N)

    if( ! wp->eventCtxN )
        goto ignore;

    ctx = ur_buffer( wp->eventCtxN );
    val = ur_ctxCell( ctx, EI_EVENT );

    switch( event->type )
    {
/*
        case GLV_EVENT_FOCUS_IN:
            ur_setId(val, UT_LOGIC);
            ur_logic(val) = 1;
            break;

        case GLV_EVENT_FOCUS_OUT:
            ur_setId(val, UT_LOGIC);
            ur_logic(val) = 0;
            break;
*/
        case GLV_EVENT_RESIZE:
            cell = CCELL( EI_RESIZE );
            if( ! ur_is(cell, UT_BLOCK) )
                return;

            // coord elements: x, y, width, height
            ur_initCoord(val, 4);
            val->coord.n[0] = wp->area.x;
            val->coord.n[1] = wp->area.y;
            val->coord.n[2] = event->x;
            val->coord.n[3] = event->y;
            break;

        case GLV_EVENT_CLOSE:
            cell = CCELL( EI_CLOSE );
            if( ! ur_is(cell, UT_BLOCK) )
                return;

            ur_setId(val, UT_NONE);
            break;

        case GLV_EVENT_BUTTON_DOWN:
            cell = CCELL( EI_MOUSE_DOWN );
            if( ! ur_is(cell, UT_BLOCK) )
                goto ignore;

            // coord elements: x, y, button
            ur_initCoord(val, 3);
            val->coord.n[0] = event->x /*- wp->area.x*/;
            val->coord.n[1] = event->y /*- wp->area.y*/;
            val->coord.n[2] = mapButton( event->code );
            break;

        case GLV_EVENT_BUTTON_UP:
            cell = CCELL( EI_MOUSE_UP );
            if( ! ur_is(cell, UT_BLOCK) )
                goto ignore;

            // coord elements: x, y, button
            ur_initCoord(val, 3);
            val->coord.n[0] = event->x /*- wp->area.x*/;
            val->coord.n[1] = event->y /*- wp->area.y*/;
            val->coord.n[2] = mapButton( event->code );
            break;

        case GLV_EVENT_MOTION:
            cell = CCELL( EI_MOUSE_MOVE );
            if( ! ur_is(cell, UT_BLOCK) )
                goto ignore;

            // coord elements: x, y, dx, dy, btn-mask
            ur_initCoord(val, 5);
            val->coord.n[0] = event->x /*- wp->area.x*/;
            val->coord.n[1] = event->y /*- wp->area.y*/;
            //val->coord.n[1] = (ui->rootH - event->y - 1) - wp->y;
            val->coord.n[2] = glEnv.mouseDeltaX;
            val->coord.n[3] = glEnv.mouseDeltaY;
            val->coord.n[4] = event->state;
            break;

        case GLV_EVENT_WHEEL:
            cell = CCELL( EI_MOUSE_WHEEL );
            if( ! ur_is(cell, UT_BLOCK) )
                goto ignore;

            ur_setId(val, UT_INT);
            ur_int(val) = event->y;
            break;

        case GLV_EVENT_KEY_DOWN:
            cell = CCELL( EI_KEY_DOWN );
            goto key_handler;

        case GLV_EVENT_KEY_UP:
            cell = CCELL( EI_KEY_UP );
key_handler:
            cell = selectKeyHandler( ut, cell, event->code, event->state );
            if( ! cell )
                goto ignore;

            ur_setId(val, UT_INT);
            ur_int(val) = event->code;
            break;

        default:
            return;
    }

    gui_doBlock( ut, cell );
    return;

ignore:

    gui_ignoreEvent( event );
}


static UStatus _makeChildren( UThread* ut, UBlockIter* bi, GWidget* parent,
                              UCell* res )
{
    GWidgetClass* wclass;
    GWidget* wp = 0;
    const UCell* cell;
    const UCell* setWord = 0;
    UAtom atom;


    while( bi->it != bi->end )
    {
        if( ur_is(bi->it, UT_SETWORD) )
        {
            setWord = bi->it++;
        }
        else if( ur_is(bi->it, UT_WORD) )
        {
            atom = ur_atom( bi->it );
            wclass = gui_widgetClass( atom );
            if( ! wclass )
            {
                if( atom == UR_ATOM_PARENT )
                {
                    if( ++bi->it == bi->end )
                    {
                        return ur_error( ut, UR_ERR_SCRIPT,
                                         "unexpected end of widget block" );
                    }
                    if( ur_is(bi->it, UT_WORD) )
                    {
                        if( ! (cell = ur_wordCell( ut, bi->it )) )
                            return UR_THROW;
                        if( ur_is(cell, UT_WIDGET) )
                            parent = ur_widgetPtr(cell);
                    }
                    ++bi->it;
                    continue;
                }
                else
                {
                    return ur_error( ut, UR_ERR_SCRIPT,
                                     "unknown widget class '%s",
                                     ur_wordCStr( bi->it ) );
                }
            }
            wp = wclass->make( ut, bi, wclass, parent );
            if( ! wp )
                return UR_THROW;

            if( setWord )
            {
                if( ! (cell = ur_wordCellM( ut, setWord )) )
                    return UR_THROW;
                ur_setId(cell, UT_WIDGET);
                ur_widgetPtr(cell) = wp;
                setWord = 0;
            }

            if( ! parent )
                wp->flags &= ~GW_CONSTRUCT;
        }
        else
        {
            return ur_error( ut, UR_ERR_TYPE,
                             "widget make expected name word! or set-word!" );
        }
    }

    if( res )
    {
        if( wp )
        {
            ur_setId(res, UT_WIDGET);
            ur_widgetPtr(res) = wp;
        }
        else
        {
            ur_setId(res, UT_NONE);
        }
    }
    return UR_OK;
}


//----------------------------------------------------------------------------



#define mouseGrabbed    wid.user16

typedef struct
{
    GWidget  wid;
    GWidget* keyFocus;
    GWidget* mouseFocus;
    GWidget* popupMenu;
    GLViewEvent lastMotion;
    char     motionTracking;
}
GUIRoot;


GWidgetClass wclass_root;

static GUIRoot* gui_rootGui( GWidget* wp )
{
    assert( wp );
    while( wp->parent )         // gui_root()
        wp = wp->parent;
    return (wp->wclass == &wclass_root) ? (GUIRoot*) wp : NULL;
}


static GWidget* root_make( UThread* ut, UBlockIter* bi,
                           const GWidgetClass* wclass, GWidget* parent )
{
    GUIRoot* ep = (GUIRoot*) gui_allocWidget( sizeof(GUIRoot), wclass, parent );
    /*
    // These are set to zero by gui_allocWidget.
    ep->keyFocus = ep->mouseFocus = 0;
    ep->mouseGrabbed = 0;
    ep->lastMotion.type = 0;
    ep->motionTracking = 0;
    */

    if( ++bi->it == bi->end )
        goto done;
    if( ur_is(bi->it, UT_BLOCK) )
    {
        if( ! _installEventContext( ut, (GWidget*) ep, bi->it, 0 ) )
            goto fail;
        ++bi->it;
    }

    if( ! _makeChildren( ut, bi, (GWidget*) ep, 0 ) )
        goto fail;

#if 0
    if( ep->wid.child )
        ep->keyFocus = ep->wid.child;
#endif

done:
    return (GWidget*) ep;

fail:
    return gui_makeFail( (GWidget*) ep );
}


static void root_free( GWidget* wp )
{
    GUIRoot* ep = (GUIRoot*) wp;
    memFree( ep->popupMenu );
    memFree( wp );
}


static void root_mark( UThread* ut, GWidget* wp )
{
    GUIRoot* ep = (GUIRoot*) wp;
    GWidget* popup;

    ur_markBlkN( ut, wp->eventCtxN );
    widget_markChildren( ut, wp );

    // If the popup menu is not currently active (i.e. linked as a child)
    // we must manually call it's mark method.
    popup = ep->popupMenu;
    if( popup && ! popup->parent )
        popup->wclass->mark( ut, popup );
}


static void _removeFocus( GUIRoot* ui, GWidget* wp )
{
    GWidget* it;

    if( ui->mouseFocus == wp )
    {
        ui->mouseFocus = 0;
        if( ui->mouseGrabbed )
            ui->mouseGrabbed = 0;
    }
    if( ui->keyFocus == wp )
    {
        ui->keyFocus = 0;
        //glv_showSoftInput( glEnv.view, 0 );
    }

    EACH_CHILD( wp, it )
        _removeFocus( ui, it );
    EACH_END
}


static void gui_removeFocus( GWidget* wp )
{
    GUIRoot* ui = gui_rootGui( wp );
    if( ui )
        _removeFocus( ui, wp );
}


static void sendFocusEvent( GWidget* wp, int eventType )
{
    if( wp )
    {
        GLViewEvent me;
        INIT_EVENT( me, eventType, 0, 0, 0, 0 );
        wp->wclass->dispatch( glEnv.guiUT, wp, &me );
    }
}


static void gui_setMouseFocus( GUIRoot* ui, GWidget* wp )
{
    if( ui->mouseFocus != wp )
    {
        sendFocusEvent( ui->mouseFocus, GLV_EVENT_FOCUS_OUT );
        ui->mouseFocus = wp;
        sendFocusEvent( wp, GLV_EVENT_FOCUS_IN );
        //printf( "KR mouse focus %s\n", wp ? wp->wclass->name : "NUL" );
    }
}


void gui_setKeyFocus( GWidget* wp )
{
    GUIRoot* ui = gui_rootGui( wp );
    if( ui )
    {
        ui->keyFocus = wp;
        //glv_showSoftInput( glEnv.view, 1 );
    }
}


/*
  \param keyFocus  Also set keyboard focus if non-zero.
*/
void gui_grabMouse( GWidget* wp, int keyFocus )
{
    GUIRoot* ui = gui_rootGui( wp );
    if( ui )
    {
        ui->mouseFocus = wp;
        ui->mouseGrabbed = 1;
        if( keyFocus )
            ui->keyFocus = wp;
    }
}


void gui_ungrabMouse( GWidget* wp )
{
    GUIRoot* ui = gui_rootGui( wp );
    if( ui )
    {
        if( ui->mouseFocus == wp )
            ui->mouseGrabbed = 0;
    }
}


int gui_hasFocus( GWidget* wp )
{
    GUIRoot* ui = gui_rootGui( wp );
    int mask = 0;
    if( ui )
    {
        if( ui->keyFocus == wp )
            mask |= GW_FOCUS_KEY;
        if( ui->mouseFocus == wp )
        {
            mask |= GW_FOCUS_MOUSE;
            if( ui->mouseGrabbed )
                mask |= GW_FOCUS_GRAB;
        }
    }
    return mask;
}


extern GWidgetClass wclass_menu;
extern void menu_present( GWidget* wp, GWidget* owner );

void gui_showMenu( GWidget* wp, UIndex dataBlkN, uint16_t selItem )
{
    GWidget* menu;
    GUIRoot* ui = gui_rootGui( wp );
    if( ui )
    {
        menu = ui->popupMenu;
        if( ! menu )
        {
            UCell arg[2];
            UBlockIter bi;
            UThread* ut = glEnv.guiUT;

            ur_setId( arg, UT_WORD );
            ur_setWordUnbound( arg, UT_WIDGET /*UR_ATOM_MENU*/ );

            ur_setId( arg+1, UT_BLOCK );
            ur_setSeries( arg+1, dataBlkN, selItem );

            bi.buf = NULL;
            bi.it  = arg;
            bi.end = arg + 2;

            menu = ui->popupMenu =
                wclass_menu.make( ut, &bi, &wclass_menu, &ui->wid );
        }
        else
        {
            // NOTE: menu_hide calls gui_unlink to remove itself from parent.
            gui_appendChild( &ui->wid, menu );
        }

        menu_present( menu, wp );

        // gui_grabMouse( menu, 1 );
        ui->mouseFocus = menu;
        ui->mouseGrabbed = 1;
        ui->keyFocus = menu;
    }
}


/*
  Returns child of wp under event x,y or zero if no active child contains
  the point.
*/
static GWidget* activeChildAt( GWidget* wp, const GLViewEvent* ev,
                               int childrenOverlap )
{
    GWidget* it;
    GWidget* top = 0;

    EACH_CHILD( wp, it )
        // If a widget is disabled so are all it's children.
        if( it->flags & (GW_HIDDEN | GW_DISABLED) )
            continue;
        if( gui_widgetContains( it, ev->x, ev->y ) )
        {
            top = it;
            if( ! childrenOverlap )
                goto found;
        }
    EACH_END

    if( top )
    {
found:
        wp = activeChildAt( top, ev, 0 );
        if( wp && ! IGNORES_INPUT(wp) )
            return wp;
        if( IGNORES_INPUT(top) )
            top = 0;
    }
    return top;
}


static GWidget* _updateFocus( GUIRoot* ui )
{
    GWidget* fw = ui->mouseFocus;
    const GLViewEvent* mev = &ui->lastMotion;

    if( mev->type != GLV_EVENT_MOTION )
        mev = 0;

    if( fw )
    {
        if( ui->mouseGrabbed )
            return fw;
        if( mev && gui_widgetContains( fw, mev->x, mev->y ) )
        {
            if( (fw = activeChildAt( fw, mev, 1 )) )
               goto change_focus;
            return ui->mouseFocus;
        }
    }

    if( mev && (fw = activeChildAt( &ui->wid, mev, 1 )) )
    {
change_focus:
        //printf( "KR _updateMouseFocus %p\n", fw );
        gui_setMouseFocus( ui, fw );
        ui->keyFocus = 0;
    }
    return fw;
}


static void root_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    GWidget* cw;
    GUIRoot* ep = (GUIRoot*) wp;

#if 0
    printf( "root_dispatch type:%d code:%d xy:%d,%d focus (mouse:%p key:%p)\n",
            ev->type, ev->code, ev->x, ev->y, ep->mouseFocus, ep->keyFocus );
#endif

    switch( ev->type )
    {
        case GLV_EVENT_RESIZE:
            if( (ev->x != wp->area.w) || (ev->y != wp->area.h) )
            {
                wp->flags |= GW_UPDATE_LAYOUT;
                wp->area.w = ev->x;
                wp->area.h = ev->y;
            }
            break;

        //case GLV_EVENT_CLOSE:

        case GLV_EVENT_BUTTON_DOWN:
            ++ep->motionTracking;
            if( ep->mouseGrabbed && (cw = ep->mouseFocus) )
                goto dispatch;

            cw = activeChildAt( wp, ev, 1 );
            gui_setMouseFocus( ep, cw );
            if( (cw = ep->mouseFocus) )
                goto dispatch_used;
            break;

        case GLV_EVENT_BUTTON_UP:
            --ep->motionTracking;
            if( ep->mouseGrabbed && (cw = ep->mouseFocus) )
                goto dispatch;
            // Fall through...

        case GLV_EVENT_WHEEL:
            if( (cw = _updateFocus( ep )) )
                goto dispatch_used;
            break;

        case GLV_EVENT_MOTION:
            if( ep->mouseGrabbed && (cw = ep->mouseFocus) )
                goto dispatch;
            if( ! ep->motionTracking )
            {
                ep->lastMotion = *ev;
                return;
            }
            /*
            cw = activeChildAt( wp, ev, 1 );
            gui_setMouseFocus( ep, cw );
            if( cw )
                goto dispatch_used;
            */
            break;

        case GLV_EVENT_KEY_DOWN:
#if 0
            if( ev->code == XK_Find )
                gui_dumpWidget( wp, 0 );
#endif
        case GLV_EVENT_KEY_UP:
            if( (cw = ep->keyFocus) )
                goto dispatch_used;
            break;

        case GLV_EVENT_FOCUS_IN:
            ep->mouseFocus = activeChildAt( wp, ev, 1 );
            // Fall through...

        case GLV_EVENT_FOCUS_OUT:
            ep->motionTracking = 0;
            cw = ep->mouseFocus;
            if( cw )
                goto dispatch;
            return;

        case GUI_EVENT_WINDOW_CREATED:
            _propagateWindowCreated( ut, wp, ev );
            return;
    }

dispatch_ctx:

    eventContextDispatch( ut, wp, ev );
    return;

dispatch:

    cw->wclass->dispatch( ut, cw, ev );
    return;

dispatch_used:

    cw->wclass->dispatch( ut, cw, ev );
    if( ev->type & GUI_EVENT_IGNORE )
    {
        gui_acceptEvent( ev );
        do
        {
            cw = cw->parent;
            if( cw == wp )
                goto dispatch_ctx;
        }
        while( IGNORES_INPUT(cw) );
        goto dispatch_used;
    }
}


static void root_sizeHint( GWidget* wp, GSizeHint* size )
{
    size->minW    =
    size->maxW    = wp->area.w;
    size->minH    =
    size->maxH    = wp->area.h;
    size->weightX =
    size->weightY = GW_WEIGHT_FIXED;
    size->policyX =
    size->policyY = GW_POL_FIXED;
}


static void root_render( GWidget* wp )
{
    if( wp->eventCtxN )
    {
        UThread* ut = glEnv.guiUT;
        const UBuffer* ctx = ur_buffer( wp->eventCtxN );
        const UCell* cell = ur_ctxCell( ctx, EI_DRAW );
        if( ur_is(cell, UT_DRAWPROG) )
        {
            ur_runDrawProg( ut, cell->series.buf );
        }
    }

    if( wp->flags & GW_UPDATE_LAYOUT )
    {
        wp->flags &= ~GW_UPDATE_LAYOUT;
        widget_fullAreaLayout( wp );
    }

    widget_renderChildren( wp );
}


//----------------------------------------------------------------------------


#define BOX_MEMBERS \
    uint16_t marginL; \
    uint16_t marginT; \
    uint16_t marginR; \
    uint16_t marginB; \
    uint8_t  spacing; \
    uint8_t  align; \
    uint8_t  rows; \
    uint8_t  cols;

typedef struct
{
    GWidget wid;
    BOX_MEMBERS
}
Box;

#define EX_PTR  Box* ep = (Box*) wp

#define GW_ALIGN_L      0x01
#define GW_ALIGN_R      0x02
#define GW_ALIGN_CH     0x03
#define GW_ALIGN_B      0x04
#define GW_ALIGN_T      0x08
#define GW_ALIGN_CV     0x0C


static void setBoxAlign( Box* wd, const UCell* ac )
{
    switch( ur_atom(ac) )
    {
        case UR_ATOM_LEFT:
            wd->align = GW_ALIGN_L;
            break;
        case UR_ATOM_RIGHT:
            wd->align = GW_ALIGN_R;
            break;
        case UR_ATOM_CENTER:
            wd->align = GW_ALIGN_CH | GW_ALIGN_CV;
            break;
        case UR_ATOM_BOTTOM:
            wd->align = GW_ALIGN_B;
            break;
        case UR_ATOM_TOP:
            wd->align = GW_ALIGN_T;
            break;
    }
}


/*
  mc must be a coord! with 2-5 elements.

    left&right, top&bottom [,spacing]
    left, top, right, bottom [,spacing]
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


static GWidget* box_make2( UThread* ut, const GWidgetClass* wclass,
                           GWidget* parent,
                           const UCell* margin, const UCell* align,
                           const UCell* child, const UCell* grid )
{
    Box* ep = (Box*) gui_allocWidget( sizeof(Box), wclass, parent );
    /*
    ep->marginL = 0;
    ep->marginT = 0;
    ep->marginR = 0;
    ep->marginB = 0;
    */
    ep->spacing = 8;

    if( margin )
        setBoxMargins( ep, margin );

    if( align )
        setBoxAlign( ep, align );

    if( grid )
    {
        ep->rows = grid->coord.n[1];
        ep->cols = grid->coord.n[0];
    }

    if( ! gui_makeWidgets( ut, child, (GWidget*) ep, 0 ) )
        return gui_makeFail( (GWidget*) ep );

    return (GWidget*) ep;
}


/*-wid-
    hbox [margins] [/center]  children
          coord!    option!   block!
*/
/*-wid-
    vbox [margins] [/center]  children
          coord!    option!   block!
*/
static const uint8_t box_args[] =
{
    GUIA_OPT,   UT_COORD,
    GUIA_OPT,   UT_OPTION,
    GUIA_ARG,   UT_BLOCK,
    GUIA_END
};

static GWidget* box_make( UThread* ut, UBlockIter* bi,
                          const GWidgetClass* wclass, GWidget* parent )
{
    const UCell* arg[ 3 ];
    if( ! gui_parseArgs( ut, bi, wclass, box_args, arg ) )
        return NULL;
    return box_make2( ut, wclass, parent, arg[0], arg[1], arg[2], NULL );
}


static void hbox_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    GWidget* it;
    GSizeHint cs;
    int count = 0;
    int marginH = ep->marginT + ep->marginB;

    size->minW    = ep->marginL + ep->marginR;
    size->minH    = marginH;
    size->maxW    = size->minW;
    size->maxH    = marginH;
    size->weightX = GW_WEIGHT_STD;
    size->weightY = GW_WEIGHT_STD;
    size->policyX = GW_POL_WEIGHTED;
    size->policyY = GW_POL_WEIGHTED;

    EACH_SHOWN_CHILD( wp, it )
        it->wclass->sizeHint( it, &cs );
        if( ! (it->flags & GW_NO_SPACE) )   // cs.minW != 0
            ++count;
        size->minW += cs.minW;

        cs.minH += marginH;
        if( size->minH < cs.minH )
            size->minH = cs.minH;

        cs.maxH += marginH;
        if( size->maxH < cs.maxH )
            size->maxH = cs.maxH;

        // Accumulate maxW unless an expander is present.
        if( size->maxW != GW_MAX_DIM )
        {
            if( cs.policyX == GW_POL_EXPANDING )
                size->maxW = GW_MAX_DIM;
            else if( cs.policyX == GW_POL_FIXED )
                size->maxW += cs.minW;
            else
                size->maxW += cs.maxW;
        }
    EACH_END

    if( count > 1 )
    {
        int space = (count - 1) * ep->spacing;
        size->minW += space;
        if( size->maxW != GW_MAX_DIM )
            size->maxW += space;
    }
}


#define MAX_LO_WIDGETS 16

typedef struct
{
    int count;
    int spaceCount;
    int required;           // Sum of hint minW or minH.
    int widgetWeight;       // Sum of non-expanding hint weightX or weightY.
    int expandWeight;       // Sum of expanding hint weightX or weightY.
    GSizeHint hint[ MAX_LO_WIDGETS ];
}
LayoutData;


/*
  Collect sizeHint structures of visible children.
*/
static void layout_query( GWidget* wp, LayoutData* lo )
{
    GWidget* it;
    GSizeHint* hint = lo->hint;

    lo->count = lo->spaceCount = 0;

    EACH_SHOWN_CHILD( wp, it )
        assert( lo->count < MAX_LO_WIDGETS );
        it->wclass->sizeHint( it, hint );

#ifdef REPORT_HINT
        printf( "hint %p %s\tmin:%d,%d max:%d,%d wt:%d,%d pol:%d,%d\n",
                it, it->wclass->name, hint->minW, hint->minH,
                hint->maxW, hint->maxH, hint->weightX, hint->weightY,
                hint->policyX, hint->policyY );
#endif

        if( ! (it->flags & GW_NO_SPACE) )
            ++lo->spaceCount;
        ++lo->count;
        ++hint;
    EACH_END
}


/*
  Set LayoutData required, widgetWeight, & expandWeight for a given axis.

  \param axis  'x' or 'y' for horizontal or vertical.
*/
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
            if( it->policyY == GW_POL_EXPANDING )
                ew += it->weightY;
            else
                weight += it->weightY;
            used += it->minH;
            ++it;
        }
    }
    else
    {
        while( it != end )
        {
            if( it->policyX == GW_POL_EXPANDING )
                ew += it->weightX;
            else
                weight += it->weightX;
            used += it->minW;
            ++it;
        }
    }
    lo->required = used;
    lo->widgetWeight = weight;
    lo->expandWeight = ew;
}


static void hbox_layout( GWidget* wp )
{
    EX_PTR;
    GWidget* it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;
    int align;

    layout_query( wp, &lo );
    layout_stats( &lo, 'x' );

    room = wp->area.w - ep->marginL - ep->marginR -
           (ep->spacing * (lo.spaceCount - 1));

    // Compute and store children area width in lo.hint[i].minW.
    if( room > lo.required )
    {
        //int excess = room - lo.required;
        int avail = room;
        hint = lo.hint;
        hend = hint + lo.count;
        while( hint != hend )
        {
            if( hint->policyX == GW_POL_WEIGHTED )
            {
                dim = (room * hint->weightX) / lo.widgetWeight;
                if( dim > hint->maxW )
                    hint->minW = hint->maxW;
                else if( dim > hint->minW )
                    hint->minW = dim;
                avail -= hint->minW;
            }
            else if( hint->policyX == GW_POL_FIXED )
            {
                avail -= hint->minW;
            }
            ++hint;
        }

        if( avail > 0 && lo.expandWeight )
        {
            // Allocate unused space to expanders.
            hint = lo.hint;
            while( hint != hend )
            {
                if( hint->policyX == GW_POL_EXPANDING )
                {
                    hint->minW = (avail * hint->weightX) / lo.expandWeight;
                }
                ++hint;
            }
        }
    }

    dim = wp->area.x + ep->marginL;
    room = wp->area.h - ep->marginT - ep->marginB;
    align = ep->align & GW_ALIGN_CV;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = dim;
        cr.y = wp->area.y + ep->marginB;
        cr.w = hint->minW;
        cr.h = MIN( hint->maxH, room );

        if( cr.h < room )
        {
            switch( align )
            {
                case GW_ALIGN_T:
                    cr.y += room - cr.h;
                    break;
                case GW_ALIGN_CV:
                    cr.y += (room - cr.h) / 2;
                    break;
            }
        }

        dim += cr.w;
        if( ! (it->flags & GW_NO_SPACE) )
            dim += ep->spacing;

#ifdef REPORT_LAYOUT
        printf( "  hbox layout  %p %s\t%d,%d,%d,%d\n",
                it, it->wclass->name, cr.x, cr.y, cr.w, cr.h );
#endif
#if 1
        // Always call layout to recompile DL.
        it->area = cr;
        it->wclass->layout( it );
#else
        if( areaUpdate( it, &cr ) )
            it->wclass->layout( it );
#endif
        ++hint;
    EACH_END
}


//----------------------------------------------------------------------------


static void vbox_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    GWidget* it;
    GSizeHint cs;
    int count = 0;
    int marginW = ep->marginL + ep->marginR;

    size->minW    = marginW;
    size->minH    = ep->marginT + ep->marginB;
    size->maxW    = marginW;
    size->maxH    = size->minH;
    size->weightX = GW_WEIGHT_STD;
    size->weightY = GW_WEIGHT_STD;
    size->policyX = GW_POL_WEIGHTED;
    size->policyY = GW_POL_WEIGHTED;

    EACH_SHOWN_CHILD( wp, it )
        it->wclass->sizeHint( it, &cs );
        if( ! (it->flags & GW_NO_SPACE) )   // cs.minH != 0
            ++count;
        size->minH += cs.minH;

        cs.minW += marginW;
        if( size->minW < cs.minW )
            size->minW = cs.minW;

        cs.maxW += marginW;
        if( size->maxW < cs.maxW )
            size->maxW = cs.maxW;

        // Accumulate maxH unless an expander is present.
        if( size->maxH != GW_MAX_DIM )
        {
            if( cs.policyY == GW_POL_EXPANDING )
                size->maxH = GW_MAX_DIM;
            else if( cs.policyY == GW_POL_FIXED )
                size->maxH += cs.minH;
            else
                size->maxH += cs.maxH;
        }
    EACH_END

    if( count > 1 )
    {
        int space = (count - 1) * ep->spacing;
        size->minH += space;
        if( size->maxH != GW_MAX_DIM )
            size->maxH += space;
    }
}


static void vbox_layout( GWidget* wp )
{
    EX_PTR;
    GWidget* it;
    GSizeHint* hint;
    GSizeHint* hend;
    LayoutData lo;
    GRect cr;
    int dim;
    int room;
    int align;

    layout_query( wp, &lo );
    layout_stats( &lo, 'y' );

    room = wp->area.h - ep->marginT - ep->marginB -
           (ep->spacing * (lo.spaceCount - 1));

    // Compute and store children area height in lo.hint[i].minH.
    if( room > lo.required )
    {
        //int excess = room - lo.required;
        int avail = room;
        hint = lo.hint;
        hend = hint + lo.count;
        while( hint != hend )
        {
            if( hint->policyY == GW_POL_WEIGHTED )
            {
                dim = (room * hint->weightY) / lo.widgetWeight;
                if( dim > hint->maxH )
                    hint->minH = hint->maxH;
                else if( dim > hint->minH )
                    hint->minH = dim;
                avail -= hint->minH;
            }
            else if( hint->policyY == GW_POL_FIXED )
            {
                avail -= hint->minH;
            }
            ++hint;
        }

        if( avail > 0 && lo.expandWeight )
        {
            // Allocate unused space to expanders.
            hint = lo.hint;
            while( hint != hend )
            {
                if( hint->policyY == GW_POL_EXPANDING )
                {
                    hint->minH = (avail * hint->weightY) / lo.expandWeight;
                }
                ++hint;
            }
        }
    }

    dim = wp->area.y + wp->area.h - ep->marginT;
    room = wp->area.w - ep->marginL - ep->marginR;
    align = ep->align & GW_ALIGN_CH;
    hint = lo.hint;
    EACH_SHOWN_CHILD( wp, it )
        cr.x = wp->area.x + ep->marginL;
        cr.w = MIN( hint->maxW, room );
        cr.h = hint->minH;
        cr.y = dim - cr.h;

        if( cr.w < room )
        {
            switch( align )
            {
                case GW_ALIGN_R:
                    cr.x += room - cr.w;
                    break;
                case GW_ALIGN_CH:
                    cr.x += (room - cr.w) / 2;
                    break;
            }
        }

        dim -= cr.h;
        if( ! (it->flags & GW_NO_SPACE) )
            dim -= ep->spacing;

#ifdef REPORT_LAYOUT
        printf( "  vbox layout  %p %s\t%d,%d,%d,%d\n",
                it, it->wclass->name, cr.x, cr.y, cr.w, cr.h );
#endif
#if 1
        // Always call layout to recompile DL.
        it->area = cr;
        it->wclass->layout( it );
#else
        if( areaUpdate( it, &cr ) )
            it->wclass->layout( it );
#endif
        ++hint;
    EACH_END
}


//----------------------------------------------------------------------------


/*-wid-
    grid size   [margins]   children
         coord! coord!      block!

    Size is colums,rows.
*/
static const uint8_t grid_args[] =
{
    GUIA_ARG,   UT_COORD,
    GUIA_OPT,   UT_COORD,
    GUIA_ARG,   UT_BLOCK,
    GUIA_END
};


static GWidget* grid_make( UThread* ut, UBlockIter* bi,
                           const GWidgetClass* wclass, GWidget* parent )
{
    const UCell* arg[ 3 ];
    if( ! gui_parseArgs( ut, bi, wclass, grid_args, arg ) )
        return NULL;
    return box_make2( ut, wclass, parent, arg[1], NULL, arg[2], arg[0] );
}


static int grid_stats( GWidget* wp, int cols, int16_t* colMin, int16_t* rowMin,
                       int16_t* colPol, int16_t* rowPol )
{
    GSizeHint cs;
    GWidget* it;
    int count = 0;
    int row = 0;
    int col = 0;

    EACH_SHOWN_CHILD( wp, it )
        it->wclass->sizeHint( it, &cs );
        if( ! (it->flags & GW_NO_SPACE) )   // cs.minH != 0
            ++count;

        if( rowMin[ row ] < cs.minH )
            rowMin[ row ] = cs.minH;
        if( colMin[ col ] < cs.minW )
            colMin[ col ] = cs.minW;

        if( colPol )
        {
            if( colPol[ col ] < cs.policyX )
                colPol[ col ] = cs.policyX;
        }
        if( rowPol )
        {
            if( rowPol[ row ] < cs.policyY )
                rowPol[ row ] = cs.policyY;
        }

        if( ++col == cols )
        {
            col = 0;
            ++row;
        }
    EACH_END

    return count;
}


static int _int16Sum( const int16_t* it, int n )
{
    const int16_t* end = it + n;
    for( n = 0; it != end; ++it )
        n += *it;
    return n;
}


static int _int16Max( const int16_t* it, int n )
{
    const int16_t* end = it + n;
    for( n = 0; it != end; ++it )
    {
        if( *it > n )
            n = *it;
    }
    return n;
}


typedef struct
{
    int16_t colMin[64];
    int16_t* colPol;
    int16_t* rowMin;
    int16_t* rowPol;
}
GridStats;


static void grid_initStats( GridStats* gs, int cols, int rows )
{
    int cells = cols * 2 + rows * 2;
    assert( cells < 64 );
    memset( gs->colMin, 0, sizeof(int16_t) * cells );

    gs->colPol = gs->colMin + cols;
    gs->rowMin = gs->colPol + cols;
    gs->rowPol = gs->rowMin + rows;
}


static void grid_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    GridStats gs;

    size->minW    = ep->marginL + ep->marginR;
    size->minH    = ep->marginT + ep->marginB;
    size->maxW    = GW_MAX_DIM;
    size->maxH    = GW_MAX_DIM;
    size->weightX = GW_WEIGHT_STD;
    size->weightY = GW_WEIGHT_STD;

    grid_initStats( &gs, ep->cols, ep->rows );
    grid_stats( wp, ep->cols, gs.colMin, gs.rowMin, gs.colPol, gs.rowPol );

    size->minW += _int16Sum( gs.colMin, ep->cols );
    size->minH += _int16Sum( gs.rowMin, ep->rows );

    if( ep->cols > 1 )
        size->minW += (ep->cols - 1) * ep->spacing;
    if( ep->rows > 1 )
        size->minH += (ep->rows - 1) * ep->spacing;

    size->policyX = _int16Max( gs.colPol, ep->cols );
    size->policyY = _int16Max( gs.rowPol, ep->rows );
}


static void _distributeSpace( int16_t* val, int16_t* pol, int n, int space )
{
    int div, rem, i;
    int total = _int16Sum( val, n );
    if( total < space )
    {
        int resizable = 0;
        for( i = 0; i < n; ++i )
        {
            if( pol[i] != GW_POL_FIXED )
                ++resizable;
        }
        if( ! resizable )
            return;
        space -= total;
        div = space / resizable;
        rem = space % resizable;
        for( i = 0; i < n; ++i )
        {
            if( pol[i] == GW_POL_FIXED )
                continue;
            val[i] += div;
            if( rem )
            {
                --rem;
                ++val[i];
            }
        }
    }
}


static void grid_layout( GWidget* wp )
{
    EX_PTR;
    GWidget* it;
    GSizeHint cs;
    GRect cr;
    GridStats gs;
    int row, col;
    int sx = wp->area.x + ep->marginL;
    int sy = wp->area.y + wp->area.h - ep->marginT;


    grid_initStats( &gs, ep->cols, ep->rows );
    grid_stats( wp, ep->cols, gs.colMin, gs.rowMin, gs.colPol, gs.rowPol );
    _distributeSpace( gs.colMin, gs.colPol, ep->cols,
                      wp->area.w - (ep->marginL + ep->marginR) -
                      ((ep->cols - 1) * ep->spacing) );
    _distributeSpace( gs.rowMin, gs.rowPol, ep->rows,
                      wp->area.h - (ep->marginT + ep->marginB) -
                      ((ep->rows - 1) * ep->spacing) );

    row = col = 0;
    EACH_SHOWN_CHILD( wp, it )
        it->wclass->sizeHint( it, &cs );

        cr.x = sx + _int16Sum( gs.colMin, col );
        if( col )
            cr.x += (col - 1) * ep->spacing;

        cr.y = sy - _int16Sum( gs.rowMin, row + 1 );
        if( row )
            cr.y -= row * ep->spacing;

        if( cs.policyX == GW_POL_FIXED )
        {
            cr.w = cs.minW;
            cr.x += (gs.colMin[ col ] - cs.minW) / 2;
        }
        else
            cr.w = gs.colMin[ col ];

        if( cs.policyY == GW_POL_FIXED )
        {
            cr.h = cs.minH;
            cr.y += (gs.rowMin[ row ] - cs.minH) / 2;
        }
        else
            cr.h = gs.rowMin[ row ];

#ifdef REPORT_LAYOUT
        printf( "  grid layout  %p %s\t%d,%d,%d,%d\n",
                it, it->wclass->name, cr.x, cr.y, cr.w, cr.h );
#endif
        // Always call layout to recompile DL.
        it->area = cr;
        it->wclass->layout( it );

        if( ++col == ep->cols )
        {
            col = 0;
            ++row;
        }
    EACH_END
}


//----------------------------------------------------------------------------


typedef struct
{
    GWidget wid;
    BOX_MEMBERS
    UIndex  dp[2];
    UIndex  titleN;
    int16_t dragX;
    int16_t dragY;
    int16_t transX;
    int16_t transY;
}
GWindow;

#undef EX_PTR
#define EX_PTR  GWindow* ep = (GWindow*) wp

#define WINDOW_HBOX     GW_FLAG_USER1
#define WINDOW_DRAG     GW_FLAG_USER2
#define WINDOW_PRESENT  GW_FLAG_USER3


/*-wid-
   window [name]   event-handler  children
          string!  none!/block!   block! 

   Free-floating movable window.
*/
static const uint8_t window_args[] =
{
    GUIA_OPT,      UT_STRING,
    GUIA_ARGW, 2,  UT_BLOCK, UT_NONE,
    GUIA_ARGW, 1,  UT_BLOCK,
    GUIA_END
};

static GWidget* window_make( UThread* ut, UBlockIter* bi,
                             const GWidgetClass* wclass, GWidget* parent )
{
    GWindow* ep;
    const UCell* arg[ 3 ];

    if( ! gui_parseArgs( ut, bi, wclass, window_args, arg ) )
        return NULL;

    ep = (GWindow*) gui_allocWidget( sizeof(GWindow), wclass, parent );

    ep->marginL = 8;
    ep->marginT = 8;
    ep->marginR = 8;
    ep->marginB = 8;
    ep->spacing = 0;

    ep->dp[0] = ur_makeDrawProg( ut );

    /* TODO: Support path?
    if( ur_sel(arg) == UR_ATOM_HBOX )
        ep->wid.flags |= WINDOW_HBOX;
    */

    if( arg[0] )
    {
        ep->titleN = arg[0]->series.buf;
        //glv_setTitle( glEnv.view, boron_cstr( ut, arg, 0 ) );
    }

    if( ur_is(arg[1], UT_BLOCK) )
    {
        if( ! _installEventContext( ut, (GWidget*) ep, arg[1], 0 ) )
            goto fail;
    }

    if( gui_makeWidgets( ut, arg[2], (GWidget*) ep, 0 ) )   // gc!
        return (GWidget*) ep;

fail:
    return gui_makeFail( (GWidget*) ep );
}


static void window_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBuffer( ut, ep->dp[0] );
    if( ep->titleN > UR_INVALID_BUF )   // Also acts as (! ur_isShared(n))
        ur_markBuffer( ut, ep->titleN );

    ur_markBlkN( ut, wp->eventCtxN );
    widget_markChildren( ut, wp );
}


static void window_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                EX_PTR;
                ep->dragX = ev->x;
                ep->dragY = ev->y;
                ep->transX = ep->transY = 0;
                wp->flags |= WINDOW_DRAG;
                gui_grabMouse( wp, 1 );
                return;
            }
            break;

        case GLV_EVENT_BUTTON_UP:
            if( wp->flags & WINDOW_DRAG )
            {
                EX_PTR;
                if( ev->code == GLV_BUTTON_LEFT )
                {
                    gui_move( wp, wp->area.x + ep->transX,
                                  wp->area.y + ep->transY );
end_drag:
                    ep->transX = ep->transY = 0;
                    wp->flags &= ~WINDOW_DRAG;
                    gui_ungrabMouse( wp );
                    return;
                }
                else if( ev->code == GLV_BUTTON_RIGHT )
                {
                    // Cancel drag.
                    goto end_drag;
                }
            }
            break;

        case GLV_EVENT_MOTION:
            if( wp->flags & WINDOW_DRAG )
            {
                EX_PTR;
                const GRect* area;
                if( wp->parent )
                {
                    area = &wp->parent->area;
                    ep->transX = gui_limitX(ev->x, area) - ep->dragX;
                    ep->transY = gui_limitY(ev->y, area) - ep->dragY;
                }
                return;
            }
            break;

        case GUI_EVENT_WINDOW_CREATED:
            _propagateWindowCreated( ut, wp, ev );
            return;
    }
    eventContextDispatch( ut, wp, ev );
}


static void window_sizeHint( GWidget* wp, GSizeHint* size )
{
    if( wp->flags & WINDOW_HBOX )
        hbox_sizeHint( wp, size );
    else
        vbox_sizeHint( wp, size );
}


static void window_layout( GWidget* wp )
{
    GSizeHint hint;
    EX_PTR;
    DPCompiler* save;
    DPCompiler dpc;
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    UThread* ut = glEnv.guiUT;


    save = ur_beginDP( &dpc );

    rc = style + CI_STYLE_WINDOW_MARGIN;
    if( ur_is(rc, UT_COORD) )
        setBoxMargins( (Box*) ep, rc );

    window_sizeHint( wp, &hint );
    if( wp->area.w < hint.minW )
        wp->area.w = hint.minW;
    if( wp->area.h < hint.minH )
        wp->area.h = hint.minH;

    if( wp->flags & WINDOW_PRESENT )
    {
        // Initial window placement.
        // For now this means centering it on screen.  In the future there
        // could be some presentation policy.
        wp->flags &= ~WINDOW_PRESENT;
        if( wp->parent )
        {
            wp->area.x = (wp->parent->area.w - wp->area.w) / 2;
            wp->area.y = (wp->parent->area.h - wp->area.h) / 2;
        }
    }

    // Set draw list variables.
    rc = style + CI_STYLE_LABEL;
    ur_initSeries( rc, UT_STRING, ep->titleN );

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );

    // Compile draw list.

    rc = style + CI_STYLE_WINDOW;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );

    if( wp->flags & WINDOW_HBOX )
        hbox_layout( wp );
    else
        vbox_layout( wp );

    ur_endDP( ut, ur_buffer(ep->dp[0]), save );
}


static void window_render( GWidget* wp )
{
    EX_PTR;
    UCell* rc;
    int drag;


    if( ! (glEnv.guiStyle = gui_style( glEnv.guiUT )) )
        return;

    if( wp->flags & GW_UPDATE_LAYOUT )
    {
        wp->flags &= ~GW_UPDATE_LAYOUT;
        window_layout( wp );
    }

    rc = glEnv.guiStyle + CI_STYLE_START_DL;
    if( ur_is(rc, UT_DRAWPROG) )
        ur_runDrawProg( glEnv.guiUT, ur_drawProgN(rc) );

    drag = wp->flags & WINDOW_DRAG;
    if( drag )
    {
        glPushMatrix();
        glTranslatef( (GLfloat) ep->transX, (GLfloat) ep->transY, 0.0f );
    }

    ur_runDrawProg( glEnv.guiUT, ep->dp[0] );
    widget_renderChildren( wp );

    if( drag )
        glPopMatrix();
}


//----------------------------------------------------------------------------


#define VIEWPORT_KEYS   GW_FLAG_USER1
#define VIEWPORT_MOVE   GW_FLAG_USER2


/*-wid-
   viewport event-handler
            block! 
*/
static const uint8_t viewport_args[] =
{
    GUIA_ARGW, 1,  UT_BLOCK,
    GUIA_END
};


static GWidget* viewport_make( UThread* ut, UBlockIter* bi,
                               const GWidgetClass* wclass, GWidget* parent )
{
    GWidget* wp;
    const UCell* arg[ 1 ];
    int emask;

    if( ! gui_parseArgs( ut, bi, wclass, viewport_args, arg ) )
        return NULL;

    wp = gui_allocWidget( sizeof(GWidget), wclass, parent );

    wp->expandMin = 0;      // For expand_sizeHint().

    if( _installEventContext( ut, wp, arg[0], &emask ) )
    {
        if( emask & ((1 << EI_KEY_DOWN) | (1 << EI_KEY_UP)) )
            wp->flags |= VIEWPORT_KEYS;
        if( emask & (1 << EI_MOUSE_MOVE) )
            wp->flags |= VIEWPORT_MOVE;
        return wp;
    }

    return gui_makeFail( wp );
}


static void viewport_mark( UThread* ut, GWidget* wp )
{
    ur_markBlkN( ut, wp->eventCtxN );
    widget_markChildren( ut, wp );
}


static void viewport_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    if( ev->type == GLV_EVENT_BUTTON_DOWN )
    {
        int trackKeys = wp->flags & VIEWPORT_KEYS;
        if( wp->flags & VIEWPORT_MOVE )
            gui_grabMouse( wp, trackKeys );
        else if( trackKeys )
            gui_setKeyFocus( wp );
    }
    else if( ev->type == GLV_EVENT_BUTTON_UP )
    {
        if( wp->flags & VIEWPORT_MOVE )
            gui_ungrabMouse( wp );
    }

    eventContextDispatch( ut, wp, ev );
}


static void viewport_layout( GWidget* wp )
{
    GLViewEvent me;
    INIT_EVENT( me, GLV_EVENT_RESIZE, 0, 0, wp->area.w, wp->area.h );
    eventContextDispatch( glEnv.guiUT, wp, &me );
    widget_fullAreaLayout( wp );
}


static void viewport_render( GWidget* wp )
{
    UThread* ut = glEnv.guiUT;
    const UBuffer* ctx = ur_buffer( wp->eventCtxN );
    const UCell* cell = ur_ctxCell( ctx, EI_DRAW );
    if( ur_is(cell, UT_DRAWPROG) )
    {
        ur_runDrawProg( ut, cell->series.buf );
    }

    widget_renderChildren( wp );
}


//----------------------------------------------------------------------------


/*-wid-
    overlay layout  [margins]  [/center]  children
            word!    coord!     option!   block!

    Place widgets over the parent area.

    Layout is 'hbox or 'vbox.
*/
static const uint8_t overlay_args[] =
{
    GUIA_ARG,   UT_WORD,
    GUIA_OPT,   UT_COORD,
    GUIA_OPT,   UT_OPTION,
    GUIA_ARG,   UT_BLOCK,
    GUIA_END
};

static GWidget* overlay_make( UThread* ut, UBlockIter* bi,
                              const GWidgetClass* wclass, GWidget* parent )
{
    GWindow* ep;
    const UCell* arg[ 4 ];

    if( ! gui_parseArgs( ut, bi, wclass, overlay_args, arg ) )
        return NULL;

    ep = (GWindow*) gui_allocWidget( sizeof(GWindow), wclass, parent );

    ep->dp[0] = ur_makeDrawProg( ut );

    if( ur_wordCStr( arg[0] )[ 0 ] == 'h' )
        ep->wid.flags |= WINDOW_HBOX;

    if( arg[1] )
        setBoxMargins( (Box*) ep, arg[1] );

    if( arg[2] )
        setBoxAlign( (Box*) ep, arg[2] );

    if( gui_makeWidgets( ut, arg[3], (GWidget*) ep, 0 ) )  // gc!
        return (GWidget*) ep;

    return gui_makeFail( (GWidget*) ep );
}


static void overlay_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBuffer( ut, ep->dp[0] );
    widget_markChildren( ut, wp );
}


static void overlay_layout( GWidget* wp )
{
    EX_PTR;
    DPCompiler* save;
    DPCompiler dpc;
    UThread* ut = glEnv.guiUT;


#ifdef REPORT_LAYOUT
    printf( "  overlay      %p %s\t%d,%d,%d,%d\n",
            wp, wp->wclass->name,
            wp->area.x, wp->area.y, wp->area.w, wp->area.h );
#endif

    if( ! glEnv.guiStyle )
    {
        // Delay layout until render.
        wp->flags |= GW_UPDATE_LAYOUT;
        return;
    }

    save = ur_beginDP( &dpc );

    //wp->area = wp->parent->area;

    if( wp->flags & WINDOW_HBOX )
        hbox_layout( wp );
    else
        vbox_layout( wp );

    ur_endDP( ut, ur_buffer(ep->dp[0]), save );
}


static void overlay_render( GWidget* wp )
{
    EX_PTR;
    UCell* rc;

    if( ! (glEnv.guiStyle = gui_style( glEnv.guiUT )) )
        return;

    if( wp->flags & GW_UPDATE_LAYOUT )
    {
        wp->flags &= ~GW_UPDATE_LAYOUT;
        overlay_layout( wp );
    }

    rc = glEnv.guiStyle + CI_STYLE_START_DL;
    if( ur_is(rc, UT_DRAWPROG) )
        ur_runDrawProg( glEnv.guiUT, ur_drawProgN(rc) );

    ur_runDrawProg( glEnv.guiUT, ep->dp[0] );
    widget_renderChildren( wp );
}


//----------------------------------------------------------------------------


GWidgetClass wclass_root =
{
    "root",
    root_make,              root_free,          root_mark,
    root_dispatch,          root_sizeHint,      widget_layoutNul,
    root_render,            eventContextSelect, widget_setNul,
    0, GW_UPDATE_LAYOUT
};


GWidgetClass wclass_hbox =
{
    "hbox",
    box_make,               widget_free,        widget_markChildren,
    widget_dispatch,        hbox_sizeHint,      hbox_layout,
    widget_renderChildren,  gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_NO_INPUT
};


GWidgetClass wclass_vbox =
{
    "vbox",
    box_make,               widget_free,        widget_markChildren,
    widget_dispatch,        vbox_sizeHint,      vbox_layout,
    widget_renderChildren,  gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_NO_INPUT
};


GWidgetClass wclass_grid =
{
    "grid",
    grid_make,              widget_free,        widget_markChildren,
    widget_dispatch,        grid_sizeHint,      grid_layout,
    widget_renderChildren,  gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_NO_INPUT
};


GWidgetClass wclass_window =
{
    "window",
    window_make,            widget_free,        window_mark,
    window_dispatch,        window_sizeHint,    window_layout,
    window_render,          eventContextSelect, widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_SELF_LAYOUT | WINDOW_PRESENT
};


GWidgetClass wclass_viewport =
{
    "viewport",
    viewport_make,          widget_free,        viewport_mark,
    viewport_dispatch,      expand_sizeHint,    viewport_layout,
    viewport_render,        eventContextSelect, widget_setNul,
    0, GW_UPDATE_LAYOUT
};


GWidgetClass wclass_overlay =
{
    "overlay",
    overlay_make,       widget_free,        overlay_mark,
    widget_dispatch,    window_sizeHint,    overlay_layout,
    overlay_render,     gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT //| GW_SELF_LAYOUT
};


GWidgetClass wclass_expand =
{
    "expand",
    expand_make,        widget_free,        widget_markNul,
    widget_dispatchNul, expand_sizeHint,    widget_layoutNul,
    widget_renderNul,   gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_NO_INPUT
};


GWidgetClass wclass_space =
{
    "space",
    space_make,         widget_free,        widget_markNul,
    widget_dispatchNul, space_sizeHint,     widget_layoutNul,
    widget_renderNul,   gui_areaSelect,     widget_setNul,
    0, GW_UPDATE_LAYOUT | GW_NO_INPUT
};


extern GWidgetClass wclass_button;
extern GWidgetClass wclass_checkbox;
extern GWidgetClass wclass_choice;
extern GWidgetClass wclass_label;
extern GWidgetClass wclass_lineedit;
extern GWidgetClass wclass_list;
extern GWidgetClass wclass_slider;
extern GWidgetClass wclass_scrollbar;
extern GWidgetClass wclass_itemview;
/*
    "console",
    "option",
    "data",         // label
    "data-edit",    // spin box, line editor
*/

void gui_addStdClasses()
{
    GWidgetClass* classes[ 19 ];
    GWidgetClass** wp = classes;

    *wp++ = &wclass_root;
    *wp++ = &wclass_expand;
    *wp++ = &wclass_space;
    *wp++ = &wclass_hbox;
    *wp++ = &wclass_vbox;
    *wp++ = &wclass_grid;
    *wp++ = &wclass_window;
    *wp++ = &wclass_viewport;
    *wp++ = &wclass_overlay;
    *wp++ = &wclass_button;
    *wp++ = &wclass_checkbox;
    *wp++ = &wclass_choice;
    *wp++ = &wclass_label;
    *wp++ = &wclass_lineedit;
    *wp++ = &wclass_list;
    *wp++ = &wclass_menu;
    *wp++ = &wclass_slider;
    *wp++ = &wclass_scrollbar;
    *wp++ = &wclass_itemview;

    gui_addWidgetClasses( classes, wp - classes );
}


//----------------------------------------------------------------------------


/*
  Return draw-prog buffer index or UR_INVALID_BUF.
*/
UIndex gui_parentDrawProg( GWidget* wp )
{
    while( (wp = wp->parent) )
    {
        if( wp->wclass == &wclass_window ||
            wp->wclass == &wclass_overlay )
            return ((GWindow*) wp)->dp[0];
    }
    return UR_INVALID_BUF;
}


/*
  Return pointer to X,Y values or 0 if there is no parent window.
*/
const int16_t* gui_parentTranslation( GWidget* wp )
{
    while( (wp = wp->parent) )
    {
        if( wp->wclass == &wclass_window ||
            wp->wclass == &wclass_overlay )
            return &((GWindow*) wp)->transX;
    }
    return 0;
}


#if 0
/*
  The select method returns non-zero if result is set, or zero if error.
  The select method must not throw errors.
*/
int gui_selectAtom( GWidget* wp, UAtom atom, UCell* result )
{
    return wp->wclass->select( wp, atom, result );
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

    if( ! boron_doBlock( ut, blkC, ur_push(ut, UT_UNSET) ) )
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
    ur_pop(ut);
}


void gui_doBlockN( UThread* ut, UIndex blkN )
{
    UCell* cell = ut->stack.ptr.cell + ut->stack.used;
    ur_setId(cell, UT_BLOCK);
    ur_setSeries(cell, blkN, 0);
    ur_setId(cell+1, UT_UNSET);
    ut->stack.used += 2;
    if( ! boron_doBlock( ut, cell, cell+1 ) )
    {
        UR_GUI_THROW;
    }
    ut->stack.used -= 2;
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
    assert( child->parent == NULL );
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
        wp->parent = prev = 0;
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
            prev = it;
        }
    }
}


/*
  Hide widget, remove it from any parent, and mark it to be freed on
  the next recycle.
*/
void gui_freeWidgetDefer( GWidget* wp )
{
    // NOTE: widget_mark() will remove any references during recycle.
    if( wp )
    {
        gui_removeFocus( wp );
        if( wp->parent )
        {
            gui_unlink( wp );
            ur_arrAppendPtr( &glEnv.rootWidgets, wp );
        }
        wp->flags |= GW_HIDDEN | GW_DISABLED | GW_DESTRUCT;
    }
}


/*
  Send GUI_EVENT_WINDOW_CREATED to all widgets so that they can re-create
  any OpenGL resources and be flagged for layout update.
*/
void gui_signalWindowCreated( int w, int h )
{
    GLViewEvent me;
    GWidget** it  = (GWidget**) glEnv.rootWidgets.ptr.v;
    GWidget** end = it + glEnv.rootWidgets.used;
    GWidget* wp;

    INIT_EVENT( me, GUI_EVENT_WINDOW_CREATED, 0, 0, w, h );
    while( it != end )
    {
        wp = *it++;
        wp->wclass->dispatch( glEnv.guiUT, wp, &me );
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


void gui_show( GWidget* wp, int show )
{
    int hidden = wp->flags & GW_HIDDEN;
    if( show )
    {
        if( hidden )
        {
            wp->flags &= ~GW_HIDDEN;
            markLayoutDirty( wp->parent );
        }
    }
    else if( ! hidden )
    {
        gui_removeFocus( wp );
        wp->flags |= GW_HIDDEN;
        markLayoutDirty( wp->parent );
    }
}


void gui_move( GWidget* wp, int x, int y )
{
    wp->area.x = x;
    wp->area.y = y;
    markLayoutDirty( wp );
}


void gui_resize( GWidget* wp, int w, int h )
{
    if( w != wp->area.w || h != wp->area.h )
    {
        GLViewEvent me;
        INIT_EVENT( me, GLV_EVENT_RESIZE, 0, 0, w, h );
        wp->wclass->dispatch( glEnv.guiUT, wp, &me );
    }
}


UBuffer* gui_styleContext( UThread* ut )
{
    UCell* cc = ur_ctxValueOfType( ur_threadContext(ut), UR_ATOM_GUI_STYLE,
                                   UT_CONTEXT );
    return cc ? ur_bufferSerM(cc) : NULL;
}


/*
  Return pointer to the cells of the gui-style context.
*/
UCell* gui_style( UThread* ut )
{
    const UBuffer* ctx = gui_styleContext( ut );
    return ctx ? ctx->ptr.cell : NULL;
}


void gui_addWidgetClasses( GWidgetClass** classTable, int count )
{
    UBuffer* ctx = &glEnv.widgetClasses;
    UBuffer atoms;
    int i;
    int index;

    {
    UBuffer* str = &glEnv.tmpStr;
    str->used = 0;
    ur_arrInit( &atoms, sizeof(UAtom), count );
    for( i = 0; i < count; ++i )
    {
        if( i )
            ur_strAppendChar( str, ' ' );
        ur_strAppendCStr( str, classTable[ i ]->name );
    }
    ur_strTermNull( str );
    ur_internAtoms( glEnv.guiUT, str->ptr.c, atoms.ptr.u16 );
    }

    ur_ctxReserve( ctx, ctx->used + count );
    for( i = 0; i < count; ++i )
    {
        GWidgetClass* wc = *classTable++;
        wc->nameAtom = atoms.ptr.u16[ i ];
        index = ur_ctxAddWordI( ctx, wc->nameAtom );
        ((GWidgetClass**) ctx->ptr.v)[ index ] = wc;
    }
    ur_ctxSort( ctx );

    ur_arrFree( &atoms );
}


GWidgetClass* gui_widgetClass( UAtom name )
{
    int i = ur_ctxLookup( &glEnv.widgetClasses, name );
    if( i < 0 )
        return 0;
    return ((GWidgetClass**) glEnv.widgetClasses.ptr.v)[ i ];
}


/*
  \param blkC       Valid block with widget layout language.
  \param parent     Pointer to parent or NULL if none.
  \param result     If not NULL then place widget! or none! value here.

  \return UR_OK/UR_THROW
*/
UStatus gui_makeWidgets( UThread* ut, const UCell* blkC, GWidget* parent,
                         UCell* result )
{
    UBlockIter bi;

#ifdef EVAL_PATH
    if( glEnv.guiArgBlkN == UR_INVALID_BUF )
    {
        glEnv.guiArgBlkN = ur_makeBlock( ut, 4 );
        ur_hold( glEnv.guiArgBlkN );
    }
#endif

    ur_blkSlice( ut, &bi, blkC );
    return _makeChildren( ut, &bi, parent, result );
}


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
#endif


// EOF
