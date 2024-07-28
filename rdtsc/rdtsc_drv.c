#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/page.h>

#define BUFFER_SIZE 12
#if (PAGE_SIZE < BUF_SIZE)
#define BUFFER_SIZE PAGE_SIZE   /* for not enough page size system */
#endif

static unsigned long file_cmd = 0;
static unsigned long file_data = 0;
static int debug;

/*
inline unsigned long long int rdtsc()
{
        unsigned long long int x;
        __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
        return x;
}

void something(void)
{
}
*/

/*
 * file_read_proc -- called when user reading
 */
static
int file_read_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    char in_data[BUFFER_SIZE];
    int return_length = 0;
    long long a, b;

    printk("**read_proc(), count = %d, off = %d\n",
	   count, (int) offset);
    if (offset == 0) {
      memset(in_data, 0, BUFFER_SIZE);
      return_length = sprintf(in_data, "%lu", file_cmd);
      if (file_data) {
	  return_length += sprintf(in_data + return_length,
				  " %lu", file_data);
      }
      return_length += sprintf(in_data + return_length, "\n");

      rdtscll(a);

      memcpy(buf, in_data, return_length);

      rdtscll(b);

      printk("**read_proc(), [%s], clock = %ld\n", in_data, (long) (b -a));
    }
    *eof = 1;
    *start = buf + offset;
    return return_length;
}

/*
 * file_write_proc -- called when user writing
 */
int file_write_proc(struct file *file, const char *buf,
                     unsigned long count, void *data)
{
    char out_data[BUFFER_SIZE];
    int length;
    char *p;
 
    printk("**write_proc(), count = %d\n", (int) count);

    length = count > BUFFER_SIZE ? BUFFER_SIZE : count; /* limit of max size */
	   
    copy_from_user(out_data, buf, length);

    p = &out_data[0];
    file_cmd = simple_strtoul(p, &p, 10);
    while(*p == ' ') p++; /* skip space */
    file_data = simple_strtoul(p, NULL, 10);

    printk("write_file(), cmd = %ld, data = %08lx\n",
	   file_cmd, file_data);

    return length;
}

static void file_create_proc()
{
    struct proc_dir_entry *entry;
    entry = create_proc_entry("file", 0, 0); /* "file" registration */
    entry->read_proc = file_read_proc; /* read routine */
    entry->write_proc = file_write_proc; /* write routine */
}

static void file_remove_proc()
{
    remove_proc_entry("file", NULL /* parent dir */);
}

static
int __init file_init_module(void)
{
    file_create_proc();
    printk(KERN_INFO "FILE, init\n");
    return 0;
}

static
void __exit file_cleanup_module(void)
{
    printk(KERN_INFO "FILE cleanup\n");
    file_remove_proc();
}

MODULE_DESCRIPTION("FILE Module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Device Drivers Limited");
MODULE_PARM(debug,       "i");
MODULE_PARM_DESC(debug, "FILE debug level (0-6)");

module_init(file_init_module);
module_exit(file_cleanup_module);
