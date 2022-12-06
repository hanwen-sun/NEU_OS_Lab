#include<linux/module.h>
#include<linux/moduleparam.h>
#include<linux/cdev.h>
#include<linux/fs.h>
#include<linux/wait.h>
#include<linux/poll.h>
#include<linux/sched.h>
#include<linux/slab.h>

#define BUFFER_MAX (10)
#define OK (0)
#define ERROR (-1)
#define DEVICE_NAME "mymodule" 

dev_t devNum;
unsigned int conseDevNum=1;
int reg_major=232;
int reg_minor=0;
int major_num=0;

static char msg_buffer[BUFFER_MAX];
static char *msg_ptr; 
struct cdev *gDev;
struct file_operations *gFile;

static ssize_t read_mymodule (struct file *, char __user *, size_t, loff_t *);
static ssize_t write_mymodule (struct file *, const char __user *, size_t, loff_t *);
int open_mymodule (struct inode *, struct file *);
int close_mymodule (struct inode *, struct file *);

static struct file_operations file_ops = {
	.read = read_mymodule,
	.write = write_mymodule,
	.open = open_mymodule,
	.release = close_mymodule
};

ssize_t read_mymodule (struct file * f, char __user * u, size_t s, loff_t * l)
{
	/*
		提示信息开始读取设备字符
		判断读取长度与BUFF大小
		使用copy_to_user
		返回读取长度
	*/
	printk("begin read!\n");
	int bytes_read = 0;
	if (*msg_ptr == 0) {
   		msg_ptr = msg_buffer;
  	}

	int len = 10;
	int i = 0;
	for(i = 0; i < len; i++)
		msg_buffer[i] = 7;
	
	while(len && *msg_ptr) {
		put_user(*(msg_ptr++), u++);
		len--;
		bytes_read++;
	}
	// char *b = buffer;

	return bytes_read;
}
ssize_t write_mymodule (struct file * f, const char __user * u, size_t s, loff_t * l)
{/*
	
		提示信息开始写入设备字符
		判断写入长度与BUFF大小
		使用copy_from_user
*/	
	printk("begin write!\n");
	int bytes_write = 0;
	if(s > BUFFER_MAX) {
		printk("it's out of buffer size!\n");
		s = 10;
	}

	// printk("copy length: %d\n", s);
	int k = copy_from_user(&msg_buffer, u, s);
	if(k != 0)
		return ERROR;

	int i = 0;
	for(i = 0; i < BUFFER_MAX; i++) {
		printk("kernel receive: No.%d character is: %d\n", i + 1, msg_buffer[i]);
	}

	return bytes_write;
}

int open_mymodule (struct inode * p , struct file * f)
{
	/*  
		判断设备的使用情况，有进程占用返回打开失败
		无进程使用，资源加一
		使用semaphore完成对临界资源的保护
	*/
	printk(KERN_INFO"mymodule device open\n");
	//printk("Device:%d.%d\n",MAJOR(inode->i_rdev),MINIOR(inode->i_rdev));

	return 0;
}
int close_mymodule (struct inode * p, struct file * f)
{
	/*
		退出前释放资源
	*/
	printk(KERN_INFO"mymodule device close\n");
	return 0;
}


int __init init_mymodule(void)
{
	/*
		生成设备号
		注册设备
		为数据结构申请内存
		初始化
		链接接口函数
	*/
	printk(KERN_INFO"Begin init %s\n",DEVICE_NAME);
	msg_ptr = msg_buffer;

    static volatile unsigned long r_eax, r_ebx, r_ecx; 
    asm ("movl %%eax, %0\n" 
    "movl %%ebx, %1\n"
    "movl %%ecx, %2\n"
    : "=m"(r_eax), "=m"(r_ebx), "=m"(r_ecx)); 
    printk("r_eax: %ld, r_ebx: %ld, r_ecx: %ld", r_eax, r_ebx, r_ecx);

	major_num = register_chrdev(0, DEVICE_NAME, &file_ops);   // 将模块注册为字符设备, 保存显示设备的主设备号;
  	if (major_num < 0) {
   		printk(KERN_ALERT "Could not register device: %d\n", major_num);
   		return major_num;
  	} else {
   		printk(KERN_INFO "mymodule loaded with device major number %d\n", major_num);
		printk("if you want to talk to the device driver, I suggest you use:\n");
		printk("mknod <name> c %d <minor>", major_num);
   		return 0;
  	} 

	return 0;
}
void __exit release_mymodule(void)
{
	/*
		注销设备数据
	*/
	unregister_chrdev(major_num, DEVICE_NAME); 
	printk(KERN_INFO"Module release\n");
	return;
}
module_init(init_mymodule);
module_exit(release_mymodule);
MODULE_LICENSE("GPL");
MODULE_VERSION("0.01");

