/*
 * STMicroelectronics hts221 sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef HTS221_H
#define HTS221_H

#define HTS221_DEV_NAME		"hts221"

#include <linux/iio/iio.h>

#define HTS221_RX_MAX_LENGTH	8
#define HTS221_TX_MAX_LENGTH	8

#define HTS221_DATA_SIZE	2

struct hts221_transfer_buffer {
	u8 rx_buf[HTS221_RX_MAX_LENGTH];
	u8 tx_buf[HTS221_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct hts221_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

#define HTS221_AVG_DEPTH	8
struct hts221_avg_avl {
	u16 avg;
	u8 val;
};

enum hts221_sensor_type {
	HTS221_SENSOR_H,
	HTS221_SENSOR_T,
	HTS221_SENSOR_MAX,
};

struct hts221_sensor {
	u8 cur_avg_idx;
	int slope, b_gen;
};

struct hts221_hw {
	const char *name;
	struct device *dev;

	struct mutex lock;
	struct iio_trigger *trig;
	int irq;

	struct hts221_sensor sensors[HTS221_SENSOR_MAX];

	u8 odr;

	const struct hts221_transfer_function *tf;
	struct hts221_transfer_buffer tb;
};

int hts221_config_drdy(struct hts221_hw *hw, bool enable);
int hts221_probe(struct iio_dev *iio_dev);
int hts221_power_on(struct hts221_hw *hw);
int hts221_power_off(struct hts221_hw *hw);
int hts221_allocate_buffers(struct hts221_hw *hw);
int hts221_allocate_trigger(struct hts221_hw *hw);

#endif /* HTS221_H */
