#ifndef __INITRD_H__
#define __INITRD_H__

#include <multiboot.h>
#include <ramfs.h>

ramfs_t fs;

unsigned long initrd_begin;
unsigned long initrd_end;

void initrd_parse_multiboot(const mboot_info_t *mboot_info);
void setup_initramfs(void);

#endif /*__INITRD_H__*/