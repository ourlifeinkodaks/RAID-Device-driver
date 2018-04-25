/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 2
 *
 * Peter Desnoyers, Northeastern Computer Science, 2011
 * $Id: homework.c 410 2011-11-07 18:42:45Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "blkdev.h"
#include <string.h>

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev {
    struct blkdev *disks[2];    /* flag bad disk by setting to NULL */
    int nblks;
};
    
static int mirror_num_blocks(struct blkdev *dev)
{
    struct mirror_dev *m_dev= dev ->private;
    return m_dev-> nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{    
    

    struct mirror_dev *m_dev = dev -> private;
    
    if(first_blk<0 || first_blk+num_blks > m_dev-> nblks)
    {
        return E_BADADDR;
    }
    int x;

    for (x = 0; x<2; x++)
    {
        struct blkdev *disk = m_dev-> disks[x];
        if(disk == NULL)
        {
            continue;
        }
        else
        {
            int val = disk -> ops -> read(disk, first_blk, num_blks, buf);

            if (val == SUCCESS)
            {
                return SUCCESS;
            }
            else if (val == E_UNAVAIL)
            {
                disk -> ops -> close(disk);
                m_dev -> disks[x] = NULL;
                continue;
            }
        }
    }
    return E_UNAVAIL;

}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
    int val[10], x;

    struct mirror_dev *m_dev = dev -> private;
    
    if(first_blk<0 || first_blk+num_blks > m_dev-> nblks)
        return E_BADADDR;

    for (int x = 0; x<2; x++)
    {
        struct blkdev *disk = m_dev -> disks[x];
        if(disk == NULL)
        {
            val[x] = E_UNAVAIL;
            continue;
        }
        else 
        {
            val[x] = disk -> ops -> write (disk, first_blk, num_blks, buf);                                      

            if ( val[x] == E_UNAVAIL )
             {
                disk -> ops -> close ( disk );
                m_dev-> disks[x] = NULL;
            }
        }
    }
    if ( val[0] == SUCCESS || val[1] == SUCCESS ) 
    {
        return SUCCESS;
    } 
    else 
    {
        return E_UNAVAIL;
    }
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    int x;

    struct mirror_dev *m_dev = dev -> private;

    for ( x=0 ; x<2 ; x++ )
    {
        struct blkdev *disk = m_dev -> disks[x];
        
        if ( disk != NULL ) 
        {
            disk -> ops -> close (disk);
        }
    }
    free(m_dev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
    .num_blocks = mirror_num_blocks,
    .read = mirror_read,
    .write = mirror_write,
    .close = mirror_close
};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{
    struct blkdev *dev = malloc(sizeof(*dev));
    struct mirror_dev *mdev = malloc(sizeof(*mdev));

    if(disks[0]->ops->num_blocks(disks[0]) != disks[1]->ops->num_blocks(disks[1]))
    {
        printf("Unequal Disk sizes !\n");
        return NULL;
    }

    mdev -> disks[0] = disks[0];
    mdev -> disks[1] = disks[1];
    mdev -> nblks = disks[0] -> ops -> num_blocks( disks[0] );
 
    dev->private = mdev;
    dev->ops = &mirror_ops;

    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    char buff[BLOCK_SIZE];
    int lba;

    struct mirror_dev *m_dev = volume -> private;

    if ( m_dev -> nblks != newdisk -> ops -> num_blocks(newdisk)) 
    {
        return E_SIZE;
    }

    int working_disk = ~i + 2;
    struct blkdev *disk = m_dev -> disks[ working_disk ];

    for (lba = 0 ; lba < m_dev->nblks ; lba++) 
    {
        int val = disk -> ops -> read( disk, lba, 1, buff);
        if (val!= SUCCESS) 
        {
            return val;
        }
        val= newdisk -> ops -> write ( newdisk, lba, 1, buff);
        if ( val != SUCCESS ) {
            return val;
        }
    }

    m_dev -> disks[i] = newdisk;
    return SUCCESS;
}

/**********  STRIPING ***************/

struct stripe_dev {
    struct blkdev **disks;
    int ndisks;     
    int nblks;     
    int unit;
    int status;
};

enum { RUNNING = 1, CRASHED = 0};

/* stripe_num_blocks(); the number of blocks contained in the stripe*/

int stripe_num_blocks(struct blkdev *dev)
{
    struct stripe_dev *s_dev = dev -> private;
    return (s_dev -> ndisks) * (s_dev -> nblks) ;
}

/* stripe_read() to read the values from the stripes*/

static int stripe_read(struct blkdev *dev, int first_blk,int num_blks, void *buf)                      
{
    
    struct stripe_dev *s_dev = dev -> private;

    if ( s_dev -> status == CRASHED) 
    {
        return E_UNAVAIL;
    }

    
    if ( first_blk < 0 || 
        first_blk + num_blks > dev->ops->num_blocks (dev))
    {
        return E_BADADDR;
    }
    int N = s_dev -> ndisks;
    int unit = s_dev -> unit;
 
    while ( num_blks > 0 ) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit );
        
        int disk_lba = (first_blk/((s_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        if ( number_in_disks > num_blks ) 
        {
            number_in_disks = num_blks;
        }
        
        struct blkdev *disk = s_dev -> disks[disk_number];
       
        int val = disk -> ops -> read ( disk, disk_lba,number_in_disks,buf);
                                           
        if ( val == SUCCESS) 
        {
            buf += number_in_disks * BLOCK_SIZE;
            first_blk += number_in_disks;
            num_blks -= number_in_disks;
        } 
        else if ( val == E_UNAVAIL ) 
        {
            disk -> ops -> close (disk);
            s_dev -> disks[ disk_number ] = NULL;
            s_dev -> status = CRASHED;
            return E_UNAVAIL;
        }
    }
    return SUCCESS;
}

/*stripe_write() write data in stripe*/

static int stripe_write(struct blkdev *dev, int first_blk,
                        int num_blks, void *buf)
{
    

    struct stripe_dev *s_dev = dev -> private;

    if ( s_dev -> status == CRASHED) 
    {
        return E_UNAVAIL;
    }

    if ( first_blk < 0 || first_blk + num_blks > dev -> ops ->num_blocks (dev))
    {
        return E_BADADDR;
    }
    int N = s_dev -> ndisks;
    int unit = s_dev -> unit;

    while ( num_blks > 0 ) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit) ;
        
        int disk_lba = (first_blk/((s_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        struct blkdev *disk = s_dev -> disks[disk_number];
       
        int val = disk -> ops -> write( disk, disk_lba,number_in_disks,buf);
                                           
        if ( val == SUCCESS) 
        {
            buf += number_in_disks * BLOCK_SIZE;
            first_blk += number_in_disks;
            num_blks -= number_in_disks;
        } 
        else if ( val == E_UNAVAIL ) 
        {
            disk -> ops -> close (disk);
            s_dev -> disks[ disk_number ] = NULL;
            s_dev -> status = CRASHED;
            
            return E_UNAVAIL;
        }

    }
    return SUCCESS;
}

/* stripe_close() to clean up
 */

static void stripe_close(struct blkdev *dev)
{
    int x;

    struct stripe_dev *s_dev = dev -> private;
    
    for ( x= 0 ; x< s_dev -> ndisks ; x++ ) 
    {
        struct blkdev *disk = s_dev -> disks[x];
        
        if ( disk != NULL )
            disk->ops->close(disk);
    }

    s_dev -> status = CRASHED;
    free (s_dev);
    free (dev);
}

struct blkdev_ops stripe_ops = {
    .num_blocks = stripe_num_blocks,
    .read = stripe_read,
    .write = stripe_write,
    .close = stripe_close
};

/* *striped_create() to created new stripe volume on the system*/

struct blkdev *striped_create(int N, struct blkdev *disks[], int unit)
{
    int x;
    int disk_size= disks[0] ->ops->num_blocks (disks[ 0 ]);
    
    for ( x= 0 ; x< N ; x++ )
     {
        if (disk_size != disks[ x] -> ops -> num_blocks(disks[x])) 
        {
            printf( "Unable to create stripe - Unequal Disk sizes.");
            return NULL;
        }
    }

    struct blkdev *dev = malloc(sizeof( *dev ));
    struct stripe_dev *s_dev = malloc (sizeof( *s_dev));

    s_dev -> disks = disks;
    s_dev -> ndisks = N;
    s_dev -> nblks = ( disk_size/unit)*unit;
    s_dev -> unit = unit;
    s_dev -> status = RUNNING;

    dev -> private = s_dev;
    dev -> ops = &stripe_ops;
    return dev;
}


/**********  RAID0 ***************/

struct raid0_dev {
    struct blkdev **disks;
    int ndisks;
    int nblks;
    int unit;

    int mode;
    int failed_disk;
};
enum { NONDEGRADED= 1, DEGRADED = 0};

int raid0_num_blocks(struct blkdev *dev)
{
    struct raid0_dev *s_dev = dev -> private;
    return (s_dev -> ndisks) * (s_dev -> nblks) ;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int raid0_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    

    struct raid0_dev *r_dev = dev -> private;

    if ( dev == NULL ) 
    {
        return E_UNAVAIL;
    }
     
    if ( first_blk < 0 || first_blk + num_blks > dev -> ops->num_blocks (dev))          
    {
        return E_BADADDR;
    }
    int unit = r_dev -> unit;
    int N = r_dev -> ndisks;
 
    while (num_blks > 0) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit) ;
        
        int disk_lba = (first_blk/((r_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        if (number_in_disks > num_blks) 
        {
            number_in_disks= num_blks;
        }
        
        struct blkdev *disk = r_dev -> disks[disk_number];

        if ( r_dev -> mode == DEGRADED )
         {
            return E_UNAVAIL;

        }
            
        

        int val = disk -> ops -> read( disk, disk_lba,number_in_disks, buf );
                                          
        if ( val == SUCCESS ) 
        {
            buf += number_in_disks * BLOCK_SIZE;
            first_blk += number_in_disks;
            num_blks -= number_in_disks;
            continue;
        } 
        else 
        {
            disk -> ops -> close(disk);
            r_dev -> disks[disk_number] = NULL;
            r_dev -> mode -= 1;
            r_dev -> failed_disk = disk_number;
            
            if ( r_dev -> mode == CRASHED) 
            {
                dev -> ops -> close(dev);
                return E_UNAVAIL;
            }
            continue;
        }
    }
    return SUCCESS;
}

/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int raid0_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf)
{
    
    struct raid0_dev *r_dev = dev -> private;

    if ( first_blk < 0 || first_blk + num_blks > dev -> ops ->num_blocks (dev))
    {
        return E_BADADDR;
    }
    
    int N = r_dev -> ndisks;
    int unit = r_dev -> unit;

    

    while ( num_blks > 0 ) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit) ;
        
        int disk_lba = (first_blk/((r_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        struct blkdev *disk = r_dev -> disks[disk_number];
       
        int val = disk -> ops -> write( disk, disk_lba,number_in_disks,buf);
                                           
        if ( val == SUCCESS) 
        {
            buf += number_in_disks * BLOCK_SIZE;
            first_blk += number_in_disks;
            num_blks -= number_in_disks;
        } 
        else if ( val == E_UNAVAIL ) 
        {
            r_dev -> mode = DEGRADED;
            disk -> ops -> close( disk );
            r_dev -> disks[ disk_number ] = NULL;
            continue;
        }
    }
    return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void raid0_close(struct blkdev *dev)
{
    int x;

    struct raid0_dev *r_dev = dev -> private;
    
    for ( x= 0 ; x< r_dev -> ndisks+1 ; x++ ) 
    {
        
        struct blkdev *disk = r_dev -> disks[ x];
        
        if ( disk != NULL )
            disk -> ops -> close (disk);
    }

    free (r_dev);
    free (dev);
}

struct blkdev_ops raid0_ops = {
    .num_blocks = raid0_num_blocks,
    .read = raid0_read,
    .write = raid0_write,
    .close = raid0_close
};

/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *raid0_create(int N, struct blkdev *disks[], int unit)
{
    int x;
    
    int disk_size = disks[0]->ops-> num_blocks(disks[0]);

    for (x= 0 ; x< N ; x++) 
    {
        if ( disk_size != disks[x] ->ops -> num_blocks(disks[x]))
        {
            printf( "Unable to create RAID0 volumes - Unequal Disk sizes.");
            return NULL;
        }
    }

    struct blkdev *dev = malloc(sizeof( *dev));
    struct raid0_dev *r_dev = malloc (sizeof( *r_dev));

    r_dev -> disks = disks;
    r_dev -> ndisks = N;
    r_dev -> nblks = ( disk_size/unit)*unit;
    r_dev -> unit = unit;
    r_dev -> mode = NONDEGRADED;

    dev -> private = r_dev;
    dev -> ops = &raid0_ops;
    return dev;
}

/**********   RAID 4  ***************/

/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do: 
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it could be faster. Don't worry about it.
 */

struct raid4_dev {
    struct blkdev **disks;
    struct blkdev *parity;
    int ndisks;
    int nblks;
    int unit;

    int mode;
    int failed_disk;
};

//enum { NONDEGRADED= 1, DEGRADED = 0};
int raid4_num_blocks( struct blkdev *volume )
{
    struct raid4_dev *r_dev = volume -> private;
    return ( r_dev -> ndisks ) * ( r_dev -> nblks );
}



void parity(int len, void *src1, void *src2, void *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; i++)
        d[i] = s1[i] ^ s2[i];
}

int get_data_for_disk( struct blkdev *volume, int disk_number, int lba, int count, void *buf)
{
    struct raid4_dev *r_dev = volume -> private;

    memset ( buf, '\0', count * BLOCK_SIZE );
    char temp[ count * BLOCK_SIZE ];
    int x, val;
    for ( x = 0 ; x < r_dev -> ndisks + 1 ; x ++) {
        if ( x != disk_number ) {
            struct blkdev *disk = r_dev -> disks[x];
            val = disk -> ops -> read ( disk, lba, count, temp );
            if ( val == E_UNAVAIL ) {
                return val;
            }
            parity( count * BLOCK_SIZE, temp, buf, buf );
        }
    }
    return SUCCESS;
}

/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */
static int raid4_read(struct blkdev * dev, int first_blk,
                      int num_blks, void *buf) 
{
    

    struct raid4_dev *r_dev = dev -> private;

    if ( dev == NULL ) 
    {
        return E_UNAVAIL;
    }
     
    if ( first_blk < 0 || first_blk + num_blks > dev -> ops->num_blocks (dev))          
    {
        return E_BADADDR;
    }
    int unit = r_dev -> unit;
    int N = r_dev -> ndisks;
 
    while (num_blks > 0) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit) ;
        
        int disk_lba = (first_blk/((r_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        if (number_in_disks > num_blks) 
        {
            number_in_disks= num_blks;
        }
        
        struct blkdev *disk = r_dev -> disks[disk_number];

        if ( r_dev -> mode == DEGRADED ) {
            if ( r_dev -> failed_disk == disk_number) 
            {
                if ( get_data_for_disk( dev, disk_number, disk_lba, number_in_disks, buf ) == SUCCESS )
                {
                    buf += number_in_disks * BLOCK_SIZE;
                    first_blk += number_in_disks;
                    num_blks -= number_in_disks;
                    continue;
                } 
                else 
                {
                    r_dev -> mode = CRASHED;
                    dev ->ops ->close(dev);
                    return E_UNAVAIL;
                }
            }
        }

        int val = disk -> ops -> read( disk, disk_lba,number_in_disks, buf );
                                          
        if ( val == SUCCESS ) 
        {
            buf += number_in_disks * BLOCK_SIZE;
            first_blk += number_in_disks;
            num_blks -= number_in_disks;
            continue;
        } 
        else 
        {
            disk -> ops -> close(disk);
            r_dev -> disks[disk_number] = NULL;
            r_dev -> mode -= 1;
            r_dev -> failed_disk = disk_number;
            
            if ( r_dev -> mode == CRASHED) 
            {
                dev -> ops -> close(dev);
                return E_UNAVAIL;
            }
            continue;
        }
    }
    return SUCCESS;
}


/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    
    if ( dev == NULL )
    {
        return E_UNAVAIL;
    }

    struct raid4_dev *r_dev = dev -> private;

    if ( first_blk < 0 || first_blk + num_blks > dev -> ops ->num_blocks ( dev )) 
    {       
        return E_BADADDR;
    }

    int unit = r_dev -> unit;
    int N = r_dev -> ndisks;


    
    while ( num_blks > 0 ) 
    {
        int disk_number = ( first_blk % ( N * unit ) / unit );
        
        int disk_lba = (first_blk/((r_dev -> ndisks) *
                        unit))* unit + (first_blk % unit);
        
        int number_in_disks = unit - ( disk_lba % unit);
        
        if (number_in_disks > num_blks) 
        {
            number_in_disks= num_blks;
        }
        
        struct blkdev *disk = r_dev -> disks[ disk_number];
        struct blkdev *parity_disk = r_dev -> parity;
        
        char temp[ number_in_disks * BLOCK_SIZE ];

        if ( r_dev -> mode == DEGRADED ) 
        {
            if ( r_dev -> failed_disk == disk_number)
             {
                if ( get_data_for_disk( dev, disk_number, disk_lba, number_in_disks, temp ) == SUCCESS )
                {
                    
                } 
                else 
                {
                    dev -> ops -> close (dev);
                    return E_UNAVAIL;
                }


            } 
            else 
            {
                if ( disk -> ops -> read ( disk, disk_lba, number_in_disks, temp ) == SUCCESS )
                {
                    disk -> ops -> write ( disk,disk_lba, number_in_disks, buf );
                } 
                else 
                {
                    dev -> ops -> close(dev);
                    return E_UNAVAIL;
                }

            }
            if ( r_dev -> failed_disk != N ) 
            {
                parity ( number_in_disks * BLOCK_SIZE, buf, temp, temp );
                char parity_buffer[ number_in_disks * BLOCK_SIZE ];

                if ( parity_disk -> ops -> read ( parity_disk, disk_lba, number_in_disks, parity_buffer) == SUCCESS ) 
                {
                    parity ( number_in_disks * BLOCK_SIZE, temp, parity_buffer, parity_buffer );
                    parity_disk -> ops -> write( parity_disk, disk_lba, number_in_disks, parity_buffer );

                    buf += number_in_disks * BLOCK_SIZE;
                    first_blk += number_in_disks;
                    num_blks -= number_in_disks;

                    continue;
                }
                else 
                {
                    dev -> ops -> close (dev);
                    return E_UNAVAIL;
                }
            }
        }

        int val = disk -> ops -> read ( disk,disk_lba, number_in_disks, temp );
        
        if ( val == SUCCESS ) 
        {
            disk -> ops -> write( disk, disk_lba, number_in_disks, buf );
            parity( number_in_disks * BLOCK_SIZE, buf, temp, temp );
            char parity_buffer[ number_in_disks * BLOCK_SIZE ];
            
            if ( parity_disk -> ops -> read ( parity_disk, disk_lba, number_in_disks, parity_buffer) == SUCCESS ) 
            {
                parity ( number_in_disks * BLOCK_SIZE, temp, parity_buffer, parity_buffer );
                parity_disk -> ops -> write( parity_disk, disk_lba, number_in_disks, parity_buffer );

                buf += number_in_disks * BLOCK_SIZE;
                first_blk += number_in_disks;
                num_blks -= number_in_disks;

                continue;
            } 
            else 
            {
                r_dev -> mode = DEGRADED;
                r_dev -> failed_disk = r_dev -> ndisks;

                parity_disk -> ops -> close ( parity_disk );
                r_dev -> parity = NULL;
                continue;
            }
        }
        else 
        {
            r_dev -> mode = DEGRADED;
            disk -> ops -> close( disk );
            r_dev -> disks[ disk_number ] = NULL;
            continue;
        }

    }
    return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev)
{
    int x;

    struct raid4_dev *r_dev = dev -> private;
    
    for ( x= 0 ; x< r_dev -> ndisks+1 ; x++ ) 
    {
        
        struct blkdev *disk = r_dev -> disks[ x];
        
        if ( disk != NULL )
            disk -> ops -> close (disk);
    }

    free (r_dev);
    free (dev);
}

struct blkdev_ops raid4_ops = {
    .num_blocks = raid4_num_blocks,
    .read = raid4_read,
    .write = raid4_write,
    .close = raid4_close
};

/* Initialize a RAID 4 volume with strip size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */
struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit)
{
    int x;
    
    int disk_size = disks[0]->ops-> num_blocks(disks[0]);

    for (x= 0 ; x< N ; x++) 
    {
        if ( disk_size != disks[x] ->ops -> num_blocks(disks[x]))
        {
            printf( "Unable to create RAID4 volumes - Unequal Disk sizes.");
            return NULL;
        }
    }

    struct blkdev *dev = malloc(sizeof( *dev));
    struct raid4_dev *r_dev = malloc (sizeof( *r_dev));

    r_dev -> disks = disks;
    struct blkdev *parity = disks[ N - 1 ];
    r_dev -> parity = parity;

    r_dev -> ndisks = N - 1;
    r_dev -> nblks = (disk_size/unit)*unit;
    r_dev -> unit = unit;

    r_dev -> mode = NONDEGRADED;
    r_dev -> failed_disk = -1;

    dev -> private = r_dev;
    dev -> ops = &raid4_ops;

    return dev;
}


/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    struct raid4_dev *r_dev = volume -> private;

    if ( r_dev->nblks > newdisk -> ops -> num_blocks( newdisk)) 
    {
        return E_SIZE;
    }

    char buf[ (r_dev ->nblks )* BLOCK_SIZE];

    get_data_for_disk(volume,i, 0,r_dev -> nblks, buf);

    newdisk ->ops ->write (newdisk, 0, r_dev -> nblks, buf);

    struct blkdev *disk = r_dev -> disks[i];
    
    if ( disk!= NULL )
    {
        disk ->ops->close(disk);
    }
    r_dev -> disks[i] = newdisk;

    return SUCCESS;
}


