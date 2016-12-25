#include <serial.h>
#include <memory.h>
#include <balloc.h>
#include <paging.h>
#include <debug.h>
#include <alloc.h>
#include <print.h>
#include <ints.h>
#include <time.h>
#include <threads.h>
#include <mutex.h>
#include <condition.h>
#include <initrd.h>

#include <ramfs.h>
#include <string.h>

#include <syscall.h>

static void qemu_gdb_hang(void)
{
#ifdef DEBUG
	static volatile int wait = 1;

	while (wait);
#endif
}

//create, write, read test
void ramfs_simple_cwr_test(ramfs_t * fs, const char * file_path)
{
    fs_desc_t desc = ramfs_open(fs, file_path);

    char test_str[] = "Hello, world!";
    size_t test_len = strlen(test_str);
    ramfs_fwrite(test_str, 1, test_len, &desc);

    char * data = mem_alloc( test_len );

    ramfs_fread(data, 1, 3, &desc);
    ramfs_fread(data+3, 1, 2, &desc);
    ramfs_fread(data+5, 1, test_len - 5, &desc);
    printf("SIMPLE TEST: %s\n", data);
    if ( !!memcmp(test_str, data, test_len) )
    {
        printf("FAIL ramfs_simple_cwr_test\n");
        while(1);
    }

    ramfs_close(&desc);

    mem_free(data);
}

void ramfs_simple_r_test(ramfs_t * fs, const char * file_path)
{
    fs_desc_t desc = ramfs_open(fs, file_path);

    char test_str[] = "Hello, world!";
    size_t test_len = strlen(test_str);

    char * data = mem_alloc( test_len );
    memset(data, 0, test_len);

    desc = ramfs_open(fs, file_path);
    size_t readed = ramfs_fread(data,1,test_len+10,&desc);
    printf("SIMPLE READ TEST(%d bytes / %d real): (%d requested) %s\n", readed, test_len, test_len+10, data);
    if ( !!memcmp(test_str, data, test_len) )
    {
        printf("FAIL ramfs_simple_r_test\n");
        while(1);
    }

    ramfs_close(&desc);

    mem_free(data);
}

void ramfs_big_writeread_test(ramfs_t * fs, const char * file_path)
{
    fs_desc_t desc;
    desc = ramfs_open(fs, file_path);

    size_t test_len = 3898;
    size_t test_shift = 2300;
    char * test_in = mem_alloc( test_len );
    *(char *)test_in = '8';
    strcpy(test_in+test_shift,"START23122894814983290409328423804823840284820480284928498204890284829482840238482948209483284923840328408329482384END");
    ramfs_fwrite(test_in, 1, test_len, &desc);
    char * test_out = mem_alloc( test_len );

    if ( !!memcmp(test_in, test_out, test_len) )
    {
        printf("OK before: test_in != test_out\n");
    }

    size_t test_read_shift = 2322;
    for ( test_read_shift = 0; test_read_shift <= test_len/2; test_read_shift++ )
    {
        ramfs_fread(test_out,1,test_read_shift,&desc);
        ramfs_fread(test_out+test_read_shift,1,test_len - test_read_shift,&desc);

        if ( !!memcmp(test_in, test_out, test_len) )
        {
            printf("ERROR after: test_in != test_out\n");
            //printf("%s\n", (char*)(test_out+test_shift));
            while(1);
        }

        ramfs_rewind( &desc );
    }

    printf("Written: [%s]\n", (char*)(test_in+test_shift));
    printf("Readed: [%s]\n", (char*)(test_out+test_shift));
}

void ramfs_print_dir(ramfs_t * fs, const char * full_path)
{
	printf("DIR CONTENT %s:\n", full_path);
	fs_desc_t dir_desc;
	fs_direntry_t entry;
	dir_desc = ramfs_open(fs, full_path);
	while ( (entry = ramfs_readdir(&dir_desc)).type != fs_entry_end )
	{
		printf("name: %s, type: %d\n", entry.name, entry.type);
	}
	ramfs_close(&dir_desc);
}

void ramfs_dir_test(ramfs_t * fs)
{
	char dir1[] = "dir_name1";
	char dir2[] = "dir_name1/dir_name2";
	char dir3[] = "dir_name1/dir_name2/dir_name3";
	char file_path0[] = "dir_name1/dir_name2/testFile0";
	char file_path1[] = "dir_name1/dir_name2/testFileName";
	ramfs_mkdir(fs, dir1);
	ramfs_mkdir(fs, dir2);
	ramfs_mkdir(fs, dir3);
	ramfs_simple_cwr_test(fs, file_path0);
	ramfs_simple_cwr_test(fs, file_path1);
	ramfs_simple_r_test(fs, file_path1);

	ramfs_print_dir(fs, dir2);
}

void ramfs_init_tests(ramfs_t * fs)
{
	(void) fs;
	//printf("size: %lu\n", sizeof(fs_block_t));
	
	ramfs_dir_test(fs);
	
	ramfs_big_writeread_test(fs, "big_file");

	
	ramfs_print_dir(fs, "initrd");
	ramfs_print_dir(fs, "initrd/simple_fs");

	//ramfs_big_writeread_test(fs, "big_file");
}

static void test_kmap(void)
{
	const size_t count = 1024;
	struct page **pages = mem_alloc(sizeof(*pages) * count);
	size_t i;

	BUG_ON(!pages);
	for (i = 0; i != count; ++i) {
		pages[i] = __page_alloc(0);
		if (!pages[i])
			break;
	}

	char *ptr = kmap(pages, i);

	BUG_ON(!ptr);
	BUG_ON((uintptr_t)ptr < HIGHER_BASE);

	for (size_t j = 0; j != i * PAGE_SIZE; ++j)
		ptr[i] = 13;

	for (size_t j = 0; j != i * PAGE_SIZE; ++j)
		BUG_ON(ptr[i] != 13);

	kunmap(ptr);
	mem_free(pages);
}

static void test_alloc(void)
{
	struct list_head head;
	unsigned long count = 0;

	list_init(&head);
	while (1) {
		struct list_head *node = mem_alloc(sizeof(*node));

		if (!node)
			break;
		BUG_ON((uintptr_t)node < HIGHER_BASE);
		++count;
		list_add(node, &head);
	}

	printf("Allocated %lu bytes\n", count * sizeof(head));

	while (!list_empty(&head)) {
		struct list_head *node = head.next;

		BUG_ON((uintptr_t)node < HIGHER_BASE);
		list_del(node);
		mem_free(node);
	}

	mem_alloc_shrink();
}

static void test_slab(void)
{
	struct list_head head;
	struct mem_cache cache;
	unsigned long count = 0;

	list_init(&head);
	mem_cache_setup(&cache, sizeof(head), sizeof(head));
	while (1) {
		struct list_head *node = mem_cache_alloc(&cache);

		if (!node)
			break;
		BUG_ON((uintptr_t)node < HIGHER_BASE);
		++count;
		list_add(node, &head);
	}

	printf("Allocated %lu bytes\n", count * sizeof(head));

	while (!list_empty(&head)) {
		struct list_head *node = head.next;

		BUG_ON((uintptr_t)node < HIGHER_BASE);
		list_del(node);
		mem_cache_free(&cache, node);
	}

	mem_cache_release(&cache);
}

static void test_buddy(void)
{
	struct list_head head;
	unsigned long count = 0;

	list_init(&head);
	while (1) {
		struct page *page = __page_alloc(0);

		if (!page)
			break;
		++count;
		list_add(&page->ll, &head);
	}

	printf("Allocated %lu pages\n", count);

	while (!list_empty(&head)) {
		struct list_head *node = head.next;
		struct page *page = CONTAINER_OF(node, struct page, ll);

		list_del(&page->ll);
		__page_free(page, 0);
	}
}

static void __th1_main(void *data)
{
	const int id = (int)(uintptr_t)data;

	for (size_t i = 0; i != 5; ++i) {
		printf("i'm %d\n", id);
		force_schedule();
	}
}

static void test_threads(void)
{
	struct thread *th1 = thread_create(&__th1_main, (void *)1);
	struct thread *th2 = thread_create(&__th1_main, (void *)2);

	thread_activate(th1);
	thread_activate(th2);

	thread_join(th2);
	thread_join(th1);

	thread_destroy(th1);
	thread_destroy(th2);
}

static void wait(unsigned long long count)
{
	const unsigned long long time = current_time();

	while (time + count > current_time())
		force_schedule();
}

static void __th2_main(void *data)
{
	struct mutex *mtx = data;

	for (size_t i = 0; i != 5; ++i) {
		mutex_lock(mtx);
		printf("%p acquired mutex\n", thread_current());
		wait(100);
		printf("%p released mutex\n", thread_current());
		mutex_unlock(mtx);
	}
}

static void test_mutex(void)
{
	struct mutex mtx;
	struct thread *th1 = thread_create(&__th2_main, (void *)&mtx);
	struct thread *th2 = thread_create(&__th2_main, (void *)&mtx);

	mutex_setup(&mtx);
	thread_activate(th1);
	thread_activate(th2);

	thread_join(th2);
	thread_join(th1);

	thread_destroy(th1);
	thread_destroy(th2);
}

struct future {
	struct mutex mtx;
	struct condition cond;
	int value;
	int set;
};

static void future_setup(struct future *future)
{
	mutex_setup(&future->mtx);
	condition_setup(&future->cond);
	future->value = 0;
	future->set = 0;
}

static void future_set(struct future *future, int value)
{
	mutex_lock(&future->mtx);
	future->value = value;
	future->set = 1;
	condition_broadcast(&future->cond);
	mutex_unlock(&future->mtx);
}

static int future_get(struct future *future)
{
	int res;

	mutex_lock(&future->mtx);
	while (!future->set)
		condition_wait(&future->cond, &future->mtx);
	res = future->value;
	mutex_unlock(&future->mtx);
	return res;
}

static void __th3_main(void *data)
{
	struct future *fut = data;

	wait(1000);
	future_set(fut, 42);
}

static void test_condition(void)
{
	struct future fut;
	struct thread *th = thread_create(&__th3_main, &fut);

	future_setup(&fut);
	thread_activate(th);
	BUG_ON(future_get(&fut) != 42);

	thread_join(th);
	thread_destroy(th);
}

static void test_syscalls()
{
	printf("syscalls tests...\n");
	const char * test_str = "Hello from syscall (write)!\n";
	syscall(0, test_str, strlen(test_str));

	//test syscall
	int return_val = syscall(2, 1, 2, 3, 4, 5);
	printf("syscall(2, 1, 2, 3, 4, 5) returned %d need %d\n", return_val, 6);

	//make non valid call
	int non_valid_no = 12;
	return_val = syscall(non_valid_no);
	printf("NONVALID syscall(%d) returned %d need %d\n", non_valid_no, return_val, ENOSYS);
	printf("EOF syscalls tests...\n");
}

static void __userth_main(void * data)
{
	//while(1);
	//use syscall
	const char * test_str = "Hello from userspace syscall!\n";
	//(void) test_str;
	syscall(0, test_str, strlen(test_str));
	syscall(0, test_str, strlen(test_str));

	test_str = "Message printed two times? OK. EOF userspace function\n";
	syscall(0, test_str, strlen(test_str));
	while(1);

	//check that we in user mode
	//__asm__ volatile("int $127" : :);
	//printf("EOF userspace syscall test\n");

	(void) data;
}

static void userspace_thread_create()
{
	printf("START userspace syscall test\n");
	struct thread *th = userthread_create(&__userth_main, "I'm in userspace!\n");
	(void) th;

	thread_activate(th);
	thread_destroy(th);

	printf("userspace_thread_create FINISHED\n");
}

void main(void *bootstrap_info)
{
	qemu_gdb_hang();

	serial_setup();
	ints_setup();
	time_setup();
	//setup_initrd(bootstrap_info);
	balloc_setup(bootstrap_info);
	paging_setup();
	page_alloc_setup();
	mem_alloc_setup();
	kmap_setup();
	setup_initramfs();
	threads_setup();
	enable_ints();

	printf("Tests Begin\n");



	ramfs_init_tests(&fs);
	test_buddy();
	test_slab();
	test_alloc();
	test_kmap();
	test_threads();
	test_mutex();
	test_condition();

	test_syscalls();
	//check that we not cracked all
	test_threads();
	userspace_thread_create();

	printf("Tests Finished\n");

	idle();
}
