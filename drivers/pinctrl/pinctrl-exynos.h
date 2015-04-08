/*
 * Exynos specific definitions for Samsung pinctrl and gpiolib driver.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Copyright (c) 2012 Linaro Ltd
 *		http://www.linaro.org
 *
 * This file contains the Exynos specific definitions for the Samsung
 * pinctrl/gpiolib interface drivers.
 *
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* External GPIO and wakeup interrupt related definitions */
#define EXYNOS_GPIO_ECON_OFFSET		0x700
#define EXYNOS_GPIO_EFLTCON_OFFSET	0x800
#define EXYNOS_GPIO_EMASK_OFFSET	0x900
#define EXYNOS_GPIO_EPEND_OFFSET	0xA00
#define EXYNOS_WKUP_ECON_OFFSET		0xE00
#define EXYNOS_WKUP_EMASK_OFFSET	0xF00
#define EXYNOS_WKUP_EPEND_OFFSET	0xF40
#define EXYNOS5430_WKUP_ECON_OFFSET	0x700
#define EXYNOS5430_WKUP_EFLTCON_OFFSET	0x800
#define EXYNOS5430_WKUP_EMASK_OFFSET	0x900
#define EXYNOS5430_WKUP_EPEND_OFFSET	0xA00
#define EXYNOS_SVC_OFFSET		0xB08
#define EXYNOS_EINT_FUNC		0xF

/* helpers to access interrupt service register */
#define EXYNOS_SVC_GROUP_SHIFT		3
#define EXYNOS_SVC_GROUP_MASK		0x1f
#define EXYNOS_SVC_NUM_MASK		7
#define EXYNOS_SVC_GROUP(x)		((x >> EXYNOS_SVC_GROUP_SHIFT) & \
						EXYNOS_SVC_GROUP_MASK)

/* Exynos specific external interrupt trigger types */
#define EXYNOS_EINT_LEVEL_LOW		0
#define EXYNOS_EINT_LEVEL_HIGH		1
#define EXYNOS_EINT_EDGE_FALLING	2
#define EXYNOS_EINT_EDGE_RISING		3
#define EXYNOS_EINT_EDGE_BOTH		4
#define EXYNOS_EINT_CON_MASK		0xF
#define EXYNOS_EINT_CON_LEN		4

/* EINT filter configuration */
#define EXYNOS_EINT_FLTCON_EN		(1 << 7)
#define EXYNOS_EINT_FLTCON_SEL		(1 << 6)
#define EXYNOS_EINT_FLTCON_WIDTH(x)	((x) & 0x3f)
#define EXYNOS_EINT_FLTCON_MASK		0xFF
#define EXYNOS_EINT_FLTCON_LEN		8

#define EXYNOS_EINT_MAX_PER_BANK	8
#define EXYNOS_EINT_NR_WKUP_EINT

#define EINT_FLTCON_NORMAL		0
#define EINT_FLTCON_PRESET0		1
#define EINT_FLTCON_PRESET01		2

#define EXYNOS_PIN_BANK_EINTN(types, pins, reg, id)	\
	{						\
		.type		= &types,		\
		.pctl_offset	= reg,			\
		.nr_pins	= pins,			\
		.eint_type	= EINT_TYPE_NONE,	\
		.name		= id			\
	}

#define EXYNOS_PIN_BANK_EINTG(types, pins, reg, id, offs)	\
	{						\
		.type		= &types,		\
		.pctl_offset	= reg,			\
		.nr_pins	= pins,			\
		.eint_type	= EINT_TYPE_GPIO,	\
		.eint_offset	= offs,			\
		.eint_fltcon_type = EINT_FLTCON_NORMAL,	\
		.name		= id			\
	}

#define EXYNOS_PIN_BANK_EINTW(types, pins, reg, id, offs)	\
	{						\
		.type		= &types,		\
		.pctl_offset	= reg,			\
		.nr_pins	= pins,			\
		.eint_type	= EINT_TYPE_WKUP,	\
		.eint_offset	= offs,			\
		.eint_fltcon_type = EINT_FLTCON_NORMAL,	\
		.name		= id			\
	}

#define EXYNOS_PIN_BANK_EINTW_EXT(types, pins, reg, id, offs, base, ftype, fcon0, fcon1)	\
	{							\
		.type			= &types,		\
		.pctl_offset		= reg,			\
		.nr_pins		= pins,			\
		.eint_type		= EINT_TYPE_WKUP,	\
		.eint_offset		= offs,			\
		.eint_ext_offset	= base,			\
		.eint_fltcon0_offset	= fcon0,		\
		.eint_fltcon1_offset	= fcon1,		\
		.eint_fltcon_type	= ftype,		\
		.name			= id			\
	}

/**
 * struct exynos_weint_data: irq specific data for all the wakeup interrupts
 * generated by the external wakeup interrupt controller.
 * @irq: interrupt number within the domain.
 * @bank: bank responsible for this interrupt
 */
struct exynos_weint_data {
	unsigned int irq;
	struct samsung_pin_bank *bank;
};

/**
 * struct exynos_muxed_weint_data: irq specific data for muxed wakeup interrupts
 * generated by the external wakeup interrupt controller.
 * @nr_banks: count of banks being part of the mux
 * @banks: array of banks being part of the mux
 */
struct exynos_muxed_weint_data {
	unsigned int nr_banks;
	struct samsung_pin_bank *banks[];
};
