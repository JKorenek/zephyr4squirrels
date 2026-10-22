#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
/*add 
aliases {
		mic=&micN;
	};

&speaker { status = "disabled"; };
&adc0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    micN: ADC1_CH6@N {
        status = "okay";
        reg = <N>; pin VDET_1 chnll ADC1_CH6  10            
        zephyr,gain = "ADC_GAIN_1_4";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
    };
};
*/

LOG_MODULE_REGISTER(main);
#define STRIP_NODE DT_ALIAS(led_strip)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)
#define BRIGHTNESS 0x20
#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(BRIGHTNESS, 0x00, 0x00), /* red */
	RGB(0x00, BRIGHTNESS, 0x00), /* green */
	RGB(0x00, 0x00, BRIGHTNESS), /* blue */
	RGB(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS), /* white */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];
static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

static const struct adc_dt_spec micchannel = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));

ZBUS_CHAN_DEFINE(mChannel,                     /* channel name */
		  struct sensor_msg,                /* message type */
		  NULL,                             /* validator */
		  NULL,                             /* user data */
		  ZBUS_OBSERVERS(sensor_listener),  /* observers */
		  ZBUS_MSG_INIT(.value = 0)         /* initial value */
);

ZBUS_CHAN_DEFINE(errChannel,                     /* channel name */
		  struct sensor_msg,                /* message type */
		  NULL,                             /* validator */
		  NULL,                             /* user data */
		  ZBUS_OBSERVERS(sensor_listener),  /* observers */
		  ZBUS_MSG_INIT(.value = 0)         /* initial value */
);


ZBUS_SUBSCRIBER_DEFINE(sensor_sub, 4);
ZBUS_CHAN_ADD_OBS(mChannel, sensor_sub, 3);

static void LED_listen(void *a, void *b, void *c)
{
	const struct zbus_channel *chan;
	struct sensor_msg msg;
	int volume=0;
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (!zbus_sub_wait(&sensor_sub, &chan, K_FOREVER)) {
		zbus_chan_read(chan, &msg, K_MSEC(250));
		printk(" value = %d\n", msg.value);
		volume=(msg.value/100)*STRIP_NUM_PIXELS;
		for (size_t i = volume; i < STRIP_NUM_PIXELS; i++) {
			pixels[i] = 3;
		}
		for (size_t i = 0; i < volume; i++) {
			pixels[i] = 0;
		}
		led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
	}
}

K_THREAD_DEFINE(subscriber_id, 1024, LED_listen, NULL, NULL, NULL, 5, 0, 0);

static void publisher_entry(void *a, void *b, void *c)
{
	struct sensor_msg msg = { .value = 0 };

	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	while (1) {
		printk("publishing value = %d\n", msg.value);
		zbus_chan_pub(&mChannel, &msg, K_NO_WAIT);

	}
}

K_THREAD_DEFINE(publisher_id, 1024, publisher_entry, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{

	int err,val;
	uint32_t buf = 0;
	struct adc_sequence sequence = {
		.buffer = &buf,
		/* buffer size in bytes, not number of samples */
		.buffer_size = sizeof(buf),
#if CONFIG_SAMPLE_ADC_CALIBRATE_REQUIRED
		.calibrate = true,
#endif
	};

	if(!adc_is_ready_dt(&micchannel)){
			
		printk("Channel not ready");
		zbus_chan_pub(&errChannel, "ADC controller device is not ready\n", K_NO_WAIT);
		return 0;
	}
	
	err = adc_channel_setup_dt(&micchannel);
	if (err < 0) {
		printk("Could not setup channel (%d)\n", err);
		zbus_chan_pub(&errChannel, &err, K_NO_WAIT);
		return 0;
	}

	while (1) {
		buf = 0;
		adc_sequence_init_dt(&micchannel, &sequence);
		adc_read_dt(&micchannel, &sequence);
		val=buf;
		
		printk("publishing value = %d\n", val);
		zbus_chan_pub(&mChannel, &buf, K_NO_WAIT);
	}
	return 0;
}
