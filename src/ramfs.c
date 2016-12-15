#include "ramfs.h"

#include <spinlock.h>

#include <stdio.h>

//#include <malloc.h>
#include <alloc.h>

#include <stdbool.h>

#include <string.h>

typedef void (*debug_print_t)( const char * s, ...);
#ifdef RAMFS_DEBUG
static debug_print_t debug_print = (debug_print_t)&printf;
#else
static void _empty( const char * s, ... ) { (void) s; }
static debug_print_t debug_print = (debug_print_t)_empty;
#endif

static void debug_print_blocks_info(const char * s, ramfs_t * fs)
{
#ifndef RAMFS_DEBUG
    return;
#endif
    (void) s;
    //debug_print("%s: free blocks: %d, used blocks: %d\n", s, list_size(&fs->free_block_list), list_size(&fs->block_list));

    if ( ( list_size(&fs->free_block_list) + list_size(&fs->block_list) != fs->blocks_count ) )
    {
        printf("!POSSIBLE ramfs BUG debug_print_blocks_info\n");
        while(1);
    }
}

//alloc memory for our ramfs
static void * ramfs_mem_alloc( size_t ramfs_blocks )
{
    //we allocate in memory: place for blocks lists heads and blocks
    size_t ramfs_size = ramfs_blocks * FS_BLOCK_SIZE;
    void * memory = mem_alloc( ramfs_size );

    if ( !memory )
    {
        printf( "Memory alloc error!\n" );
        while(1);
    }

    return memory;
}

static inline void _mark_block_used( fs_block_t * new_block, ramfs_t * fs )
{
#ifdef RAMFS_DEBUG
    size_t fbl_before = list_size(&fs->free_block_list);
    size_t bl_before = list_size(&fs->block_list);
    //debug_print_blocks_info("_mark_block_used before", fs);
#endif

    list_del(&new_block->free_block_list);

    list_init(&new_block->block_list);
    list_add(&new_block->block_list, &fs->block_list);

    new_block->type = fs_notinitialized_block;

#ifdef RAMFS_DEBUG
    //debug_print_blocks_info("_mark_block_used after", fs);
    if ( ( fbl_before - 1 ) != list_size(&fs->free_block_list) || ( bl_before + 1 ) != list_size(&fs->block_list) )
    {
        printf("!POSSIBLE ramfs BUG _mark_block_used need: %lu, %lu\n", ( fbl_before - 1 ), ( bl_before + 1 ));
        while(1);
    }
#endif
}

static char * filename_from_path( const char * full_path )
{
    size_t path_length = strlen(full_path);

    char * c;
    for ( int pos = path_length - 1; pos >= 0; pos-- )
    {
        c = (char *)full_path + pos;

        if ( *c == '/' )
        {
            return c + 1;
        }
    }

    return c;
}

static inline void _init_mark_block_free( fs_block_t * new_block, ramfs_t * fs )
{
    list_init(&new_block->free_block_list);
    list_add(&new_block->free_block_list, &fs->free_block_list);
}

static inline void _mark_block_filepart( fs_block_t * new_block, fs_desc_t * desc )
{
    new_block->type = fs_file_contpart;
    list_init(&new_block->file_contpart.parts_list);
    list_add(&new_block->file_contpart.parts_list, desc->file.parts_list_file_head);
}

static inline void _init_filehead_block( fs_block_t * file_head_block, list_head_t * parent_dir_list, fs_desc_t * desc )
{
    file_head_block->type = fs_file_head;
    list_init(&file_head_block->file_head.parts_list);

    //dirs
    list_init(&file_head_block->file_head.dir_list);

    if ( parent_dir_list )
        list_add(&file_head_block->file_head.dir_list, parent_dir_list);

    strcpy(file_head_block->file_head.name, filename_from_path(desc->path) );
}

static inline void _init_dirhead_block( const char * path, fs_block_t * dir_head_block, list_head_t * parent_dir_list )
{
    dir_head_block->type = fs_dir_head;
    list_init(&dir_head_block->dir_head.dir_content_list);
    list_init(&dir_head_block->dir_head.dir_list);

    if ( parent_dir_list )
        list_add(&dir_head_block->dir_head.dir_list, parent_dir_list);

    strcpy(dir_head_block->dir_head.name, filename_from_path(path) );
}

static inline void _init_file_desc( fs_block_t * head_block, fs_desc_t * desc )
{
    //add link to head to file descriptor
    desc->file.parts_list_file_head = &head_block->file_head.parts_list;
    desc->type = fs_file;
}

static inline void _init_dir_desc( fs_block_t * head_block, fs_desc_t * desc )
{
    desc->type = fs_dir;
    desc->dir.dir_headblock = head_block;
    desc->dir.curr_read_filehead = &head_block->dir_head.dir_content_list;
}

static fs_block_t * find_head_block_by_name_in_root( ramfs_t * fs, const char * name )
{
    fs_block_t * head_block;
    list_head_t * curr;
    list_for_each( curr, &fs->block_list )
    {
        head_block = LIST_ENTRY( curr, fs_block_t, block_list );
        //debug_print("need %s\n", name);
        if ( head_block->type != fs_file_head && head_block->type != fs_dir_head ) continue;

        //file must be in root
        if ( !list_empty(&head_block->file_head.dir_list) ) continue;

        //debug_print("readed %s\n", head_block->file_head.name);

        //it's not important interpret block as dir head of file head in that context, because offset of .name in structure are equal
        if ( !strcmp( head_block->file_head.name, name ) )
        {
            //printf("BLOCK BY NAME FOUND!");
            return head_block;
        }
    }

    return 0;
}

static fs_block_t * find_head_block_by_name_and_parent( const char * name, fs_block_t * parent_block )
{
    fs_block_t * head_block;
    list_head_t * curr;
    //not important file_head or dir_head because addresses are same
    list_for_each( curr, &parent_block->dir_head.dir_content_list )
    {
        head_block = LIST_ENTRY( curr, fs_block_t, file_head.dir_list );
        //debug_print("need %s\n", name);

        //debug_print("readed %s\n", head_block->file_head.name);

        //it's not important interpret block as dir head of file head in that context, because offset of .name in structure are equal
        if ( !strcmp( head_block->file_head.name, name ) )
        {
            //printf("BLOCK BY NAME FOUND!");
            return head_block;
        }
    }

    return 0;
}

static fs_block_t * find_head_block_by_path( ramfs_t * fs, const char * full_path )
{
    char * filename = filename_from_path( full_path );

    size_t path_length = strlen(full_path);

    fs_block_t * parent = 0;

    char buf[FS_MAX_PATH_SIZE];
    char * path_part_startpos = (char *)full_path;
    char * c;
    for ( size_t pos = 0; pos < path_length; pos++ )
    {
        c = (char *)full_path + pos;

        if ( *c == '/' || pos == path_length - 1 )
        {
            if ( pos != 0 )
            {
                memset(buf, 0, sizeof(buf));
                memcpy(buf, path_part_startpos, (size_t)(c - path_part_startpos + ( (pos == path_length - 1) ? 1 : 0 ) ));

                debug_print("path part: %s \n", buf);

                if ( !parent )
                {
                    debug_print("Searching %s in root\n", buf);
                    parent = find_head_block_by_name_in_root( fs, buf );
                } else {
                    debug_print("Searching %s in %s\n", buf, parent->dir_head.name);
                    parent = find_head_block_by_name_and_parent( buf, parent );
                }
#ifdef RAMFS_DEBUG
                if ( parent )
                {
                    debug_print("Found block with name: %s\n", parent->file_head.name);
                }
#endif
                if ( !parent )
                {
                    return 0;
                }

                if ( filename == path_part_startpos )
                {
                    debug_print("path part name: %s \n", filename);
                    return parent;
                }
            }

            path_part_startpos = c + 1;
        }
    }

    return 0;
}

void ramfs_init( ramfs_t * fs, size_t ramfs_blocks )
{
    if ( fs->type == fs_main_struct )
    {
        printf("ramfs already initialized!\n");
	while(1);
    }

    fs->type = fs_main_struct;
    fs->blocks_count = ramfs_blocks;

    void * memory = ramfs_mem_alloc( fs->blocks_count );
    fs->blocks = memory;

    list_init(&fs->block_list);
    list_init(&fs->free_block_list);

    //debug_print_blocks_info("ramfs_init before init", fs);

    //in first sizeof( list_head_t ) * fs->blocks_count bytes we store list_heads
    fs_block_t * curr_block;
    for ( size_t i = 0; i < fs->blocks_count; i++ )
    {
        curr_block = fs->blocks + i;

        _init_mark_block_free(curr_block, fs);
    }

    fs->used_blocks = 0;
    fs->free_blocks = fs->blocks_count;

    debug_print_blocks_info("ramfs_init after init", fs);

    spin_setup(&fs->lock);
}

void ramfs_destroy(ramfs_t * fs)
{
    mem_free(fs->blocks);
    memset(fs, 0, sizeof(*fs));
}

static void ramfs_open_check_existance( fs_desc_t * desc )
{
    fs_block_t * head_block = find_head_block_by_path( desc->fs, desc->path );

    if ( head_block )
    {
        debug_print("FILE/DIR FOUND: %s\n", head_block->file_head.name);
//while(1);
        if ( head_block->type == fs_file_head )
            _init_file_desc( head_block, desc );

        if ( head_block->type == fs_dir_head )
            _init_dir_desc( head_block, desc );
    } else {
        desc->type = fs_not_exists;
    }
}

static fs_block_t * _get_free_block( ramfs_t * fs )
{
#ifdef RAMFS_DEBUG
    debug_print_blocks_info("_get_free_block before allocation", fs);
#endif
    //search for free block
    fs_block_t * block;
    list_head_t * curr;
    list_for_each( curr, &fs->free_block_list )
    {
        block = LIST_ENTRY( curr, fs_block_t, free_block_list );

        return block;
    }
#ifdef RAMFS_DEBUG
    debug_print_blocks_info("_get_free_block before allocation", fs);
#endif
    return 0;
}

static void ramfs_dir_create( fs_desc_t * desc, list_head_t * parent_dir_list )
{
#ifdef RAMFS_DEBUG
    debug_print( "Creating dir head block...\n" );
#endif
    //search for free block and add it to block_list
    fs_block_t * dir_head_block = _get_free_block( desc->fs );

    _mark_block_used( dir_head_block, desc->fs );

    _init_dirhead_block( desc->path, dir_head_block, parent_dir_list );

    _init_dir_desc( dir_head_block, desc );
}

static void ramfs_open_file_create( fs_desc_t * desc, list_head_t * parent_dir_list )
{
#ifdef RAMFS_DEBUG
    debug_print( "File not exists. Creating file...\n" );
#endif
    //search for free block and add it to block_list
    fs_block_t * file_head_block = _get_free_block( desc->fs );

    _mark_block_used( file_head_block, desc->fs );

    _init_filehead_block( file_head_block, parent_dir_list, desc );

    _init_file_desc( file_head_block, desc );
}

static void ramfs_open_init_readpos( fs_desc_t * desc )
{
    desc->file.curr_read_block = desc->file.parts_list_file_head->next;
    desc->file.curr_read_block_pos = 0;
    desc->file.eof = false;
}

void ramfs_close(fs_desc_t * desc)
{
    memset( desc, 0, sizeof(*desc) );
    desc->type = fs_closed;
}

static list_head_t * parent_dirlist_check_find( fs_desc_t * desc, const char * full_path )
{
    //check that parent directory exists
    char * filename = filename_from_path( full_path );
    size_t path_len = (size_t)(filename - full_path);
    //truncate last '/'
    path_len = ( path_len ) ? ( path_len - 1 ) : 0;

    fs_block_t * dir_head_block = 0;

    //ok. not in root
    if ( path_len )
    {
        char path[FS_MAX_PATH_SIZE];
        memset(path, 0, sizeof(path));
        memcpy(path, full_path, path_len);

        debug_print("DIR FOR NEW FILE/DIR: %s\n", path);

        dir_head_block = find_head_block_by_path( desc->fs, path );

        if ( !dir_head_block )
        {
            debug_print("ERROR! DIR %s FOR NEW FILE/DIR NOT FOUND!\n", path);
            desc->type = fs_error_occured;
            return 0;
        }
    }

    list_head_t * parent_dir_list = 0;
    if ( dir_head_block )
    {
        parent_dir_list = &dir_head_block->dir_head.dir_content_list;
    }

    return parent_dir_list;
}

fs_desc_t ramfs_open( ramfs_t * fs, const char * full_path )
{
#ifdef RAMFS_DEBUG
    debug_print( "Opening file/dir...\n" );
#endif

    fs_desc_t desc;
    desc.fs = fs;
    desc.type = fs_not_exists;
    strcpy(desc.path, full_path);

    const int enable = spin_lock_irqsave(&fs->lock);

    ramfs_open_check_existance( &desc );

    //create file
    if ( desc.type == fs_not_exists )
    {
        //check that parent directory exists
        list_head_t * parent_dir_list = parent_dirlist_check_find( &desc, full_path );

        if ( desc.type == fs_error_occured )
        {
            spin_unlock_irqrestore(&fs->lock, enable);

            return desc;
        }

        //create head block
        ramfs_open_file_create( &desc, parent_dir_list );
    }

    //init file descriptor
    if ( desc.type == fs_file )
    {
        ramfs_open_init_readpos( &desc );

        if ( list_empty(desc.file.curr_read_block) )
        {
            debug_print("fopen: File is empty!\n");
            //return;
        }
    }

    spin_unlock_irqrestore(&fs->lock, enable);

    return desc;
}

fs_direntry_t ramfs_readdir(fs_desc_t * desc)
{
    fs_direntry_t result;

    if ( desc->type != fs_dir )
    {
        result.type = fs_entry_end;
	desc->type = fs_error_occured;
        return result;
    }

    fs_block_t * curr_entry_headblock;

    const int enable = spin_lock_irqsave(&desc->fs->lock);

    list_head_t * pos = desc->dir.curr_read_filehead->next;

    if ( pos == &desc->dir.dir_headblock->dir_head.dir_content_list )
    {
        result.type = fs_entry_end;
    } else {
        curr_entry_headblock = LIST_ENTRY( pos, fs_block_t, file_head.dir_list );

        memset(result.name, 0, sizeof(result.name));
        strcpy(result.name, curr_entry_headblock->file_head.name);

        if ( curr_entry_headblock->type == fs_file_head )
            result.type = fs_entry_file;
        else
            result.type = fs_entry_dir;
    }

    desc->dir.curr_read_filehead = pos;

    spin_unlock_irqrestore(&desc->fs->lock, enable);

    return result;
}

void ramfs_mkdir( ramfs_t * fs, const char * full_path )
{
#ifdef RAMFS_DEBUG
    debug_print( "Creating dir %s...\n", full_path );
#endif

    fs_desc_t desc;
    desc.fs = fs;
    desc.type = fs_not_exists;
    strcpy(desc.path, full_path);

    const int enable = spin_lock_irqsave(&fs->lock);

    ramfs_open_check_existance( &desc );

    //create dir
    if ( desc.type == fs_not_exists )
    {
        //check that parent directory exists
        list_head_t * parent_dir_list = parent_dirlist_check_find( &desc, full_path );

        if ( desc.type == fs_error_occured )
        {
	    spin_unlock_irqrestore(&fs->lock, enable);

            return;
        }

        ramfs_dir_create( &desc, parent_dir_list );
    }

    ramfs_close(&desc);

    spin_unlock_irqrestore(&fs->lock, enable);
}

void ramfs_fwrite(const void * ptr, size_t size, size_t count, fs_desc_t * desc)
{
    if ( desc->type != fs_file )
    {
        printf("Attempt to apply ramfs_fwrite to not fs_file descriptor!\n");
        return;
    }
#ifdef RAMFS_DEBUG
    debug_print( "Writing in file...\n" );
#endif
    size_t data_length = size * count;
    //size_t file_size_in_blocks = list_size(desc->parts_list_file_head);
    //size_t blocks_fsize = FS_BLOCK_SIZE * file_size_in_blocks;

    const int enable = spin_lock_irqsave(&desc->fs->lock);

    fs_block_t * last_file_block = LIST_ENTRY( desc->file.parts_list_file_head->prev, fs_block_t, file_contpart.parts_list );

    //size_t real_file_size = FS_BLOCK_SIZE * ( file_size_in_blocks - 1 ) + last_file_block->file_contpart.data_len;

    //file have only head or not enough space in last file part block for new data
    if ( list_empty(desc->file.parts_list_file_head) || ( FS_BLOCK_DATA_SIZE - last_file_block->file_contpart.data_len ) < data_length )
    {
        size_t new_blocks_num = data_length / FS_BLOCK_DATA_SIZE + 1;
        debug_print("Not enough space in last block. Adding %lu blocks...\n", new_blocks_num);

        debug_print_blocks_info("ramfs_fwrite before block alloc", desc->fs);

        size_t i = 0;
        fs_block_t * file_part_block;
        list_head_t * curr;
        //debug_print("free_block_list addr: %llx\n", &desc->fs->free_block_list);
        _list_for_each( curr, &desc->fs->free_block_list )
        {
            if ( i >= new_blocks_num ) break;
            i++;

            file_part_block = LIST_ENTRY( curr, fs_block_t, free_block_list );
            file_part_block->file_contpart.data_len = 0;

            _mark_block_used(file_part_block, desc->fs);
            //debug_print("1fwrite: parts list size: %d\n", list_size(desc->file.parts_list_file_head));
            _mark_block_filepart(file_part_block, desc);

            //debug_print("%d free block found and used!\n", i);
            //debug_print_blocks_info("ramfs_fwrite inside cycle alloc", desc->fs);
            //debug_print("2fwrite: parts list size: %d\n", list_size(desc->file.parts_list_file_head));
        }

        if ( i != new_blocks_num )
        {
            debug_print("BUG!");
            while(1);
        }
#ifdef RAMFS_DEBUG
        debug_print_blocks_info("ramfs_fwrite after block alloc", desc->fs);
#endif
    }

    if ( FS_BLOCK_DATA_SIZE == last_file_block->file_contpart.data_len )
    {
        last_file_block = LIST_ENTRY( last_file_block->file_contpart.parts_list.next, fs_block_t, file_contpart.parts_list );
    }

    //debug_print("Write data to first block with %lu bytes available (%lu total)\n", FS_BLOCK_DATA_SIZE - last_file_block->file_contpart.data_len, FS_BLOCK_DATA_SIZE);
    //debug_print("%lu bytes data given!\n", data_length);

    //write data to blocks
    const uint8_t * data_ptr = ptr;
    for ( size_t i = 0, bytes_to_write, free_bytes_in_block = FS_BLOCK_DATA_SIZE - last_file_block->file_contpart.data_len; i < data_length; )
    {
	debug_print("ramfs_fwrite curr block type %d\n", last_file_block->type);
	if ( last_file_block->type == fs_file_head )
	{
	    last_file_block = LIST_ENTRY( last_file_block->file_contpart.parts_list.next, fs_block_t, file_contpart.parts_list );
            free_bytes_in_block = FS_BLOCK_DATA_SIZE;
	}

        if ( !( ( data_length - i ) / ( free_bytes_in_block + 1 ) ) )
        {
            bytes_to_write = ( data_length - i ) % ( free_bytes_in_block + 1 );
        }
        else
        {
            bytes_to_write = free_bytes_in_block;
        }

        //debug_print("%lu : %lu \n", ( data_length - i ), ( free_bytes_in_block + 1 ));
        //debug_print("%lu bytes in block free!\n", free_bytes_in_block);

        memcpy(last_file_block->file_contpart.data + last_file_block->file_contpart.data_len, data_ptr, bytes_to_write);
        last_file_block->file_contpart.data_len = bytes_to_write;
        data_ptr += bytes_to_write;
        i += bytes_to_write;

        last_file_block = LIST_ENTRY( last_file_block->file_contpart.parts_list.next, fs_block_t, file_contpart.parts_list );
        free_bytes_in_block = FS_BLOCK_DATA_SIZE;

        //debug_print("%lu bytes written to block!\n", bytes_to_write);
        //break;
    }
#ifdef RAMFS_DEBUG
    debug_print("end fwrite: parts list size: %d\n", list_size(desc->file.parts_list_file_head->next));
#endif
    if ( list_empty(desc->file.curr_read_block) )
    {
        debug_print("fwrite: File is empty! parts list size: %d\n", list_size(desc->file.parts_list_file_head->next));
        debug_print("ERROR\n");
        while(1);
        return;
    }

    ramfs_open_init_readpos( desc );

    spin_unlock_irqrestore(&desc->fs->lock, enable);
    //ramfs_open_init_readpos( desc );
}

size_t ramfs_fread(void * ptr, size_t size, size_t count, fs_desc_t * desc)
{
    if ( desc->type != fs_file )
    {
        printf("Attempt to apply ramfs_fread to not fs_file descriptor!\n");
        return 0;
    }
#ifdef RAMFS_DEBUG
    //debug_print( "Reading file...\n" );
#endif
    //check empty files
    if ( list_empty(desc->file.curr_read_block) )
    {
        debug_print("fread: File is empty!\n");
        return 0;
    }

    size_t data_length = size * count;

    const int enable = spin_lock_irqsave(&desc->fs->lock);

    //desc.read_pos

    fs_block_t * curr_read_block = LIST_ENTRY( desc->file.curr_read_block, fs_block_t, file_contpart.parts_list );
    uint8_t * curr_block_data = curr_read_block->file_contpart.data + desc->file.curr_read_block_pos;

    //debug_print("%lu read position!\n", desc->file.curr_read_block_pos);
    //return;
    uint8_t * data_ptr = ptr;
    size_t bytes_to_read = 0;
    size_t i;
    for ( i = 0; i < data_length;  )
    {
        if ( !( ( data_length - i ) / ( curr_read_block->file_contpart.data_len + 1 ) ) )
        {
            bytes_to_read = ( data_length - i ) % ( curr_read_block->file_contpart.data_len + 1 );
        }
        else
        {
            bytes_to_read = curr_read_block->file_contpart.data_len - desc->file.curr_read_block_pos;
        }
        debug_print("%lu bytes of data in block available! (block type %d)\n", curr_read_block->file_contpart.data_len, curr_read_block->type);

        memcpy(data_ptr, curr_block_data, bytes_to_read);
        data_ptr += bytes_to_read;
        i += bytes_to_read;

        //desc->file.curr_read_block = &curr_read_block->file_contpart.parts_list;
        debug_print("%lu bytes read from block!\n", bytes_to_read);

        desc->file.curr_read_block_pos += bytes_to_read;

        debug_print("%lu %lu\n", desc->file.curr_read_block_pos, curr_read_block->file_contpart.data_len);
        if ( desc->file.curr_read_block_pos == curr_read_block->file_contpart.data_len )
        {
            curr_read_block = LIST_ENTRY( curr_read_block->file_contpart.parts_list.next, fs_block_t, file_contpart.parts_list );
            curr_block_data = curr_read_block->file_contpart.data;

	    debug_print("check that next block is head!\n");
	    //if ( &curr_read_block->file_contpart.parts_list == desc->file.parts_list_file_head )
	    if ( curr_read_block->type == fs_file_head )
	    {
                if ( i != data_length )
                {
                    desc->file.eof = true;
                } else {
                    ramfs_open_init_readpos( desc );
		}

                debug_print("next read part list is a file head! %d bytes read (need %d)\n", i, data_length);
                spin_unlock_irqrestore(&desc->fs->lock, enable);

                return i;
	    }

            desc->file.curr_read_block = &curr_read_block->file_contpart.parts_list;
            desc->file.curr_read_block_pos = 0;
        }
    }

    //debug_print("%lu bytes read\n", i);
    if ( i != data_length )
    {
        printf("read less that requested!\n");
        //while(1);
    }

    spin_unlock_irqrestore(&desc->fs->lock, enable);

    return i;
}

