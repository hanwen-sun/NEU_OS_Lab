obj-m += mymodule.o   # 表示将该可执行文件编译进内核;
CURRENT_PATH = $(shell pwd)# 模块所在当前路径;
Linux_Kernel = $(shell uname -r)# linux内核代码的当前版本;
# make -C:  make执行子文件中的makefile(即源代码目录下)
# M= 让构造module目标之前返回当前目录
# 作用就说将当前编写的module加入kernel;

all:
	make -C /lib/modules/$(Linux_Kernel)/build M=$(CURRENT_PATH) modules
clean:
	make -C /lib/modules/$(Linux_Kernel)/build M=$(CURRENT_PATH) clean

test:
	sudo dmesg -C   # 清除缓存信息
	sudo insmod mymodule.ko
	lsmod | grep "mymodule"
	sudo rmmod mymodule.ko
	dmesg

test_device:
	sudo dmesg -C   # 清除缓存信息
	sudo insmod mymodule.ko
	lsmod | grep "mymodule"
	dmesg
	sudo mknod /dev/chardev c 237 0
	cat /dev/chardev
	sudo rm /dev/chardev
	sudo rmmod mymodule.ko
	dmesg

register:
	sudo dmesg -C   # 清除缓存信息
	sudo insmod mymodule.ko
	sudo mknod /dev/chardev c 237 0
	dmesg
	gcc test.c -o test
	./test
	sudo rm /dev/chardev
	sudo rmmod mymodule.ko
	rm -rf test
	dmesg

	