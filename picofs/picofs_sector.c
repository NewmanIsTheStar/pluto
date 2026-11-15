/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <limits.h> 
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <hardware/flash.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "picofs.h"


#define SET_BIT(arr, b)   ((arr)[(b) / CHAR_BIT] |= (1U << ((b) % CHAR_BIT)))
#define CLEAR_BIT(arr, b) ((arr)[(b) / CHAR_BIT] &= ~(1U << ((b) % CHAR_BIT)))
#define FLIP_BIT(arr, b)  ((arr)[(b) / CHAR_BIT] ^= (1U << ((b) % CHAR_BIT)))
#define GET_BIT(arr, b)   (((arr)[(b) / CHAR_BIT] >> ((b) % CHAR_BIT)) & 1U)

//prototypes
bool picofs_sector_in_use(uint32_t sector);
bool picofs_sector_erased(uint32_t sector);


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif
extern FILE_STATUS_T picofs_files[FS_NUM_FID]; 
extern SemaphoreHandle_t picofs_mutex;

//static variables


/*!
 * \brief erase obsolete sectors
 * 
 *    
 * \return 0 on success
 */
int picofs_erase_obsolete_sectors(bool picofs_mutext_held)
{
    int err = -1;
    uint32_t i;
    uint32_t start_sector;
    uint32_t end_sector;
    uint32_t num_sectors_erased = 0;        


    if (picofs_mutext_held)
    {
        err = 0;
    }
    else if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        err = 0;
    }

    if (!err)
    {
        picofs_refresh_files();

        for(start_sector=0; start_sector < FS_NUM_SECTORS; start_sector++)
        {
            if (!picofs_sector_in_use(start_sector) && !picofs_sector_erased(start_sector))
            {
                end_sector = start_sector;

                // try to expand range
                for (i = start_sector; i < FS_NUM_SECTORS; i++)
                {
                    if ((!picofs_sector_in_use(i)) && !picofs_sector_erased(i))
                    {
                        end_sector = i;
                    }
                    else
                    {
                        break;
                    }
                }

                if (start_sector == end_sector)
                {
                    picofs_printf("--ERASING Sector %d\n", start_sector);
                    num_sectors_erased++;
                }
                else
                {
                    picofs_printf("--ERASING Sectors %d to %d\n", start_sector, end_sector);
                    num_sectors_erased += (end_sector - start_sector + 1);
                }

                picofs_flash_erase_sector_range(start_sector, end_sector);
                start_sector = end_sector;
            }
            else
            {
                //picofs_printf("Not erasing sector %d @ %p because it is either in use or already erased\n", start_sector, FS_START+(start_sector*FS_SECTOR_SIZE));

                if (!picofs_sector_erased(start_sector))
                {
                    picofs_printf("Not erasing sector %d @ %p because it is in use\n", start_sector, FS_START+(start_sector*FS_SECTOR_SIZE));
                }
            }
        }

        if (!picofs_mutext_held)
        {
            xSemaphoreGive(picofs_mutex); 
        }

        picofs_printf("Total Sectors erased: %d\n", num_sectors_erased);

        picofs_refresh_files();
    }

    return(err);
}

/*!
 * \brief check if sector used by any file
 * 
 * \param[in]   sector     sector to check
 * 
 *     
 * \return 0 on success
 */
bool picofs_sector_in_use(uint32_t sector)
{
    uint32_t start_sector = 0;
    uint32_t end_sector = 0;
    bool in_use = false;
    int i;
    
    // check ram
    for(i=0; i < FS_NUM_FID; i++)
    {
        if (picofs_files[i].valid && picofs_files[i].trailer)
        {
            start_sector = picofs_get_start_sector(picofs_files[i].trailer);

            end_sector = picofs_get_end_sector(picofs_files[i].trailer);

            if ((sector >= start_sector) && (sector <= end_sector))
            {
                // sector overlaps with latest version of file
                in_use = true;

                // check if deleted file that is ready for erasure
                if ((picofs_files[i].trailer->file_status & STS_DELETED) && picofs_deleted_file_ready_for_erasure(picofs_files[i].trailer))
                {
                    // deleted file is safe to erase
                    in_use = false;                    
                }
                else
                {
                    // the sector is in use
                    picofs_printf("sector %d in use by file %s FID %d SEQ %d\n", sector, picofs_files[i].trailer->name, picofs_files[i].trailer->file_id, picofs_files[i].trailer->file_sequence);
                    break;
                }
            }
        }
    }

    if (!in_use)
    {
        // check open READ file descriptors (writes are in RAM cache and not affected by erasing flash sectors
        // ==>> TODO: this comment is not correct now that we support  large files!!!
        for (i = 0; i < FS_MAX_FILE_DESCRIPTORS; i++) 
        {
            if ((custom_fds[i].in_use) && (custom_fds[i].flags & FREAD) && (custom_fds[i].flash_file_trailer))
            {
                start_sector = picofs_get_start_sector(custom_fds[i].flash_file_trailer);

                end_sector = picofs_get_end_sector(custom_fds[i].flash_file_trailer);

                if ((sector >= start_sector) && (sector <= end_sector))
                {
                    in_use= true;
                    break;
                }

                //TODO: atomically shift reader's FD to point to latest SEQ if the reader is using an obsolete copy due to consolidation
            }
        }
    }

    return(in_use);
}

/*!
 * \brief check if sector is erased
 * 
 * \param[in]   sector     sector to check
 * 
 *     
 * \return 0 on success
 */
bool picofs_sector_erased(uint32_t sector)
{
    uint32_t start_sector = 0;
    uint32_t end_sector = 0;
    bool erased = true;
    int i;
    char *cell;
    
    for(cell = (char *)(FLASH_SCAN_START + sector *FS_SECTOR_SIZE); cell < (char *)(FLASH_SCAN_START + (sector+1) *FS_SECTOR_SIZE); cell++)
    {        
        if (*cell != FS_ERASED_CELL_VALUE)
        {            
            erased = false;
            break;
        }
    }

    return(erased);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
uint32_t picofs_get_start_sector(FILE_TRAILER_T *trailer)
{
    uint32_t start_block;
    
    start_block = ((((char *)trailer + sizeof(FILE_TRAILER_T)) - trailer->file_size) - FS_START)/FS_SECTOR_SIZE; 

    return(start_block);
}

/*!
 * \brief check if sector is obsolete
 * 
 * \param[in]   filename     name to find
 * 
 * \param[out]  trailer      pointer to file trailer
 *  *     
 * \return 0 on success
 */
uint32_t picofs_get_end_sector(FILE_TRAILER_T *trailer)
{
    uint32_t end_block;
    
    end_block = (((char *)trailer + sizeof(FILE_TRAILER_T) - 1) - FS_START)/FS_SECTOR_SIZE; 

    return(end_block);
}

/*!
 * \brief erase obsolete sectors in an unsafe manner (zombie files can be resurrected if operation is interrupted e.g. power fails)
 * 
 *    
 * \return 0 on success
 */
int picofs_purge_obsolete_sectors(bool picofs_mutext_held)
{
    int err = -1;
    uint32_t i, j;
    uint32_t start_sector;
    uint32_t end_sector;
    uint32_t num_sectors_erased = 0;        
    uint8_t sector_bitmap[(FS_NUM_SECTORS+1)/8];

    if (picofs_mutext_held)
    {
        err = 0;
    }
    else if (xSemaphoreTake(picofs_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        err = 0;
    }

    if (!err)
    {
        // initialize
        memset(sector_bitmap, 0, sizeof(sector_bitmap));
        picofs_refresh_files();

        // record erased sectors in the bitmap
        for(i=0; i < FS_NUM_SECTORS; i++)
        {
            if (picofs_sector_erased(i))
            {
                SET_BIT(sector_bitmap, i);
            }
        }

        // record the sectors used by the latest version of each file in the bitmap
        for(i=0; i < FS_NUM_FID; i++)
        {
            if (picofs_files[i].valid && picofs_files[i].trailer && !(picofs_files[i].trailer->file_status & STS_DELETED)) 
            {
                start_sector = picofs_get_start_sector(picofs_files[i].trailer);
                end_sector = picofs_get_end_sector(picofs_files[i].trailer);

                for (j=start_sector; j <= end_sector; j++)
                {
                    SET_BIT(sector_bitmap, j);
                }
            }
        }

        // TODO: need to also check open file descriptors and and this to bitmap


        // erase all sectors that are not in use or already erased
        for(start_sector=0; start_sector<FS_NUM_SECTORS; start_sector++)
        {
            if (!(GET_BIT(sector_bitmap,start_sector)))
            {
                end_sector = start_sector;
                while (!(GET_BIT(sector_bitmap, end_sector+1)) && end_sector < (FS_NUM_SECTORS-1))
                {
                    end_sector++;
                }

                if (start_sector == end_sector)
                {
                    picofs_printf("--ERASING Sector %d\n", start_sector);
                    num_sectors_erased++;
                }
                else
                {
                    picofs_printf("--ERASING Sectors %d to %d\n", start_sector, end_sector);
                    num_sectors_erased += (end_sector - start_sector + 1);
                }

                picofs_flash_erase_sector_range(start_sector, end_sector);
                start_sector = end_sector;                
            }
        }

        if (!picofs_mutext_held)
        {
            xSemaphoreGive(picofs_mutex); 
        }

        picofs_printf("Total Sectors erased: %d\n", num_sectors_erased);

        picofs_refresh_files();
    }

    return(err);
}