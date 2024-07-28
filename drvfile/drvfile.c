#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <asm/page.h>

#define BUFFER_SIZE 2048
#if (PAGE_SIZE < BUF_SIZE)
#define BUFFER_SIZE PAGE_SIZE   /* for not enough page size system */
#endif

static char filename[BUFFER_SIZE]; /* should not include '\n' */
static char filedata[BUFFER_SIZE]; /* should include '\n' */

/*
 * fs file open / read / write / close
 */
#define file_err(format, arg...)  printk(KERN_ERR "%s : " format "\n", __FUNCTION__, ## arg)
#ifdef DEBUG
#define file_dbg(format, arg...)  printk(KERN_ERR "%s : " format "\n", __FUNCTION__, ## arg)
#else
#define file_dbg(format, arg...) do {} while (0)
#endif

struct file *file_open(char *filename, int flags, int mode)
{
	struct file *filp;

	filp = filp_open(filename, flags, mode);
	if (filp==NULL || IS_ERR(filp)) {
		file_err("cannot open file = %s", filename);
		return NULL;  /* Or do something else */
	}
	if (filp->f_op->read == NULL || filp->f_op->write == NULL) {
		file_err("File(system) doesn't allow reads / writes");
		return NULL;
	}
	if (!S_ISREG(filp->f_dentry->d_inode->i_mode)) {
		filp_close(filp, NULL);
		file_err("%s is NOT a regular file", filename);
		return NULL;  /* Or do something else */
	}
	file_dbg("file mode = %08x, f_pos = %d",
		 filp->f_dentry->d_inode->i_mode, (int) filp->f_pos);
	return(filp);
}

int file_read(struct file *filp, void *buf, int count)
{
	mm_segment_t oldfs;
	int BytesRead;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	BytesRead = filp->f_op->read(filp, buf, count, &filp->f_pos);

	file_dbg("BytesRead = %d", BytesRead);

	set_fs(oldfs);
	return BytesRead;
}

int file_write(struct file *filp, void *buf, int count)
{
	mm_segment_t oldfs;
	int BytesWrite;

        oldfs = get_fs();
        set_fs(KERNEL_DS);
	/* filp->f_pos = StartPos; */
        BytesWrite = filp->f_op->write(filp, buf, count, &filp->f_pos);

	file_dbg("BytesWrite = %d", BytesWrite);

        set_fs(oldfs);
	return BytesWrite;
}

void file_close(struct file *filp)
{
	fput(filp);
	filp_close(filp, NULL);
}

/*
 * read_proc -- called when user reading
 */
int filename_read(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
	char in_data[BUFFER_SIZE];
	int return_length = 0;

	file_dbg("count = %d, off = %d", count, (int) offset);
	if (offset == 0) {
		memset(in_data, 0, BUFFER_SIZE);
		return_length = sprintf(in_data, "%s\n", filename);
		memcpy(buf, in_data, return_length);
		file_dbg("filename = [%s]", filename);
	}
	*eof = 1;
	*start = buf + offset;
	return return_length;
}

int filedata_read(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
	struct file *filp;
	char in_data[BUFFER_SIZE];
	int return_length = 0;
	int len = 0;

	file_dbg("count = %d, off = %d", count, (int) offset);
	if (offset == 0
	    && (filp = file_open(filename, O_RDONLY, 0)) != NULL) {
		len = file_read(filp, filedata, BUFFER_SIZE);
		file_close(filp);

		memset(in_data, 0, BUFFER_SIZE);
		return_length = sprintf(in_data, "%s", filedata);
		memcpy(buf, in_data, return_length);
		file_dbg("filedata = [%s], size = %d", filedata, len);
	}
	*eof = 1;
	*start = buf + offset;
	return return_length;
}

/*
 * write_proc -- called when user writing
 */
int filename_write(struct file *file, const char *buf,
                     unsigned long count, void *data)
{
	int length;
 
	file_dbg("count = %d", (int) count);
	length = count > BUFFER_SIZE ? BUFFER_SIZE : count; /* limit of max size */
	   
	copy_from_user(filename, buf, length);
	if (filename[length-1] == '\n') /* Is last char LF? */
		filename[length-1] = '\0';
	filename[length] = '\0';

	file_dbg("finename = [%s], size = %d", filename, length);

	return length;
}

int filedata_write(struct file *file, const char *buf,
                     unsigned long count, void *data)
{
	struct file *filp;
	int length;
	int len = 0;
 
	file_dbg("count = %d", (int) count);
	length = count > BUFFER_SIZE ? BUFFER_SIZE : count; /* limit of max size */
	   
	copy_from_user(filedata, buf, length);
	filedata[length] = '\0';

	filp = file_open(filename, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if (filp != NULL) {
		len = file_write(filp, filedata, length);
		file_close(filp);

		file_dbg("writedata = [%s], size = %d, wrote = %d", filedata, length, len);
	}
	return length;
}

/*
 * top level routine
 */
static void file_create_proc(void)
{
    struct proc_dir_entry *entry;

    strcpy(filename, "/tmp/testfile");
    strcpy(filedata, "This is a test data.\n");

    entry = create_proc_entry("filename", 0, 0); /* "filename" registration */
    entry->read_proc = filename_read; /* read routine */
    entry->write_proc = filename_write; /* write routine */

    entry = create_proc_entry("filedata", 0, 0); /* "filedata" registration */
    entry->read_proc = filedata_read; /* read routine */
    entry->write_proc = filedata_write; /* write routine */
}

static void file_remove_proc(void)
{
    remove_proc_entry("filedata", NULL);
    remove_proc_entry("filename", NULL);
}

int __init file_init_module(void)
{
    file_create_proc();
    file_dbg("init file");
    return 0;
}

void __exit file_cleanup_module(void)
{
    file_dbg("cleanup file");
    file_remove_proc();
}

MODULE_DESCRIPTION("FILE IO from kernel space");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Device Drivers Limited");

module_init(file_init_module);
module_exit(file_cleanup_module);
