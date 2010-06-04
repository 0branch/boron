#ifndef GUI_H
#define GUI_H
/*
  Boron OpenGL GUI
  Copyright 2010 Karl Robillard

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


#include <GL/glv.h>
#include "urlan.h"


enum GUIEvent
{
    GUI_EVENT_TIMER = GLV_EVENT_USER,
    GUI_EVENT_JOYSTICK
};


// Order must match gui-style-proto in gx.t.
enum ContextIndexStyle
{
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
    CI_STYLE_BUTTON_SIZE,   // coord!
    CI_STYLE_BUTTON_UP,     // Draw list
    CI_STYLE_BUTTON_DOWN,   // Draw list
    CI_STYLE_BUTTON_HOVER,  // Draw list
    CI_STYLE_CHECKBOX_SIZE, // coord!
    CI_STYLE_CHECKBOX_UP,   // Draw list
    CI_STYLE_CHECKBOX_DOWN, // Draw list
    CI_STYLE_LABEL_DL,      // Draw list
    CI_STYLE_EDITOR,        // Draw list
    CI_STYLE_EDITOR_ACTIVE, // Draw list
    CI_STYLE_EDITOR_CURSOR, // Draw list
    CI_STYLE_LIST_HEADER,   // Draw list
    CI_STYLE_LIST_ITEM,     // Draw list
    CI_STYLE_LIST_ITEM_SELECTED // Draw list
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


typedef struct
{
    UThread*  ut;
    GWidget*  keyFocus;
    GWidget*  mouseFocus;
    UCell*    style;
    UAtom     atom_gui_style;
    UAtom     atom_hbox;
    UAtom     atom_action;
    UAtom     atom_text;
    UAtom     atom_selection;
    int16_t   rootW;
    int16_t   rootH;
    char      mouseGrabbed;
}
GUI;


#define GW_FIXED        0
#define GW_WEIGHTED     1
#define GW_EXPANDING    2

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


typedef void (*GRenderFunc)( GUI*, GWidget* );
typedef void (*GMarkFunc)  ( UThread*, GWidget* );

typedef struct
{
    const char* name;
    GWidget* (*make)    ( UThread*, UBlockIter* );
    void     (*free)    ( GWidget* );
    void     (*mark)    ( UThread*, GWidget* );
    void     (*dispatch)( UThread*, GWidget*, const GLViewEvent* );
    void     (*sizeHint)( GWidget*, GSizeHint* );
    void     (*layout)  ( UThread*, GWidget* );
    void     (*render)  ( GUI*, GWidget* );
    int      (*select)  ( GWidget*, UAtom, UCell* );
    UAtom       nameAtom;
    uint16_t    flags;
}
GWidgetClass;


#define GW_HIDDEN        0x0001
#define GW_DISABLED      0x0002
#define GW_UPDATE_LAYOUT 0x0004
#define GW_RECYCLE       0x0008
//#define GW_PARENT        0x0010
#define GW_FLAG_USER1    0x0100
#define GW_FLAG_USER2    0x0200


struct GWidget
{
    GWidgetClass* wclass;
    GWidget*  parent;
    GWidget*  child;
    GWidget*  next;
    GRect     area;
    uint16_t  flags;
};


void gui_init( GUI*, UThread* );
void gui_cleanup( GUI* );
void gui_dispatch( GUI*, GLViewEvent* );
void gui_resizeRootArea( GUI*, int width, int height );
void gui_doBlock( UThread*, const UCell* );

GWidget* gui_allocWidget( int size, GWidgetClass* );
void     gui_freeWidget( GWidget* );
void     gui_appendChild( GWidget* parent, GWidget* child );
void     gui_enable( GWidget*, int active );
void     gui_show( GWidget*, int show );
int      gui_widgetContains( const GWidget*, int x, int y );
/*
void     gui_render( GUI*, GWidgetId );
int      gui_selectAtom( GUI*, GWidget*, UAtom atom, UCell* result );
*/
GWidget* gui_root( GWidget* );
void     gui_setKeyFocus( GUI*, GWidget* );
void     gui_setMouseFocus( GUI*, GWidget* );
void     gui_grabMouse( GUI*, GWidget* );
void     gui_ungrabMouse( GUI*, GWidget* );
/*
UIndex   gui_rootDP( const GUI*, GWidgetId parent );
*/


// gui_styleCell may only be used by sizeHint, layout, & render methods.
#define gui_styleCell(index)   (ui->style + (index))


#endif /*GUI_H*/
