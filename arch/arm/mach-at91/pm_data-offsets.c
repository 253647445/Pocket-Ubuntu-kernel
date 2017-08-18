#include <linux/stddef.h>
#include <linux/kbuild.h>
#include "pm.h"

int main(void)
{
	DEFINE(PM_DATA_PMC,		offsetof(struct at91_pm_data, pmc));
	DEFINE(PM_DATA_RAMC0,		offsetof(struct at91_pm_data, ramc[0]));
	DEFINE(PM_DATA_RAMC1,		offsetof(struct at91_pm_data, ramc[1]));
	DEFINE(PM_DATA_MEMCTRL,	offsetof(struct at91_pm_data, memctrl));
	DEFINE(PM_DATA_MODE,		offsetof(struct at91_pm_data, mode));
	return 0;
}
