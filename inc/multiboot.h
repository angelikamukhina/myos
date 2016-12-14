#ifndef __MULTIBOOT_H__
#define __MULTIBOOT_H__

//https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
struct mboot_info {
	uint32_t flags;
	uint32_t ignore0[4];
	/* Boot-Module list */
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t ignore1[4];
	uint32_t mmap_size;
	uint32_t mmap_addr;
} __attribute__((packed));
typedef struct mboot_info mboot_info_t;

struct multiboot_mod_list
{
	/* the memory used goes from bytes 'mod_start' to 'mod_end-1' inclusive */
	uint32_t mod_start;
	uint32_t mod_end;

	/* Module command line */
	uint32_t cmdline;

	/* padding to take it to 16 bytes (must be zero) */
	uint32_t pad;
} __attribute__((packed));
typedef struct multiboot_mod_list multiboot_module_t;

#endif /*__MULTIBOOT_H__*/