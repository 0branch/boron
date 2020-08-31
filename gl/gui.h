#ifndef GUI_H
#define GUI_H
/*
  Boron OpenGL GUI
  Copyright 2010-2012 Karl Robillard

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


#include <glv.h>
#include "urlan.h"


#define GUI_DCLICK_TIME 0.5


enum GUIEvent
{
    GUI_EVENT_TIMER = GLV_EVENT_USER,
    GUI_EVENT_JOYSTICK,
    GUI_EVENT_WINDOW_CREATED,
    GUI_EVENT_WINDOW_DESTROYED,
    GUI_EVENT_MENU_SELECTION
};


// Order must match gui-style-proto in boot.b.
enum ContextIndexStyle
{
    CI_STYLE_WIDGET_SH,     // shader!
    CI_STYLE_TEXTURE,       // texture!
    CI_STYLE_TEX_SIZE,      // coord!
    CI_STYLE_CONTROL_FONT,  // font!
    CI_STYLE_TITLE_FONT,    // font!
    CI_STYLE_EDIT_FONT,     // font!
    CI_STYLE_LIST_FONT,     // font!
    CI_STYLE_LABEL,         // Variable for use by draw lists.
    CI_STYLE_AREA,          // Variable for use by draw lists.
    CI_STYLE_START_DL,      // Draw list
    CI_STYLE_WINDOW_MARGIN, // coord!
    CI_STYLE_WINDOW,        // Draw list
    CI_STYLE_BUTTON_SIZE,   // coord! (minW, minH, init-maxW)
    CI_STYLE_BUTTON_UP,     // Draw list
    CI_STYLE_BUTTON_DOWN,   // Draw list
    CI_STYLE_BUTTON_HOVER,  // Draw list
    CI_STYLE_CHECKBOX_SIZE, // coord!
    CI_STYLE_CHECKBOX_UP,   // Draw list
    CI_STYLE_CHECKBOX_DOWN, // Draw list
    CI_STYLE_CHOICE_SIZE,   // coord! (minW, minH, init-maxW)
    CI_STYLE_CHOICE,        // Draw list
    CI_STYLE_CHOICE_ITEM,   // Draw list
    CI_STYLE_CMENU_BG,      // Draw list
    CI_STYLE_CMENU_ITEM_SELECTED, // Draw list
    CI_STYLE_MENU_MARGIN,   // coord!
    CI_STYLE_MENU_BG,       // Draw list
    CI_STYLE_MENU_ITEM_SELECTED, // Draw list
    CI_STYLE_LABEL_DL,      // Draw list
    CI_STYLE_EDITOR,        // Draw list
    CI_STYLE_EDITOR_ACTIVE, // Draw list
    CI_STYLE_EDITOR_CURSOR, // Draw list
    CI_STYLE_LIST_BG,       // Draw list
    CI_STYLE_LIST_HEADER,   // Draw list
    CI_STYLE_LIST_ITEM,     // Draw list
    CI_STYLE_LIST_ITEM_SELECTED, // Draw list
    CI_STYLE_SLIDER_SIZE,   // coord!
    CI_STYLE_SLIDER,        // Draw list
    CI_STYLE_SLIDER_GROOVE, // Draw list
    CI_STYLE_SCROLL_SIZE,   // coord!
    CI_STYLE_SCROLL_BAR,    // Draw list
    CI_STYLE_SCROLL_KNOB    // Draw list
};


enum GUIArgumentOp
{
    GUIA_ARG,       // Argument of one specific type.
    GUIA_ARGM,      // Argument matching more than one type.
    GUIA_ARGW,      // Arg. from word!/path!/paren! matching one or more types.
    GUIA_OPT,       // Optional argument of one specific type.
    GUIA_OPTM,      // Optional argument matching more than one type.
    GUIA_ANY,       // Skip one value of any type.
    GUIA_END        // Terminator of parse program.
};


#define ur_widgetPtr(c) ((UCellWidget*) (c))->wp

typedef struct GWidget GWidget;

typedef struct
{
    UCellId  id;
    uint32_t pad;
    GWidget* wp;
}
UCellWidget;


#define GW_MAX_DIM      0x7fff
#define GW_WEIGHT_STD   4
#define GW_WEIGHT_FIXED 1

enum GUISizePolicy
{
    GW_POL_FIXED,       // Size set to min.
    GW_POL_WEIGHTED,    // Size between min & max, based on relative weight.
    GW_POL_EXPANDING    // Similar to WEIGHTED but also allocated excess space.
};


typedef struct
{
    int16_t minW;
    int16_t minH;
    int16_t maxW;
    int16_t maxH;
    uint8_t weightX;
    uint8_t weightY;
    uint8_t policyX;
    uint8_t policyY;
}
GSizeHint;


typedef struct
{
    int16_t x, y, w, h;
}
GRect;


typedef void (*GRenderFunc)( GWidget* );
typedef void (*GMarkFunc)  ( UThread*, GWidget* );
typedef struct GWidgetClass GWidgetClass;

struct GWidgetClass
{
    const char* name;
    GWidget* (*make)    ( UThread*, UBlockIter*, const GWidgetClass*,
                          GWidget* );
    void     (*free)    ( GWidget* );
    void     (*mark)    ( UThread*, GWidget* );
    void     (*dispatch)( UThread*, GWidget*, const GLViewEvent* );
    void     (*sizeHint)( GWidget*, GSizeHint* );
    void     (*layout)  ( GWidget* );
    void     (*render)  ( GWidget* );
    int      (*select)  ( GWidget*, UAtom, UCell* );
    UStatus  (*set)     ( UThread*, GWidget*, const UCell* );
    UAtom       nameAtom;
    uint16_t    flags;
};


#define GW_HIDDEN        0x0001
#define GW_DISABLED      0x0002
#define GW_UPDATE_LAYOUT 0x0004
#define GW_RECYCLE       0x0008
#define GW_NO_SPACE      0x0010
#define GW_NO_INPUT      0x0020
#define GW_SELF_LAYOUT   0x0040
#define GW_DESTRUCT      0x0080
#define GW_FLAG_USER1    0x0100
#define GW_FLAG_USER2    0x0200
#define GW_FLAG_USER3    0x0400
#define GW_CONTEXT_MENU  0x4000
#define GW_CONSTRUCT     0x8000


struct GWidget
{
    const GWidgetClass* wclass;
    GWidget*  parent;
    GWidget*  child;
    GWidget*  next;
    GRect     area;
    uint16_t  flags;
    uint16_t  user16;
    uint32_t  user32;
};


// gui_hasFocus values
#define GW_FOCUS_KEY     0x0001
#define GW_FOCUS_MOUSE   0x0002
#define GW_FOCUS_GRAB    0x0004

#define GUI_EVENT_IGNORE    0x1000
#define gui_ignoreEvent(ev)     ((GLViewEvent*) ev)->type |= GUI_EVENT_IGNORE
#define gui_acceptEvent(ev)     ((GLViewEvent*) ev)->type &= 0x0fff
#define gui_value(ut)   ur_ctxCell(ur_threadContext(ut), glEnv.guiValueIndex)

// gui_showMenu selItem
#define GUI_ITEM_UNSET  0xffff

void gui_addWidgetClasses( GWidgetClass** classTable, int count );
GWidgetClass* gui_widgetClass( UAtom name );
UStatus gui_makeWidgets( UThread*, const UCell* blkC, GWidget* parent,
                         UCell* result );
UStatus gui_parseArgs( UThread*, UBlockIter*, const GWidgetClass*,
                       const uint8_t* pc, const UCell** args );

void gui_doBlock( UThread*, const UCell* );
void gui_doBlockN( UThread*, UIndex );

GWidget* gui_allocWidget( int size, const GWidgetClass*, GWidget* parent );
GWidget* gui_makeFail( GWidget* );
void     gui_freeWidget( GWidget* );
void     gui_freeWidgetDefer( GWidget* );
void     gui_appendChild( GWidget* parent, GWidget* child );
void     gui_enable( GWidget*, int active );
void     gui_show( GWidget*, int show );
void     gui_move( GWidget*, int x, int y );
void     gui_resize( GWidget*, int w, int h );
int      gui_widgetContains( const GWidget*, int x, int y );
UBuffer* gui_styleContext( UThread* );
UCell*   gui_style( UThread* );
void     gui_initRectCoord( UCell* cell, GWidget*, UAtom what );
int      gui_areaSelect( GWidget*, UAtom atom, UCell* result );

void     gui_render( UThread*, GWidget* );
GWidget* gui_root( GWidget* );
void     gui_setKeyFocus( GWidget* );
/*
void     gui_setMouseFocus( GWidget* );
*/
void     gui_grabMouse( GWidget*, int keyFocus );
void     gui_ungrabMouse( GWidget* );
void     gui_signalWindowCreated( int w, int h );
int      gui_hasFocus( GWidget* );
void     gui_showMenu( GWidget*, int px, int py, UIndex dataBlkN,
                       uint16_t selItem );
UIndex   gui_parentDrawProg( GWidget* );
const int16_t* gui_parentTranslation( GWidget* );
int      gui_limitX( int x, const GRect* );
int      gui_limitY( int y, const GRect* );

void widget_free( GWidget* );
void widget_markChildren( UThread*, GWidget* );
void widget_dispatch( UThread*, GWidget*, const GLViewEvent* );
void widget_dispatchNul( UThread*, GWidget*, const GLViewEvent* );
void widget_renderNul( GWidget* );
UStatus widget_setNul( UThread*, GWidget*, const UCell* );
void widget_renderChildren( GWidget* );
#define widget_markNul      0
#define widget_layoutNul    widget_renderNul


#endif /*GUI_H*/
