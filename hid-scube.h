#ifndef __HID_SIMUCUBE_H
#define __HID_SIMUCUBE_H

#define SIMUCUBE_VENDOR_ID 0x16d0
#define SIMFEEL_PEDALS_VENDOR_ID 0x10c4
#define POLSIMER_VENDOR_ID 0x16c0

// wheelbases
#define SIMUCUBE_1_WHEELBASE_DEVICE_ID 0x0d5a
#define SIMUCUBE_2_SPORT_WHEELBASE_DEVICE_ID 0x0d61
#define SIMUCUBE_2_PRO_WHEELBASE_DEVICE_ID 0x0d60
#define SIMUCUBE_2_ULTIMATE_WHEELBASE_DEVICE_ID 0x0d5f

// pedals
#define SIMFEEL_PEDALS_DEVICE_ID 0x8ccc

/* uncertain if i should bother adding wheels since Simucube doesn't have a locked in ecosystem as Fanatec,
meaning adding device ID for compatible wheels a daunting and futile task since it might only
be applicable for wireless wheels connected to to Simucube wheelbase. */
// wheels 
#define POLSIMER_F74N_DEVICE_ID 0x0486

// quirks
#define FTEC_FF                 0x001
#define FTEC_PEDALS             0x002 // possibly moot, keep until i know i cant piggyback standalone pedals.
#define FTEC_WHEELBASE_LEDS     0x004 // remove, simucube wheelbases don't have leds.
#define FTEC_HIGHRES			0x008
#define FTEC_TUNING_MENU		0x010

// report sizes
#define FTEC_TUNING_REPORT_SIZE 64
#define FTEC_WHEEL_REPORT_SIZE 34


// misc
#define FTECFF_MAX_EFFECTS 16


struct ftecff_effect_state {
	struct ff_effect effect;
	struct ff_envelope *envelope;
	unsigned long start_at;
	unsigned long play_at;
	unsigned long stop_at;
	unsigned long flags;
	unsigned long time_playing;
	unsigned long updated_at;
	unsigned int phase;
	unsigned int phase_adj;
	unsigned int count;
	unsigned int cmd;
	unsigned int cmd_start_time;
	unsigned int cmd_start_count;
	int direction_gain;
	int slope;
};

struct ftecff_effect_parameters {
	int level;
	int d1;
	int d2;
	int k1;
	int k2;
	unsigned int clip;
};

struct ftecff_slot {
	int id;
	struct ftecff_effect_parameters parameters;
	u8 current_cmd[7];
	int is_updated;
	int effect_type;
	u8 cmd;
};

struct ftec_tuning_classdev {
	struct device *dev;
	// the data from the last update we got from the device, shifted by 1
	u8 ftec_tuning_data[FTEC_TUNING_REPORT_SIZE];
};

struct ftec_drv_data {
	unsigned long quirks;
	spinlock_t report_lock; /* Protect output HID report */
	spinlock_t timer_lock;
	struct hrtimer hrtimer;
	struct hid_device *hid;
	struct hid_report *report;
	struct ftecff_slot slots[5];
	struct ftecff_effect_state states[FTECFF_MAX_EFFECTS];
	int effects_used;	
	u16 range;
	u16 max_range;
	u16 min_range;
#ifdef CONFIG_LEDS_CLASS
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif    
	u8 wheel_id;
	struct ftec_tuning_classdev tuning;
};

#define FTEC_TUNING_ATTRS \
	FTEC_TUNING_ATTR(SLOT, 0x02, "Slot", ftec_conv_noop_to, ftec_conv_noop_from, 1, 5) \
	FTEC_TUNING_ATTR(SEN, 0x03, "Sensivity", ftec_conv_sens_to, ftec_conv_sens_from, 90, 0) \
	FTEC_TUNING_ATTR(FF, 0x04, "Force Feedback Strength", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(SHO, 0x05, "Wheel Vibration Motor", ftec_conv_times_ten, ftec_conv_div_ten, 0, 100) \
	FTEC_TUNING_ATTR(BLI, 0x06, "Break Level Indicator", ftec_conv_noop_to, ftec_conv_noop_from, 0, 101) \
	FTEC_TUNING_ATTR(DRI, 0x09, "Drift Mode", ftec_conv_signed_to, ftec_conv_noop_from, -5, 3) \
	FTEC_TUNING_ATTR(FOR, 0x0a, "Force Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(SPR, 0x0b, "Spring Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(DPR, 0x0c, "Damper Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(NDP, 0x0d, "Natural Damber", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(NFR, 0x0e, "Natural Friction", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(FEI, 0x11, "Force Effect Intensity", ftec_conv_noop_to, ftec_conv_steps_ten, 0, 100) \
	FTEC_TUNING_ATTR(INT, 0x14, "FFB Interpolation Filter", ftec_conv_noop_to, ftec_conv_noop_from, 0, 20) \
	FTEC_TUNING_ATTR(NIN, 0x15, "Natural Inertia", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(FUL, 0x16, "FullForce", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \

enum ftec_tuning_attrs_enum {
#define FTEC_TUNING_ATTR(id, addr, desc, conv_to, conv_from, min, max) \
	id,
FTEC_TUNING_ATTRS
	FTEC_TUNING_ATTR_NONE
#undef FTEC_TUNING_ATTR
};

#endif
