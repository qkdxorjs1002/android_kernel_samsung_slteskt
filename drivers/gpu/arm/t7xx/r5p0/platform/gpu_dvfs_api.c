/* drivers/gpu/t6xx/kbase/src/platform/gpu_dvfs_api.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_dvfs_api.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"

#define GPU_SET_CLK_VOL(kbdev, prev_clk, clk, vol)			\
({			\
	if (prev_clk < clk) {			\
		gpu_control_set_voltage(kbdev, vol);			\
		gpu_control_set_clock(kbdev, clk);			\
	} else {			\
		gpu_control_set_clock(kbdev, clk);			\
		gpu_control_set_voltage(kbdev, vol);			\
	}			\
})

extern struct kbase_device *pkbdev;

static int gpu_check_target_clock(struct exynos_context *platform, int clock)
{
	int target_clock = clock;

	DVFS_ASSERT(platform);

	if (gpu_dvfs_get_level(clock) < 0)
		return -1;

#ifdef CONFIG_MALI_DVFS
	if (!platform->dvfs_status)
		return clock;

	if ((platform->max_lock > 0) &&
			((clock >= platform->max_lock) || (platform->cur_clock >= platform->max_lock)))
		target_clock = platform->max_lock;
	else if ((platform->min_lock > 0) &&
			((clock <= platform->min_lock) || (platform->cur_clock <= platform->min_lock)))
		target_clock = platform->min_lock;
#endif /* CONFIG_MALI_DVFS */

	return target_clock;
}

static int gpu_update_cur_level(struct exynos_context *platform)
{
	unsigned long flags;
	int level = 0;

	DVFS_ASSERT(platform);

	level = gpu_dvfs_get_level(platform->cur_clock);
	if (level >= 0) {
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		platform->step = level;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	} else {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: invalid dvfs level returned %d\n", __func__, platform->cur_clock);
		return -1;
	}

	return 0;
}

int gpu_set_target_clk_vol(int clk)
{
	int ret = 0, target_clk = 0, target_vol = 0;
	int prev_clk = 0;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock and voltage in the power-off state!\n", __func__);
		return -1;
	}

	target_clk = gpu_check_target_clock(platform, clk);
	if (target_clk < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: mismatch clock error (source %d, target %d)\n", __func__, clk, target_clk);
		return -1;
	}

	target_vol = MAX(gpu_dvfs_get_voltage(target_clk) + platform->voltage_margin, platform->cold_min_vol);

	prev_clk = gpu_get_cur_clock(platform);

	mutex_lock(&platform->gpu_clock_lock);
	GPU_SET_CLK_VOL(kbdev, prev_clk, target_clk, target_vol);
	mutex_unlock(&platform->gpu_clock_lock);

	ret = gpu_update_cur_level(platform);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clk[%d -> %d], vol[%d (margin : %d)]\n", prev_clk, target_clk, target_vol, platform->voltage_margin);

	return ret;
}

#ifdef CONFIG_MALI_DVFS
int gpu_dvfs_clock_lock(gpu_dvfs_lock_command lock_command, gpu_dvfs_lock_type lock_type, int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	int i;
	bool dirty = false;
	unsigned long flags;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status)
		return 0;

	if ((lock_type < TMU_LOCK) || (lock_type >= NUMBER_LOCK)) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: invalid lock type is called (%d)\n", __func__, lock_type);
		return -1;
	}

	switch (lock_command) {
	case GPU_DVFS_MAX_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "max lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_max_lock[lock_type] = clock;
		platform->max_lock = clock;

		if (platform->max_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_max_lock[i] > 0)
					platform->max_lock = MIN(platform->max_lock, platform->user_max_lock[i]);
			}
		} else {
			platform->max_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->max_lock > 0) && (platform->cur_clock >= platform->max_lock))
			gpu_set_target_clk_vol(platform->max_lock);

		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "lock max clk[%d], user lock[%d], current clk[%d]\n", platform->max_lock,
				platform->user_max_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MIN_LOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		if (gpu_dvfs_get_level(clock) < 0) {
			spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
			GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "min lock error: invalid clock value %d\n", clock);
			return -1;
		}

		platform->user_min_lock[lock_type] = clock;
		platform->min_lock = clock;

		if (platform->min_lock > 0) {
			for (i = 0; i < NUMBER_LOCK; i++) {
				if (platform->user_min_lock[i] > 0)
					platform->min_lock = MAX(platform->min_lock, platform->user_min_lock[i]);
			}
		} else {
			platform->min_lock = clock;
		}

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

		if ((platform->min_lock > 0)&& (platform->cur_clock < platform->min_lock)
						&& (platform->min_lock <= platform->max_lock))
			gpu_set_target_clk_vol(platform->min_lock);

		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "lock min clk[%d], user lock[%d], current clk[%d]\n", platform->min_lock,
				platform->user_min_lock[lock_type], platform->cur_clock);
		break;
	case GPU_DVFS_MAX_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_max_lock[lock_type] = 0;
		platform->max_lock = platform->gpu_max_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_max_lock[i] > 0) {
				dirty = true;
				platform->max_lock = MIN(platform->user_max_lock[i], platform->max_lock);
			}
		}

		if (!dirty)
			platform->max_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "unlock max clk\n");
		break;
	case GPU_DVFS_MIN_UNLOCK:
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

		platform->user_min_lock[lock_type] = 0;
		platform->min_lock = platform->gpu_min_clock;

		for (i = 0; i < NUMBER_LOCK; i++) {
			if (platform->user_min_lock[i] > 0) {
				dirty = true;
				platform->min_lock = MAX(platform->user_min_lock[i], platform->min_lock);
			}
		}

		if (!dirty)
			platform->min_lock = 0;

		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
		GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "unlock min clk\n");
		break;
	default:
		break;
	}

	return 0;
}

static void gpu_dvfs_timer_control(bool enable)
{
	unsigned long flags;
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	if (!platform->dvfs_status) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: DVFS is disabled\n", __func__);
		return;
	}

	if (kbdev->pm.metrics.timer_active && !enable) {
#if !defined(SLSI_SUBSTITUTE)
		hrtimer_cancel(&kbdev->pm.metrics.timer);
#else
		del_timer(&kbdev->pm.metrics.tlist);
#endif /* SLSI_SUBSTITUTE */
	} else if (!kbdev->pm.metrics.timer_active && enable) {
#if !defined(SLSI_SUBSTITUTE)
		hrtimer_start(&kbdev->pm.metrics.timer, HR_TIMER_DELAY_MSEC(platform->polling_speed), HRTIMER_MODE_REL);
#else
		kbdev->pm.metrics.tlist.expires = jiffies + msecs_to_jiffies(platform->polling_speed);
		add_timer_on(&kbdev->pm.metrics.tlist, 0);
#endif /* SLSI_SUBSTITUTE */
		spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
		platform->down_requirement = platform->table[platform->step].stay_count;
		spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);
	}

	spin_lock_irqsave(&kbdev->pm.metrics.lock, flags);
	kbdev->pm.metrics.timer_active = enable;
	spin_unlock_irqrestore(&kbdev->pm.metrics.lock, flags);
}

void gpu_dvfs_timer_control_locked(bool enable)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	DVFS_ASSERT(platform);

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	gpu_dvfs_timer_control(enable);
	mutex_unlock(&platform->gpu_dvfs_handler_lock);
}

int gpu_dvfs_on_off(bool enable)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

	DVFS_ASSERT(platform);

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	if (enable && !platform->dvfs_status) {
		gpu_set_target_clk_vol(platform->cur_clock);
		gpu_dvfs_handler_init(kbdev);
		gpu_dvfs_timer_control(true);
	} else if (!enable && platform->dvfs_status) {
		gpu_dvfs_timer_control(false);
		gpu_dvfs_handler_deinit(kbdev);
		gpu_set_target_clk_vol(platform->gpu_dvfs_config_clock);
	} else {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: impossible state to change dvfs status (current: %d, request: %d)\n",
				__func__, platform->dvfs_status, enable);
		return -1;
	}
	mutex_unlock(&platform->gpu_dvfs_handler_lock);

	return 0;
}

int gpu_dvfs_governor_change(int governor_type)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	mutex_lock(&platform->gpu_dvfs_handler_lock);
	gpu_dvfs_governor_setting(platform, governor_type);
	mutex_unlock(&platform->gpu_dvfs_handler_lock);

	return 0;
}
#endif /* CONFIG_MALI_DVFS */

int gpu_dvfs_init_time_in_state(void)
{
#ifdef CONFIG_MALI_DEBUG_SYS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = gpu_dvfs_get_level(platform->gpu_max_clock); i <= gpu_dvfs_get_level(platform->gpu_min_clock); i++)
		platform->table[i].time = 0;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_update_time_in_state(int clock)
{
#ifdef CONFIG_MALI_DEBUG_SYS
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	u64 current_time;
	static u64 prev_time;
	int level = gpu_dvfs_get_level(clock);

	DVFS_ASSERT(platform);

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	if ((level >= gpu_dvfs_get_level(platform->gpu_max_clock)) && (level <= gpu_dvfs_get_level(platform->gpu_min_clock)))
		platform->table[level].time += current_time-prev_time;

	prev_time = current_time;
#endif /* CONFIG_MALI_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_get_level(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return i;
	}

	return -1;
}

int gpu_dvfs_get_voltage(int clock)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	int i;

	DVFS_ASSERT(platform);

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == clock)
			return platform->table[i].voltage;
	}

	return -1;
}

int gpu_dvfs_get_cur_asv_abb(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->table[platform->step].asv_abb;
}

int gpu_dvfs_get_clock(int level)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->table[level].clock;
}

int gpu_dvfs_get_step(void)
{
	struct kbase_device *kbdev = pkbdev;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

	return platform->table_size;
}
