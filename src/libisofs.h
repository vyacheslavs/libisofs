/*
 * Copyright (c) 2007 Vreixo Formoso
 * 
 * This file is part of the libisofs project; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. See COPYING file for details.
 */
#ifndef LIBISO_LIBISOFS_H_
#define LIBISO_LIBISOFS_H_

#include <sys/stat.h>
#include <stdint.h>

struct burn_source;

typedef struct Iso_Image IsoImage;

typedef struct Iso_Node IsoNode;
typedef struct Iso_Dir IsoDir;
typedef struct Iso_Symlink IsoSymlink;
typedef struct Iso_File IsoFile;
typedef struct Iso_Special IsoSpecial;

typedef struct Iso_Dir_Iter IsoDirIter;

/**
 * The type of an IsoNode.
 * 
 * When an user gets an IsoNode from an image, (s)he can use
 * iso_node_get_type() to get the current type of the node, and then
 * cast to the appropriate subtype. For example:
 * 
 * ...
 * IsoNode *node;
 * res = iso_dir_iter_next(iter, &node);
 * if (res == 1 && iso_node_get_type(node) == LIBISO_DIR) {
 *      IsoDir *dir = (IsoDir *)node;
 *      ...
 * }
 */
enum IsoNodeType {
    LIBISO_DIR,
    LIBISO_FILE,
    LIBISO_SYMLINK,
    LIBISO_SPECIAL,
    LIBISO_BOOT
};

/**
 * Flag used to hide a file in the RR/ISO or Joliet tree.
 * 
 * \see iso_node_set_hidden
 */
enum IsoHideNodeFlag {
    LIBISO_HIDE_ON_RR = 1 << 0,
    LIBISO_HIDE_ON_JOLIET = 1 << 1
};

/**
 * Holds the options for the image generation.
 */
typedef struct
{
    int level; /**< ISO level to write at. */

    /** Which extensions to support. */
    unsigned int rockridge :1;

    /* relaxed constraints */
    unsigned int omit_version_numbers :1;
    unsigned int allow_deep_paths :1;
    //int relaxed_constraints; /**< see ecma119_relaxed_constraints_flag */

    //unsigned int copy_eltorito:1;
    /**<
     * In multisession discs, select whether to copy el-torito catalog
     * and boot image. Copy is needed for isolinux images, that need to
     * be patched. However, it can lead to problems when the image is 
     * not present in the iso filesystem, because we can't figure out
     * its size. In those cases, we only copy 1 block of data.
     */

    /**< If files should be sorted based on their weight. */
    unsigned int sort_files :1;

    /**
     * The following options set the default values for files and directory
     * permissions, gid and uid. All these take one of three values: 0, 1 or 2.
     * If 0, the corresponding attribute will be kept as setted in the IsoNode.
     * Unless you have changed it, it corresponds to the value on disc, so it
     * is suitable for backup purposes. If set to 1, the corresponding attrib.
     * will be changed by a default suitable value. Finally, if you set it to 
     * 2, the attrib. will be changed with the value specified in the options
     * below. Note that for mode attributes, only the permissions are set, the
     * file type remains unchanged.
     */
    unsigned int replace_dir_mode :2;
    unsigned int replace_file_mode :2;
    unsigned int replace_uid :2;
    unsigned int replace_gid :2;

    mode_t dir_mode; /** Mode to use on dirs when replace_dir_mode == 2. */
    mode_t file_mode; /** Mode to use on files when replace_file_mode == 2. */
    uid_t uid; /** uid to use when replace_uid == 2. */
    gid_t gid; /** gid to use when replace_gid == 2. */

    char *output_charset; /**< NULL to use default charset */
    //    uint32_t ms_block;
    /**< 
     * Start block for multisession. When this is greater than 0, 
     * it's suppossed to be the lba of the next writable address 
     * on disc; all block lba on image will take this into account,
     * and files from a previous session will not be written on 
     * image. This behavior is only suitable for images to be
     * appended to a multisession disc.
     * When this is 0, no multisession image will be created. If 
     * some files are taken from a previous image, its contents
     * will be written again to the new image. Use this with new
     * images or if you plan to modify an existin image. 
     */
    //    struct data_source* src;
    //                /**<
    //                 * When modifying a image, this is the source of the original
    //                 * image, used to read file contents.
    //                 * Otherwise it can be NULL.
    //                 */
    //    uint8_t *overwrite;
    //                /**<
    //                 * When not NULL, it should point to a buffer of at least 
    //                 * 64KiB, where libisofs will write the contents that should 
    //                 * be written at the beginning of a overwriteable media, to
    //                 * grow the image.
    //                 * You shoudl initialize the buffer either with 0s, or with
    //                 * the contents of the first blocks of the image you're
    //                 * growing. In most cases, 0 is good enought.
    //                 */
} Ecma119WriteOpts;

typedef struct Iso_Data_Source IsoDataSource;

/**
 * Data source used by libisofs for reading an existing image.
 * 
 * It offers homogeneous read access to arbitrary blocks to different sources
 * for images, such as .iso files, CD/DVD drives, etc... 
 * 
 * To create a multisession image, libisofs needs a IsoDataSource, that the
 * user must provide. The function iso_data_source_new_from_file() constructs 
 * an IsoDataSource that uses POSIX I/O functions to access data. You can use 
 * it with regular .iso images, and also with block devices that represent a 
 * drive.
 */
struct Iso_Data_Source {
    
    /** 
     * Reference count for the data source. Should be 1 when a new source
     * is created. Don't access it directly, but with iso_data_source_ref()
     * and iso_data_source_unref() functions.
     */
    unsigned int refcount;

    /**
     * Opens the given source. You must open() the source before any attempt
     * to read data from it. The open is the right place for grabbing the 
     * underlying resources.
     * 
     * @return
     *      1 if success, < 0 on error
     */
    int (*open)(IsoDataSource *src);

    /**
     * Close a given source, freeing all system resources previously grabbed in
     * open().
     * 
     * @return
     *      1 if success, < 0 on error
     */
    int (*close)(IsoDataSource *src);

    /** 
     * Read an arbitrary block (2048 bytes) of data from the source.
     * 
     * @param lba 
     *     Block to be read.
     * @param buffer 
     *     Buffer where the data will be written. It should have at least 
     *     2048 bytes.
     * @return
     *      1 if success, < 0 on error
     */
    int (*read_block)(IsoDataSource *src, uint32_t lba, uint8_t *buffer);

    /** 
     * Clean up the source specific data. Never call this directly, it is
     * automatically called by iso_data_source_unref() when refcount reach 
     * 0.
     */
    void (*free_data)(IsoDataSource *);

    /** Source specific data */
    void *data;
};

/**
 * Options for image reading.
 * There are four kind of options:
 * - Related to multisession support.
 *   In most cases, an image begins at LBA 0 of the data source. However,
 *   in multisession discs, the later image begins in the last session on
 *   disc. The block option can be used to specify the start of that last
 *   session.
 * - Related to the tree that will be read.
 *   As default, when Rock Ridge extensions are present in the image, that
 *   will be used to get the tree. If RR extensions are not present, libisofs
 *   will use the Joliet extensions if available. Finally, the plain ISO-9660
 *   tree is used if neither RR nor Joliet extensions are available. With
 *   norock, nojoliet, and preferjoliet options, you can change this
 *   default behavior.
 * - Related to default POSIX attributes.
 *   When Rock Ridege extensions are not used, libisofs can't figure out what
 *   are the the permissions, uid or gid for the files. You should supply
 *   default values for that.
 */
struct iso_read_opts
{
    /** 
     * Block where the image begins, usually 0, can be different on a 
     * multisession disc.
     */
    uint32_t block; 

    unsigned int norock : 1; /*< Do not read Rock Ridge extensions */
    unsigned int nojoliet : 1; /*< Do not read Joliet extensions */

    /** 
     * When both Joliet and RR extensions are present, the RR tree is used. 
     * If you prefer using Joliet, set this to 1. 
     */
    unsigned int preferjoliet : 1; 
    
    uid_t uid; /**< Default uid when no RR */
    gid_t gid; /**< Default uid when no RR */
    mode_t mode; /**< Default mode when no RR (only permissions) */
    //TODO differ file and dir mode
    //option to convert names to lower case?
    
    char *input_charset;
};

/**
 * Return information for image.
 * Both size, hasRR and hasJoliet will be filled by libisofs with suitable 
 * values.
 */
struct iso_read_image_features
{
    /** It will be set to 1 if RR extensions are present, to 0 if not. */
    unsigned int hasRR:1; 
    
    /** It will be set to 1 if Joliet extensions are present, to 0 if not. */
    unsigned int hasJoliet:1; 
    
    /** 
     * Will be filled with the size (in 2048 byte block) of the image, as 
     * reported in the PVM. 
     */
    uint32_t size; 
};

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
int iso_image_new(const char *name, IsoImage **image);

/**
 * TODO we should have two "create" methods, one for grow images, another
 * for images from scratch. Or we can just have a flag in Ecma119WriteOpts.
 */
int iso_image_create(IsoImage *image, Ecma119WriteOpts *opts,
                     struct burn_source **burn_src);

/**
 * Import a previous session or image, for growing or modify.
 * 
 * @param image
 *     The image context to which old image will be imported. Note that all
 *     files added to image, and image attributes, will be replaced with the
 *     contents of the old image. TODO support for merging old image files
 * @param src
 *     Data Source from which old image will be read.
 * @param opts
 *     Options for image import
 * @param features
 *     Will be filled with the features of the old image. You can pass NULL
 *     if you're not interested on them.
 * @return
 *     1 on success, < 0 on error
 */
int iso_image_import(IsoImage *image, IsoDataSource *src,
                     struct iso_read_opts *opts, 
                     struct iso_read_image_features *features);

/**
 * Increments the reference counting of the given image.
 */
void iso_image_ref(IsoImage *image);

/**
 * Decrements the reference couting of the given image.
 * If it reaches 0, the image is free, together with its tree nodes (whether 
 * their refcount reach 0 too, of course).
 */
void iso_image_unref(IsoImage *image);

/**
 * Get the root directory of the image.
 * No extra ref is added to it, so you musn't unref it. Use iso_node_ref()
 * if you want to get your own reference.
 */
IsoDir *iso_image_get_root(const IsoImage *image);

/**
 * Fill in the volset identifier for a image.
 */
void iso_image_set_volset_id(IsoImage *image, const char *volset_id);

/** 
 * Get the volset identifier. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_volset_id(const IsoImage *image);

/**
 * Fill in the volume identifier for a image.
 */
void iso_image_set_volume_id(IsoImage *image, const char *volume_id);

/** 
 * Get the volume identifier. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_volume_id(const IsoImage *image);

/**
 * Fill in the publisher for a image.
 */
void iso_image_set_publisher_id(IsoImage *image, const char *publisher_id);

/** 
 * Get the publisher of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_publisher_id(const IsoImage *image);

/**
 * Fill in the data preparer for a image.
 */
void iso_image_set_data_preparer_id(IsoImage *image,
                                    const char *data_preparer_id);

/** 
 * Get the data preparer of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_data_preparer_id(const IsoImage *image);

/**
 * Fill in the system id for a image. Up to 32 characters.
 */
void iso_image_set_system_id(IsoImage *image, const char *system_id);

/** 
 * Get the system id of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_system_id(const IsoImage *image);

/**
 * Fill in the application id for a image. Up to 128 chars.
 */
void iso_image_set_application_id(IsoImage *image, const char *application_id);

/** 
 * Get the application id of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_application_id(const IsoImage *image);

/**
 * Fill copyright information for the image. Usually this refers
 * to a file on disc. Up to 37 characters.
 */
void iso_image_set_copyright_file_id(IsoImage *image,
                                     const char *copyright_file_id);

/** 
 * Get the copyright information of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_copyright_file_id(const IsoImage *image);

/**
 * Fill abstract information for the image. Usually this refers
 * to a file on disc. Up to 37 characters.
 */
void iso_image_set_abstract_file_id(IsoImage *image,
                                    const char *abstract_file_id);

/** 
 * Get the abstract information of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_abstract_file_id(const IsoImage *image);

/**
 * Fill biblio information for the image. Usually this refers
 * to a file on disc. Up to 37 characters.
 */
void iso_image_set_biblio_file_id(IsoImage *image, const char *biblio_file_id);

/** 
 * Get the biblio information of a image. 
 * The returned string is owned by the image and should not be freed nor
 * changed.
 */
const char *iso_image_get_biblio_file_id(const IsoImage *image);

/**
 * Increments the reference counting of the given node.
 */
void iso_node_ref(IsoNode *node);

/**
 * Decrements the reference couting of the given node.
 * If it reach 0, the node is free, and, if the node is a directory,
 * its children will be unref() too.
 */
void iso_node_unref(IsoNode *node);

/**
 * Get the type of an IsoNode.
 */
enum IsoNodeType iso_node_get_type(IsoNode *node);

/**
 * Set the name of a node. Note that if the node is already added to a dir
 * this can fail if dir already contains a node with the new name.
 * 
 * @param node
 *      The node whose name you want to change. Note that you can't change
 *      the name of the root.
 * @param name 
 *      The name for the node. If you supply an empty string or a 
 *      name greater than 255 characters this returns with failure, and
 *      node name is not modified.
 * @return 
 *      1 on success, < 0 on error
 */
int iso_node_set_name(IsoNode *node, const char *name);

/**
 * Get the name of a node.
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_node_get_name(const IsoNode *node);

/**
 * Set the permissions for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 * 
 * @param mode 
 *     bitmask with the permissions of the node, as specified in 'man 2 stat'.
 *     The file type bitfields will be ignored, only file permissions will be
 *     modified.
 */
void iso_node_set_permissions(IsoNode *node, mode_t mode);

/** 
 * Get the permissions for the node 
 */
mode_t iso_node_get_permissions(const IsoNode *node);

/** 
 * Get the mode of the node, both permissions and file type, as specified in
 * 'man 2 stat'.
 */
mode_t iso_node_get_mode(const IsoNode *node);

/**
 * Set the user id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_uid(IsoNode *node, uid_t uid);

/**
 * Get the user id of the node.
 */
uid_t iso_node_get_uid(const IsoNode *node);

/**
 * Set the group id for the node. This attribute is only useful when 
 * Rock Ridge extensions are enabled.
 */
void iso_node_set_gid(IsoNode *node, gid_t gid);

/**
 * Get the group id of the node.
 */
gid_t iso_node_get_gid(const IsoNode *node);

/** 
 * Set the time of last modification of the file
 */
void iso_node_set_mtime(IsoNode *node, time_t time);

/** 
 * Get the time of last modification of the file
 */
time_t iso_node_get_mtime(const IsoNode *node);

/** 
 * Set the time of last access to the file
 */
void iso_node_set_atime(IsoNode *node, time_t time);

/** 
 * Get the time of last access to the file 
 */
time_t iso_node_get_atime(const IsoNode *node);

/** 
 * Set the time of last status change of the file 
 */
void iso_node_set_ctime(IsoNode *node, time_t time);

/** 
 * Get the time of last status change of the file 
 */
time_t iso_node_get_ctime(const IsoNode *node);

/**
 * Set if the node will be hidden in RR/ISO tree, Joliet tree or both.
 * 
 * If the file is setted as hidden in one tree, it won't be included there, so
 * it won't be visible in a OS accessing CD using that tree. For example,
 * GNU/Linux systems access to Rock Ridge / ISO9960 tree in order to see
 * what is recorded on CD, while MS Windows make use of the Joliet tree. If a
 * file is hidden only in Joliet, it won't be visible in Windows systems,
 * while still visible in Linux.
 * 
 * If a file is hidden in both trees, it won't be written to image.
 * 
 * @param node 
 *      The node that is to be hidden.
 * @param hide_attrs 
 *      IsoHideNodeFlag's to set the trees in which file will be hidden.
 */
void iso_node_set_hidden(IsoNode *node, int hide_attrs);

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
 *      - 0 not replace (will fail with ISO_NODE_NAME_NOT_UNIQUE)
 *      - 1 replace 
 *      TODO #00006 define more values
 *          to replace only if both are the same kind of file
 *          if both are dirs, add contents (and what to do with conflicts?)
 * @return
 *     number of nodes in dir if succes, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir or child are NULL
 *         ISO_NODE_ALREADY_ADDED, if child is already added to other dir 
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_WRONG_ARG_VALUE, if child == dir, or replace != (0,1)
 */
int iso_dir_add_node(IsoDir *dir, IsoNode *child, int replace);

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
int iso_dir_get_node(IsoDir *dir, const char *name, IsoNode **node);

/**
 * Get the number of children of a directory.
 * 
 * @return
 *     >= 0 number of items, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir is NULL
 */
int iso_dir_get_nchildren(IsoDir *dir);

/**
 * Removes a child from a directory.
 * The child is not freed, so you will become the owner of the node. Later
 * you can add the node to another dir (calling iso_dir_add_node), or free
 * it if you don't need it (with iso_node_unref).
 * 
 * @return 
 *     1 on success, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if node is NULL
 *         ISO_NODE_NOT_ADDED_TO_DIR, if node doesn't belong to a dir
 */
int iso_node_take(IsoNode *node);

/**
 * Removes a child from a directory and free (unref) it.
 * If you want to keep the child alive, you need to iso_node_ref() it
 * before this call, but in that case iso_node_take() is a better
 * alternative.
 * 
 * @return 
 *     1 on success, < 0 error
 */
int iso_node_remove(IsoNode *node);

/**
 * Get an iterator for the children of the given dir.
 * 
 * You can iterate over the children with iso_dir_iter_next. When finished,
 * you should free the iterator with iso_dir_iter_free.
 * You musn't delete a child of the same dir, using iso_node_take() or
 * iso_node_remove(), while you're using the iterator. You can use 
 * iso_node_take_iter() or iso_node_remove_iter() instead.
 * 
 * You can use the iterator in the way like this
 * 
 * IsoDirIter *iter;
 * IsoNode *node;
 * if ( iso_dir_get_children(dir, &iter) != 1 ) {
 *     // handle error
 * }
 * while ( iso_dir_iter_next(iter, &node) == 1 ) {
 *     // do something with the child
 * }
 * iso_dir_iter_free(iter);
 * 
 * An iterator is intended to be used in a single iteration over the
 * children of a dir. Thus, it should be treated as a temporary object,
 * and free as soon as possible. 
 *  
 * @return
 *     1 success, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if dir or iter are NULL
 *         ISO_OUT_OF_MEM
 */
int iso_dir_get_children(const IsoDir *dir, IsoDirIter **iter);

/**
 * Get the next child.
 * Take care that the node is owned by its parent, and will be unref() when
 * the parent is freed. If you want your own ref to it, call iso_node_ref()
 * on it.
 * 
 * @return
 *     1 success, 0 if dir has no more elements, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if node or iter are NULL
 *         ISO_ERROR, on wrong iter usage, usual caused by modiying the
 *         dir during iteration
 */
int iso_dir_iter_next(IsoDirIter *iter, IsoNode **node);

/**
 * Check if there're more children.
 * 
 * @return
 *     1 dir has more elements, 0 no, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if iter is NULL
 */
int iso_dir_iter_has_next(IsoDirIter *iter);

/** 
 * Free a dir iterator.
 */
void iso_dir_iter_free(IsoDirIter *iter);

/**
 * Removes a child from a directory during an iteration, without freeing it.
 * It's like iso_node_take(), but to be used during a directory iteration.
 * The node removed will be the last returned by the iteration.
 * 
 * The behavior on two call to this function without calling iso_dir_iter_next
 * between then is undefined, and should never occur. (TODO protect against this?)
 * 
 * @return
 *     1 on succes, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if iter is NULL
 *         ISO_ERROR, on wrong iter usage, for example by call this before
 *         iso_dir_iter_next.
 */
int iso_dir_iter_take(IsoDirIter *iter);

/**
 * Removes a child from a directory during an iteration and unref() it.
 * It's like iso_node_remove(), but to be used during a directory iteration.
 * The node removed will be the last returned by the iteration.
 * 
 * The behavior on two call to this function without calling iso_tree_iter_next
 * between then is undefined, and should never occur. (TODO protect against this?)
 * 
 * @return
 *     1 on succes, < 0 error
 *     Possible errors:
 *         ISO_NULL_POINTER, if iter is NULL
 *         ISO_ERROR, on wrong iter usage, for example by call this before
 *         iso_dir_iter_next.
 */
int iso_dir_iter_remove(IsoDirIter *iter);

/**
 * Get the destination of a node.
 * The returned string belongs to the node and should not be modified nor
 * freed. Use strdup if you really need your own copy.
 */
const char *iso_symlink_get_dest(const IsoSymlink *link);

/**
 * Set the destination of a link.
 * 
 * @param dest
 *     New destination for the link. It must be a non-empty string, otherwise
 *     this function doesn't modify previous destination.
 */
void iso_symlink_set_dest(IsoSymlink *link, const char *dest);

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
void iso_node_set_sort_weight(IsoNode *node, int w);

/**
 * Get the sort weight of a file.
 */
int iso_file_get_sort_weight(IsoFile *file);

/**
 * Add a new directory to the iso tree. Permissions, owner and hidden atts
 * are taken from parent, you can modify them later.
 * 
 * @param parent 
 *      the dir where the new directory will be created
 * @param name
 *      name for the new dir. If a node with same name already exists on
 *      parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param dir
 *      place where to store a pointer to the newly created dir. No extra
 *      ref is addded, so you will need to call iso_node_ref() if you really
 *      need it. You can pass NULL in this parameter if you don't need the
 *      pointer.
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent or name are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_MEM_ERROR
 */
int iso_tree_add_new_dir(IsoDir *parent, const char *name, IsoDir **dir);

/*
 TODO #00007 expose Strem and thi function:
 int iso_tree_add_new_file(IsoDir *parent, const char *name, stream, file)
 */

/**
 * Add a new symlink to the directory tree. Permissions are set to 0777, 
 * owner and hidden atts are taken from parent. You can modify any of them 
 * later.
 *  
 * @param parent 
 *      the dir where the new symlink will be created
 * @param name
 *      name for the new symlink. If a node with same name already exists on
 *      parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param dest
 *      destination of the link
 * @param link
 *      place where to store a pointer to the newly created link. No extra
 *      ref is addded, so you will need to call iso_node_ref() if you really
 *      need it. You can pass NULL in this parameter if you don't need the
 *      pointer
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent, name or dest are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_MEM_ERROR
 */
int iso_tree_add_new_symlink(IsoDir *parent, const char *name,
                             const char *dest, IsoSymlink **link);

/**
 * Add a new special file to the directory tree. As far as libisofs concerns,
 * an special file is a block device, a character device, a FIFO (named pipe)
 * or a socket. You can choose the specific kind of file you want to add
 * by setting mode propertly (see man 2 stat).
 * 
 * Note that special files are only written to image when Rock Ridge 
 * extensions are enabled. Moreover, a special file is just a directory entry
 * in the image tree, no data is written beyond that.
 * 
 * Owner and hidden atts are taken from parent. You can modify any of them 
 * later.
 * 
 * @param parent
 *      the dir where the new special file will be created
 * @param name
 *      name for the new special file. If a node with same name already exists 
 *      on parent, this functions fails with ISO_NODE_NAME_NOT_UNIQUE.
 * @param mode
 *      file type and permissions for the new node. Note that you can't
 *      specify any kind of file here, only special types are allowed. i.e,
 *      S_IFSOCK, S_IFBLK, S_IFCHR and S_IFIFO are valid types; S_IFLNK, 
 *      S_IFREG and S_IFDIR aren't.
 * @param dev
 *      device ID, equivalent to the st_rdev field in man 2 stat.
 * @param special
 *      place where to store a pointer to the newly created special file. No 
 *      extra ref is addded, so you will need to call iso_node_ref() if you 
 *      really need it. You can pass NULL in this parameter if you don't need 
 *      the pointer.
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if parent, name or dest are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_WRONG_ARG_VALUE if you select a incorrect mode
 *         ISO_MEM_ERROR
 */
int iso_tree_add_new_special(IsoDir *parent, const char *name, mode_t mode,
                             dev_t dev, IsoSpecial **special);

/**
 * Set whether to follow or not symbolic links when added a file from a source
 * to IsoImage. Default behavior is to not follow symlinks.
 */
void iso_tree_set_follow_symlinks(IsoImage *image, int follow);

/**
 * Get current setting for follow_symlinks.
 * 
 * @see iso_tree_set_follow_symlinks
 */
int iso_tree_get_follow_symlinks(IsoImage *image);

/**
 * Set whether to skip or not hidden files when adding a directory recursibely.
 * Default behavior is to not ignore them, i.e., to add hidden files to image.
 */
void iso_tree_set_ignore_hidden(IsoImage *image, int skip);

/**
 * Get current setting for ignore_hidden.
 * 
 * @see iso_tree_set_ignore_hidden
 */
int iso_tree_get_ignore_hidden(IsoImage *image);

/**
 * Set whether to stop or not when an error happens when adding recursively a 
 * directory to the iso tree. Default value is to skip file and continue.
 */
void iso_tree_set_stop_on_error(IsoImage *image, int stop);

/**
 * Get current setting for stop_on_error.
 * 
 * @see iso_tree_set_stop_on_error
 */
int iso_tree_get_stop_on_error(IsoImage *image);

/**
 * Add a new node to the image tree, from an existing file. 
 * 
 * TODO comment Builder and Filesystem related issues when exposing both
 * 
 * All attributes will be taken from the source file. The appropriate file
 * type will be created.
 * 
 * @param image
 *      The image
 * @param parent
 *      The directory in the image tree where the node will be added.
 * @param path
 *      The path of the file to add in the filesystem.
 * @param node
 *      place where to store a pointer to the newly added file. No 
 *      extra ref is addded, so you will need to call iso_node_ref() if you 
 *      really need it. You can pass NULL in this parameter if you don't need 
 *      the pointer.
 * @return
 *     number of nodes in parent if success, < 0 otherwise
 *     Possible errors:
 *         ISO_NULL_POINTER, if image, parent or path are NULL
 *         ISO_NODE_NAME_NOT_UNIQUE, a node with same name already exists
 *         ISO_MEM_ERROR
 */
int iso_tree_add_node(IsoImage *image, IsoDir *parent, const char *path,
                      IsoNode **node);

/**
 * Add the contents of a dir to a given directory of the iso tree.
 * 
 * TODO comment Builder and Filesystem related issues when exposing both
 * 
 * @param image
 *      TODO expose dir rec options and explain that here
 * @param parent
 *      Directory on the image tree where to add the contents of the dir
 * @param dir
 *      Path to a dir in the filesystem
 * @return 
 *     number of nodes in parent if success, < 0 otherwise
 */
int iso_tree_add_dir_rec(IsoImage *image, IsoDir *parent, const char *dir);

/**
 * Locate a node by its path on image.
 * 
 * @param node
 *     Location for a pointer to the node, it will filled with NULL if the 
 *     given path does not exists on image.
 *     The node will be owned by the image and shouldn't be unref(). Just call
 *     iso_node_ref() to get your own reference to the node.
 *     Note that you can pass NULL is the only thing you want to do is check
 *     if a node with such path really exists.
 * @return
 *      1 found, 0 not found, < 0 error
 */
int iso_tree_path_to_node(IsoImage *image, const char *path, IsoNode **node);

/**
 * Increments the reference counting of the given IsoDataSource.
 */
void iso_data_source_ref(IsoDataSource *src);

/**
 * Decrements the reference counting of the given IsoDataSource, freeing it
 * if refcount reach 0.
 */
void iso_data_source_unref(IsoDataSource *src);

/**
 * Create a new IsoDataSource from a local file. This is suitable for
 * accessing regular .iso images, or to acces drives via its block device
 * and standard POSIX I/O calls.
 * 
 * @param path
 *     The path of the file
 * @param src
 *     Will be filled with the pointer to the newly created data source.
 * @return
 *    1 on success, < 0 on error.
 */
int iso_data_source_new_from_file(const char *path, IsoDataSource **src);

#define ISO_MSGS_MESSAGE_LEN 4096

/** 
 * Control queueing and stderr printing of messages from a given IsoImage.
 * Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
 * "NOTE", "UPDATE", "DEBUG", "ALL".
 * 
 * @param image          The image
 * @param queue_severity Gives the minimum limit for messages to be queued.
 *                       Default: "NEVER". If you queue messages then you
 *                       must consume them by iso_msgs_obtain().
 * @param print_severity Does the same for messages to be printed directly
 *                       to stderr.
 * @param print_id       A text prefix to be printed before the message.
 * @return               >0 for success, <=0 for error
 */
int iso_image_set_msgs_severities(IsoImage *img, char *queue_severity,
                                  char *print_severity, char *print_id);
/** 
 * Obtain the oldest pending message from a IsoImage message queue which has at
 * least the given minimum_severity. This message and any older message of
 * lower severity will get discarded from the queue and is then lost forever.
 * 
 * Severity may be one of "NEVER", "FATAL", "SORRY", "WARNING", "HINT",
 * "NOTE", "UPDATE", "DEBUG", "ALL". To call with minimum_severity "NEVER"
 * will discard the whole queue.
 * 
 * @param image      The image whose messages we want to obtain
 * @param error_code Will become a unique error code as listed in messages.h
 * @param msg_text   Must provide at least ISO_MSGS_MESSAGE_LEN bytes.
 * @param os_errno   Will become the eventual errno related to the message
 * @param severity   Will become the severity related to the message and
 *                   should provide at least 80 bytes.
 * @return 1 if a matching item was found, 0 if not, <0 for severe errors
 */
int iso_image_obtain_msgs(IsoImage *image, char *minimum_severity,
                          int *error_code, char msg_text[], int *os_errno,
                          char severity[]);

/**
 * Return the messenger object handle used by the given image. This handle
 * may be used by related libraries to replace their own compatible
 * messenger objects and thus to direct their messages to the libisofs
 * message queue. See also: libburn, API function burn_set_messenger().
 * 
 * @return the handle. Do only use with compatible
 */
void *iso_image_get_messenger(IsoImage *image);

#endif /*LIBISO_LIBISOFS_H_*/
