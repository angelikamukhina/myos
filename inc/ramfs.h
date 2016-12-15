#ifndef RAMFS_H
#define RAMFS_H

#include <spinlock.h>

#include <list.h>
#include <stdint.h>
#include <stdbool.h>

//#define RAMFS_DEBUG 1

//in bytes
#define FS_BLOCK_SIZE 1024
#define FS_MAX_PATH_SIZE 256
#define FS_BLOCK_DATA_SIZE ( FS_BLOCK_SIZE - ( sizeof( size_t ) + 2 * sizeof( list_head_t ) + sizeof( fs_struct_type_t ) ) )

/* main structures */
typedef enum fs_struct_type {
    fs_file_head,
    fs_file_contpart,
    fs_dir_head,
    fs_notinitialized_block,
    fs_main_struct
} fs_struct_type_t;

//file head
typedef struct fs_block_file_head {
    list_head_t dir_list;      //entity in directory files list
    char name[ FS_MAX_PATH_SIZE ];
    list_head_t parts_list;    //entiry in file parts list
} __attribute__((packed)) fs_block_file_head_t;

//part of file
typedef struct fs_block_file_contpart {
    list_head_t parts_list;    //entiry in file parts list
    size_t data_len;
    uint8_t data[ FS_BLOCK_DATA_SIZE ];

    struct spinlock lock;      //protect file data when we write/read them
} __attribute__((packed)) fs_block_file_contpart_t;

//dir head
typedef struct fs_block_dir_head {
    list_head_t dir_list;      //entity in directory files list
    char name[ FS_MAX_PATH_SIZE ];
    list_head_t dir_content_list;
} __attribute__((packed)) fs_block_dir_head_t;

//fs cluster(block) structure
typedef struct fs_block {
    fs_struct_type_t type;
    union {
        list_head_t block_list; //entity in list of used blocks
        list_head_t free_block_list;
    };
    union {
        fs_block_file_head_t file_head;
        fs_block_file_contpart_t file_contpart;
        fs_block_dir_head_t dir_head;
    };
} __attribute__((packed)) fs_block_t;

typedef struct ramfs {
    fs_struct_type_t type;
    //void * memory;
    list_head_t * list_heads;
    fs_block_t * blocks;
    list_head_t block_list;
    size_t used_blocks;
    list_head_t free_block_list;
    size_t free_blocks;
    size_t blocks_count;

    struct spinlock lock;         //protect free_block_list, block_list
} ramfs_t;

/* fs descriptor */
//file descriptor types
typedef enum fs_desc_type {
    fs_error_occured,
    fs_closed,
    fs_file,
    fs_dir,
    fs_not_exists
} fs_desc_type_t;

typedef struct fs_desc_file {
    bool eof;
    list_head_t * parts_list_file_head;
    list_head_t * curr_read_block;
    size_t curr_read_block_pos;
} fs_desc_file_t;

typedef struct fs_desc_dir {
    fs_block_t * dir_headblock;
    list_head_t * curr_read_filehead;
} fs_desc_dir_t;

typedef struct fs_desc {
    ramfs_t * fs;
    fs_desc_type_t type;
    char path[ FS_MAX_PATH_SIZE ];

    //cache
    union {
        fs_desc_file_t file;
        fs_desc_dir_t dir;
        //list_head_t * dir_head_blocks_list;
    };
} fs_desc_t;

/* readdir */
typedef enum fs_direntry_type {
    fs_entry_file,
    fs_entry_dir,
    fs_entry_end
} fs_direntry_type_t;

typedef struct fs_direntry {
    fs_direntry_type_t type;
    char name[ FS_MAX_PATH_SIZE ];
} fs_direntry_t;

void ramfs_init(ramfs_t * fs, size_t ramfs_size);
void ramfs_destroy(ramfs_t * fs);
fs_desc_t ramfs_open(ramfs_t * fs, const char * full_path);
size_t ramfs_fread(void * ptr, size_t size, size_t count, fs_desc_t * desc);
fs_direntry_t ramfs_readdir(fs_desc_t * desc);
void ramfs_fwrite(const void * ptr, size_t size, size_t count, fs_desc_t * desc);
void ramfs_close(fs_desc_t * desc);
void ramfs_mkdir(ramfs_t * fs, const char * full_path);

//we can delete list entities in body without problems but we lost ordering and need to manualy break cycle
#define _list_for_each(pos, head) \
  for (pos = (head)->next; pos != (head);	\
       pos = (head)->next)

#endif // RAMFS_H
