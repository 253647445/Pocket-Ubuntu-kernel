/*
 * Copyright (C) STMicroelectronics 2016
 *
 * Author: Benjamin Gaignard <benjamin.gaignard@st.com>
 *
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/timer/stm32-timer-trigger.h>
#include <linux/iio/trigger.h>
#include <linux/mfd/stm32-timers.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define MAX_TRIGGERS 6
#define MAX_VALIDS 5

/* List the triggers created by each timer */
static const void *triggers_table[][MAX_TRIGGERS] = {
	{ TIM1_TRGO, TIM1_CH1, TIM1_CH2, TIM1_CH3, TIM1_CH4,},
	{ TIM2_TRGO, TIM2_CH1, TIM2_CH2, TIM2_CH3, TIM2_CH4,},
	{ TIM3_TRGO, TIM3_CH1, TIM3_CH2, TIM3_CH3, TIM3_CH4,},
	{ TIM4_TRGO, TIM4_CH1, TIM4_CH2, TIM4_CH3, TIM4_CH4,},
	{ TIM5_TRGO, TIM5_CH1, TIM5_CH2, TIM5_CH3, TIM5_CH4,},
	{ TIM6_TRGO,},
	{ TIM7_TRGO,},
	{ TIM8_TRGO, TIM8_CH1, TIM8_CH2, TIM8_CH3, TIM8_CH4,},
	{ TIM9_TRGO, TIM9_CH1, TIM9_CH2,},
	{ }, /* timer 10 */
	{ }, /* timer 11 */
	{ TIM12_TRGO, TIM12_CH1, TIM12_CH2,},
};

/* List the triggers accepted by each timer */
static const void *valids_table[][MAX_VALIDS] = {
	{ TIM5_TRGO, TIM2_TRGO, TIM3_TRGO, TIM4_TRGO,},
	{ TIM1_TRGO, TIM8_TRGO, TIM3_TRGO, TIM4_TRGO,},
	{ TIM1_TRGO, TIM2_TRGO, TIM5_TRGO, TIM4_TRGO,},
	{ TIM1_TRGO, TIM2_TRGO, TIM3_TRGO, TIM8_TRGO,},
	{ TIM2_TRGO, TIM3_TRGO, TIM4_TRGO, TIM8_TRGO,},
	{ }, /* timer 6 */
	{ }, /* timer 7 */
	{ TIM1_TRGO, TIM2_TRGO, TIM4_TRGO, TIM5_TRGO,},
	{ TIM2_TRGO, TIM3_TRGO,},
	{ }, /* timer 10 */
	{ }, /* timer 11 */
	{ TIM4_TRGO, TIM5_TRGO,},
};

struct stm32_timer_trigger {
	struct device *dev;
	struct regmap *regmap;
	struct clk *clk;
	u32 max_arr;
	const void *triggers;
	const void *valids;
};

static int stm32_timer_start(struct stm32_timer_trigger *priv,
			     unsigned int frequency)
{
	unsigned long long prd, div;
	int prescaler = 0;
	u32 ccer, cr1;

	/* Period and prescaler values depends of clock rate */
	div = (unsigned long long)clk_get_rate(priv->clk);

	do_div(div, frequency);

	prd = div;

	/*
	 * Increase prescaler value until we get a result that fit
	 * with auto reload register maximum value.
	 */
	while (div > priv->max_arr) {
		prescaler++;
		div = prd;
		do_div(div, (prescaler + 1));
	}
	prd = div;

	if (prescaler > MAX_TIM_PSC) {
		dev_err(priv->dev, "prescaler exceeds the maximum value\n");
		return -EINVAL;
	}

	/* Check if nobody else use the timer */
	regmap_read(priv->regmap, TIM_CCER, &ccer);
	if (ccer & TIM_CCER_CCXE)
		return -EBUSY;

	regmap_read(priv->regmap, TIM_CR1, &cr1);
	if (!(cr1 & TIM_CR1_CEN))
		clk_enable(priv->clk);

	regmap_write(priv->regmap, TIM_PSC, prescaler);
	regmap_write(priv->regmap, TIM_ARR, prd - 1);
	regmap_update_bits(priv->regmap, TIM_CR1, TIM_CR1_ARPE, TIM_CR1_ARPE);

	/* Force master mode to update mode */
	regmap_update_bits(priv->regmap, TIM_CR2, TIM_CR2_MMS, 0x20);

	/* Make sure that registers are updated */
	regmap_update_bits(priv->regmap, TIM_EGR, TIM_EGR_UG, TIM_EGR_UG);

	/* Enable controller */
	regmap_update_bits(priv->regmap, TIM_CR1, TIM_CR1_CEN, TIM_CR1_CEN);

	return 0;
}

static void stm32_timer_stop(struct stm32_timer_trigger *priv)
{
	u32 ccer, cr1;

	regmap_read(priv->regmap, TIM_CCER, &ccer);
	if (ccer & TIM_CCER_CCXE)
		return;

	regmap_read(priv->regmap, TIM_CR1, &cr1);
	if (cr1 & TIM_CR1_CEN)
		clk_disable(priv->clk);

	/* Stop timer */
	regmap_update_bits(priv->regmap, TIM_CR1, TIM_CR1_CEN, 0);
	regmap_write(priv->regmap, TIM_PSC, 0);
	regmap_write(priv->regmap, TIM_ARR, 0);

	/* Make sure that registers are updated */
	regmap_update_bits(priv->regmap, TIM_EGR, TIM_EGR_UG, TIM_EGR_UG);
}

static ssize_t stm32_tt_store_frequency(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t len)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct stm32_timer_trigger *priv = iio_trigger_get_drvdata(trig);
	unsigned int freq;
	int ret;

	ret = kstrtouint(buf, 10, &freq);
	if (ret)
		return ret;

	if (freq == 0) {
		stm32_timer_stop(priv);
	} else {
		ret = stm32_timer_start(priv, freq);
		if (ret)
			return ret;
	}

	return len;
}

static ssize_t stm32_tt_read_frequency(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct iio_trigger *trig = to_iio_trigger(dev);
	struct stm32_timer_trigger *priv = iio_trigger_get_drvdata(trig);
	u32 psc, arr, cr1;
	unsigned long long freq = 0;

	regmap_read(priv->regmap, TIM_CR1, &cr1);
	regmap_read(priv->regmap, TIM_PSC, &psc);
	regmap_read(priv->regmap, TIM_ARR, &arr);

	if (cr1 & TIM_CR1_CEN) {
		freq = (unsigned long long)clk_get_rate(priv->clk);
		do_div(freq, psc + 1);
		do_div(freq, arr + 1);
	}

	return sprintf(buf, "%d\n", (unsigned int)freq);
}

static IIO_DEV_ATTR_SAMP_FREQ(0660,
			      stm32_tt_read_frequency,
			      stm32_tt_store_frequency);

static char *master_mode_table[] = {
	"reset",
	"enable",
	"update",
	"compare_pulse",
	"OC1REF",
	"OC2REF",
	"OC3REF",
	"OC4REF"
};

static ssize_t stm32_tt_show_master_mode(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct stm32_timer_trigger *priv = dev_get_drvdata(dev);
	u32 cr2;

	regmap_read(priv->regmap, TIM_CR2, &cr2);
	cr2 = (cr2 & TIM_CR2_MMS) >> TIM_CR2_MMS_SHIFT;

	return snprintf(buf, PAGE_SIZE, "%s\n", master_mode_table[cr2]);
}

static ssize_t stm32_tt_store_master_mode(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t len)
{
	struct stm32_timer_trigger *priv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(master_mode_table); i++) {
		if (!strncmp(master_mode_table[i], buf,
			     strlen(master_mode_table[i]))) {
			regmap_update_bits(priv->regmap, TIM_CR2,
					   TIM_CR2_MMS, i << TIM_CR2_MMS_SHIFT);
			/* Make sure that registers are updated */
			regmap_update_bits(priv->regmap, TIM_EGR,
					   TIM_EGR_UG, TIM_EGR_UG);
			return len;
		}
	}

	return -EINVAL;
}

static IIO_CONST_ATTR(master_mode_available,
	"reset enable update compare_pulse OC1REF OC2REF OC3REF OC4REF");

static IIO_DEVICE_ATTR(master_mode, 0660,
		       stm32_tt_show_master_mode,
		       stm32_tt_store_master_mode,
		       0);

static struct attribute *stm32_trigger_attrs[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_master_mode.dev_attr.attr,
	&iio_const_attr_master_mode_available.dev_attr.attr,
	NULL,
};

static const struct attribute_group stm32_trigger_attr_group = {
	.attrs = stm32_trigger_attrs,
};

static const struct attribute_group *stm32_trigger_attr_groups[] = {
	&stm32_trigger_attr_group,
	NULL,
};

static const struct iio_trigger_ops timer_trigger_ops = {
	.owner = THIS_MODULE,
};

static int stm32_setup_iio_triggers(struct stm32_timer_trigger *priv)
{
	int ret;
	const char * const *cur = priv->triggers;

	while (cur && *cur) {
		struct iio_trigger *trig;

		trig = devm_iio_trigger_alloc(priv->dev, "%s", *cur);
		if  (!trig)
			return -ENOMEM;

		trig->dev.parent = priv->dev->parent;
		trig->ops = &timer_trigger_ops;

		/*
		 * sampling frequency and master mode attributes
		 * should only be available on trgo trigger which
		 * is always the first in the list.
		 */
		if (cur == priv->triggers)
			trig->dev.groups = stm32_trigger_attr_groups;

		iio_trigger_set_drvdata(trig, priv);

		ret = devm_iio_trigger_register(priv->dev, trig);
		if (ret)
			return ret;
		cur++;
	}

	return 0;
}

static int stm32_counter_read_raw(struct iio_dev *indio_dev,
				  struct iio_chan_spec const *chan,
				  int *val, int *val2, long mask)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	{
		u32 cnt;

		regmap_read(priv->regmap, TIM_CNT, &cnt);
		*val = cnt;

		return IIO_VAL_INT;
	}
	case IIO_CHAN_INFO_SCALE:
	{
		u32 smcr;

		regmap_read(priv->regmap, TIM_SMCR, &smcr);
		smcr &= TIM_SMCR_SMS;

		*val = 1;
		*val2 = 0;

		/* in quadrature case scale = 0.25 */
		if (smcr == 3)
			*val2 = 2;

		return IIO_VAL_FRACTIONAL_LOG2;
	}
	}

	return -EINVAL;
}

static int stm32_counter_write_raw(struct iio_dev *indio_dev,
				   struct iio_chan_spec const *chan,
				   int val, int val2, long mask)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		regmap_write(priv->regmap, TIM_CNT, val);

		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		/* fixed scale */
		return -EINVAL;
	}

	return -EINVAL;
}

static const struct iio_info stm32_trigger_info = {
	.driver_module = THIS_MODULE,
	.read_raw = stm32_counter_read_raw,
	.write_raw = stm32_counter_write_raw
};

static const char *const stm32_enable_modes[] = {
	"always",
	"gated",
	"triggered",
};

static int stm32_enable_mode2sms(int mode)
{
	switch (mode) {
	case 0:
		return 0;
	case 1:
		return 5;
	case 2:
		return 6;
	}

	return -EINVAL;
}

static int stm32_set_enable_mode(struct iio_dev *indio_dev,
				 const struct iio_chan_spec *chan,
				 unsigned int mode)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	int sms = stm32_enable_mode2sms(mode);

	if (sms < 0)
		return sms;

	regmap_update_bits(priv->regmap, TIM_SMCR, TIM_SMCR_SMS, sms);

	return 0;
}

static int stm32_sms2enable_mode(int mode)
{
	switch (mode) {
	case 0:
		return 0;
	case 5:
		return 1;
	case 6:
		return 2;
	}

	return -EINVAL;
}

static int stm32_get_enable_mode(struct iio_dev *indio_dev,
				 const struct iio_chan_spec *chan)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	u32 smcr;

	regmap_read(priv->regmap, TIM_SMCR, &smcr);
	smcr &= TIM_SMCR_SMS;

	return stm32_sms2enable_mode(smcr);
}

static const struct iio_enum stm32_enable_mode_enum = {
	.items = stm32_enable_modes,
	.num_items = ARRAY_SIZE(stm32_enable_modes),
	.set = stm32_set_enable_mode,
	.get = stm32_get_enable_mode
};

static const char *const stm32_quadrature_modes[] = {
	"channel_A",
	"channel_B",
	"quadrature",
};

static int stm32_set_quadrature_mode(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     unsigned int mode)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, TIM_SMCR, TIM_SMCR_SMS, mode + 1);

	return 0;
}

static int stm32_get_quadrature_mode(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	u32 smcr;

	regmap_read(priv->regmap, TIM_SMCR, &smcr);
	smcr &= TIM_SMCR_SMS;

	return smcr - 1;
}

static const struct iio_enum stm32_quadrature_mode_enum = {
	.items = stm32_quadrature_modes,
	.num_items = ARRAY_SIZE(stm32_quadrature_modes),
	.set = stm32_set_quadrature_mode,
	.get = stm32_get_quadrature_mode
};

static const char *const stm32_count_direction_states[] = {
	"up",
	"down"
};

static int stm32_set_count_direction(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     unsigned int mode)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, TIM_CR1, TIM_CR1_DIR, mode);

	return 0;
}

static int stm32_get_count_direction(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	u32 cr1;

	regmap_read(priv->regmap, TIM_CR1, &cr1);

	return (cr1 & TIM_CR1_DIR);
}

static const struct iio_enum stm32_count_direction_enum = {
	.items = stm32_count_direction_states,
	.num_items = ARRAY_SIZE(stm32_count_direction_states),
	.set = stm32_set_count_direction,
	.get = stm32_get_count_direction
};

static ssize_t stm32_count_get_preset(struct iio_dev *indio_dev,
				      uintptr_t private,
				      const struct iio_chan_spec *chan,
				      char *buf)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	u32 arr;

	regmap_read(priv->regmap, TIM_ARR, &arr);

	return snprintf(buf, PAGE_SIZE, "%u\n", arr);
}

static ssize_t stm32_count_set_preset(struct iio_dev *indio_dev,
				      uintptr_t private,
				      const struct iio_chan_spec *chan,
				      const char *buf, size_t len)
{
	struct stm32_timer_trigger *priv = iio_priv(indio_dev);
	unsigned int preset;
	int ret;

	ret = kstrtouint(buf, 0, &preset);
	if (ret)
		return ret;

	regmap_write(priv->regmap, TIM_ARR, preset);
	regmap_update_bits(priv->regmap, TIM_CR1, TIM_CR1_ARPE, TIM_CR1_ARPE);

	return len;
}

static const struct iio_chan_spec_ext_info stm32_trigger_count_info[] = {
	{
		.name = "preset",
		.shared = IIO_SEPARATE,
		.read = stm32_count_get_preset,
		.write = stm32_count_set_preset
	},
	IIO_ENUM("count_direction", IIO_SEPARATE, &stm32_count_direction_enum),
	IIO_ENUM_AVAILABLE("count_direction", &stm32_count_direction_enum),
	IIO_ENUM("quadrature_mode", IIO_SEPARATE, &stm32_quadrature_mode_enum),
	IIO_ENUM_AVAILABLE("quadrature_mode", &stm32_quadrature_mode_enum),
	IIO_ENUM("enable_mode", IIO_SEPARATE, &stm32_enable_mode_enum),
	IIO_ENUM_AVAILABLE("enable_mode", &stm32_enable_mode_enum),
	{}
};

static const struct iio_chan_spec stm32_trigger_channel = {
	.type = IIO_COUNT,
	.channel = 0,
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE),
	.ext_info = stm32_trigger_count_info,
	.indexed = 1
};

static struct stm32_timer_trigger *stm32_setup_counter_device(struct device *dev)
{
	struct iio_dev *indio_dev;
	int ret;

	indio_dev = devm_iio_device_alloc(dev,
					  sizeof(struct stm32_timer_trigger));
	if (!indio_dev)
		return NULL;

	indio_dev->name = dev_name(dev);
	indio_dev->dev.parent = dev;
	indio_dev->info = &stm32_trigger_info;
	indio_dev->num_channels = 1;
	indio_dev->channels = &stm32_trigger_channel;
	indio_dev->dev.of_node = dev->of_node;

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret)
		return NULL;

	return iio_priv(indio_dev);
}

/**
 * is_stm32_timer_trigger
 * @trig: trigger to be checked
 *
 * return true if the trigger is a valid stm32 iio timer trigger
 * either return false
 */
bool is_stm32_timer_trigger(struct iio_trigger *trig)
{
	return (trig->ops == &timer_trigger_ops);
}
EXPORT_SYMBOL(is_stm32_timer_trigger);

static int stm32_timer_trigger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm32_timer_trigger *priv;
	struct stm32_timers *ddata = dev_get_drvdata(pdev->dev.parent);
	unsigned int index;
	int ret;

	if (of_property_read_u32(dev->of_node, "reg", &index))
		return -EINVAL;

	if (index >= ARRAY_SIZE(triggers_table) ||
	    index >= ARRAY_SIZE(valids_table))
		return -EINVAL;

	/* Create an IIO device only if we have triggers to be validated */
	if (*valids_table[index])
		priv = stm32_setup_counter_device(dev);
	else
		priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);

	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	priv->regmap = ddata->regmap;
	priv->clk = ddata->clk;
	priv->max_arr = ddata->max_arr;
	priv->triggers = triggers_table[index];
	priv->valids = valids_table[index];

	ret = stm32_setup_iio_triggers(priv);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	return 0;
}

static const struct of_device_id stm32_trig_of_match[] = {
	{ .compatible = "st,stm32-timer-trigger", },
	{ /* end node */ },
};
MODULE_DEVICE_TABLE(of, stm32_trig_of_match);

static struct platform_driver stm32_timer_trigger_driver = {
	.probe = stm32_timer_trigger_probe,
	.driver = {
		.name = "stm32-timer-trigger",
		.of_match_table = stm32_trig_of_match,
	},
};
module_platform_driver(stm32_timer_trigger_driver);

MODULE_ALIAS("platform: stm32-timer-trigger");
MODULE_DESCRIPTION("STMicroelectronics STM32 Timer Trigger driver");
MODULE_LICENSE("GPL v2");
