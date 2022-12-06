#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("abin");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.01");

static int __init lkm_example_init(void) {
 printk(KERN_INFO "Hello, World!\n");
 asm  volatile(“movl %%cr3, %0”:”=r”(iValue)); // 输出观察寄存器的值;
 return 0;
}
static void __exit lkm_example_exit(void) {
 printk(KERN_INFO "Goodbye, World!\n");
}

module_init(lkm_example_init);
module_exit(lkm_example_exit); 