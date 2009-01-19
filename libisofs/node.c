/*
 * Copyright (c) 2007 Vreixo Formoso
 *
 * This file is part of the libisofs project; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See COPYING file for details.
 */

#include "libisofs.h"
#include "node.h"
#include "stream.h"

#ifdef Libisofs_with_aaiP
#include "aaip_0_2.h"
#endif


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

struct dir_iter_data
{
    /* points to the last visited child, to NULL before start */
    IsoNode *pos;

    /* Some control flags.
     * bit 0 -> 1 if next called, 0 reseted at start or on deletion
     */
    int flag;
};

/**
 * Increments the reference counting of the given node.
 */
void iso_node_ref(IsoNode *node)
{
    ++node->refcount;
}

/**
 * Decrements the reference couting of the given node.
 * If it reach 0, the node is free, and, if the node is a directory,
 * its children will be unref() too.
 */
void iso_node_unref(IsoNode *node)
{
    if (--node->refcount == 0) {
        switch (node->type) {
        case LIBISO_DIR:
            {
                IsoNode *child = ((IsoDir*)node)->children;
                while (child != NULL) {
                    IsoNode *tmp = child->next;
                    child->parent = NULL;
                    iso_node_unref(child);
                    child = tmp;
                }
            }
            break;
        case LIBISO_FILE:
            {
                IsoFile *file = (IsoFile*) node;
                iso_stream_unref(file->stream);
            }
            break;
        case LIBISO_SYMLINK:
            {
                IsoSymlink *link = (IsoSymlink*) node;
                free(link->dest);
            }
        default:
            /* other kind of nodes does not need to delete anything here */
            break;
        }

        if (node->xinfo) {
            IsoExtendedInfo *info = node->xinfo;
            while (info != NULL) {
                IsoExtendedInfo *tmp = info->next;

                /* free extended info */
                info->process(info->data, 1);
                free(info);
                info = tmp;
            }
        }
        free(node->name);
        free(node);
    }
}

/**
 * Add extended information to the given node. Extended info allows
 * applications (and libisofs itself) to add more information to an IsoNode.
 * You can use this facilities to associate new information with a given
 * node.
 *
 * Each node keeps a list of added extended info, meaning you can add several
 * extended info data to each node. Each extended info you add is identified
 * by the proc parameter, a pointer to a function that knows how to manage
 * the external info data. Thus, in order to add several types of extended
 * info, you need to define a "proc" function for each type.
 *
 * @param node
 *      The node where to add the extended info
 * @param proc
 *      A function pointer used to identify the type of the data, and that
 *      knows how to manage it
 * @param data
 *      Extended info to add.
 * @return
 *      1 if success, 0 if the given node already has extended info of the
 *      type defined by the "proc" function, < 0 on error
 */
int iso_node_add_xinfo(IsoNode *node, iso_node_xinfo_func proc, void *data)
{
    IsoExtendedInfo *info;
    IsoExtendedInfo *pos;

    if (node == NULL || proc == NULL) {
        return ISO_NULL_POINTER;
    }

    pos = node->xinfo;
    while (pos != NULL) {
        if (pos->process == proc) {
            return 0; /* extended info already added */
        }
        pos = pos->next;
    }

    info = malloc(sizeof(IsoExtendedInfo));
    if (info == NULL) {
        return ISO_OUT_OF_MEM;
    }
    info->next = node->xinfo;
    info->data = data;
    info->process = proc;
    node->xinfo = info;
    return ISO_SUCCESS;
}

/**
 * Remove the given extended info (defined by the proc function) from the
 * given node.
 *
 * @return
 *      1 on success, 0 if node does not have extended info of the requested
 *      type, < 0 on error
 */
int iso_node_remove_xinfo(IsoNode *node, iso_node_xinfo_func proc)
{
    IsoExtendedInfo *pos, *prev;

    if (node == NULL || proc == NULL) {
        return ISO_NULL_POINTER;
    }

    prev = NULL;
    pos = node->xinfo;
    while (pos != NULL) {
        if (pos->process == proc) {
            /* this is the extended info we want to remove */
            pos->process(pos->data, 1);

            if (prev != NULL) {
                prev->next = pos->next;
            } else {
                node->xinfo = pos->next;
            }
            free(pos);
            return ISO_SUCCESS;
        }
        prev = pos;
        pos = pos->next;
    }
    /* requested xinfo not found */
    return 0;
}

/**
 * Get the given extended info (defined by the proc function) from the
 * given node.
 *
 * @param data
 *      Will be filled with the extended info corresponding to the given proc
 *      function
 * @return
 *      1 on success, 0 if node does not have extended info of the requested
 *      type, < 0 on error
 */
int iso_node_get_xinfo(IsoNode *node, iso_node_xinfo_func proc, void **data)
{
    IsoExtendedInfo *pos;

    if (node == NULL || proc == NULL || data == NULL) {
        return ISO_NULL_POINTER;
    }

    pos = node->xinfo;
    while (pos != NULL) {
        if (pos->process == proc) {
            /* this is the extended info we want */
            *data = pos->data;
            return ISO_SUCCESS;
        }
        pos = pos->next;
    }
    /* requested xinfo not found */
    return 0;
}

/**
 * Get the type of an IsoNode.
 */
enum IsoNodeType iso_node_get_type(IsoNode *node)
{
    return node->type;
}

/**
 * Set the name of a node.
 *
 * @param name  The name in UTF-8 encoding
 */
int iso_node_set_name(IsoNode *node, const char *name)
{
    char *new;

    if ((IsoNode*)node->parent == node) {
        /* you can't change name of the root node */
        return ISO_WRONG_ARG_VALUE;
    }

    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    if (node->parent != NULL) {
        /* check if parent already has a node with same name */
        if (iso_dir_get_node(node->parent, name, NULL) == 1) {
            return ISO_NODE_NAME_NOT_UNIQUE;
        }
    }

    new = strdup(name);
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    free(node->name);
    node->name = new;
    if (node->parent != NULL) {
        IsoDir *parent;
        int res;
        /* take and add again to ensure correct children order */
        parent = node->parent;
        iso_node_take(node);
        res = iso_dir_add_node(parent, node, 0);
        if (res < 0) {
            return res;
        }
    }
    return ISO_SUCCESS;
}

/**
 * Get the name of a node (in UTF-8).
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_node_get_name(const IsoNode *node)
{
    return node->name;
}

/**
 * Set the permissions for the node. This attribute is only useful when
 * Rock Ridge extensions are enabled.
 *
 * @param mode
 *     bitmask with the permissions of the node, as specified in 'man 2 stat'.
 *     The file type bitfields will be ignored, only file permissions will be
 *     modified.
 */
void iso_node_set_permissions(IsoNode *node, mode_t mode)
{
    node->mode = (node->mode & S_IFMT) | (mode & ~S_IFMT);

#ifdef Libisofs_with_aaiP

/* Linux man 5 acl says:
     The permissions defined by ACLs are a superset of the permissions speci-
     fied by the file permission bits. The permissions defined for the file
     owner correspond to the permissions of the ACL_USER_OBJ entry.  The per-
     missions defined for the file group correspond to the permissions of the
     ACL_GROUP_OBJ entry, if the ACL has no ACL_MASK entry. If the ACL has an
     ACL_MASK entry, then the permissions defined for the file group corre-
     spond to the permissions of the ACL_MASK entry. The permissions defined
     for the other class correspond to the permissions of the ACL_OTHER_OBJ
     entry.

     Modification of the file permission bits results in the modification of
     the permissions in the associated ACL entries. Modification of the per-
     missions in the ACL entries results in the modification of the file per-
     mission bits.

*/
    /* >>> if the node has ACL info : */
      /* >>> update ACL */;

#endif

}

/**
 * Get the permissions for the node
 */
mode_t iso_node_get_permissions(const IsoNode *node)
{
    return node->mode & ~S_IFMT;
}

/**
 * Get the mode of the node, both permissions and file type, as specified in
 * 'man 2 stat'.
 */
mode_t iso_node_get_mode(const IsoNode *node)
{
    return node->mode;
}

/**
 * Set the user id for the node. This attribute is only useful when
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_uid(IsoNode *node, uid_t uid)
{
    node->uid = uid;
}

/**
 * Get the user id of the node.
 */
uid_t iso_node_get_uid(const IsoNode *node)
{
    return node->uid;
}

/**
 * Set the group id for the node. This attribute is only useful when
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_gid(IsoNode *node, gid_t gid)
{
    node->gid = gid;
}

/**
 * Get the group id of the node.
 */
gid_t iso_node_get_gid(const IsoNode *node)
{
    return node->gid;
}

/**
 * Set the time of last modification of the file
 */
void iso_node_set_mtime(IsoNode *node, time_t time)
{
    node->mtime = time;
}

/**
 * Get the time of last modification of the file
 */
time_t iso_node_get_mtime(const IsoNode *node)
{
    return node->mtime;
}

/**
 * Set the time of last access to the file
 */
void iso_node_set_atime(IsoNode *node, time_t time)
{
    node->atime = time;
}

/**
 * Get the time of last access to the file
 */
time_t iso_node_get_atime(const IsoNode *node)
{
    return node->atime;
}

/**
 * Set the time of last status change of the file
 */
void iso_node_set_ctime(IsoNode *node, time_t time)
{
    node->ctime = time;
}

/**
 * Get the time of last status change of the file
 */
time_t iso_node_get_ctime(const IsoNode *node)
{
    return node->ctime;
}

void iso_node_set_hidden(IsoNode *node, int hide_attrs)
{
    /* you can't hide root node */
    if ((IsoNode*)node->parent != node) {
        node->hidden = hide_attrs;
    }
}

/**
 * Add a new node to a dir. Note that this function don't add a new ref to
 * the node, so you don't need to free it, it will be automatically freed
 * when the dir is deleted. Of course, if you want to keep using the node
 * after the dir life, you need to iso_node_ref() it.
 *
 * @param dir
 *     the dir where to add the node
 * @param child
 *     the node to add. You must ensure that the node hasn't previously added
 *     to other dir, and that the node name is unique inside the child.
 *     Otherwise this function will return a failure, and the child won't be
 *     inserted.
 * @param replace
 *     if the dir already contains a node with the same name, whether to
 *     replace or not the old node with this.
 * @return
 *     number of nodes in dir if succes, < 0 otherwise
 */
int iso_dir_add_node(IsoDir *dir, IsoNode *child,
                     enum iso_replace_mode replace)
{
    IsoNode **pos;

    if (dir == NULL || child == NULL) {
        return ISO_NULL_POINTER;
    }
    if ((IsoNode*)dir == child) {
        return ISO_WRONG_ARG_VALUE;
    }

    /*
     * check if child is already added to another dir, or if child
     * is the root node, where parent == itself
     */
    if (child->parent != NULL || child->parent == (IsoDir*)child) {
        return ISO_NODE_ALREADY_ADDED;
    }

    iso_dir_find(dir, child->name, &pos);
    return iso_dir_insert(dir, child, pos, replace);
}

/**
 * Locate a node inside a given dir.
 *
 * @param name
 *     The name of the node
 * @param node
 *     Location for a pointer to the node, it will filled with NULL if the dir
 *     doesn't have a child with the given name.
 *     The node will be owned by the dir and shouldn't be unref(). Just call
 *     iso_node_ref() to get your own reference to the node.
 *     Note that you can pass NULL is the only thing you want to do is check
 *     if a node with such name already exists on dir.
 * @return
 *     1 node found, 0 child has no such node, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir or name are NULL
 */
int iso_dir_get_node(IsoDir *dir, const char *name, IsoNode **node)
{
    int ret;
    IsoNode **pos;
    if (dir == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }

    ret = iso_dir_exists(dir, name, &pos);
    if (ret == 0) {
        if (node) {
            *node = NULL;
        }
        return 0; /* node not found */
    }

    if (node) {
        *node = *pos;
    }
    return 1;
}

/**
 * Get the number of children of a directory.
 *
 * @return
 *     >= 0 number of items, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir is NULL
 */
int iso_dir_get_children_count(IsoDir *dir)
{
    if (dir == NULL) {
        return ISO_NULL_POINTER;
    }
    return dir->nchildren;
}

static
int iter_next(IsoDirIter *iter, IsoNode **node)
{
    struct dir_iter_data *data;
    if (iter == NULL || node == NULL) {
        return ISO_NULL_POINTER;
    }

    data = iter->data;

    /* clear next flag */
    data->flag &= ~0x01;

    if (data->pos == NULL) {
        /* we are at the beginning */
        data->pos = iter->dir->children;
        if (data->pos == NULL) {
            /* empty dir */
            *node = NULL;
            return 0;
        }
    } else {
        if (data->pos->parent != iter->dir) {
            /* this can happen if the node has been moved to another dir */
            /* TODO specific error */
            return ISO_ERROR;
        }
        if (data->pos->next == NULL) {
            /* no more children */
            *node = NULL;
            return 0;
        } else {
            /* free reference to current position */
            iso_node_unref(data->pos); /* it is never last ref!! */

            /* advance a position */
            data->pos = data->pos->next;
        }
    }

    /* ok, take a ref to the current position, to prevent internal errors
     * if deleted somewhere */
    iso_node_ref(data->pos);
    data->flag |= 0x01; /* set next flag */

    /* return pointed node */
    *node = data->pos;
    return ISO_SUCCESS;
}

/**
 * Check if there're more children.
 *
 * @return
 *     1 dir has more elements, 0 no, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if iter is NULL
 */
static
int iter_has_next(IsoDirIter *iter)
{
    struct dir_iter_data *data;
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    data = iter->data;
    if (data->pos == NULL) {
        return iter->dir->children == NULL ? 0 : 1;
    } else {
        return data->pos->next == NULL ? 0 : 1;
    }
}

static
void iter_free(IsoDirIter *iter)
{
    struct dir_iter_data *data;
    data = iter->data;
    if (data->pos != NULL) {
        iso_node_unref(data->pos);
    }
    free(data);
}

static IsoNode** iso_dir_find_node(IsoDir *dir, IsoNode *node)
{
    IsoNode **pos;
    pos = &(dir->children);
    while (*pos != NULL && *pos != node) {
        pos = &((*pos)->next);
    }
    return pos;
}

/**
 * Removes a child from a directory.
 * The child is not freed, so you will become the owner of the node. Later
 * you can add the node to another dir (calling iso_dir_add_node), or free
 * it if you don't need it (with iso_node_unref).
 *
 * @return
 *     1 on success, < 0 error
 */
int iso_node_take(IsoNode *node)
{
    IsoNode **pos;
    IsoDir* dir;

    if (node == NULL) {
        return ISO_NULL_POINTER;
    }
    dir = node->parent;
    if (dir == NULL) {
        return ISO_NODE_NOT_ADDED_TO_DIR;
    }
    pos = iso_dir_find_node(dir, node);
    if (pos == NULL) {
        /* should never occur */
        return ISO_ASSERT_FAILURE;
    }

    /* notify iterators just before remove */
    iso_notify_dir_iters(node, 0);

    *pos = node->next;
    node->parent = NULL;
    node->next = NULL;
    dir->nchildren--;
    return ISO_SUCCESS;
}

/**
 * Removes a child from a directory and free (unref) it.
 * If you want to keep the child alive, you need to iso_node_ref() it
 * before this call, but in that case iso_node_take() is a better
 * alternative.
 *
 * @return
 *     1 on success, < 0 error
 */
int iso_node_remove(IsoNode *node)
{
    int ret;
    ret = iso_node_take(node);
    if (ret == ISO_SUCCESS) {
        iso_node_unref(node);
    }
    return ret;
}

/*
 * Get the parent of the given iso tree node. No extra ref is added to the
 * returned directory, you must take your ref. with iso_node_ref() if you
 * need it.
 *
 * If node is the root node, the same node will be returned as its parent.
 *
 * This returns NULL if the node doesn't pertain to any tree
 * (it was removed/take).
 */
IsoDir *iso_node_get_parent(IsoNode *node)
{
    return node->parent;
}

/* TODO #00005 optimize iso_dir_iter_take */
static
int iter_take(IsoDirIter *iter)
{
    struct dir_iter_data *data;
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }

    data = iter->data;

    if (!(data->flag & 0x01)) {
        return ISO_ERROR; /* next not called or end of dir */
    }

    if (data->pos == NULL) {
        return ISO_ASSERT_FAILURE;
    }

    /* clear next flag */
    data->flag &= ~0x01;

    return iso_node_take(data->pos);
}

static
int iter_remove(IsoDirIter *iter)
{
    int ret;
    IsoNode *pos;
    struct dir_iter_data *data;

    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    data = iter->data;
    pos = data->pos;

    ret = iter_take(iter);
    if (ret == ISO_SUCCESS) {
        /* remove node */
        iso_node_unref(pos);
    }
    return ret;
}

void iter_notify_child_taken(IsoDirIter *iter, IsoNode *node)
{
    IsoNode *pos, *pre;
    struct dir_iter_data *data;
    data = iter->data;

    if (data->pos == node) {
        pos = iter->dir->children;
        pre = NULL;
        while (pos != NULL && pos != data->pos) {
            pre = pos;
            pos = pos->next;
        }
        if (pos == NULL || pos != data->pos) {
            return;
        }

        /* dispose iterator reference */
        iso_node_unref(data->pos);

        if (pre == NULL) {
            /* node is a first position */
            iter->dir->children = pos->next;
            data->pos = NULL;
        } else {
            pre->next = pos->next;
            data->pos = pre;
            iso_node_ref(pre); /* take iter ref */
        }
    }
}

static
struct iso_dir_iter_iface iter_class = {
        iter_next,
        iter_has_next,
        iter_free,
        iter_take,
        iter_remove,
        iter_notify_child_taken
};

int iso_dir_get_children(const IsoDir *dir, IsoDirIter **iter)
{
    IsoDirIter *it;
    struct dir_iter_data *data;

    if (dir == NULL || iter == NULL) {
        return ISO_NULL_POINTER;
    }
    it = malloc(sizeof(IsoDirIter));
    if (it == NULL) {
        return ISO_OUT_OF_MEM;
    }
    data = malloc(sizeof(struct dir_iter_data));
    if (data == NULL) {
        free(it);
        return ISO_OUT_OF_MEM;
    }

    it->class = &iter_class;
    it->dir = (IsoDir*)dir;
    data->pos = NULL;
    data->flag = 0x00;
    it->data = data;

    if (iso_dir_iter_register(it) < 0) {
        free(it);
        return ISO_OUT_OF_MEM;
    }

    iso_node_ref((IsoNode*)dir); /* tak a ref to the dir */
    *iter = it;
    return ISO_SUCCESS;
}

int iso_dir_iter_next(IsoDirIter *iter, IsoNode **node)
{
    if (iter == NULL || node == NULL) {
        return ISO_NULL_POINTER;
    }
    return iter->class->next(iter, node);
}

int iso_dir_iter_has_next(IsoDirIter *iter)
{
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    return iter->class->has_next(iter);
}

void iso_dir_iter_free(IsoDirIter *iter)
{
    if (iter != NULL) {
        iso_dir_iter_unregister(iter);
        iter->class->free(iter);
        iso_node_unref((IsoNode*)iter->dir);
        free(iter);
    }
}

int iso_dir_iter_take(IsoDirIter *iter)
{
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    return iter->class->take(iter);
}

int iso_dir_iter_remove(IsoDirIter *iter)
{
    if (iter == NULL) {
        return ISO_NULL_POINTER;
    }
    return iter->class->remove(iter);
}

/**
 * Get the destination of a node.
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_symlink_get_dest(const IsoSymlink *link)
{
    return link->dest;
}

/**
 * Set the destination of a link.
 */
int iso_symlink_set_dest(IsoSymlink *link, const char *dest)
{
    char *d;
    if (!iso_node_is_valid_link_dest(dest)) {
        /* guard against null or empty dest */
        return ISO_WRONG_ARG_VALUE;
    }
    d = strdup(dest);
    if (d == NULL) {
        return ISO_OUT_OF_MEM;
    }
    free(link->dest);
    link->dest = d;
    return ISO_SUCCESS;
}

/**
 * Sets the order in which a node will be written on image. High weihted files
 * will be written first, so in a disc them will be written near the center.
 *
 * @param node
 *      The node which weight will be changed. If it's a dir, this function
 *      will change the weight of all its children. For nodes other that dirs
 *      or regular files, this function has no effect.
 * @param w
 *      The weight as a integer number, the greater this value is, the
 *      closer from the begining of image the file will be written.
 */
void iso_node_set_sort_weight(IsoNode *node, int w)
{
    if (node->type == LIBISO_DIR) {
        IsoNode *child = ((IsoDir*)node)->children;
        while (child) {
            iso_node_set_sort_weight(child, w);
            child = child->next;
        }
    } else if (node->type == LIBISO_FILE) {
        ((IsoFile*)node)->sort_weight = w;
    }
}

/**
 * Get the sort weight of a file.
 */
int iso_file_get_sort_weight(IsoFile *file)
{
    return file->sort_weight;
}

/**
 * Get the size of the file, in bytes
 */
off_t iso_file_get_size(IsoFile *file)
{
    return iso_stream_get_size(file->stream);
}

/**
 * Get the IsoStream that represents the contents of the given IsoFile.
 *
 * If you open() the stream, it should be close() before image generation.
 *
 * @return
 *      The IsoStream. No extra ref is added, so the IsoStream belong to the
 *      IsoFile, and it may be freed together with it. Add your own ref with
 *      iso_stream_ref() if you need it.
 *
 * @since 0.6.4
 */
IsoStream *iso_file_get_stream(IsoFile *file)
{
    return file->stream;
}

/**
 * Get the device id (major/minor numbers) of the given block or
 * character device file. The result is undefined for other kind
 * of special files, of first be sure iso_node_get_mode() returns either
 * S_IFBLK or S_IFCHR.
 *
 * @since 0.6.6
 */
dev_t iso_special_get_dev(IsoSpecial *special)
{
    return special->dev;
}

/**
 * Get the block lba of a file node, if it was imported from an old image.
 *
 * @param file
 *      The file
 * @param lba
 *      Will be filled with the kba
 * @param flag
 *      Reserved for future usage, submit 0
 * @return
 *      1 if lba is valid (file comes from old image), 0 if file was newly
 *      added, i.e. it does not come from an old image, < 0 error
 *
 * @since 0.6.4
 */
int iso_file_get_old_image_lba(IsoFile *file, uint32_t *lba, int flag)
{
    int ret;
    int section_count;
    struct iso_file_section *sections;
    if (file == NULL || lba == NULL) {
        return ISO_NULL_POINTER;
    }
    ret = iso_file_get_old_image_sections(file, &section_count, &sections, flag);
    if (ret <= 0) {
        return ret;
    }
    if (section_count != 1) {
        free(sections);
        return ISO_WRONG_ARG_VALUE;
    }
    *lba = sections[0].block;
    free(sections);
    return 0;
}



/*
 * Like iso_file_get_old_image_lba(), but take an IsoNode.
 *
 * @return
 *      1 if lba is valid (file comes from old image), 0 if file was newly
 *      added, i.e. it does not come from an old image, 2 node type has no
 *      LBA (no regular file), < 0 error
 *
 * @since 0.6.4
 */
int iso_node_get_old_image_lba(IsoNode *node, uint32_t *lba, int flag)
{
    if (node == NULL) {
        return ISO_NULL_POINTER;
    }
    if (ISO_NODE_IS_FILE(node)) {
        return iso_file_get_old_image_lba((IsoFile*)node, lba, flag);
    } else {
        return 2;
    }
}

/**
 * Check if a given name is valid for an iso node.
 *
 * @return
 *     1 if yes, 0 if not
 */
int iso_node_is_valid_name(const char *name)
{
    /* a name can't be NULL */
    if (name == NULL) {
        return 0;
    }

    /* guard against the empty string or big names... */
    if (name[0] == '\0' || strlen(name) > 255) {
        return 0;
    }

    /* ...against "." and ".." names... */
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        return 0;
    }

    /* ...and against names with '/' */
    if (strchr(name, '/') != NULL) {
        return 0;
    }
    return 1;
}

/**
 * Check if a given path is valid for the destination of a link.
 *
 * @return
 *     1 if yes, 0 if not
 */
int iso_node_is_valid_link_dest(const char *dest)
{
    int ret;
    char *ptr, *brk_info, *component;

    /* a dest can't be NULL */
    if (dest == NULL) {
        return 0;
    }

    /* guard against the empty string or big dest... */
    if (dest[0] == '\0' || strlen(dest) > PATH_MAX) {
        return 0;
    }

    /* check that all components are valid */
    if (!strcmp(dest, "/")) {
        /* "/" is a valid component */
        return 1;
    }

    ptr = strdup(dest);
    if (ptr == NULL) {
        return 0;
    }

    ret = 1;
    component = strtok_r(ptr, "/", &brk_info);
    while (component) {
        if (strcmp(component, ".") && strcmp(component, "..")) {
            ret = iso_node_is_valid_name(component);
            if (ret == 0) {
                break;
            }
        }
        component = strtok_r(NULL, "/", &brk_info);
    }
    free(ptr);

    return ret;
}

void iso_dir_find(IsoDir *dir, const char *name, IsoNode ***pos)
{
    *pos = &(dir->children);
    while (**pos != NULL && strcmp((**pos)->name, name) < 0) {
        *pos = &((**pos)->next);
    }
}

int iso_dir_exists(IsoDir *dir, const char *name, IsoNode ***pos)
{
    IsoNode **node;

    iso_dir_find(dir, name, &node);
    if (pos) {
        *pos = node;
    }
    return (*node != NULL && !strcmp((*node)->name, name)) ? 1 : 0;
}

int iso_dir_insert(IsoDir *dir, IsoNode *node, IsoNode **pos,
                   enum iso_replace_mode replace)
{
    if (*pos != NULL && !strcmp((*pos)->name, node->name)) {
        /* a node with same name already exists */
        switch(replace) {
        case ISO_REPLACE_NEVER:
            return ISO_NODE_NAME_NOT_UNIQUE;
        case ISO_REPLACE_IF_NEWER:
            if ((*pos)->mtime >= node->mtime) {
                /* old file is newer */
                return ISO_NODE_NAME_NOT_UNIQUE;
            }
            break;
        case ISO_REPLACE_IF_SAME_TYPE_AND_NEWER:
            if ((*pos)->mtime >= node->mtime) {
                /* old file is newer */
                return ISO_NODE_NAME_NOT_UNIQUE;
            }
            /* fall down */
        case ISO_REPLACE_IF_SAME_TYPE:
            if ((node->mode & S_IFMT) != ((*pos)->mode & S_IFMT)) {
                /* different file types */
                return ISO_NODE_NAME_NOT_UNIQUE;
            }
            break;
        case ISO_REPLACE_ALWAYS:
            break;
        default:
            /* CAN'T HAPPEN */
            return ISO_ASSERT_FAILURE;
        }

        /* if we are reach here we have to replace */
        node->next = (*pos)->next;
        (*pos)->parent = NULL;
        (*pos)->next = NULL;
        iso_node_unref(*pos);
        *pos = node;
        node->parent = dir;
        return dir->nchildren;
    }

    node->next = *pos;
    *pos = node;
    node->parent = dir;

    return ++dir->nchildren;
}

/* iterators are stored in a linked list */
struct iter_reg_node {
    IsoDirIter *iter;
    struct iter_reg_node *next;
};

/* list header */
static
struct iter_reg_node *iter_reg = NULL;

/**
 * Add a new iterator to the registry. The iterator register keeps track of
 * all iterators being used, and are notified when directory structure
 * changes.
 */
int iso_dir_iter_register(IsoDirIter *iter)
{
    struct iter_reg_node *new;
    new = malloc(sizeof(struct iter_reg_node));
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    new->iter = iter;
    new->next = iter_reg;
    iter_reg = new;
    return ISO_SUCCESS;
}

/**
 * Unregister a directory iterator.
 */
void iso_dir_iter_unregister(IsoDirIter *iter)
{
    struct iter_reg_node **pos;
    pos = &iter_reg;
    while (*pos != NULL && (*pos)->iter != iter) {
        pos = &(*pos)->next;
    }
    if (*pos) {
        struct iter_reg_node *tmp = (*pos)->next;
        free(*pos);
        *pos = tmp;
    }
}

void iso_notify_dir_iters(IsoNode *node, int flag)
{
    struct iter_reg_node *pos = iter_reg;
    while (pos != NULL) {
        IsoDirIter *iter = pos->iter;
        if (iter->dir == node->parent) {
            iter->class->notify_child_taken(iter, node);
        }
        pos = pos->next;
    }
}

int iso_node_new_root(IsoDir **root)
{
    IsoDir *dir;

    dir = calloc(1, sizeof(IsoDir));
    if (dir == NULL) {
        return ISO_OUT_OF_MEM;
    }
    dir->node.refcount = 1;
    dir->node.type = LIBISO_DIR;
    dir->node.atime = dir->node.ctime = dir->node.mtime = time(NULL);
    dir->node.mode = S_IFDIR | 0555;

    /* set parent to itself, to prevent root to be added to another dir */
    dir->node.parent = dir;
    *root = dir;
    return ISO_SUCCESS;
}

int iso_node_new_dir(char *name, IsoDir **dir)
{
    IsoDir *new;

    if (dir == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }

    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    new = calloc(1, sizeof(IsoDir));
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    new->node.refcount = 1;
    new->node.type = LIBISO_DIR;
    new->node.name = name;
    new->node.mode = S_IFDIR;
    *dir = new;
    return ISO_SUCCESS;
}

int iso_node_new_file(char *name, IsoStream *stream, IsoFile **file)
{
    IsoFile *new;

    if (file == NULL || name == NULL || stream == NULL) {
        return ISO_NULL_POINTER;
    }

    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    new = calloc(1, sizeof(IsoFile));
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    new->node.refcount = 1;
    new->node.type = LIBISO_FILE;
    new->node.name = name;
    new->node.mode = S_IFREG;
    new->stream = stream;

    *file = new;
    return ISO_SUCCESS;
}

int iso_node_new_symlink(char *name, char *dest, IsoSymlink **link)
{
    IsoSymlink *new;

    if (link == NULL || name == NULL || dest == NULL) {
        return ISO_NULL_POINTER;
    }

    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* check if destination is valid */
    if (!iso_node_is_valid_link_dest(dest)) {
        /* guard against null or empty dest */
        return ISO_WRONG_ARG_VALUE;
    }

    new = calloc(1, sizeof(IsoSymlink));
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    new->node.refcount = 1;
    new->node.type = LIBISO_SYMLINK;
    new->node.name = name;
    new->dest = dest;
    new->node.mode = S_IFLNK;
    *link = new;
    return ISO_SUCCESS;
}

int iso_node_new_special(char *name, mode_t mode, dev_t dev,
                         IsoSpecial **special)
{
    IsoSpecial *new;

    if (special == NULL || name == NULL) {
        return ISO_NULL_POINTER;
    }
    if (S_ISLNK(mode) || S_ISREG(mode) || S_ISDIR(mode)) {
        return ISO_WRONG_ARG_VALUE;
    }

    /* check if the name is valid */
    if (!iso_node_is_valid_name(name)) {
        return ISO_WRONG_ARG_VALUE;
    }

    new = calloc(1, sizeof(IsoSpecial));
    if (new == NULL) {
        return ISO_OUT_OF_MEM;
    }
    new->node.refcount = 1;
    new->node.type = LIBISO_SPECIAL;
    new->node.name = name;

    new->node.mode = mode;
    new->dev = dev;

    *special = new;
    return ISO_SUCCESS;
}


/* ts A90116 */
/* >>> describe

   @param flag      bit15 = free memory
*/
int iso_node_get_attrs(IsoNode *node, size_t *num_attrs,
              char ***names, size_t **value_lengths, char ***values, int flag)
{

#ifdef Libisofs_with_aaiP

    void *xipt;
    struct aaip_state *aaip= NULL;
    unsigned char *rpt, *aa_string;
    size_t len, todo, consumed;
    int is_done = 0, first_round= 1, ret;

    if (flag & (1 << 15))
        aaip_get_decoded_attrs(&aaip, num_attrs, names,
                               value_lengths, values, 1 << 15);
    *num_attrs = 0;
    *names = NULL;
    *value_lengths = NULL;
    *values = NULL;
    if (flag & (1 << 15))
        return 1;

    ret = iso_node_get_xinfo(node, aaip_xinfo_func, &xipt);
    if (ret != 1)
        return 1;

    aa_string = rpt = (unsigned char *) xipt;
    len = aaip_count_bytes(rpt, 0);
    while (!is_done) {
        todo = len - (rpt - aa_string);
        if (todo > 2048)
            todo = 2048;
        if (todo == 0) {

           /* >>> Out of data while still prompted to submit */;

           /* >>> invent better error code */
           return ISO_ERROR;
        }
        /* Allow 1 million bytes of memory consumption, 100,000 attributes */
        ret = aaip_decode_attrs(&aaip, "AA", (size_t) 1000000, (size_t) 100000,
                                rpt, todo, &consumed, first_round);
        rpt+= consumed;
        first_round= 0;
        if (ret == 1)
            continue;
        if (ret == 2)
             break;

         /* >>> "aaip_decode_attrs() reports error */;

         /* >>> invent better error code */
         return ISO_ERROR;
    }

    if(rpt - aa_string != len) {

         /* >>>  "aaip_decode_attrs() returns 2 but still bytes are left" */

         /* >>> invent better error code */
         return ISO_ERROR;
    }

    ret = aaip_get_decoded_attrs(&aaip, num_attrs, names,
                                 value_lengths, values, 0);
    if(ret != 1) {

         /* >>> aaip_get_decoded_attrs() failed */;

         return ISO_OUT_OF_MEM;
    }

#else /* Libisofs_with_aaiP */

    *num_attrs = 0;
    *names = NULL;
    *value_lengths = NULL;
    *values = NULL;

#endif /* ! Libisofs_with_aaiP */

    return 1;
}


/* ts A90116 */
int iso_node_get_acl_text(IsoNode *node, char **text, int flag)
{
    size_t num_attrs = 0, *value_lengths = NULL, i, consumed, text_fill = 0;
    size_t v_len;
    char **names = NULL, **values = NULL;
    unsigned char *v_data;
    int ret, from_posix= 0;
    mode_t st_mode;

    if (flag & (1 << 15)) {
        if (*text != NULL)
            free(*text);
        *text = NULL;
        return 1;
    }

#ifdef Libisofs_with_aaiP

    *text = NULL;

    ret = iso_node_get_attrs(node, &num_attrs, &names,
                             &value_lengths, &values, 0);
    if (ret < 0)
        return ret;

    for(i = 0; i < num_attrs; i++) {
        if (names[i][0]) /* searching the empty name */
            continue;

        v_data = (unsigned char *) values[i];
        v_len = value_lengths[i];

        if (flag & 1) {
            /* Skip "access" ACL and address "default" ACL instead */
            ret = aaip_decode_acl(v_data, v_len,
                                  &consumed, NULL, (size_t) 0, &text_fill, 1);
            if (ret <= 0)
                goto bad_decode;
            if (ret != 2) {
                ret = 0;
                goto ex;
            }
            v_data += consumed;
            v_len -= consumed;
        }
        
        ret = aaip_decode_acl(v_data, v_len,
                              &consumed, NULL, (size_t) 0, &text_fill, 1);
        if (ret <= 0)
            goto bad_decode;
        if (text_fill == 0) {
            ret = 0;
            goto ex;
        }
        *text = calloc(text_fill + 32, 1); /* 32 for aaip_update_acl_st_mode */
        if (*text == NULL) {
            ret = ISO_OUT_OF_MEM;
            goto ex;
        }
        ret = aaip_decode_acl(v_data, v_len,
                              &consumed, *text, text_fill, &text_fill, 0);
        if (ret <= 0)
            goto bad_decode;
        break;
    }
    
    if (*text == NULL && !(flag & 16)) {
        from_posix = 1;
        *text = calloc(32, 1); /* 32 for aaip_update_acl_st_mode */
    }
    if (*text != NULL) {
        st_mode = iso_node_get_permissions(node);
        aaip_add_acl_st_mode(*text, st_mode, 0);
        text_fill = strlen(*text);
    }

    if (text == NULL)
        ret = 0;
    else
        ret = 1 + from_posix;
ex:;
    iso_node_get_attrs(node, &num_attrs, &names,
                       &value_lengths, &values, 1 << 15); /* free memory */
    return ret;

bad_decode:;

    /* >>> something is wrong with the attribute value */;

    /* >>> invent better error code */
    ret = ISO_ERROR;
    goto ex;

#else /* Libisofs_with_aaiP */

    *text = NULL;
    return 0;

#endif /* ! Libisofs_with_aaiP */

}


int iso_local_set_acl_text(char *disk_path, char *text, int flag)
{

#ifdef Libisofs_with_aaiP

 return aaip_set_acl_text(disk_path, text, flag);

#else /* Libisofs_with_aaiP */

 return 0;
 
#endif /* ! Libisofs_with_aaiP */

}

