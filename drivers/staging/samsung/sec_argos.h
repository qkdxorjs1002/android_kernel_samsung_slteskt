#ifndef _SEC_ARGOS_H
#define _SEC_ARGOS_H

extern int irq_set_affinity(unsigned int irq, const struct cpumask *mask);
extern void exynos_dm_hotplug_enable(void);
extern void exynos_dm_hotplug_disable(void);

#endif
