#ifndef HASHMAP_H
#define HASHMAP_H


extern void hashmap_values( UThread*, const UCell* mapC, UBuffer* blk );
extern void hashmap_clear( UThread*, UCell* mapC );
extern UStatus hashmap_insert( UThread*, const UCell* mapC, const UCell* keyC,
                               const UCell* valueC );
extern UStatus hashmap_remove( UThread*, const UCell* mapC, const UCell* keyC );
extern const UCell* hashmap_select( UThread*, const UCell* cell,
                                    const UCell* sel, UCell* tmp );


#endif //HASHMAP_H
