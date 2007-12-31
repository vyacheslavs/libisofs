/*
 * Little program to output the contents of an iso image.
 * Note that this is not an API example, but a little program for test
 * purposes.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "messages.h"
#include "libisofs.h"
#include "fs_image.h"

int main(int argc, char **argv)
{
    int result;
    IsoImageFilesystem *fs;
    IsoDataSource *src;
    struct iso_read_opts opts = {
        0, /* block */
        0, /* norock */
        0, /* nojoliet */
        0, /* preferjoliet */
        0, /* uid; */
        0, /* gid; */
        0, /* mode */
        NULL, /* messenger */
        "UTF-8" /* input_charset */
    };

    if (argc != 2) {
        printf ("You need to specify a valid path\n");
        return 1;
    }

    result = libiso_msgs_new(&opts.messenger, 0);
    if (result <= 0) {
        printf ("Can't create messenger\n");
        return 1;
    }
    libiso_msgs_set_severities(opts.messenger, LIBISO_MSGS_SEV_NEVER, 
                               LIBISO_MSGS_SEV_ALL, "", 0);
    
    result = iso_data_source_new_from_file(argv[1], &src);
    if (result < 0) {
        printf ("Error creating data source\n");
        return 1;
    }

    result = iso_image_filesystem_new(src, &opts, &fs);
    if (result < 0) {
        printf ("Error creating filesystem\n");
        return 1;
    }
    
    iso_filesystem_unref((IsoFilesystem*)fs);
    iso_data_source_unref(src);
	return 0;
}
