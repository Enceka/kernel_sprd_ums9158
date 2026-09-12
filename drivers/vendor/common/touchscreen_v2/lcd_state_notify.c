
#include <linux/notifier.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>

#ifdef CONFIG_TOUCHSCREEN_LCD_NOTIFY
static BLOCKING_NOTIFIER_HEAD(lcd_notifier_list);
static BLOCKING_NOTIFIER_HEAD(tpd_notifier_list);

/* ####################################################*/
int lcd_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lcd_notifier_list, nb);
}
EXPORT_SYMBOL(lcd_notifier_register_client);

int lcd_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lcd_notifier_list, nb);
}
EXPORT_SYMBOL(lcd_notifier_unregister_client);


int lcd_notifier_call_chain(unsigned long val)
{
	return blocking_notifier_call_chain(&lcd_notifier_list, val, NULL);
}
EXPORT_SYMBOL(lcd_notifier_call_chain);

/* ####################################################*/
int tpd_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&tpd_notifier_list, nb);
}
EXPORT_SYMBOL(tpd_notifier_register_client);

int tpd_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&tpd_notifier_list, nb);
}
EXPORT_SYMBOL(tpd_notifier_unregister_client);

int tpd_notifier_call_chain(unsigned long val)
{
	return blocking_notifier_call_chain(&tpd_notifier_list, val, NULL);
}
EXPORT_SYMBOL_GPL(tpd_notifier_call_chain);

/*
 * Upstream builds this file as part of a built-in (bool) touchscreen stack, so
 * its module_init/module_exit just became extra initcalls.  Here the whole
 * framework is one module (tlsc6x.ko) and a module can only have one of each,
 * so they are dropped - they only printed a line - along with the duplicate
 * MODULE_* macros, which ztp_core.c already provides.
 */
#endif

