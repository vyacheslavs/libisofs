/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "eltorito.h"
#include "stream.h"
#include "error.h"
#include "fsource.h"
#include "image.h"
#include "messages.h"

#include <stdlib.h>
#include <string.h>

/**
 * This table should be written with accuracy values at offset
 * 8 of boot image, when used ISOLINUX boot loader
 */
struct boot_info_table {
    uint8_t bi_pvd              BP(1, 4);  /* LBA of primary volume descriptor */
    uint8_t bi_file             BP(5, 8);  /* LBA of boot file */
    uint8_t bi_length           BP(9, 12); /* Length of boot file */
    uint8_t bi_csum             BP(13, 16); /* Checksum of boot file */
    uint8_t bi_reserved         BP(17, 56); /* Reserved */  
};

/**
 * Structure for each one of the four entries in a partition table on a
 * hard disk image.
 */
struct partition_desc {
    uint8_t boot_ind;
    uint8_t begin_chs[3];
    uint8_t type;
    uint8_t end_chs[3];
    uint8_t start[4];
    uint8_t size[4];
};

/**
 * Structures for a Master Boot Record of a hard disk image.
 */
struct hard_disc_mbr {
    uint8_t code_area[440];
    uint8_t opt_disk_sg[4];
    uint8_t pad[2];
    struct partition_desc partition[4];
    uint8_t sign1;
    uint8_t sign2;
};

/**
 * Sets the load segment for the initial boot image. This is only for
 * no emulation boot images, and is a NOP for other image types.
 */
void el_torito_set_load_seg(ElToritoBootImage *bootimg, short segment)
{
    if (bootimg->type != ELTORITO_NO_EMUL)
        return;
    bootimg->load_seg = segment;
}

/**
 * Sets the number of sectors (512b) to be load at load segment during
 * the initial boot procedure. This is only for no emulation boot images, 
 * and is a NOP for other image types.
 */
void el_torito_set_load_size(ElToritoBootImage *bootimg, short sectors)
{
    if (bootimg->type != ELTORITO_NO_EMUL)
        return;
    bootimg->load_size = sectors;
}

/**
 * Marks the specified boot image as not bootable
 */
void el_torito_set_no_bootable(ElToritoBootImage *bootimg)
{
    bootimg->bootable = 0;
}

/**
 * Specifies that this image needs to be patched. This involves the writting
 * of a 56 bytes boot information table at offset 8 of the boot image file.
 * The original boot image file won't be modified.
 * This is needed for isolinux boot images.
 */
void el_torito_patch_isolinux_image(ElToritoBootImage *bootimg)
{
    bootimg->isolinux = 1;
}

static
int iso_tree_add_boot_node(IsoDir *parent, const char *name, IsoBoot **boot)
{
    IsoBoot *node;
    IsoNode **pos;
    time_t now;

    if (parent == NULL || name == NULL || boot == NULL) {
        return ISO_NULL_POINTER;
    }
    if (boot) {
        *boot = NULL;
    }
    
    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* find place where to insert */
    pos = &(parent->children);
    while (*pos != NULL && strcmp((*pos)->name, name) < 0) {
        pos = &((*pos)->next);
    }
    if (*pos != NULL && !strcmp((*pos)->name, name)) {
        /* a node with same name already exists */
        return ISO_NODE_NAME_NOT_UNIQUE;
    }

    node = calloc(1, sizeof(IsoBoot));
    if (node == NULL) {
        return ISO_MEM_ERROR;
    }

    node->node.refcount = 1;
    node->node.type = LIBISO_BOOT;
    node->node.name = strdup(name);
    if (node->node.name == NULL) {
        free(node);
        return ISO_MEM_ERROR;
    }

    /* atributes from parent */
    node->node.mode = S_IFREG | (parent->node.mode & 0444);
    node->node.uid = parent->node.uid;
    node->node.gid = parent->node.gid;
    node->node.hidden = parent->node.hidden;

    /* current time */
    now = time(NULL);
    node->node.atime = now;
    node->node.ctime = now;
    node->node.mtime = now;
    
    node->msblock = 0;

    /* add to dir */
    node->node.parent = parent;
    node->node.next = *pos;
    *pos = (IsoNode*)node;

    if (boot) {
        *boot = node;
    }
    return ++parent->nchildren;
}


static 
int create_image(IsoImage *image, const char *image_path,
                 enum eltorito_boot_media_type type,
                 struct el_torito_boot_image **bootimg)
{
    int ret;
    struct el_torito_boot_image *boot;
    int boot_media_type = 0;
    int load_sectors = 0; /* number of sector to load */
    unsigned char partition_type = 0;
    IsoNode *imgfile;
    IsoStream *stream;

    ret = iso_tree_path_to_node(image, image_path, &imgfile);
    if (ret < 0) {
        return ret;
    }
    if (ret == 0) {
        return ISO_FILE_DOESNT_EXIST;
    }
        
    if (imgfile->type != LIBISO_FILE) {
        return ISO_BOOT_IMAGE_NOT_VALID;
    }
    
    stream = ((IsoFile*)imgfile)->stream;
    
    /* we need to read the image at least two times */
    if (!iso_stream_is_repeatable(stream)) {
        return ISO_BOOT_IMAGE_NOT_VALID;
    }
    
    switch (type) {
    case ELTORITO_FLOPPY_EMUL:
        switch (iso_stream_get_size(stream)) {
        case 1200 * 1024:
            boot_media_type = 1; /* 1.2 meg diskette */
            break;
        case 1440 * 1024:
            boot_media_type = 2; /* 1.44 meg diskette */
            break;
        case 2880 * 1024:
            boot_media_type = 3; /* 2.88 meg diskette */
            break;
        default:
            iso_msg_sorry(image->messenger, LIBISO_EL_TORITO_WRONG_IMG, 
                          "Invalid image size %d Kb. Must be one of 1.2, 1.44"
                          "or 2.88 Mb", iso_stream_get_size(stream) / 1024);
            return ISO_BOOT_IMAGE_NOT_VALID;
            break;  
        }
        /* it seems that for floppy emulation we need to load 
         * a single sector (512b) */
        load_sectors = 1;
        break;
    case ELTORITO_HARD_DISC_EMUL:
        {
        size_t i;
        struct hard_disc_mbr mbr;
        int used_partition;
        
        /* read the MBR on disc and get the type of the partition */
        ret = iso_stream_open(stream);
        if (ret < 0) {
            iso_msg_sorry(image->messenger, LIBISO_EL_TORITO_WRONG_IMG, 
                          "Can't open image file.");
            return ret;
        }
        ret = iso_stream_read(stream, &mbr, sizeof(mbr));
        iso_stream_close(stream);
        if (ret != sizeof(mbr)) {
            iso_msg_sorry(image->messenger, LIBISO_EL_TORITO_WRONG_IMG, 
                          "Can't read MBR from image file.");
            return ret < 0 ? ret : ISO_FILE_READ_ERROR;
        }
        
        /* check valid MBR signature */
        if ( mbr.sign1 != 0x55 || mbr.sign2 != 0xAA ) {
            iso_msg_sorry(image->messenger, LIBISO_EL_TORITO_WRONG_IMG, 
                          "Invalid MBR. Wrong signature.");
            return ISO_BOOT_IMAGE_NOT_VALID;
        }
        
        /* ensure single partition */
        used_partition = -1;
        for (i = 0; i < 4; ++i) {
            if (mbr.partition[i].type != 0) {
                /* it's an used partition */
                if (used_partition != -1) {
                    iso_msg_sorry(image->messenger, LIBISO_EL_TORITO_WRONG_IMG, 
                                  "Invalid MBR. At least 2 partitions: %d and " 
                                  "%d, are being used\n", used_partition, i);
                    return ISO_BOOT_IMAGE_NOT_VALID;
                } else
                    used_partition = i;
            }
        }
        partition_type = mbr.partition[used_partition].type;
        }
        boot_media_type = 4;
        
        /* only load the MBR */
        load_sectors = 1;
        break;
    case ELTORITO_NO_EMUL:
        boot_media_type = 0;
        break;  
    }
    
    boot = calloc(1, sizeof(struct el_torito_boot_image));
    if (boot == NULL) {
        return ISO_MEM_ERROR;
    }
    boot->image = (IsoFile*)imgfile;
    iso_node_ref(imgfile); /* get our ref */
    boot->bootable = 1;
    boot->type = boot_media_type;
    boot->load_size = load_sectors;
    boot->partition_type = partition_type;
    
    if (bootimg) {
        *bootimg = boot;
    }
    
    return ISO_SUCCESS;
}

int iso_image_set_boot_image(IsoImage *image, const char *image_path,
                             enum eltorito_boot_media_type type,
                             const char *catalog_path,
                             ElToritoBootImage **boot)
{
    int ret;
    struct el_torito_boot_catalog *catalog;
    ElToritoBootImage *boot_image= NULL;
    IsoBoot *cat_node= NULL;
    
    if (image == NULL || image_path == NULL || catalog_path == NULL) {
        return ISO_NULL_POINTER;
    }
    if (image->bootcat != NULL) {
        return ISO_IMAGE_ALREADY_BOOTABLE;
    }
    
    /* create the node for the catalog */
    {
        IsoDir *parent;
        char *catdir = NULL, *catname = NULL;
        catdir = strdup(catalog_path);
        if (catdir == NULL) {
            return ISO_MEM_ERROR;
        }
        
        /* get both the dir and the name */ 
        catname = strrchr(catdir, '/');
        if (catname == NULL) {
            free(catdir);
            return ISO_WRONG_ARG_VALUE;
        }
        if (catname == catdir) {
            /* we are apending catalog to root node */
            parent = image->root;
        } else {
            IsoNode *p;
            catname[0] = '\0';
            ret = iso_tree_path_to_node(image, catdir, &p);
            if (ret <= 0) {
                free(catdir);
                return ret < 0 ? ret : ISO_FILE_DOESNT_EXIST;
            }
            if (p->type != LIBISO_DIR) {
                free(catdir);
                return ISO_WRONG_ARG_VALUE;
            }
            parent = (IsoDir*)p;
        }
        catname++;
        ret = iso_tree_add_boot_node(parent, catname, &cat_node);
        free(catdir);
        if (ret < 0) {
            return ret;
        }
    }
    
    /* create the boot image */
    ret = create_image(image, image_path, type, &boot_image);
    if (ret < 0) {
        goto boot_image_cleanup;
    }
    
    /* creates the catalog with the given image */
    catalog = malloc(sizeof(struct el_torito_boot_catalog));
    if (catalog == NULL) {
        ret = ISO_MEM_ERROR;
        goto boot_image_cleanup;
    }
    catalog->image = boot_image;
    catalog->node = cat_node;
    iso_node_ref((IsoNode*)cat_node);
    image->bootcat = catalog;
    
    if (boot) {
        *boot = boot_image;
    }
    
    return ISO_SUCCESS;
    
boot_image_cleanup:;
    if (cat_node) {
        iso_node_take((IsoNode*)cat_node);
        iso_node_unref((IsoNode*)cat_node);
    }
    if (boot_image) {
        iso_node_unref((IsoNode*)boot_image->image);
        free(boot_image);
    }
    return ret;
}

/**
 * Get El-Torito boot image of an ISO image, if any.
 * 
 * This can be useful, for example, to check if a volume read from a previous
 * session or an existing image is bootable. It can also be useful to get
 * the image and catalog tree nodes. An application would want those, for 
 * example, to prevent the user removing it.
 * 
 * Both nodes are owned by libisofs and should not be freed. You can get your
 * own ref with iso_node_ref(). You can can also check if the node is already 
 * on the tree by getting its parent (note that when reading El-Torito info 
 * from a previous image, the nodes might not be on the tree even if you haven't 
 * removed them). Remember that you'll need to get a new ref 
 * (with iso_node_ref()) before inserting them again to the tree, and probably 
 * you will also need to set the name or permissions.
 * 
 * @param image
 *      The image from which to get the boot image.
 * @param boot
 *      If not NULL, it will be filled with a pointer to the boot image, if 
 *      any. That  object is owned by the IsoImage and should not be freed by 
 *      the user, nor dereferenced once the last reference to the IsoImage was
 *      disposed via iso_image_unref().
 * @param imgnode 
 *      When not NULL, it will be filled with the image tree node. No extra ref
 *      is added, you can use iso_node_ref() to get one if you need it.
 * @param catnode 
 *      When not NULL, it will be filled with the catnode tree node. No extra 
 *      ref is added, you can use iso_node_ref() to get one if you need it.
 * @return
 *      1 on success, 0 is the image is not bootable (i.e., it has no El-Torito
 *      image), < 0 error.
 */
int iso_image_get_boot_image(IsoImage *image, ElToritoBootImage **boot,
                             IsoFile **imgnode, IsoBoot **catnode)
{
    if (image == NULL) {
        return ISO_NULL_POINTER;
    }
    if (image->bootcat == NULL) {
        return 0;
    }
    
    /* ok, image is bootable */
    if (boot) {
        *boot = image->bootcat->image;
    }
    if (imgnode) {
        *imgnode = image->bootcat->image->image;
    }
    if (catnode) {
        *catnode = image->bootcat->node;
    }
    return ISO_SUCCESS;
}

/**
 * Removes the El-Torito bootable image. 
 * 
 * The IsoBoot node that acts as placeholder for the catalog is also removed
 * for the image tree, if there.
 * If the image is not bootable (don't have el-torito boot image) this function
 * just returns.
 */
void iso_image_remove_boot_image(IsoImage *image)
{
    if (image == NULL || image->bootcat == NULL)
        return;
    
    /* 
     * remove catalog node from its parent 
     * (the reference will be disposed next) 
     */
    iso_node_take((IsoNode*)image->bootcat->node);
    
    /* free boot catalog and image, including references to nodes */
    el_torito_boot_catalog_free(image->bootcat);
    image->bootcat = NULL;
}

void el_torito_boot_catalog_free(struct el_torito_boot_catalog *cat)
{
    struct el_torito_boot_image *image;
    
    if (cat == NULL) {
        return;
    }
    
    image = cat->image;
    iso_node_unref((IsoNode*)image->image);
    free(image);
    iso_node_unref((IsoNode*)cat->node);
    free(cat);
}

/**
 * Stream that generates the contents of a El-Torito catalog.
 */
struct catalog_stream
{
    struct el_torito_boot_catalog *catalog;
    uint8_t buffer[BLOCK_SIZE];
    int offset; /* -1 if stream is not openned */
};

static void 
write_validation_entry(uint8_t *buf)
{
    size_t i;
    int checksum;
    
    struct el_torito_validation_entry *ve = 
        (struct el_torito_validation_entry*)buf;
    ve->header_id[0] = 1;
    ve->platform_id[0] = 0; /* 0: 80x86, 1: PowerPC, 2: Mac */
    ve->key_byte1[0] = 0x55;
    ve->key_byte2[0] = 0xAA;
    
    /* calculate the checksum, to ensure sum of all words is 0 */
    checksum = 0;
    for (i = 0; i < sizeof(struct el_torito_validation_entry); i += 2) {
        checksum -= buf[i];
        checksum -= (buf[i] << 8);
    }
    iso_lsb(ve->checksum, checksum, 2);
}
