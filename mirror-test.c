#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"


int main(int argc, char **argv)
{
    
    struct blkdev *d1 = image_create(argv[1]);
    struct blkdev *d2 = image_create(argv[2]);
    
    struct blkdev *disks[] = {d1, d2};
    struct blkdev *mirror = mirror_create(disks);

    
    assert(mirror != NULL);
    assert(mirror->ops->num_blocks(mirror) == d1->ops->num_blocks(d1));

    const int TOTAL_BLKS = 10;
    
    int lba = 2;
    int count = 4;
    char zs[ count * BLOCK_SIZE ];
    memset( zs, 'Z', count * BLOCK_SIZE );

    
    assert ( mirror -> ops -> write ( mirror, -2, count, zs )
             == E_BADADDR );
    assert ( mirror -> ops -> write ( mirror, lba, 15, zs)
             == E_BADADDR );

    
    assert ( mirror -> ops -> write ( mirror, lba, count, zs )
             == SUCCESS );

    
    char test1[ count * BLOCK_SIZE ];
    assert ( mirror -> ops -> read ( mirror, lba, count, test1 )
             == SUCCESS );
    assert ( strncmp ( zs, test1, count * BLOCK_SIZE ) == 0 );

    
    lba = 0, count = TOTAL_BLKS;
    char ys[ TOTAL_BLKS * BLOCK_SIZE ];
    char test2[ TOTAL_BLKS * BLOCK_SIZE ];
    memset ( ys, 'Y', TOTAL_BLKS * BLOCK_SIZE );
    assert ( mirror -> ops -> write ( mirror, lba, count, ys )
             == SUCCESS );
    assert ( mirror -> ops -> read ( mirror, lba, count, test2 )
             == SUCCESS );
    assert ( strncmp( ys, test2, count * BLOCK_SIZE ) == 0 );


    image_fail ( d1 );
    lba = 6, count = 3;
    char xs[ count * BLOCK_SIZE ];
    char test3[ count * BLOCK_SIZE ];
    memset ( xs, 'X', count * BLOCK_SIZE );
    assert ( mirror -> ops -> write ( mirror, lba, count, xs )
             == SUCCESS );
    assert ( mirror -> ops -> read ( mirror, lba, count, test3 )
             == SUCCESS );
    assert ( strncmp( xs, test3, count * BLOCK_SIZE ) == 0 );

    
    struct blkdev *replace_disk = image_create( "mirror/replace_disk.img" );
    assert ( replace_disk != NULL );
    assert ( replace_disk -> ops -> num_blocks( replace_disk ) );
    
    assert ( mirror_replace ( mirror, 0, replace_disk ) == SUCCESS );
    assert ( mirror -> ops -> read ( mirror, lba, count, test3 )
             == SUCCESS );
    assert ( strncmp( xs, test3, count * BLOCK_SIZE ) == 0 );
    
    lba = 8, count = 2;
    char ws[ count * BLOCK_SIZE ];
    char test4[ count * BLOCK_SIZE ];
    memset ( ws, 'W', count * BLOCK_SIZE );
    assert ( mirror -> ops -> write ( mirror, lba, count, ws )
             == SUCCESS );
    assert ( mirror -> ops -> read ( mirror, lba, count, test4 )
             == SUCCESS );
    assert ( strncmp( ws, test4, count * BLOCK_SIZE ) == 0 );

    
    image_fail ( d2 );
    lba = 0, count = 3;
    char vs[ count * BLOCK_SIZE ];
    char test5[ count * BLOCK_SIZE ];
    memset ( vs, 'V', count * BLOCK_SIZE );
    assert ( mirror -> ops -> write ( mirror, lba, count, vs )
             == SUCCESS );
    assert ( mirror -> ops -> read ( mirror, lba, count, test5 )
             == SUCCESS );
    assert ( strncmp( vs, test5, count * BLOCK_SIZE ) == 0 );

    
    image_fail( replace_disk );
    assert ( mirror -> ops -> write ( mirror, lba, count, vs )
             == E_UNAVAIL );
    assert ( mirror -> ops -> read ( mirror, lba, count, test5 )
             == E_UNAVAIL );

    
    mirror -> ops -> close ( mirror );
    printf("Mirror Test: SUCCESS\n");
    return 0;
}