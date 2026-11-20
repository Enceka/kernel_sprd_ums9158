/*
 * Spreadtrum pmic watchdog driver
 * Copyright (C) 2020 Spreadtrum - http://www.spreadtrum.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/alarmtimer.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/sipc.h>
#include <uapi/linux/sched/types.h>

#define SPRD_PMIC_WDT_LOAD_LOW		0x0
#define SPRD_PMIC_WDT_LOAD_HIGH		0x4
#define SPRD_PMIC_WDT_CTRL		0x8
#define SPRD_PMIC_WDT_INT_CLR		0xc
#define SPRD_PMIC_WDT_INT_RAW		0x10
#define SPRD_PMIC_WDT_INT_MSK		0x14
#define SPRD_PMIC_WDT_CNT_LOW		0x18
#define SPRD_PMIC_WDT_CNT_HIGH		0x1c
#define SPRD_PMIC_WDT_LOCK			0x20
#define SPRD_PMIC_WDT_IRQ_LOAD_LOW		0x2c
#define SPRD_PMIC_WDT_IRQ_LOAD_HIGH		0x30

/* WDT_CTRL */
#define SPRD_PMIC_WDT_INT_EN_BIT		BIT(0)
#define SPRD_PMIC_WDT_CNT_EN_BIT		BIT(1)
#define SPRD_PMIC_WDT_NEW_VER_EN		BIT(2)
#define SPRD_PMIC_WDT_RST_EN_BIT		BIT(3)

/* WDT_INT_CLR */
#define SPRD_PMIC_WDT_INT_CLEAR_BIT		BIT(0)
#define SPRD_PMIC_WDT_RST_CLEAR_BIT		BIT(3)

/* WDT_INT_RAW */
#define SPRD_PMIC_WDT_INT_RAW_BIT		BIT(0)
#define SPRD_PMIC_WDT_RST_RAW_BIT		BIT(3)
#define SPRD_PMIC_WDT_LD_BUSY_BIT		BIT(4)

/* pmic wdt eb register */
#define SC2731_WDT_EB		0x0c08
#define SC2731_WDT_RTC_EB	0x0c10
#define SC2731_WDT_EN		BIT(2)
#define SC2731_WDT_RTC_EN	BIT(2)
#define SC2730_WDT_EB		0x1808
#define SC2730_WDT_RTC_EB	0x1810
#define SC2730_WDT_EN		BIT(2)
#define SC2730_WDT_RTC_EN	BIT(2)
#define SC2721_WDT_EB		0x0c08
#define SC2721_WDT_RTC_EB	0x0c10
#define SC2721_WDT_EN		BIT(2)
#define SC2721_WDT_RTC_EN	BIT(2)
#define SC2720_WDT_EB		0x0c08
#define SC2720_WDT_RTC_EB	0x0c10
#define SC2720_WDT_EN		BIT(2)
#define SC2720_WDT_RTC_EN	BIT(2)
#define ump9620_WDT_EB		0x2008
#define ump9620_WDT_RTC_EB	0x2010
#define ump9620_WDT_EN		BIT(2)
#define ump9620_WDT_RTC_EN	BIT(2)

/* 1s equal to 32768 counter steps */
#define SPRD_PMIC_WDT_CNT_STEP		32768

#define SPRD_PMIC_WDT_UNLOCK_KEY		0xe551
#define SPRD_PMIC_WDT_MIN_TIMEOUT		3
#define SPRD_PMIC_WDT_MAX_TIMEOUT		60

#define SPRD_PMIC_WDT_CNT_HIGH_SHIFT		16
#define SPRD_PMIC_WDT_LOW_VALUE_MASK		GENMASK(15, 0)
#define SPRD_PMIC_WDT_LOAD_TIMEOUT		1000

#define SPRD_PMIC_WDT_TIMEOUT		60
#define SPRD_PMIC_WDT_PRETIMEOUT	0
#define SPRD_PMIC_WDT_FEEDTIME		45
#define SPRD_PMIC_WDTEN_MAGIC "e551"
#define SPRD_PMIC_WDTEN_MAGIC_LEN_MAX  10
#define SPRD_DSWDTEN_MAGIC "enabled"
#define SPRD_DSWDTEN_MAGIC_LEN_MAX  10
#define PMIC_WDT_WAKE_UP_MS 2000
#define RETRY_CNT_MAX		3
#define SPRD_PMIC_WDT_LOAD_VAULE_HIGH	0x96

#define SPRD_WDT_EN	0
#define SPRD_DSWDT_EN	1

static const char *const pmic_wdt_info[] = {"wdten", "dswdten"};

struct sprd_pmic_wdt {
	struct regmap		*regmap;
	struct device		*dev;
	u32			base;
	bool wdten;
	bool sleep_en;
	bool normal_mode;
	bool cm4_owned;
	bool stopping;
	unsigned long sbuf_events;
	struct alarm wdt_timer;
	struct kthread_worker wdt_kworker;
	struct kthread_delayed_work wdt_kwork;
	struct task_struct *wdt_thread;
	struct task_struct *feed_task;
	int wdt_enabled;
	u32 wdt_flag;
	u32 wdt_cmd;
	u32 wdt_cmd_offset;
	u32 wdt_ctrl;
	u64 wdt_load;
	struct mutex *lock;
	const struct sprd_pmic_wdt_data *data;
};

#ifndef CONFIG_SPRD_DEBUG
static int pmic_timeout = 300;
static int feed_period = 250;
#endif

static DEFINE_MUTEX(sprd_wdt_mutex);

struct sprd_pmic_wdt_data {
	u32 wdt_eb_reg;
	u32 wdt_rtc_eb_reg;
	u32 wdt_en;
	u32 wdt_rtc_en;
};

static struct sprd_pmic_wdt_data sc2731_data = {
	.wdt_eb_reg = SC2731_WDT_EB,
	.wdt_rtc_eb_reg = SC2731_WDT_RTC_EB,
	.wdt_en = SC2731_WDT_EN,
	.wdt_rtc_en = SC2731_WDT_RTC_EN,
};

static struct sprd_pmic_wdt_data sc2730_data = {
	.wdt_eb_reg = SC2730_WDT_EB,
	.wdt_rtc_eb_reg = SC2730_WDT_RTC_EB,
	.wdt_en = SC2730_WDT_EN,
	.wdt_rtc_en = SC2730_WDT_RTC_EN,
};

static struct sprd_pmic_wdt_data sc2721_data = {
	.wdt_eb_reg = SC2721_WDT_EB,
	.wdt_rtc_eb_reg = SC2721_WDT_RTC_EB,
	.wdt_en = SC2721_WDT_EN,
	.wdt_rtc_en = SC2721_WDT_RTC_EN,
};

static struct sprd_pmic_wdt_data sc2720_data = {
	.wdt_eb_reg = SC2720_WDT_EB,
	.wdt_rtc_eb_reg = SC2720_WDT_RTC_EB,
	.wdt_en = SC2720_WDT_EN,
	.wdt_rtc_en = SC2720_WDT_RTC_EN,
};

static struct sprd_pmic_wdt_data ump9620_data = {
	.wdt_eb_reg = ump9620_WDT_EB,
	.wdt_rtc_eb_reg = ump9620_WDT_RTC_EB,
	.wdt_en = ump9620_WDT_EN,
	.wdt_rtc_en = ump9620_WDT_RTC_EN,
};


static int sprd_pmic_wdt_on(struct sprd_pmic_wdt *pmic_wdt)
{
	int nwrite, len, retry_cnt, ret, read_ret;
	int timeout = 100;
	char *p_cmd;
	u32 val, ctrl, rtc;

	while (pmic_wdt->wdt_cmd < ARRAY_SIZE(pmic_wdt_info)) {
		if (pmic_wdt->wdt_cmd == SPRD_WDT_EN) {
			if (pmic_wdt->wdten)
				p_cmd = "watchdog on";
			else
				p_cmd = "watchdog rstoff";
		} else {
			if (pmic_wdt->sleep_en)
				p_cmd = "dswdt on";
			else
				p_cmd = "dswdt off";
		}
		len = strlen(p_cmd) + 1;
		ret = -ETIMEDOUT;
		for (retry_cnt = 0; retry_cnt < RETRY_CNT_MAX; retry_cnt++) {
			if (READ_ONCE(pmic_wdt->stopping))
				return -ECANCELED;
			if (test_bit(SBUF_NOTIFY_READY, &pmic_wdt->sbuf_events))
				return -EAGAIN;

			pm_wakeup_event(pmic_wdt->dev, PMIC_WDT_WAKE_UP_MS);
			if (pmic_wdt->wdt_cmd_offset < len) {
				nwrite = sbuf_write(SIPC_ID_PM_SYS, SMSG_CH_TTY, 0,
						    p_cmd + pmic_wdt->wdt_cmd_offset,
						    len - pmic_wdt->wdt_cmd_offset,
						    msecs_to_jiffies(timeout));
				pr_err("cm4 watchdog on/off: len = %d, nwrite = %d\n", len, nwrite);
				if (nwrite < 0) {
					ret = nwrite;
				} else {
					pmic_wdt->wdt_cmd_offset += nwrite;
					ret = pmic_wdt->wdt_cmd_offset == len ? 0 : -EIO;
				}
				if (ret)
					dev_warn(pmic_wdt->dev,
						 "%s write failed: %d, sent %u/%d, retry %d\n",
						 pmic_wdt_info[pmic_wdt->wdt_cmd], ret,
						 pmic_wdt->wdt_cmd_offset, len, retry_cnt + 1);
			}
			msleep(1000);
			read_ret = regmap_read(pmic_wdt->regmap,
					       pmic_wdt->base + SPRD_PMIC_WDT_LOAD_HIGH,
					       &val);
			if (read_ret) {
				ret = read_ret;
				dev_warn(pmic_wdt->dev, "%s load read failed: %d, retry %d\n",
					 pmic_wdt_info[pmic_wdt->wdt_cmd], ret, retry_cnt + 1);
				continue;
			}
			if (pmic_wdt->wdt_cmd_offset == len &&
			    val != SPRD_PMIC_WDT_LOAD_VAULE_HIGH) {
				if (pmic_wdt->wdt_cmd == SPRD_WDT_EN) {
					ret = regmap_read(pmic_wdt->regmap,
							  pmic_wdt->base + SPRD_PMIC_WDT_CTRL,
							  &ctrl);
					if (ret)
						continue;
					ret = regmap_read(pmic_wdt->regmap,
							  pmic_wdt->data->wdt_rtc_eb_reg, &rtc);
					if (ret)
						continue;
					if ((pmic_wdt->wdten &&
					     (!(ctrl & SPRD_PMIC_WDT_CNT_EN_BIT) ||
					      !(ctrl & SPRD_PMIC_WDT_RST_EN_BIT) ||
					      !(rtc & pmic_wdt->data->wdt_rtc_en))) ||
					    (!pmic_wdt->wdten &&
					     (ctrl & SPRD_PMIC_WDT_RST_EN_BIT))) {
						pmic_wdt->wdt_cmd_offset = 0;
						ret = -ETIMEDOUT;
						continue;
					}
					pmic_wdt->cm4_owned = true;
				}
				dev_info(pmic_wdt->dev, "%s command complete, load high = %#x\n",
					 pmic_wdt_info[pmic_wdt->wdt_cmd], val);
				pmic_wdt->wdt_cmd++;
				pmic_wdt->wdt_cmd_offset = 0;
				ret = 0;
				break;
			}
			if (pmic_wdt->wdt_cmd_offset == len) {
				pmic_wdt->wdt_cmd_offset = 0;
				ret = -ETIMEDOUT;
			}
		}
		if (ret)
			return ret;
	}

	if (READ_ONCE(pmic_wdt->stopping))
		return -ECANCELED;
	if (test_bit(SBUF_NOTIFY_READY, &pmic_wdt->sbuf_events))
		return -EAGAIN;
	WRITE_ONCE(pmic_wdt->wdt_flag, SPRD_PMIC_WDT_UNLOCK_KEY);
	dev_info(pmic_wdt->dev, "CM4 watchdog commands complete\n");

	return 0;
}

static void sprd_pmic_wdt_init(int event, void *data)
{
	struct sprd_pmic_wdt *pmic_wdt = data;

	if (READ_ONCE(pmic_wdt->stopping))
		return;

	switch (event) {
	case SBUF_NOTIFY_READY:
		dev_info(pmic_wdt->dev, "sbuf ready for pmic wdt init!\n");
		set_bit(SBUF_NOTIFY_READY, &pmic_wdt->sbuf_events);
		break;
	case SBUF_NOTIFY_READ:
		if (READ_ONCE(pmic_wdt->wdt_flag) == SPRD_PMIC_WDT_UNLOCK_KEY)
			return;
		break;
	default:
		return;
	}
	pm_wakeup_event(pmic_wdt->dev, PMIC_WDT_WAKE_UP_MS);
	kthread_queue_delayed_work(&pmic_wdt->wdt_kworker, &pmic_wdt->wdt_kwork, 0);
}

static void sprd_pmic_wdt_work(struct kthread_work *work)
{
	struct sprd_pmic_wdt *pmic_wdt = container_of(work,
						 struct sprd_pmic_wdt,
						 wdt_kwork.work);
	struct task_struct *feed_task = NULL;
	int ret;

	mutex_lock(pmic_wdt->lock);
	if (READ_ONCE(pmic_wdt->stopping))
		goto out;
	if (test_and_clear_bit(SBUF_NOTIFY_READY, &pmic_wdt->sbuf_events)) {
		pmic_wdt->wdt_cmd = 0;
		pmic_wdt->wdt_cmd_offset = 0;
		WRITE_ONCE(pmic_wdt->wdt_flag, 0);
	}
	if (pmic_wdt->wdt_flag == SPRD_PMIC_WDT_UNLOCK_KEY)
		goto out;

	ret = sprd_pmic_wdt_on(pmic_wdt);
	if (pmic_wdt->cm4_owned) {
		feed_task = pmic_wdt->feed_task;
		pmic_wdt->feed_task = NULL;
	}
	if (ret && !READ_ONCE(pmic_wdt->stopping)) {
		dev_err(pmic_wdt->dev, "CM4 watchdog command %u failed: %d, retrying\n",
			pmic_wdt->wdt_cmd, ret);
		kthread_queue_delayed_work(&pmic_wdt->wdt_kworker, &pmic_wdt->wdt_kwork,
					   msecs_to_jiffies(PMIC_WDT_WAKE_UP_MS));
	}
out:
	mutex_unlock(pmic_wdt->lock);
	if (feed_task)
		kthread_stop(feed_task);
}

static bool sprd_pmic_wdt_en(const char *wdten_name)
{
	struct device_node *cmdline_node;
	const char *cmd_line, *wdten_name_p;
	char wdten_value[SPRD_PMIC_WDTEN_MAGIC_LEN_MAX] = "NULL";
	int ret, i;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);

	if (ret) {
		pr_err("sprd_pmic_wdt can't parse bootargs property\n");
		return false;
	}

	for (i = 0; i < ARRAY_SIZE(pmic_wdt_info); i++) {
		ret = strncmp(wdten_name, pmic_wdt_info[i], strlen(pmic_wdt_info[i]));
		if (!ret) {
			switch (i) {
			case SPRD_WDT_EN:
				wdten_name_p = strstr(cmd_line, "sprdboot.wdten=");
				if (!wdten_name_p) {
					pr_err("sprd_pmic_wdt can't find sprdboot.wdten\n");
					return false;
				}
				sscanf(wdten_name_p, "sprdboot.wdten=%8s", wdten_value);
				if (strncmp(wdten_value, SPRD_PMIC_WDTEN_MAGIC,
					    strlen(SPRD_PMIC_WDTEN_MAGIC)))
					return false;
				break;
			case SPRD_DSWDT_EN:
				wdten_name_p = strstr(cmd_line, "sprdboot.dswdten=");
				if (!wdten_name_p) {
					pr_err("sprd_pmic_wdt can't find sprdboot.dswdten\n");
					return false;
				}
				sscanf(wdten_name_p, "sprdboot.dswdten=%8s", wdten_value);
				if (strncmp(wdten_value, SPRD_DSWDTEN_MAGIC,
					    strlen(SPRD_DSWDTEN_MAGIC)))
					return false;
				break;
			default:
				return false;
			}
		}
	}

	return true;
}

#ifndef CONFIG_SPRD_DEBUG
static bool sprd_pmic_wdt_get_normal_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmdline;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		pr_err("Can't parse bootargs\n");
		return false;
	}

	if (strstr(cmdline, "sprdboot.mode=normal"))
		return true;
	else if (strstr(cmdline, "sprdboot.mode=alarm"))
		return true;
	else
		return false;
}

static inline void sprd_pmic_wdt_lock(struct sprd_pmic_wdt *pmic_wdt)
{
	regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_LOCK, 0);
}

static inline void sprd_pmic_wdt_unlock(struct sprd_pmic_wdt *pmic_wdt)
{
	regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_LOCK,
		     SPRD_PMIC_WDT_UNLOCK_KEY);
}

static int sprd_pmic_wdt_load_value(struct sprd_pmic_wdt *pmic_wdt, u32 timeout)
{
	u32 val, delay_cnt = 0;
	u32 tmr_step = timeout * SPRD_PMIC_WDT_CNT_STEP;
	int ret;

	pr_err("sprd pmic wdt:pmic_timeout %d,feed %d\n", timeout, feed_period);

	/*
	 * Waiting the load value operation done,
	 * it needs two or three RTC clock cycles.
	 */
	do {
		ret = regmap_read(pmic_wdt->regmap,
				  pmic_wdt->base + SPRD_PMIC_WDT_INT_RAW, &val);
		if (ret)
			return ret;
		if (!(val & SPRD_PMIC_WDT_LD_BUSY_BIT))
			break;

		cpu_relax();
	} while (delay_cnt++ < SPRD_PMIC_WDT_LOAD_TIMEOUT);

	if (delay_cnt >= SPRD_PMIC_WDT_LOAD_TIMEOUT) {
		pr_err("sprd pmic wdt check busy bit timeout!\n");
		return -EBUSY;
	}

	sprd_pmic_wdt_unlock(pmic_wdt);
	ret = regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_LOAD_HIGH,
			   (tmr_step >> SPRD_PMIC_WDT_CNT_HIGH_SHIFT) &
			   SPRD_PMIC_WDT_LOW_VALUE_MASK);
	if (ret)
		goto out_lock;
	ret = regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_LOAD_LOW,
			   tmr_step & SPRD_PMIC_WDT_LOW_VALUE_MASK);
out_lock:
	sprd_pmic_wdt_lock(pmic_wdt);
	if (ret)
		return ret;
	pmic_wdt->wdt_load = jiffies;

	return 0;
}

static int sprd_pmic_wdt_start(struct sprd_pmic_wdt *pmic_wdt)
{
	u32 val;
	int ret;

	ret = sprd_pmic_wdt_load_value(pmic_wdt, pmic_timeout);
	if (ret)
		return ret;
	sprd_pmic_wdt_unlock(pmic_wdt);
	ret = regmap_read(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_CTRL, &val);
	if (ret)
		goto out_lock;
	val |= SPRD_PMIC_WDT_CNT_EN_BIT | SPRD_PMIC_WDT_RST_EN_BIT;
	ret = regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_CTRL, val);
	if (ret)
		goto out_lock;
	ret = regmap_read(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_CTRL,
			  &pmic_wdt->wdt_ctrl);
out_lock:
	sprd_pmic_wdt_lock(pmic_wdt);

	return ret;
}

static int sprd_pmic_wdt_enable(struct sprd_pmic_wdt *pmic_wdt)
{
	u32 val;
	int ret;

	ret = regmap_update_bits(pmic_wdt->regmap, pmic_wdt->data->wdt_eb_reg,
				 pmic_wdt->data->wdt_en, pmic_wdt->data->wdt_en);
	if (ret)
		return ret;
	ret = regmap_update_bits(pmic_wdt->regmap, pmic_wdt->data->wdt_rtc_eb_reg,
				 pmic_wdt->data->wdt_rtc_en, pmic_wdt->data->wdt_rtc_en);
	if (ret)
		return ret;

	sprd_pmic_wdt_unlock(pmic_wdt);
	ret = regmap_read(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_CTRL, &val);
	if (ret)
		goto out_lock;
	val |= SPRD_PMIC_WDT_NEW_VER_EN;
	ret = regmap_write(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_CTRL, val);
out_lock:
	sprd_pmic_wdt_lock(pmic_wdt);
	if (ret)
		return ret;
	pmic_wdt->wdt_enabled = 1;

	return 0;
}

static int pmic_wdt_kick(struct sprd_pmic_wdt *pmic_wdt)
{
	u32 val;
	int ret;

	ret = regmap_read(pmic_wdt->regmap, pmic_wdt->base + SPRD_PMIC_WDT_LOAD_HIGH,
			  &val);
	if (ret)
		return ret;
	if (val != SPRD_PMIC_WDT_LOAD_VAULE_HIGH)
		return 0;

	return sprd_pmic_wdt_load_value(pmic_wdt, pmic_timeout);
}

static int sprd_wdt_feeder(void *data)
{
	struct sprd_pmic_wdt *pmic_wdt = data;
	unsigned long delay;
	int ret;

	while (!kthread_should_stop()) {
		delay = feed_period * HZ;
		mutex_lock(pmic_wdt->lock);
		if (pmic_wdt->wdt_enabled && !pmic_wdt->cm4_owned &&
		    !READ_ONCE(pmic_wdt->stopping)) {
			ret = pmic_wdt_kick(pmic_wdt);
			if (ret) {
				dev_err_ratelimited(pmic_wdt->dev, "failed to feed pmic wdt: %d\n",
						    ret);
				delay = msecs_to_jiffies(PMIC_WDT_WAKE_UP_MS);
			}
		}
		mutex_unlock(pmic_wdt->lock);

		set_current_state(TASK_INTERRUPTIBLE);
		if (!kthread_should_stop())
			schedule_timeout(delay);
		__set_current_state(TASK_RUNNING);
	}

	return 0;
}

static int sprd_wdt_feeder_init(struct sprd_pmic_wdt *pmic_wdt)
{
	int cpu = 0;
	int ret;

	pmic_wdt->feed_task = kthread_create_on_node(sprd_wdt_feeder, pmic_wdt,
						    cpu_to_node(cpu), "watchdog_feeder/%d", cpu);
	if (IS_ERR(pmic_wdt->feed_task)) {
		ret = PTR_ERR(pmic_wdt->feed_task);
		pmic_wdt->feed_task = NULL;
		return ret;
	}

	kthread_bind(pmic_wdt->feed_task, cpu);
	ret = sprd_pmic_wdt_start(pmic_wdt);
	if (ret)
		return ret;
	wake_up_process(pmic_wdt->feed_task);

	pr_err("sprd pmic wdt:pmic_timeout %d,feed %d\n", pmic_timeout, feed_period);

	return 0;
}
#endif

static int sprd_pmic_wdt_probe(struct platform_device *pdev)
{
	int ret, rval;
	struct device_node *node = pdev->dev.of_node;
	struct sprd_pmic_wdt *pmic_wdt;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
#ifndef CONFIG_SPRD_DEBUG
	u32 val, ctrl, rtc;
#endif

	pmic_wdt = devm_kzalloc(&pdev->dev, sizeof(*pmic_wdt), GFP_KERNEL);
	if (!pmic_wdt)
		return -ENOMEM;

	pmic_wdt->dev = &pdev->dev;
	pmic_wdt->data = of_device_get_match_data(&pdev->dev);
	if (!pmic_wdt->data) {
		dev_err(&pdev->dev, "can not get private data!\n");
		return -ENODEV;
	}

	pmic_wdt->lock = &sprd_wdt_mutex;
	mutex_init(pmic_wdt->lock);

	pmic_wdt->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pmic_wdt->regmap) {
		dev_err(&pdev->dev, "sprd pmic wdt probe failed!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "reg", &pmic_wdt->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get pmic wdt base address\n");
		return ret;
	}

	ret = device_init_wakeup(pmic_wdt->dev, true);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize wakeup: %d\n", ret);
		goto err_wakeup;
	}

	kthread_init_worker(&pmic_wdt->wdt_kworker);
	kthread_init_delayed_work(&pmic_wdt->wdt_kwork, sprd_pmic_wdt_work);
	pmic_wdt->wdt_thread = kthread_run(kthread_worker_fn, &pmic_wdt->wdt_kworker,
					   "pmic_wdt_worker");
	if (IS_ERR(pmic_wdt->wdt_thread)) {
		ret = PTR_ERR(pmic_wdt->wdt_thread);
		pmic_wdt->wdt_thread = NULL;
		dev_err(&pdev->dev, "failed to run pmic_wdt_thread: %d\n", ret);
		goto err_wakeup;
	} else {
		sched_setscheduler(pmic_wdt->wdt_thread, SCHED_FIFO, &param);
	}

	pmic_wdt->wdten = sprd_pmic_wdt_en("wdten");
	pmic_wdt->sleep_en = sprd_pmic_wdt_en("dswdten");

	rval = sbuf_register_notifier(SIPC_ID_PM_SYS, SMSG_CH_TTY, 0,
				      sprd_pmic_wdt_init, pmic_wdt);
	if (rval) {
		dev_err(&pdev->dev, "sbuf notifier failed rval = %d\n", rval);
		ret = -EPROBE_DEFER; //depends on UNISOC_SIPC_SPIPE for SP9863-GO
		WRITE_ONCE(pmic_wdt->stopping, true);
		goto err_stop_worker;
	}

	mutex_lock(pmic_wdt->lock);
#ifndef CONFIG_SPRD_DEBUG
	pmic_wdt->normal_mode = sprd_pmic_wdt_get_normal_mode();
	if (pmic_wdt->normal_mode && !pmic_wdt->cm4_owned &&
	    !sbuf_status(SIPC_ID_PM_SYS, SMSG_CH_TTY)) {
		ret = regmap_read(pmic_wdt->regmap,
				  pmic_wdt->base + SPRD_PMIC_WDT_LOAD_HIGH, &val);
		if (ret)
			goto err_ownership;
		ret = regmap_read(pmic_wdt->regmap,
				  pmic_wdt->base + SPRD_PMIC_WDT_CTRL, &ctrl);
		if (ret)
			goto err_ownership;
		ret = regmap_read(pmic_wdt->regmap, pmic_wdt->data->wdt_rtc_eb_reg, &rtc);
		if (ret)
			goto err_ownership;
		if (val != SPRD_PMIC_WDT_LOAD_VAULE_HIGH &&
		    (ctrl & SPRD_PMIC_WDT_CNT_EN_BIT) &&
		    (rtc & pmic_wdt->data->wdt_rtc_en) &&
		    ((ctrl & SPRD_PMIC_WDT_RST_EN_BIT) || !pmic_wdt->wdten))
			goto out_probe;
	}
	if (pmic_wdt->normal_mode && !pmic_wdt->cm4_owned) {
		ret = sprd_pmic_wdt_enable(pmic_wdt);
		if (ret) {
			dev_err(&pdev->dev, "failed to enable wdt: %d\n", ret);
			goto err_unregister;
		}
		ret = sprd_wdt_feeder_init(pmic_wdt);
		if (ret) {
			dev_err(&pdev->dev, "failed to start wdt feeder: %d\n", ret);
			goto err_unregister;
		}
	}
out_probe:
#endif

	platform_set_drvdata(pdev, pmic_wdt);
	mutex_unlock(pmic_wdt->lock);

	return 0;

#ifndef CONFIG_SPRD_DEBUG
err_ownership:
	dev_err(&pdev->dev, "failed to read wdt ownership: %d\n", ret);
	ret = -EPROBE_DEFER;
err_unregister:
	WRITE_ONCE(pmic_wdt->stopping, true);
	mutex_unlock(pmic_wdt->lock);
	rval = sbuf_unregister_notifier(SIPC_ID_PM_SYS, SMSG_CH_TTY, 0);
	if (rval)
		dev_err(&pdev->dev, "sbuf unregister notifier failed: %d\n", rval);
#endif
err_stop_worker:
	kthread_cancel_delayed_work_sync(&pmic_wdt->wdt_kwork);
	kthread_flush_worker(&pmic_wdt->wdt_kworker);
	kthread_stop(pmic_wdt->wdt_thread);
#ifndef CONFIG_SPRD_DEBUG
	if (pmic_wdt->feed_task) {
		kthread_stop(pmic_wdt->feed_task);
		pmic_wdt->feed_task = NULL;
	}
#endif
err_wakeup:
	device_init_wakeup(pmic_wdt->dev, false);
	return ret;
}

static int sprd_pmic_wdt_remove(struct platform_device *pdev)
{
	struct sprd_pmic_wdt *pmic_wdt = dev_get_drvdata(&pdev->dev);
	int rval;

	WRITE_ONCE(pmic_wdt->stopping, true);
	rval = sbuf_unregister_notifier(SIPC_ID_PM_SYS, SMSG_CH_TTY, 0);
	if (rval)
		dev_err(&pdev->dev, "sbuf unregitster notifier failed rval = %d\n", rval);

	kthread_cancel_delayed_work_sync(&pmic_wdt->wdt_kwork);
	kthread_flush_worker(&pmic_wdt->wdt_kworker);
	kthread_stop(pmic_wdt->wdt_thread);

#ifndef CONFIG_SPRD_DEBUG
	if (!IS_ERR_OR_NULL(pmic_wdt->feed_task)) {
		kthread_stop(pmic_wdt->feed_task);
		pmic_wdt->feed_task = NULL;
	}
#endif

	device_init_wakeup(pmic_wdt->dev, false);

	return rval;
}

static const struct of_device_id sprd_pmic_wdt_of_match[] = {
	{.compatible = "sprd,sc2731-wdt", .data = &sc2731_data},
	{.compatible = "sprd,sc2730-wdt", .data = &sc2730_data},
	{.compatible = "sprd,sc2721-wdt", .data = &sc2721_data},
	{.compatible = "sprd,sc2720-wdt", .data = &sc2720_data},
	{.compatible = "sprd,ump9620-wdt", .data = &ump9620_data},
	{}
};

static struct platform_driver sprd_pmic_wdt_driver = {
	.probe = sprd_pmic_wdt_probe,
	.remove = sprd_pmic_wdt_remove,
	.driver = {
		.name = "sprd-pmic-wdt",
		.of_match_table = sprd_pmic_wdt_of_match,
	},
};
module_platform_driver(sprd_pmic_wdt_driver);

MODULE_AUTHOR("Ling Xu <ling_ling.xu@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum PMIC Watchdog Timer Controller Driver");
MODULE_LICENSE("GPL v2");
