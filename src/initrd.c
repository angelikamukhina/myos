#include <stdint.h>
#include <multiboot.h>
#include <initrd.h>
#include <string.h>
#include <print.h>
#include <initramfs.h>
#include <ramfs.h>
#include <memory.h>
#include <stdbool.h>
#include <memory.h>
#include <stdlib.h>

//align macros
#include <kernel.h>

//in fs blocks
#define RAMFS_SIZE 100

void initrd_parse_multiboot(const mboot_info_t *mboot_info)
{
	if ((mboot_info->flags & (1ul << 3)) == 0 || !mboot_info->mods_count)
	{
		printf("ERROR! multiboot mods not found or not correctly initialized!\n");
		while (1);
	}

	multiboot_module_t * mods = (multiboot_module_t *)((uintptr_t)mboot_info->mods_addr);

	for (uint32_t i = 0; i != mboot_info->mods_count; i++)
	{
		multiboot_module_t * mod = mods + i;

		//INITRAMFS_MAGIC len is 6
		if (mod->mod_end - mod->mod_start < 6 
			|| memcmp((void *)(uintptr_t)mod->mod_start, INITRAMFS_MAGIC, 6))
			continue;

		initrd_begin = mod->mod_start;
		initrd_end = mod->mod_end;

		return;
	}

	printf("ERROR! Not initrd module found!\n");
	while (1);
}

static uint32_t hex_to_ul(char *data)
{
	static char buffer[8+1];//8 in struct + null byte

	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, data, sizeof(buffer) - 1);
	return strtoul(buffer, 0, 16);
}

static void extract_cpio(char * data, uint32_t length)
{
	uint32_t pos = 0;
	bool in_root = true;
	
	fs_desc_t desc;

	while ( pos < length && ( length - pos ) >= sizeof(cpio_header_t) )
	{
		cpio_header_t * head = (void *)(data + pos);
		uint32_t mode = hex_to_ul(head->mode);
		uint32_t namesize = hex_to_ul(head->namesize);
		uint32_t filesize = hex_to_ul(head->filesize);

		pos += sizeof(cpio_header_t);
		char * filename = data + pos;
		
		pos = ALIGN_CONST((pos + namesize), 4);

		if (pos > length || strcmp(filename, END_OF_ARCHIVE) == 0)
		{
			break;
		}

		char * filecontent = data + pos;

		pos += filesize;
		
		(void)filename;
		(void) filecontent;

		if (S_ISDIR(mode))
		{
			if (!in_root)
			{
				//printf("dir: %s\n", filename);
				ramfs_mkdir(&fs, filename);
			}
			else
			{
				in_root = false;
			}
		}
		else if (S_ISREG(mode) && pos <= length)
		{
			//printf("file %s\n", filename);
			desc = ramfs_open(&fs, filename);
			ramfs_fwrite(filename, 1, strlen(filename), &desc);
			ramfs_close(&desc);
			(void) desc;
		}

		pos = ALIGN_CONST(pos, 4);
	}
}

void setup_initramfs(void)
{
	ramfs_init( &fs, RAMFS_SIZE );
	ramfs_mkdir( &fs, "initrd" );

	//initrd_begin - physical address
	extract_cpio(va(initrd_begin), initrd_end - initrd_begin);

	__page_alloc_zone_free(initrd_begin, initrd_end);
}