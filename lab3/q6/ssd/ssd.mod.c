#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x25cfd397, "module_layout" },
	{ 0x3ec8886f, "param_ops_int" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0xc5e35f9c, "cdev_del" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0x89e07698, "cdev_add" },
	{ 0xe663280e, "cdev_init" },
	{ 0xd8e484f0, "register_chrdev_region" },
	{ 0x2f287f0d, "copy_to_user" },
	{ 0x57b09822, "up" },
	{ 0x362ef408, "_copy_from_user" },
	{ 0x670c0597, "down_interruptible" },
	{ 0x50eedeb8, "printk" },
	{ 0xb4390f9a, "mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "B49752B4A0A40BE91584AFB");
