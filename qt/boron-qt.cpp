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


#include <assert.h>
#include <QCloseEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTextCursor>
#include "boron-qt.h"
#include "bignum.h"
#include "urlan_atoms.h"
#include "CBParser.h"


#define UT  qEnv.ut


extern "C" int boron_doBlock( UThread*, const UCell* blkC, UCell* res );
extern "C" int boron_doVoid( UThread*, const UCell* blkC );


inline bool qEnvExists() { return qEnv.atom_close > 0; }


struct LayoutInfo
{
    LayoutInfo() : box(0), grid(0) {}

    QLayout* layout()
    {
        if( box )
            return box;
        return grid;
    }

    void addLayout( QLayout* );
    void addWidget( QWidget* );

    QBoxLayout*  box;
    QGridLayout* grid;
    int columns;
    int colN;
    int rowN;
};


QtEnv qEnv;


//----------------------------------------------------------------------------


void LayoutInfo::addLayout( QLayout* lo )
{
    if( box )
    {
        box->addLayout( lo );
    }
    else
    {
        grid->addLayout( lo, rowN, colN );

        ++colN;
        if( colN == columns )
        {
            colN = 0;
            ++rowN;
        }
    }
}


void LayoutInfo::addWidget( QWidget* wid )
{
    if( box )
    {
        box->addWidget( wid );
    }
    else
    {
        grid->addWidget( wid, rowN, colN );

        ++colN;
        if( colN == columns )
        {
            colN = 0;
            ++rowN;
        }
    }
}


//----------------------------------------------------------------------------


/*
  Returns REC id.
*/
int WIDPool::add( QObject* ptr, int type, int flags )
{
    int index;
    if( _freeCount )
    {
        index = _freeIndex;
        REC& rec = _records[ index ];

        _freeIndex = rec.type;

        rec.type   = type;
        rec.flags  = flags;
        rec.object = ptr;

        --_freeCount;
    }
    else
    {
        index = _records.size();
        REC rec;
        rec.type   = type;
        rec.flags  = flags;
        rec.object = ptr;
        _records.push_back( rec ); 
    }
    return index;
}


void WIDPool::remove( int id )
{
    if( qEnvExists() )
    {
        REC& rec = _records[ id ];

        assert( rec.widget ); 

        rec.type = _freeIndex;
        _freeIndex = id;
        rec.object = 0;

        ++_freeCount;
    }
}


WIDPool::REC* WIDPool::record( int id )
{
    if( (id > -1) && id < _records.size() )
    {
        REC* rec = &_records[ id ];
        if( rec->object )
            return rec;
    }
    return 0;
}


//----------------------------------------------------------------------------


static void cellToQString( const UCell* val, QString& str )
{
    switch( ur_type(val) )
    {
        case UT_NONE:
            str.clear();
            break;

        case UT_CHAR:
            str = QChar( ur_int(val) );
            break;

        case UT_INT:
            str.setNum( ur_int(val) );
            break;

        case UT_DECIMAL:
            str.setNum( ur_decimal(val) );
            break;

        case UT_BIGNUM:
            str.setNum( bignum_l(val) );
            break;

        case UT_STRING:
        case UT_FILE:
        {
            USeriesIter si;
            int len;
            ur_seriesSlice( UT, &si, val );
            len = si.end - si.it;
            switch( si.buf->form )
            {
                case UR_ENC_LATIN1:
                    str = QString::fromLatin1( si.buf->ptr.c + si.it, len );
                    break;
                case UR_ENC_UTF8:
                    str = QString::fromUtf8( si.buf->ptr.c + si.it, len );
                    break;
                case UR_ENC_UCS2:
                    str = QString::fromUtf16( si.buf->ptr.u16 + si.it, len );
                    break;
            }
        }
            break;

        default:
        {
            UBuffer buf;
            ur_strInit( &buf, UR_ENC_LATIN1, 0 );
            ur_toStr( UT, val, &buf, 0 );
            str = QString::fromLatin1( buf.ptr.c, buf.used );
            ur_strFree( &buf );
        }
            break;
    }
}


static QString qstring( const UCell* cell )
{
    QString str;
    cellToQString( cell, str );
    return str;
}


static void toUString( const QString& text, UCell* res )
{
    int len = text.size();
    UBuffer* buf = ur_makeStringCell( UT, UR_ENC_UCS2, len, res );
    memcpy( buf->ptr.u16, text.constData(), len );
    buf->used = len;
}


//----------------------------------------------------------------------------


/*
  Note that argc is a reference!  If it is not, QApplication will
  store the stack position of the local argc and crash when argc is used
  after the constructor exits.
*/
BoronApp::BoronApp( int& argc, char** argv ) : QApplication( argc, argv )
{
    setQuitOnLastWindowClosed( false );
}


/*
   Handle exception and reset thread.
*/
void boron_qtException( UThread* ut )
{
    UCell* ex = boron_exception( ut );
    if( ur_is(ex, UT_ERROR) )
    {
//#ifdef _WIN32
        QString msg;
        cellToQString( ex, msg );
        QMessageBox::warning( 0, "Script Error", msg,
                              QMessageBox::Ok, QMessageBox::NoButton );
//#else
//        orPrintNative( orErrorThrown );
//#endif
    }
    else if( ur_is(ex, UT_WORD) )
    {
        if( ur_atom(ex) == UR_ATOM_QUIT )
            qApp->quit();
    }
    boron_reset( ut );
}


QLayout* ur_qtLayout( LayoutInfo& parent, const UCell* blkC );

static void tabBlock( QTabWidget* tab, const UCell* it, const UCell* label )
{
    LayoutInfo layout;
    QLayout* lo;
    QWidget* wid = new QWidget;

    lo = ur_qtLayout( layout, it );
    wid->setLayout( lo );

    tab->addTab( wid, qstring(label) );
}


static void tabWidgetBlock( UThread* ut, QTabWidget* tab, const UCell* blkC )
{
    UBlockIter bi;
    const UCell* label = 0;

    ur_blkSlice( ut, &bi, blkC );
    ur_foreach( bi )
    {
        if( ur_is(bi.it, UT_STRING) )
        {
            label = bi.it;
        }
        else if( ur_is(bi.it, UT_WORD) )
        {
            const UCell* val = ur_wordCell( ut, bi.it );
            if( label && ur_is(val, UT_BLOCK) )
            {
                tabBlock( tab, val, label );
                label = 0;
            }
        }
        else if( ur_is(bi.it, UT_BLOCK) )
        {
            if( label )
            {
                tabBlock( tab, bi.it, label );
                label = 0;
            }
        }
    }
}


static void setWID( const UCell*& prev, int id )
{
    if( prev )
    {
        UCell* val = ur_wordCellM( UT, prev );
        assert( val );

        ur_setId(val, UT_INT);
        ur_int(val) = id;

        prev = 0;
    }
}


static char layoutRules[] =
  "  'hbox block!\n"
  "| 'vbox block!\n"
  "| 'spacer\n"
  "| 'label string!\n"
  "| 'label word!\n"
  "| 'button string! block!\n"
  "| 'checkbox string!\n"
  "| 'combo string!\n"
  "| 'combo block!\n"
  "| 'tip string!\n"
  "| 'title string!\n"
  "| 'resize coord!\n"
  "| 'line-edit string!\n"
  "| 'line-edit\n"
  "| 'text-edit string!\n"
  "| 'text-edit\n"
  "| 'group string! block!\n"
  "| 'read-only\n"
  "| 'on-event block!\n"
  "| 'tab block!\n"
  "|  string!\n"
  "|  block!\n"
  "|  set-word!\n"
  "| 'grid int! block!\n"
  "| 'progress int!\n"
  "| 'weight int!\n"
  ;


enum LayoutRules
{
    LD_HBOX,
    LD_VBOX,
    LD_SPACER,
    LD_LABEL_STR,
    LD_LABEL_WORD,
    LD_BUTTON,
    LD_CHECKBOX,
    LD_COMBO_STR,
    LD_COMBO_BLK,
    LD_TIP,
    LD_TITLE,
    LD_RESIZE,
    LD_LINE_EDIT_STR,
    LD_LINE_EDIT,
    LD_TEXT_EDIT_STR,
    LD_TEXT_EDIT,
    LD_GROUP,
    LD_READ_ONLY,
    LD_ON_EVENT,
    LD_TAB,
    LD_STRING,
    LD_BLOCK,
    LD_SET_WORD,
    LD_GRID,
    LD_PROGRESS,
    LD_WEIGHT
};


/*
   Layout Language

   \return Layout pointer or zero if error generated.
*/
QLayout* ur_qtLayout( LayoutInfo& parent, const UCell* blkC )
{
    CBParser cbp;
    UBlockIter bi;
    LayoutInfo lo;
    const UCell* val;
    const UCell* setWord = 0;
    QWidget* wid = 0;
    UThread* ut = UT;
    int match;


    ur_blkSlice( ut, &bi, blkC );

    cbp_beginParse( &cbp, bi.it, bi.end, ur_bufferE( qEnv.layoutRules ) );
    while( (match = cbp_matchRule( &cbp )) > -1 )
    {
        switch( match )
        {
            case LD_HBOX:
            case LD_VBOX:
            {
                wid = 0;

                lo.grid = 0;
                lo.box  = new QBoxLayout( (match == LD_HBOX) ? 
                                          QBoxLayout::LeftToRight :
                                          QBoxLayout::TopToBottom );
                if( parent.layout() )
                    parent.addLayout( lo.box );

                val = cbp.values + 1;
                ur_qtLayout( lo, val );
            }
                break;

            case LD_SPACER:
                if( ! parent.box )
                    goto no_layout;

                parent.box->addStretch( 1 );
                break;

            case LD_LABEL_STR:
            case LD_LABEL_WORD:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SLabel* pw = new SLabel;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );

                val = cbp.values + 1;
                if( ur_is(val, UT_WORD) )
                {
                    val = ur_wordCell( ut, val );
                }
                QString txt;
                cellToQString( val, txt );
                pw->setText( txt );

                wid = pw;
            }
                break;

            case LD_BUTTON:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SButton* pw = new SButton;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                pw->setText( qstring( cbp.values + 1 ) );
                pw->setBlock( cbp.values + 2 );
                wid = pw;
            }
                break;

            case LD_CHECKBOX:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SCheck* pw = new SCheck;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                pw->setText( qstring( cbp.values + 1 ) );
                wid = pw;
            }
                break;

            case LD_COMBO_STR:
            case LD_COMBO_BLK:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SCombo* pw = new SCombo;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                wid = pw;

                val = cbp.values + 1;
                if( ur_is(val, UT_BLOCK) )
                {
                    UBlockIter bi2;
                    ur_blkSlice( ut, &bi2, val ); 
                    ur_foreach( bi2 )
                    {
                        if( ur_is(bi2.it, UT_STRING) )
                            pw->addItem( qstring( bi2.it ) );
                    }
                }
                else
                {
                    pw->addItem( qstring( val ) );
                }
            }
                break;

            case LD_TAB:
            {
                if( ! parent.layout() )
                    goto no_layout;

                STabWidget* pw = new STabWidget;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                wid = pw;

                tabWidgetBlock( ut, pw, cbp.values + 1 );
            }
                break;

            case LD_TIP:
                if( wid )
                    wid->setToolTip( qstring(cbp.values + 1) );
                break;

            case LD_TITLE:
            {
                if( qEnv.curWidget )
                    qEnv.curWidget->setWindowTitle( qstring(cbp.values + 1) );
            }
                break;

            case LD_RESIZE:
            {
                if( qEnv.curWidget )
                {
                    val = cbp.values + 1;
                    qEnv.curWidget->resize( val->coord.n[0], val->coord.n[1] );
                }
            }
                break;

            case LD_LINE_EDIT_STR:
            case LD_LINE_EDIT:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SLineEdit* pw = new SLineEdit;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                wid = pw;

                if( match == LD_LINE_EDIT_STR )
                    pw->setText( qstring( cbp.values + 1 ) );
            }
                break;

            case LD_TEXT_EDIT_STR:
            case LD_TEXT_EDIT:
            {
                if( ! parent.layout() )
                    goto no_layout;

                STextEdit* pw = new STextEdit;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                wid = pw;

                if( match == LD_TEXT_EDIT_STR )
                {
                    QString str;
                    cellToQString( cbp.values + 1, str );
                    if( str[0] == '<' )
                        pw->setHtml( str );
                    else
                        pw->setPlainText( str );
                }
            }
                break;

            case LD_GROUP:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SGroup* pw = new SGroup;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                wid = pw;

                val = cbp.values + 1;
                pw->setTitle( qstring( val ) );

                ++val;

                LayoutInfo lo2;
                pw->setLayout( ur_qtLayout(lo2, val) );
            }
                break;

            case LD_READ_ONLY:
                if( wid )
                {
                    QTextEdit* tedit = qobject_cast<QTextEdit*>( wid );
                    if( tedit )
                        tedit->setReadOnly( true );
                }
                break;

            case LD_ON_EVENT:
                if( qEnv.curWidget )
                {
                    SWidget* sw = qobject_cast<SWidget*>( qEnv.curWidget );
                    if( sw )
                        sw->setEventBlock( cbp.values + 1 );
                }
                break;

            case LD_STRING:
                if( wid )
                {
                    SCombo* combo = qobject_cast<SCombo*>( wid );
                    if( combo )
                    {
                        combo->addItem( qstring( cbp.values ) );
                        break;
                    }

                    QTextEdit* texted = qobject_cast<QTextEdit*>( wid );
                    if( texted )
                    {
                        QString str;
                        cellToQString( cbp.values, str );
                        if( str[0] == '<' )
                            texted->setHtml( str );
                        else
                            texted->setPlainText( str );
                    }
                }
                break;

            case LD_BLOCK:
                if( wid )
                {
                    SCombo* combo = qobject_cast<SCombo*>( wid );
                    if( combo )
                        combo->setBlock( cbp.values );
                }
                break;

            case LD_SET_WORD:
                setWord = cbp.values;
                break;

            case LD_GRID:
                val = cbp.values + 1;

                wid = 0;

                lo.box     = 0;
                lo.grid    = new QGridLayout;
                lo.columns = ur_int(val);
                lo.colN    = 0;
                lo.rowN    = 0;

                if( parent.layout() )
                    parent.addLayout( lo.grid );

                ++val;
                ur_qtLayout( lo, val );
                break;

            case LD_PROGRESS:
            {
                if( ! parent.layout() )
                    goto no_layout;

                SProgress* pw = new SProgress;
                parent.addWidget( pw );
                setWID( setWord, pw->_wid );
                pw->setRange( 0, ur_int(cbp.values+1) );
                wid = pw;
            }
                break;

            case LD_WEIGHT:
                if( parent.box && wid )
                {
                    parent.box->setStretchFactor(wid, ur_int(cbp.values+1));
                }
                break;
        }
    }

    if( cbp.values )
    {
        ur_error( ut, UR_ERR_SCRIPT, "layout parse error" );
        bi.buf = ur_bufferSer( blkC );
        ur_appendTrace( ut, blkC->series.buf, cbp.values - bi.buf->ptr.cell );
        return 0;
    }

    return lo.layout();

no_layout:

    ur_error( ut, UR_ERR_SCRIPT, "layout requires hbox or vbox" );
    return 0;
}


static int catchExit( UThread* ut, UCell* res )
{
    UBuffer* blk = ur_errorBlock(ut);
    if( blk->used > 1 )
    {
        UCell* cell = blk->ptr.cell + blk->used - 1;
        if( ur_is(cell, UT_WORD) )
        {
            if( ur_atom(cell) == qEnv.atom_exec_exit )
            {
                *res = cell[-1];
                blk->used -= 2;
                return qEnv.atom_exec_exit;
            }
            else if( ur_atom(cell) == qEnv.atom_close )
            {
                *res = cell[-1];
                blk->used -= 2;
                return qEnv.atom_close;
            }
        }
    }
    ur_setId(res, UT_UNSET);
    return 0;
}


/*-cf-
    exec
        wid int!/none!
    return: Thrown exit/close value.
*/
CFUNC( cfunc_exec )
{
    (void) ut;
    if( ur_is(a1, UT_INT) )
    {
        WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
        if( rec )
        {
            if( rec->type == WT_Dialog )
            {
                QDialog* saveDialog = qEnv.dialog;

                qEnv.dialog = (QDialog*) rec->widget;
                qEnv.dialog->exec();
                qEnv.dialog = saveDialog;

                catchExit( ut, res );
            }
            else if( rec->type == WT_Widget )
            {
                QWidget* widget = rec->widget;

                widget->show();
                qEnv.loop->exec();
                if( catchExit( ut, res ) == qEnv.atom_close )
                    widget->hide();
            }
            else
            {
                rec = 0;
            }
        }
        if( ! rec )
        {
            return ur_error( ut, UR_ERR_TYPE,
                             "Invalid widget ID %d", ur_int(a1) );
        }
    }
    else
    {
        // Update.
        qEnv.loop->processEvents();
        catchExit( ut, res );
    }
    return UR_OK;
}


/*-cf-
    exec-exit
        code int!/none!/logic!/word!
    return: throws word
*/
CFUNC( cfunc_execExit )
{
    (void) res;

    if( qEnv.dialog )
        qEnv.dialog->accept();
    else
        qEnv.loop->quit();

    ur_blkAppendCells( ur_errorBlock(ut), a1, 1 );
    return boron_throwWord( ut, qEnv.atom_exec_exit );
}


/*-cf-
    close
        code int!/none!/logic!/word!
    return: throws word
*/
CFUNC( cfunc_close )
{
    (void) res;

    if( qEnv.dialog )
        qEnv.dialog->accept();
    else
        qEnv.loop->quit();

    ur_blkAppendCells( ur_errorBlock(ut), a1, 1 );
    return boron_throwWord( ut, qEnv.atom_close );
}


/*-cf-
    show
        wid int!
    return: unset!
*/
CFUNC( cfunc_show )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error(ut, UR_ERR_TYPE, "show expected WID int!");

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        if( rec->type == WT_Widget )
        {
            rec->widget->show();

            // Update.
            qEnv.loop->processEvents();
            catchExit( ut, a1 );
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


static int layoutWidget( QWidget* wid, UCell* a1 )
{
    LayoutInfo li;
    QLayout* lo;

    QWidget* saveWidget = qEnv.curWidget;
    qEnv.curWidget = wid;

    lo = ur_qtLayout( li, a1 );

    qEnv.curWidget = saveWidget;

    if( lo )
    {
        wid->setLayout( lo );
        return 1;
    }
    else
    {
        delete wid;
        boron_qtException( UT );
        return 0;
    }
}


/*-cf-
    dialog
        layout block!
    return: int!/none!
*/
CFUNC( cfunc_dialog )
{
    (void) ut;
    SDialog* dlg = new SDialog;
    dlg->setModal( true );
    if( layoutWidget( dlg, a1 ) )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = dlg->_wid;
        return UR_OK;
    }
    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    widget
        layout block!
    return: int!/none!
*/
CFUNC( cfunc_widget )
{
    (void) ut;
    SWidget* wid = new SWidget;
    if( layoutWidget( wid, a1 ) )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = wid->_wid;
        return UR_OK;
    }
    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    destroy
        wid int!
    return: none!
*/
CFUNC( cfunc_destroy )
{
    (void) ut;
    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
        delete rec->object;
    ur_setId(res, UT_NONE);
    return UR_OK;
}


/*-cf-
    request-file
        title string!
        /save
        /dir
        /init path
    return: unset!
*/
CFUNC( cfunc_requestFile )
{
#define REF_RF_SAVE     0x01
#define REF_RF_DIR      0x02
#define REF_RF_INIT     0x04
#define REF_RF_PATH     (a1+1)
    QString title;
    QString fn;
    QString dir;
    uint32_t opt = CFUNC_OPTIONS;
    (void) ut;

    if( opt & REF_RF_INIT )
        cellToQString( REF_RF_PATH, dir );
    else
        dir = qEnv.filePath;

    cellToQString( a1, title );

    if( opt & REF_RF_DIR )
        fn = QFileDialog::getExistingDirectory( 0, title, dir );
    else if( opt & REF_RF_SAVE )
        fn = QFileDialog::getSaveFileName( 0, title, dir );
    else
        fn = QFileDialog::getOpenFileName( 0, title, dir );

    if( fn.isEmpty() )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        qEnv.filePath = fn;
        toUString( fn, res );
        ur_type(res) = UT_FILE;
    }
    return UR_OK;
}


/*-cf-
    enable
        wid int!
    return: unset!
*/
CFUNC( cfunc_enable )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error(ut, UR_ERR_TYPE, "enable expected WID int!");

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        QWidget* widget = qobject_cast<QWidget*>( rec->object );
        if( widget )
            widget->setEnabled( true );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    disable
        wid int!
    return: unset!
*/
CFUNC( cfunc_disable )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error(ut, UR_ERR_TYPE, "disable expected WID int!");

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        QWidget* widget = qobject_cast<QWidget*>( rec->object );
        if( widget )
            widget->setEnabled( false );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    widget-value
        wid int!
    return: unset!
*/
CFUNC( cfunc_widgetValue )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error(ut, UR_ERR_TYPE, "widget-value expected WID int!");

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        switch( rec->type )
        {
            case WT_CheckBox:
            {
                Qt::CheckState state = ((QCheckBox*) rec->widget)->checkState();
                ur_setId(res, UT_LOGIC);
                ur_int(res) = (state == Qt::Unchecked) ? 0 : 1;
            }
                return UR_OK;

            case WT_Combo:
                ur_setId(res, UT_INT);
                ur_int(res) = ((QComboBox*) rec->widget)->currentIndex() + 1;
                return UR_OK;

            case WT_LineEdit:
            {
                QString text = ((QLineEdit*) rec->widget)->text();
                toUString( text, res );
            }
                return UR_OK;

            case WT_TextEdit:
            {
                QString text = ((QTextEdit*) rec->widget)->toPlainText();
                toUString( text, res );
            }
                return UR_OK;

            default:
                break;
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    set-widget-value
        wid int!
        val
    return: unset!
*/
CFUNC( cfunc_setWidgetValue )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error(ut, UR_ERR_TYPE, "set-widget-value expected WID int!");

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        const UCell* a2 = a1 + 1;
        switch( rec->type )
        {
            case WT_CheckBox:
            {
                Qt::CheckState state;

                if( ur_isTrue( a2 ) )
                    state = Qt::Checked;
                else
                    state = Qt::Unchecked;

                ((QCheckBox*) rec->widget)->setCheckState( state );
            }
                break;

            case WT_Combo:
                if( ur_is(a2, UT_INT) )
                {
                    ((QComboBox*)rec->widget)->setCurrentIndex( ur_int(a2)-1 );
                }
                else if( ur_is(a2, UT_LOGIC) )
                {
                    ((QComboBox*)rec->widget)->setCurrentIndex( ur_int(a2) );
                }
                break;

            case WT_Label:
            {
                QString txt;
                cellToQString( a2, txt );
                ((QLabel*) rec->widget)->setText( txt );
            }
                break;

            case WT_LineEdit:
            {
                QString txt;
                cellToQString( a2, txt );
                ((QLineEdit*) rec->widget)->setText( txt );
            }
                break;

            case WT_TextEdit:
            {
                QString txt;
                cellToQString( a2, txt );
                ((QTextEdit*) rec->widget)->setPlainText( txt );
            }
                break;

            case WT_Progress:
                if( ur_is(a2, UT_INT) )
                {
                    ((QProgressBar*) rec->widget)->setValue( ur_int(a2) );
                }
                else if( ur_is(a2, UT_NONE) )
                {
                    ((QProgressBar*) rec->widget)->reset();
                }
                break;

            default:
                break;
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    append-text
        wid int!
        val
    return: unset!
*/
CFUNC( cfunc_appendText )
{
    if( ! ur_is(a1, UT_INT) )
        return ur_error( ut, UR_ERR_TYPE, "append-text expected WID int!" );

    WIDPool::REC* rec = qEnv.pool.record( ur_int(a1) );
    if( rec )
    {
        const UCell* a2 = a1 + 1;
        switch( rec->type )
        {
            case WT_LineEdit:
            {
                QString cur;
                QString txt;
                cellToQString( a2, txt );
                cur = ((QLineEdit*) rec->widget)->text();
                cur.append( txt );
                ((QLineEdit*) rec->widget)->setText( cur );
            }
                break;

            case WT_TextEdit:
            {
                QString txt;
                QTextCursor cursor;

                cellToQString( a2, txt );

                QTextEdit* edit = (QTextEdit*) rec->widget;

                cursor = edit->textCursor();
                cursor.movePosition( QTextCursor::End );
                cursor.insertText( txt );

                //edit->insertPlainText( txt );
                //edit->append( txt );  // Adds linefeed before text.
            }
                break;

            default:
                break;
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    message
        title   string!
        msg     string!
        /warn
    return: unset!
*/
CFUNC( cfunc_message )
{
#define REF_MSG_WARN    0x01
    QString str[2];
    (void) ut;

    cellToQString( a1, str[0] );
    cellToQString( a1+1, str[1] );

    if( CFUNC_OPTIONS & REF_MSG_WARN )
        QMessageBox::warning( 0, str[0], str[1] );
    else
        QMessageBox::information( 0, str[0], str[1] );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    question
        title   string!
        msg     string!
        button1 string!
        button2 string!
        /default button int!
    return: Button number or none! if cancelled.
*/
CFUNC( cfunc_question )
{
#define REF_QUESTION_DEF    0x01
    QString str[4];
    int def = 0;
    int i;
    (void) ut;

    for( i = 0; i < 4; ++i )
        cellToQString( a1 + i, str[i] );

    if( CFUNC_OPTIONS & REF_QUESTION_DEF )
        def = ur_int(a1+5) - 1;

    i = QMessageBox::question( 0, str[0], str[1], str[2], str[3],
                               QString(), def );
    if( i < 0 )
    {
        ur_setId(res, UT_NONE);
    }
    else
    {
        ur_setId(res, UT_INT);
        ur_int(res) = i + 1;
    }
    return UR_OK;
}


//----------------------------------------------------------------------------


void boron_initQt( UThread* ut )
{
    UCell tmp;

    qEnv.ut = ut;
    qEnv.curWidget = 0;
    qEnv.dialog    = 0;
    qEnv.loop      = new QEventLoop( qApp );

    ur_internAtoms( ut, "close exec-exit ready removed created changed",
                    &qEnv.atom_close );

    qEnv.layoutRules = ur_tokenize( ut, layoutRules,
                                        layoutRules + sizeof(layoutRules) - 1,
                                        &tmp );
    ur_hold( qEnv.layoutRules );

    boron_addCFunc( ut, cfunc_exec,           "exec wid" );
    boron_addCFunc( ut, cfunc_execExit,       "exec-exit code" );
    boron_addCFunc( ut, cfunc_close,          "close code" );
    boron_addCFunc( ut, cfunc_show,           "show wid" );
    boron_addCFunc( ut, cfunc_dialog,         "dialog layout" );
    boron_addCFunc( ut, cfunc_widget,         "widget layout" );
    boron_addCFunc( ut, cfunc_destroy,        "destroy wid" );
    boron_addCFunc( ut, cfunc_requestFile,
                            "request-file title /save /dir /init path" );
    boron_addCFunc( ut, cfunc_enable,         "enable wid" );
    boron_addCFunc( ut, cfunc_disable,        "disable wid" );
    boron_addCFunc( ut, cfunc_widgetValue,    "widget-value wid" );
    boron_addCFunc( ut, cfunc_setWidgetValue, "set-widget-value wid val" );
    boron_addCFunc( ut, cfunc_appendText,     "append-text wid val" );
    boron_addCFunc( ut, cfunc_message,        "message title msg /warn" );
    boron_addCFunc( ut, cfunc_question,
                            "question title msg b1 b2 /default def" );


    {
    UAtom atoms[3];
    UIndex n;

    ur_internAtoms( ut, "index event file", atoms );

    qEnv.comboCtxN = n = ur_makeContext( ut, 1 );
    ur_hold( n );
    ur_ctxAddWord( ur_buffer(n), atoms[0] );

    /*
    orMakeContext( &ctx, 2 );
    ur_ctxAddWord( &ctx, atoms[1] );
    ur_ctxAddWord( &ctx, atoms[2] );
    */
    }
}


void boron_freeQt()
{
    // Here we ensure objects which are holding blocks are destroyed before
    // qEnv.ut goes away.

    qEnv.atom_close = 0;    // Using atom_close as qEnvExists indicator.

    WIDPool::iterator it;
    for( it = qEnv.pool.begin(); it != qEnv.pool.end(); ++it )
    {
        // Records without DeleteObject flag set are assumed to be child
        // widgets of a WT_Dialog or WT_Widget.

        WIDPool::REC& rec = *it;
        if( rec.object && (rec.flags & WIDPool::DeleteObject) )
        {
            delete rec.object;
        }
    }
}


//----------------------------------------------------------------------------


ExeBlock::~ExeBlock()
{
    if( hold != UR_INVALID_HOLD )
        ur_releaseBuffer( UT, hold );
}


void ExeBlock::setBlock( const UCell* val )
{
    n     = val->series.buf;
    index = val->series.it; 
    hold  = ur_holdBuffer( UT, n );
}


void ExeBlock::switchWord( UAtom atom )
{
    if( n )
    {
        UThread* ut = UT;
        UBlockIter bi;

        bi.buf = ur_bufferE( n );
        bi.it  = bi.buf->ptr.cell + index;
        bi.end = bi.buf->ptr.cell + bi.buf->used;

        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_WORD) && (ur_atom(bi.it) == atom) )
            {
                if( ++bi.it == bi.end )
                    break;
                if( ur_is(bi.it, UT_BLOCK) )
                {
                    if( ! boron_doVoid( ut, bi.it ) )
                        boron_qtException( ut );
                    break;
                }
            }
        }
    }
}


//----------------------------------------------------------------------------


SWidget::SWidget()
{
    _wid = qEnv.pool.add( this, WT_Widget, WIDPool::DeleteObject );
}


SWidget::~SWidget()
{
    qEnv.pool.remove( _wid );
    //printf("~SWidget\n");
}


void SWidget::setEventBlock( const UCell* val )
{
    _blk.setBlock( val );
}


void SWidget::closeEvent( QCloseEvent* e )
{
    _blk.switchWord( qEnv.atom_close ); 
    e->accept();
}


//----------------------------------------------------------------------------


SButton::SButton()
{
    _wid = qEnv.pool.add( this, WT_Button );
}


SButton::~SButton()
{
    qEnv.pool.remove( _wid );
}


void SButton::setBlock( const UCell* val )
{
    if( ! _blk.n )
        connect( this, SIGNAL(clicked()), this, SLOT(slotDo()) );

    _blk.setBlock( val );
}


void SButton::slotDo()
{
    if( _blk.n )
    {
        UCell tmp;
        UThread* ut = UT;

        ur_setId(&tmp, UT_BLOCK);
        ur_setSeries(&tmp, _blk.n, _blk.index);

        if( ! boron_doVoid( ut, &tmp) )
            boron_qtException( ut );
    }
}


//----------------------------------------------------------------------------


SCombo::SCombo()
{
    _wid = qEnv.pool.add( this, WT_Combo );
}


SCombo::~SCombo()
{
    qEnv.pool.remove( _wid );
}


void SCombo::setBlock( const UCell* val )
{
    if( ! _blk.n )
        connect( this, SIGNAL(activated(int)), this, SLOT(slotDo(int)) );

    _blk.setBlock( val );

    UThread* ut = UT;
    ur_bind( ut, ur_buffer( _blk.n ),
                 ur_buffer( qEnv.comboCtxN ), UR_BIND_THREAD );
}


void SCombo::slotDo( int index )
{
    if( _blk.n )
    {
        UCell tmp;
        UCell* ival;
        UThread* ut = UT;

        ival = ur_buffer(qEnv.comboCtxN)->ptr.cell;
        ur_setId(ival, UT_INT);
        ur_int(ival) = index + 1;      // one-based index.

        ur_setId(&tmp, UT_BLOCK);
        ur_setSeries(&tmp, _blk.n, 0 );

        if( ! boron_doVoid( ut, &tmp ) )
            boron_qtException( ut );
    }
}


//----------------------------------------------------------------------------


#define WIDGET_CODE(oo,wt) \
    oo::oo() { _wid = qEnv.pool.add( this, wt ); } \
    oo::~oo() { qEnv.pool.remove( _wid ); }


WIDGET_CODE(SCheck,WT_CheckBox)
WIDGET_CODE(SGroup,WT_Group)
WIDGET_CODE(SLabel,WT_Label)
WIDGET_CODE(SLineEdit,WT_LineEdit)
WIDGET_CODE(SProgress,WT_Progress)
WIDGET_CODE(STabWidget,WT_Tab)
WIDGET_CODE(STextEdit,WT_TextEdit)


SDialog::SDialog()
{
    _wid = qEnv.pool.add( this, WT_Dialog, WIDPool::DeleteObject );
}


SDialog::~SDialog()
{
    qEnv.pool.remove( _wid );
    //printf("~SDialog\n");
}


// EOF
