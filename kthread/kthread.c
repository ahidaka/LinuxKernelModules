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
#include <linux/sysctl.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/unistd.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include<linux/mm.h>
#include<asm/semaphore.h>
#include<linux/ioctl.h>
#include<linux/types.h>
#include<linux/seq_file.h>
#include<linux/smp_lock.h>
#include<linux/delay.h>
#include<linux/locks.h>
#include<linux/kernel_stat.h>
#include<linux/completion.h>
#include<asm/io.h>
#include<asm/bitops.h>

#define BUFFER_SIZE 12
#if (PAGE_SIZE < BUF_SIZE)
#define BUFFER_SIZE PAGE_SIZE   /* for not enough page size system */
#endif

static unsigned long kth_cmd = 0;
static int debug;

DECLARE_WAIT_QUEUE_HEAD (kth_wait);

static struct clientdata {
	unsigned long jiffies;
	task_queue *queue;
} kth_data;

static struct timer_list kth_timer;

static void kth_timedout(unsigned long ptr)
{
	if (debug) {
		printk("** kth_timedout\n");
	}
        wake_up_interruptible(&kth_wait);  /* awake the process */
}

static unsigned int kth_sleep(unsigned long ms)
{
	if (debug) {
		printk("** kth_sleep strat\n");
	}
        kth_data.jiffies = jiffies;
        kth_data.queue = NULL;      /* don't requeue */

        init_timer(&kth_timer);              /* init the timer structure */
        kth_timer.function = kth_timedout;

        printk("**init_timer & timer_function done = %ld ms\n", ms);

        kth_timer.data = (unsigned long)&kth_data;
        /* kth_timer.expires = jiffies + HZ; * one second */
        kth_timer.expires = jiffies + (ms / 10); /* ms millisecond */

        add_timer(&kth_timer);
        interruptible_sleep_on(&kth_wait);
        del_timer_sync(&kth_timer);  /* in case a signal woke us up */

	if (debug) {
		printk("** end of kth_sleep **\n");
	}
        return jiffies;
}

/*
 * kernel thread
 */
typedef struct kth_thread_s {
        void                    (*run) (void *data);
        void                    *data;
        wait_queue_head_t    wqueue;
        unsigned long           flags;
        struct completion       *event;
        struct task_struct      *tsk;
        const char              *name;
} kth_thread_t;

static inline void kth_init_signals (void)
{
        current->exit_signal = SIGCHLD;
        siginitsetinv(&current->blocked, sigmask(SIGKILL));
}

static inline void kth_flush_signals (void)
{
        spin_lock(&current->sigmask_lock);
        flush_signals(current);
        spin_unlock(&current->sigmask_lock);
}

static kth_thread_t *kth_sleep_thread;

#define THREAD_WAKEUP  0

/*
 * thread main
 */
void kth_do_sleep(void *data)
{
	int i;
	long sec = (long) data;

        printk("*** KTH sleep thread got woken up ...\n");

	for(i = 1; i < 10; i++) {
		printk("** %d: thread ON, sec = %ld\n", i, sec);
		kth_sleep(sec * 1000);
	}
        printk("*** KTH sleep thread finished ...\n");
}

int kth_thread(void *arg)
{
        kth_thread_t *thread = arg;

        lock_kernel();
        daemonize();
        sprintf(current->comm, thread->name);
        kth_init_signals();
        kth_flush_signals();

        thread->tsk = current;
        current->policy = SCHED_OTHER;
        /* current->nice = -20; */
        unlock_kernel();
        complete(thread->event);

        while (thread->run) {
                void (*run)(void *data);
                DECLARE_WAITQUEUE(wait, current);

                add_wait_queue(&thread->wqueue, &wait);
                set_task_state(current, TASK_INTERRUPTIBLE);
                if (!test_bit(THREAD_WAKEUP, &thread->flags)) {
                        printk("*** thread %p went to sleep.\n", thread);
                        schedule();
                        printk("*** thread %p woke up.\n", thread);
                }
                current->state = TASK_RUNNING;
                remove_wait_queue(&thread->wqueue, &wait);
                clear_bit(THREAD_WAKEUP, &thread->flags);

                run = thread->run;
                if (run) {
                        run(thread->data);
                        run_task_queue(&tq_disk);
                }
                if (signal_pending(current))
                        kth_flush_signals();
        }
        complete(thread->event);

	return(0);
}

/*
 * support routines
 */
void kth_wakeup_thread(kth_thread_t *thread)
{
        printk("** waking up KTH thread %p.\n", thread);
        set_bit(THREAD_WAKEUP, &thread->flags);
        wake_up(&thread->wqueue);
}

kth_thread_t *kth_register_thread(void (*run) (void *),
				void *data, const char *name)
{
	int ret;
	kth_thread_t *thread;
        struct completion event;

        thread = (kth_thread_t *) kmalloc(sizeof(kth_thread_t), GFP_KERNEL);
        if (!thread)
                return NULL;

        memset(thread, 0, sizeof(kth_thread_t));
        init_waitqueue_head(&thread->wqueue);

	init_completion(&event);
	thread->event=&event;
	thread->run=run;
	thread->data=data;
	thread->name=name;

        ret = kernel_thread(kth_thread, thread, 0);
        if (ret < 0) {
                kfree(thread);
                return NULL;
        }

        wait_for_completion(&event);
	return(thread);
}

void kth_interrupt_thread(kth_thread_t *thread)
{
        if (!thread->tsk) {
                printk("** interrupt error\n");
                return;
        }
        printk("** interrupting KTH-thread pid %d\n", thread->tsk->pid);
        send_sig(SIGKILL, thread->tsk, 1);
}

void kth_unregister_thread(kth_thread_t *thread)
{
        struct completion event;

        init_completion(&event);

        thread->event = &event;
        thread->run = NULL;
        thread->name = NULL;
        kth_interrupt_thread(thread);
        wait_for_completion(&event);

        kfree(thread);
}

/*
 * kth_read_proc -- called when user reading
 */
int kth_read_proc(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
	char in_data[BUFFER_SIZE];
	unsigned long req_cmd = 0;
	int return_length = 0;

	printk("**read_proc(), count = %d, off = %d\n",
	       count, (int) offset);
	if (offset == 0) {

		req_cmd = kth_cmd;

		if (req_cmd > 0 && req_cmd < 20) {
			(void) kth_sleep(req_cmd * 1000);
		}
		memset(in_data, 0, BUFFER_SIZE);
		return_length = sprintf(in_data, "%lu\n", req_cmd);
		memcpy(buf, in_data, return_length);
		printk("**read_proc(), cmd = %lu\n", simple_strtoul(in_data, NULL, 10));
	}

	kth_interrupt_thread(kth_sleep_thread);

	*eof = 1;
	*start = buf + offset;
	return return_length;
}

/*
 * kth_write_proc -- called when user writing
 */
int kth_write_proc(struct file *file, const char *buf,
                     unsigned long count, void *data)
{
	char out_data[BUFFER_SIZE];
	int length;
	char *p;
 
	printk("**write_proc(), count = %d\n", (int) count);

	length = count > BUFFER_SIZE ? BUFFER_SIZE : count; /* limit of max size */
	   
	copy_from_user(out_data, buf, length);

	p = &out_data[0];
	kth_cmd = simple_strtoul(p, &p, 10);

	printk("write_file(), cmd = %ld\n",
	       kth_cmd);

	kth_wakeup_thread(kth_sleep_thread);

	return length;
}

/*
 *
 */
static void kth_create_proc(void)
{
	struct proc_dir_entry *entry;
	entry = create_proc_entry("kthread", 0, 0);
	entry->read_proc = kth_read_proc; /* read routine */
	entry->write_proc = kth_write_proc; /* write routine */

	kth_sleep_thread = kth_register_thread(kth_do_sleep, (void *) 5, "kth_sleep");
}

static void kth_remove_proc(void)
{
	kth_unregister_thread(kth_sleep_thread);

	remove_proc_entry("kthread", NULL);
}

int __init kth_init_module(void)
{
	kth_create_proc();
	printk("<1>KTH, init\n");
	return 0;
}

void __exit kth_cleanup_module(void)
{
	printk("<1>KTH cleanup\n");
	kth_remove_proc();
}

MODULE_DESCRIPTION("KTH Module");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Device Drivers Limited");
MODULE_PARM(debug,       "i");
MODULE_PARM_DESC(debug, "KTH debug level (0-6)");

module_init(kth_init_module);
module_exit(kth_cleanup_module);
