#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/input.h>

#include "hid-scube.h"

// adjustabel initial value for break load cell
int init_load = 4;
module_param(init_load, int, 0);

int simucubeff_init(struct hid_device *hdev);
void simucubeff_remove(struct hid_device *hdev);
int simucubeff_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size);

// Would have been loveley if gotzl had made more comments on what *_get_load *_set_load functions were supposed to do.
static u8 simucube_get_load(struct hid_device *hid)
{
    struct list_head *report_list = &hid->report_enum[HID_INPUT_REPORT].report_list;
    struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
    struct simucube_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	dbg_hid(" ... get_load; %i\n", hid_report_len(report));

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x06;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);

    hid_hw_request(hid, report, HID_REQ_GET_REPORT);
    hid_hw_wait(hid);

    return report->field[0]->value[10];
}

static void simucube_set_load(struct hid_device *hid, u8 val)
{
	struct simucube_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	dbg_hid(" ... set_load %02X\n", val);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x16;
	value[4] = val+1; // actual value has an offset of 1 
	// is that offset applicable for simucube wheelbases?
	value[5] = 0x00;
	value[6] = 0x00;
	
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

 
// If _set_rumble is for a wheelbase function, then keep it. Otherwise _set_rumble might be moot since pedals are a standalone peripheral 
// when using Simucube wheelbases. 
// Will keep this around for the time being since it might be possible to piggyback and enable pedal support.
static void simucube_set_rumble(struct hid_device *hid, u32 val)
{
	struct simucube_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	int i;

	dbg_hid(" ... set_rumble %02X\n", val);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = drv_data->quirks & SIMUCUBE_PEDALS ? 0x04 : 0x03;
	value[4] = (val>>16)&0xff;
	value[5] = (val>>8)&0xff;
	value[6] = (val)&0xff;

	// TODO: see ftecff.c::fix_values
	if (!(drv_data->quirks & SIMUCUBE_PEDALS)) {
		for(i=0;i<7;i++) {
			if (value[i]>=0x80)
				value[i] = -0x100 + value[i];
		}
	}

	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

static ssize_t simucube_load_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct hid_device *hid = to_hid_device(dev);
	struct simucube_drv_data *drv_data;
	size_t count;

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return 0;
	}

	count = scnprintf(buf, PAGE_SIZE, "%u\n", simucube_get_load(hid));
	return count;
}

static ssize_t simucube_load_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	u8 load;
	if (kstrtou8(buf, 0, &load) == 0) {
		simucube_set_load(hid, load);
	}
	return count;
}
static DEVICE_ATTR(load, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, simucube_load_show, simucube_load_store);

static ssize_t simucube_rumble_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	u32 rumble;
	if (kstrtou32(buf, 0, &rumble) == 0) {
		simucube_set_rumble(hid, rumble);
	}
	return count;
}
static DEVICE_ATTR(rumble, S_IWUSR | S_IWGRP, NULL, simucube_rumble_store);

static int simucube_init(struct hid_device *hdev) {
	struct list_head *report_list = &hdev->report_enum[HID_OUTPUT_REPORT].report_list;
	struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
	struct simucube_drv_data *drv_data;

	dbg_hid(" ... %i %i %i %i\n%i %i %i %i\n\n", 
		report->id, report->type, // report->application,
		report->maxfield, report->size,
		report->field[0]->logical_minimum,report->field[0]->logical_maximum,
		report->field[0]->physical_minimum,report->field[0]->physical_maximum
		);

	// Check that the report looks ok
	if (!hid_validate_values(hdev, HID_OUTPUT_REPORT, 0, 0, 7))
		return -1;

	drv_data = hid_get_drvdata(hdev);
	if (!drv_data) {
		hid_err(hdev, "Cannot add device, private driver data not allocated\n");
		return -1;
	}

	drv_data->report = report;
	drv_data->hid = hdev;
	spin_lock_init(&drv_data->report_lock);
	return 0;
}

static int simucube_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	struct simucube_drv_data *drv_data;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;
	int ret;

	dbg_hid("%s: ifnum %d\n", __func__, iface_num);

	drv_data = kzalloc(sizeof(struct simucube_drv_data), GFP_KERNEL);
	if (!drv_data) {
		hid_err(hdev, "Insufficient memory, cannot allocate driver data\n");
		return -ENOMEM;
	}
	drv_data->quirks = id->driver_data;
	// I'm guessing this is for bumpstop range
	drv_data->min_range = 90;
	drv_data->max_range = 900; // technically max_range is unlimited, but True_drive has default set to 900 iirc 
	
	hid_set_drvdata(hdev, (void *)drv_data);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	if (drv_data->quirks & SIMUCUBE_FF) {
		connect_mask &= ~HID_CONNECT_FF;
	}

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	ret = simucube_init(hdev);
	if (ret) {
		hid_err(hdev, "hw init failed\n");
		goto err_stop;
	}
	
	if (drv_data->quirks & SIMUCUBE_TUNING_MENU) {
		// Open the device to receive reports with tuning menu data 
		ret = hid_hw_open(hdev);
		if (ret < 0) {
			hid_err(hdev, "hw open failed\n");
			goto err_stop;
		}
	}

	if (drv_data->quirks & SIMUCUBE_FF) {
		ret = simucubeff_init(hdev);
		if (ret) {
			hid_err(hdev, "ff init failed\n");
			goto err_stop;
		}
	}

// if this is for pedal rumble effects only then it could be safely removed
	if (hdev->product == SIMUCUBE_1_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_SPORT_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_PRO_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_ULTIMATE_WHEELBASE_DEVICE_ID {
		ret = device_create_file(&hdev->dev, &dev_attr_rumble);
		if (ret)
			hid_warn(hdev, "Unable to create sysfs interface for \"rumble\", errno %d\n", ret);
	}
	
	if (drv_data->quirks & SIMUCUBE_PEDALS) {
		struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
		struct input_dev *inputdev = hidinput->input;

		// if these bits are not set, the pedals are not recognized in newer proton/wine verisons
		set_bit(EV_KEY, inputdev->evbit);
		set_bit(BTN_WHEEL, inputdev->keybit);

		if (init_load >= 0 && init_load < 10) {
			simucube_set_load(hdev, init_load);
		}

		ret = device_create_file(&hdev->dev, &dev_attr_load);
		if (ret)
			hid_warn(hdev, "Unable to create sysfs interface for \"load\", errno %d\n", ret);
	}

	return 0;

//err_close:
//	hid_hw_close(hdev);
err_stop:
	hid_hw_stop(hdev);
err_free:
	kfree(drv_data);
	return ret;
}

static void simucube_remove(struct hid_device *hdev)
{
	struct simucube_drv_data *drv_data = hid_get_drvdata(hdev);
    
	if (drv_data->quirks & SIMUCUBE_PEDALS) {
		device_remove_file(&hdev->dev, &dev_attr_load);
	}
// if this is for pedal rumble effects only then it can be safely removed
	if (hdev->product == SIMUCUBE_1_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_SPORT_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_PRO_WHEELBASE_DEVICE_ID ||
		hdev->product == SIMUCUBE_2_ULTIMATE_WHEELBASE_DEVICE_ID {
		device_remove_file(&hdev->dev, &dev_attr_rumble);
	}
	
	if (drv_data->quirks & SIMUCUBE_FF) {
		simucubeff_remove(hdev);
	}

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
	kfree(drv_data);
}

static int simucube_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
	struct simucube_drv_data *drv_data = hid_get_drvdata(hdev);
	if (drv_data->quirks & SIMUCUBE_FF) {
		return simucubeff_raw_event(hdev, report, data, size);
	}
	return 0;
}

static const struct hid_device_id devices[] = {
	// Pedals need vendor and Device ID added to simucube.h 
	{ HID_USB_DEVICE(SIMFEEL_PEDALS_VENDOR_ID, SIMFEEL_PEDALS_DEVICE_ID), .driver_data = SIMUCUBE_PEDALS }, 
	{ HID_USB_DEVICE(SIMUCUBE_VENDOR_ID, SIMUCUBE_1_WHEELBASE_DEVICE_ID), .driver_data = SIMUCUBE_FF | SIMUCUBE_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(SIMUCUBE_VENDOR_ID, SIMUCUBE_2_SPORT_WHEELBASE_DEVICE_ID), .driver_data = SIMUCUBE_FF | SIMUCUBE_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(SIMUCUBE_VENDOR_ID, SIMUCUBE_2_PRO_WHEELBASE_DEVICE_ID), .driver_data = SIMUCUBE_FF | SIMUCUBE_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(SIMUCUBE_VENDOR_ID, SIMUCUBE_2_ULTIMATE_WHEELBASE_DEVICE_ID), .driver_data = SIMUCUBE_FF | SIMUCUBE_TUNING_MENU | FTEC_HIGHRES },
    { }
};

MODULE_DEVICE_TABLE(hid, devices);

// Strange to create a function specifically for csl_elite wheelbase, but might be remenants from gotzl's driver before adding additional Fanatec wheelbases?
static struct hid_driver ftec_csl_elite = {
	.name = "ftec_csl_elite",
	.id_table = devices,
        .probe = simucube_probe,
        .remove = simucube_remove,
	.raw_event = simucube_raw_event,
};
module_hid_driver(ftec_csl_elite)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("T-Fork");
MODULE_DESCRIPTION("Driver for Simucube wheelbases");
