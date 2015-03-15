#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/acpi.h>

MODULE_AUTHOR("Ruslan Marchenko <me@ruff.mobi>");
MODULE_DESCRIPTION("ASUS Wireless Radio Control Driver");
MODULE_LICENSE("GPL");

ACPI_MODULE_NAME("ASHS")

static int asus_ashs_add(struct acpi_device *dev);
static int asus_ashs_remove(struct acpi_device *dev);
static void asus_ashs_notify(struct acpi_device *dev, u32 event);

static const struct acpi_device_id asus_ashs_dev_ids[] = {
	{"ATK4002", 0},
	{},
};
MODULE_DEVICE_TABLE(acpi, asus_ashs_dev_ids);

int wapf = 0;
int wldp = 0;
int btdp = 0;
int wrst = 0;
int brst = 0;
static struct acpi_driver asus_ashs_driver = {
	.name = "ashs",
	.class = "ASHS",
	.ids = asus_ashs_dev_ids,
	.ops =
	{
		.add = asus_ashs_add,
		.remove = asus_ashs_remove,
		.notify = asus_ashs_notify
	},
	.owner = THIS_MODULE,
};
static u32 ashs_set_int(acpi_handle handle, const char *func, int param)
{
	struct acpi_object_list params;
	union acpi_object in_obj;
	u64 ret;
	acpi_status status;
	params.count = 1;
	params.pointer = &in_obj;
	in_obj.type = ACPI_TYPE_INTEGER;
	in_obj.integer.value = param;

	status = acpi_evaluate_integer(handle, (acpi_string) func, &params, &ret);
	if(!ACPI_SUCCESS(status))
	    ACPI_ERROR((AE_INFO,"Integer evaluation(set) for %s failed[%d]",func,status));

	return ret;
}
static u32 ashs_get_int(acpi_handle handle, const char *func)
{
	u64 val = 1844;
	acpi_status status;

	status = acpi_evaluate_integer(handle, (acpi_string) func, NULL, &val);

	if(!ACPI_SUCCESS(status))
	    ACPI_ERROR((AE_INFO,"Integer evaluation(get) for %s failed[%d]",func,status));

	return val;
}
static void ashs_refresh_internals(void)
{
	wapf = ashs_get_int(NULL,"\\_SB.ATKD.WAPF");
	wldp = ashs_get_int(NULL,"\\_SB_.WLDP"),
	wrst = ashs_get_int(NULL,"\\_SB_.WRST"),
	btdp = ashs_get_int(NULL,"\\_SB_.BTDP");
	brst = ashs_get_int(NULL,"\\_SB_.BRST");
}
#define ACPI_CR(x,f) if(!ACPI_SUCCESS(x)) ACPI_ERROR((AE_INFO,"Method Execution for %s failed[%d]",f,x))
static void ashs_toggle_wireless(void) {
	acpi_status status;
	ashs_refresh_internals();
	if((wapf & 4)) {
		// Send key event (software handler)
		status = acpi_execute_simple_method(NULL,"\\_SB_.ATKD.IANE",0x88);
	} else if((wapf & 0x1)==0) {
		// Hardware toggle
		int key, val;
		if(wrst || brst) {
			val = 0;
			key = 0x74;
		} else {
			val = 1;
			key = 0x73;
		}
		status = acpi_execute_simple_method(NULL,"\\OWGD",val);
		ACPI_CR(status,"OWGD");
		status = acpi_execute_simple_method(NULL,"\\OWLD",val);
		ACPI_CR(status,"OLGD");
		status = acpi_execute_simple_method(NULL,"\\OBTD",val);
		ACPI_CR(status,"OBTD");
		status = acpi_execute_simple_method(NULL,"\\_SB_.ATKD.IANE",key);
		ACPI_CR(status,"_SB_.ATKD.IANE");
	} else {
		// Toggle radios in sequence
		// Assuming both are off - enable both
		int ws=1, bs=1, wk=0x5E, bk=0x7D;
		if(wrst && brst) {
			// Disable WL first
			ws = 0;
			wk = 0x5F;
		} else if(brst) {
			// WL off, BT on - swap that thing
			bs = 0;
			bk = 0x7E;
		} else if(wrst) {
			// WL on, BT off - disable both
			ws = 0;
			bs = 0;
			wk = 0x5F;
			bk = 0x7E;
		}
		status = acpi_execute_simple_method(NULL,"\\OWGD",(ws || bs));
		ACPI_CR(status,"OWGD");
		status = acpi_execute_simple_method(NULL,"\\OWLD",ws);
		ACPI_CR(status,"OLGD");
		status = acpi_execute_simple_method(NULL,"\\OBTD",bs);
		ACPI_CR(status,"OBTD");
		status = acpi_execute_simple_method(NULL,"\\_SB_.ATKD.IANE",wk);
		ACPI_CR(status,"_SB_.ATKD.IANE(W)");
		status = acpi_execute_simple_method(NULL,"\\_SB_.ATKD.IANE",bk);
		ACPI_CR(status,"_SB_.ATKD.IANE(B)");
	}
}
static ssize_t ashs_show_sts(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ashs_refresh_internals();

	return sprintf(buf,
			"Wireless ACPI :\n"
			"\\OWGS:\t%d\n"
			"\\OHWR:\t%d\n"
			"\\ORST:\t%d\n"
			"\\_SB_.WLDP:\t%d\n"
			"\\_SB_.WRST:\t%d\n"
			"\\_SB_.BTDP:\t%d\n"
			"\\_SB_.BRST:\t%d\n"
			"\\_SB_.ATKD.WAPF:\t%d\n",
			ashs_get_int(NULL,"\\OHWR"),
			ashs_get_int(NULL,"\\OWGS"),
			ashs_get_int(NULL,"\\ORST"),
			wldp,
			wrst,
			btdp,
			brst,
			wapf);
}
static ssize_t ashs_set_owg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	uint num;

	if (!buf)
		return -EINVAL;
	ret = kstrtouint(buf, 0, &num);
	if (ret == -EINVAL || num < 0 || num > 1)
		return -EINVAL;
	return ashs_set_int(NULL,"\\_SB_.ASHS.HSWC",num);
}

/**
 *  0-1 - OWGD(param), ret 1
 *  2 - get status (ret 4 or 5)
 *  3 - ret FF
 *  4 - OWGD(0), ret 1
 *  5 - OWGD(1), ret 1
 *  80 - ret 1
 */
static DEVICE_ATTR(owg, S_IRUGO | S_IWUSR, ashs_show_sts, ashs_set_owg);

static struct attribute *ashs_attribs[] = {
	&dev_attr_owg.attr,
	NULL
};

static const struct attribute_group ashs_attr_group = {
	.attrs = ashs_attribs,
};

static int asus_ashs_add(struct acpi_device *device)
{
	int result;
	wapf = ashs_get_int(NULL,"\\_SB.ATKD.WAPF");
	result = sysfs_create_group(&device->dev.kobj, &ashs_attr_group);
	return result;
}
	
static int asus_ashs_remove(struct acpi_device *device)
{
	sysfs_remove_group(&device->dev.kobj, &ashs_attr_group);
	return 0;
}

static void asus_ashs_notify(struct acpi_device* dev, u32 event)
{
	ashs_toggle_wireless();

	pr_info("asus_ashs_notify %x\n", event);
	kobject_uevent(&dev->dev.kobj, KOBJ_CHANGE);
}

static int __init ashs_init(void)
{
	int result = 0;

	result = acpi_bus_register_driver(&asus_ashs_driver);
	if (result < 0) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error registering driver\n"));
		return -ENODEV;
	}

	return 0;
}

static void __exit ashs_exit(void)
{
	acpi_bus_unregister_driver(&asus_ashs_driver);
}


module_init(ashs_init);
module_exit(ashs_exit);
