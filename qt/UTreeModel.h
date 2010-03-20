

#include "urlan.h"
#include <QAbstractItemModel>
#include <QStringList>


class UTreeModel : public QAbstractItemModel
{
public:

    UTreeModel( QObject* parent, const UCell* hdr, const UCell* data );
    ~UTreeModel();

    QVariant data( const QModelIndex& , int role ) const;
    QVariant headerData( int section, Qt::Orientation, int role ) const;
    QModelIndex index( int row, int column, const QModelIndex& parent ) const;
    QModelIndex parent( const QModelIndex& index ) const;
    int rowCount( const QModelIndex& parent ) const;
    int columnCount( const QModelIndex& parent ) const;

    void blockSlice( const QModelIndex& index, UCell* res );

private:

    UIndex _blkN;
    UIndex _hold;
    int    _rows;
    int    _cols;
    QStringList _hdr;
};


