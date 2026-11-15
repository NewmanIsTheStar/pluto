/**
 * Copyright (c) 2025 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include "picofs.h"


//prototypes


// external variables
extern PICOFS_FD_T custom_fds[FS_MAX_FILE_DESCRIPTORS];
extern FILE_STATUS_T picofs_files[FS_NUM_FID];
#if FAKE_FLASH == 1
extern FILE_TEST_T test_filesystem[FS_TEST_ROWS];
#endif

//static variables

 

int picofs_unlink(const char *name, uint8_t fid) 
{
    int err = -1;
    int i;
    int fd;

    fd = picofs_find_available_fd();   // NB we are bypassing the wrappers so have to manage file descriptors directly in this function
    
    if (fd == -1) 
    {
        errno = ENFILE; // Too many open files
        return -1;
    }

    if (picofs_open_file(fd, name, O_WRONLY | O_TRUNC, fid, true, 0))   
    {
        errno = ENOENT; // File not found
        return -1;
    }

    custom_fds[fd].in_use = true;

    custom_fds[fd].file_status |= STS_DELETED;  // mark for deletion
    custom_fds[fd].data_len = 0;                // empty file
 
    picofs_close_file(fd, true);
    
    picofs_release_fd(fd);

    return 0;
}


/*!
 * \brief delete multiple files with names that match filter
 * 
 * \param[in]   filter       names to find
 * 
 *      
 * \return 0 on success
 */
int picofs_unlink_multiple(char *filter)
{
    int err = 0;
    int unlink_err = 0;
    int i;
    int num_files_deleted = 0;

    for (i=0; i<FS_NUM_FID; i++)
    {
        if (picofs_files[i].valid && !(picofs_files[i].trailer->file_status & STS_DELETED))
        {
            if (filter && picofs_wildcard_match(filter, picofs_files[i].trailer->name))
            {
                unlink_err = picofs_unlink(NULL, i);
                
                if (unlink_err)
                {
                    picofs_printf("unlink: error %d while deleting %s\n", unlink_err, picofs_files[i].trailer->name);
                    err = -1;
                }
                else
                {
                    num_files_deleted++;
                }
            }           
        }
    }

    if (num_files_deleted > 1)
    {
        picofs_printf("%d files deleted\n", num_files_deleted);
    }

    return(err);
}