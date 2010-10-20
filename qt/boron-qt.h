#ifndef QBORON_H
#define QBORON_H
/*============================================================================
    Boron Qt Module
    Copyright (C) 2005-2009  Karl Robillard

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
============================================================================*/


#include "boron.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>
#include <QPushButton>
#include <QProgressBar>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeView>
#include <QTextEdit>


enum eWidgetType
{
    WT_Null,
    WT_Button,
    WT_Dialog,
    WT_Label,
    WT_LineEdit,
    WT_CheckBox,
    WT_SpinBox,
    WT_Combo,
    WT_Tab,
    WT_TextEdit,
    WT_TreeView,
    WT_Group,
    WT_Progress,
    WT_Watcher,
    WT_Widget
};


class WIDPool
{
public:

    enum eRecFlags
    {
        DeleteObject = 0x0001
    };

    struct REC
    {
        short type;
        short flags;
        union
        {
            QObject* object;
            QWidget* widget;
        };
    };

    WIDPool() : _freeCount(0), _freeIndex(-1) {}

    int  add( QObject*, int type, int flags = 0 );
    void remove( int id );
    REC* record( int id );

    typedef QVector<REC>::iterator  iterator;
    iterator begin() { return _records.begin(); }
    iterator end()   { return _records.end(); }

private:

    int _freeCount;
    int _freeIndex;
    QVector<REC> _records; 
};


struct QtEnv
{
    UThread* ut;

    UAtom atom_close;
    UAtom atom_exec_exit;
    UAtom atom_ready;
    UAtom atom_removed;
    UAtom atom_created;
    UAtom atom_changed;

    UIndex layoutRules; 
    UIndex comboCtxN;

    QWidget* curWidget;
    QDialog* dialog;
    QString  filePath;     // Saves directory selected in last request-file.
    WIDPool  pool;

    QEventLoop* loop;
};


class BoronApp : public QApplication
{
    Q_OBJECT

public:

    BoronApp( int& argc, char** argv );
};


class ExeBlock
{
public:
    ExeBlock() : n(0), hold(UR_INVALID_HOLD) {}
    ~ExeBlock();

    void setBlock( const UCell* );
    void switchWord( UAtom );

    UIndex n;
    UIndex index;
    UIndex hold;
};


class SButton : public QPushButton
{
    Q_OBJECT

public:

    SButton();
    ~SButton();

    int _wid;

    void setBlock( const UCell* v );

public slots:

    void slotDo();

private:

    ExeBlock _blk;
};


class SCombo : public QComboBox
{
    Q_OBJECT

public:

    SCombo();
    ~SCombo();

    int _wid;

    void setBlock( const UCell* );

public slots:

    void slotDo( int );

private:

    ExeBlock _blk;
};


class STreeView : public QTreeView
{
    Q_OBJECT

public:

    STreeView();
    ~STreeView();

    int _wid;

    void setBlock( const UCell* );

public slots:

    void slotDo( const QModelIndex& );

private:

    ExeBlock _blk;
};


class SWidget : public QWidget
{
    Q_OBJECT

public:

    SWidget();
    ~SWidget();

    int _wid;

    void setEventBlock( const UCell* );

protected:

    void closeEvent( QCloseEvent* );

private:

    ExeBlock _blk;
};


#define DEF_WIDGET(oo,qo) \
    class oo : public qo { public: oo(); ~oo(); int _wid; }


DEF_WIDGET(SCheck,QCheckBox);
DEF_WIDGET(SSpinBox,QSpinBox);
DEF_WIDGET(SDialog,QDialog);
DEF_WIDGET(SGroup,QGroupBox);
DEF_WIDGET(SLabel,QLabel);
DEF_WIDGET(SLineEdit,QLineEdit);
DEF_WIDGET(STabWidget,QTabWidget);
DEF_WIDGET(STextEdit,QTextEdit);
DEF_WIDGET(SProgress,QProgressBar);


extern QtEnv qEnv;

extern void boron_initQt( UThread* );
extern void boron_freeQt();
extern void boron_doBlockQt( UThread*, const UCell* blkC );


#endif /*QBORON_H*/
