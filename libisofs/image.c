/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "image.h"
#include "node.h"
#include "messages.h"
#include "eltorito.h"

#include <stdlib.h>
#include <string.h>

/**
 * Create a new image, empty.
 * 
 * The image will be owned by you and should be unref() when no more needed.
 * 
 * @param name
 *     Name of the image. This will be used as volset_id and volume_id.
 * @param image
 *     Location where the image pointer will be stored.
 * @return
 *     1 sucess, < 0 error
 */
int iso_image_new(const char *name, IsoImage **image)
{
    int res;
    IsoImage *img;

    if (image == NULL) {
        return ISO_NULL_POINTER;
    }

    img = calloc(1, sizeof(IsoImage));
    if (img == NULL) {
        return ISO_OUT_OF_MEM;
    }

    /* local filesystem will be used by default */
    res = iso_local_filesystem_new(&(img->fs));
    if (res < 0) {
        free(img);
        return ISO_OUT_OF_MEM;
    }

    /* use basic builder as default */
    res = iso_node_basic_builder_new(&(img->builder));
    if (res < 0) {
        iso_filesystem_unref(img->fs);
        free(img);
        return ISO_OUT_OF_MEM;
    }

    /* fill image fields */
    res = iso_node_new_root(&img->root);
    if (res < 0) {
        iso_node_builder_unref(img->builder);
        iso_filesystem_unref(img->fs);
        free(img);
        return res;
    }
    img->refcount = 1;
    img->id = iso_message_id++;

    if (name != NULL) {
        img->volset_id = strdup(name);
        img->volume_id = strdup(name);
    }
    *image = img;
    return ISO_SUCCESS;
}

/**
 * Increments the reference counting of the given image.
 */
void iso_image_ref(IsoImage *image)
{
    ++image->refcount;
}

/**
 * Decrements the reference couting of the given image.
 * If it reaches 0, the image is free, together with its tree nodes (whether 
 * their refcount reach 0 too, of course).
 */
void iso_image_unref(IsoImage *image)
{
    if (--image->refcount == 0) {
        int nexcl;

        /* we need to free the image */
        if (image->user_data_free != NULL) {
            /* free attached data */
            image->user_data_free(image->user_data);
        }

        for (nexcl = 0; nexcl < image->nexcludes; ++nexcl) {
            free(image->excludes[nexcl]);
        }
        free(image->excludes);

        iso_node_unref((IsoNode*)image->root);
        iso_node_builder_unref(image->builder);
        iso_filesystem_unref(image->fs);
        el_torito_boot_catalog_free(image->bootcat);
        free(image->volset_id);
        free(image->volume_id);
        free(image->publisher_id);
        free(image->data_preparer_id);
        free(image->system_id);
        free(image->application_id);
        free(image->copyright_file_id);
        free(image->abstract_file_id);
        free(image->biblio_file_id);
        free(image);
    }
}

/**
 * Attach user defined data to the image. Use this if your application needs
 * to store addition info together with the IsoImage. If the image already
 * has data attached, the old data will be freed.
 * 
 * @param data
 *      Pointer to application defined data that will be attached to the
 *      image. You can pass NULL to remove any already attached data.
 * @param give_up
 *      Function that will be called when the image does not need the data
 *      any more. It receives the data pointer as an argumente, and eventually
 *      causes data to be free. It can be NULL if you don't need it.
 */
int iso_image_attach_data(IsoImage *image, void *data, void (*give_up)(void*))
{
    if (image == NULL || (data != NULL && free == NULL)) {
        return ISO_NULL_POINTER;
    }
    
    if (image->user_data != NULL) {
        /* free previously attached data */
        if (image->user_data_free) {
            image->user_data_free(image->user_data);
        }
        image->user_data = NULL;
        image->user_data_free = NULL;
    }
    
    if (data != NULL) {
        image->user_data = data;
        image->user_data_free = give_up;
    }
    return ISO_SUCCESS;
}

/**
 * The the data previously attached with iso_image_attach_data()
 */
void *iso_image_get_attached_data(IsoImage *image)
{
    return image->user_data;
}

IsoDir *iso_image_get_root(const IsoImage *image)
{
    return image->root;
}

void iso_image_set_volset_id(IsoImage *image, const char *volset_id)
{
    free(image->volset_id);
    image->volset_id = strdup(volset_id);
}

const char *iso_image_get_volset_id(const IsoImage *image)
{
    return image->volset_id;
}

void iso_image_set_volume_id(IsoImage *image, const char *volume_id)
{
    free(image->volume_id);
    image->volume_id = strdup(volume_id);
}

const char *iso_image_get_volume_id(const IsoImage *image)
{
    return image->volume_id;
}

void iso_image_set_publisher_id(IsoImage *image, const char *publisher_id)
{
    free(image->publisher_id);
    image->publisher_id = strdup(publisher_id);
}

const char *iso_image_get_publisher_id(const IsoImage *image)
{
    return image->publisher_id;
}

void iso_image_set_data_preparer_id(IsoImage *image,
                                    const char *data_preparer_id)
{
    free(image->data_preparer_id);
    image->data_preparer_id = strdup(data_preparer_id);
}

const char *iso_image_get_data_preparer_id(const IsoImage *image)
{
    return image->data_preparer_id;
}

void iso_image_set_system_id(IsoImage *image, const char *system_id)
{
    free(image->system_id);
    image->system_id = strdup(system_id);
}

const char *iso_image_get_system_id(const IsoImage *image)
{
    return image->system_id;
}

void iso_image_set_application_id(IsoImage *image, const char *application_id)
{
    free(image->application_id);
    image->application_id = strdup(application_id);
}

const char *iso_image_get_application_id(const IsoImage *image)
{
    return image->application_id;
}

void iso_image_set_copyright_file_id(IsoImage *image,
                                     const char *copyright_file_id)
{
    free(image->copyright_file_id);
    image->copyright_file_id = strdup(copyright_file_id);
}

const char *iso_image_get_copyright_file_id(const IsoImage *image)
{
    return image->copyright_file_id;
}

void iso_image_set_abstract_file_id(IsoImage *image,
                                    const char *abstract_file_id)
{
    free(image->abstract_file_id);
    image->abstract_file_id = strdup(abstract_file_id);
}

const char *iso_image_get_abstract_file_id(const IsoImage *image)
{
    return image->abstract_file_id;
}

void iso_image_set_biblio_file_id(IsoImage *image, const char *biblio_file_id)
{
    free(image->biblio_file_id);
    image->biblio_file_id = strdup(biblio_file_id);
}

const char *iso_image_get_biblio_file_id(const IsoImage *image)
{
    return image->biblio_file_id;
}

int iso_image_get_msg_id(IsoImage *image)
{
    return image->id;
}
