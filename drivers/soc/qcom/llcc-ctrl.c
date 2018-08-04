#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>

#undef pr_fmt
#define pr_fmt(fmt) "0x1Fu: %s: " fmt, __func__

#define PLATFORM_DEVICE_NAME "1100000.qcom,llcc:qcom,sdm845-llcc"

struct llcc_drv_data {
	struct regmap *llcc_map;
	const struct llcc_slice_config *slice_data;
	struct mutex slice_mutex;
	u32 llcc_config_data_sz;
	u32 max_slices;
	u32 b_off;
	u32 no_banks;
	unsigned long *llcc_slice_map;
};

static int dump = -1;
static int disable = -1;
static int enable = -1;
module_param(dump, int, S_IRUSR);
module_param(disable, int, S_IRUSR);
module_param(enable, int, S_IRUSR);

static struct llcc_slice_desc *llcc_slice_get_entry(int index)
{
	struct device *dev;
	struct llcc_drv_data *drv;
	const struct llcc_slice_config *llcc_data_ptr;
	struct llcc_slice_desc *desc;
	struct platform_device *pdev;
	int ret;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, PLATFORM_DEVICE_NAME);
	if (!dev) {
		pr_err("failed to find llcc device.\n");
		return ERR_PTR(-ENODEV);
	}

	pdev = to_platform_device(dev);
	drv = platform_get_drvdata(pdev);

	if (!drv) {
		pr_err("cannot find platform driver data\n");
		ret = -EFAULT;
		goto err;
	}

	llcc_data_ptr = drv->slice_data;

	while (llcc_data_ptr) {
		if (llcc_data_ptr->usecase_id == index)
			break;
		llcc_data_ptr++;
	}

	if (llcc_data_ptr == NULL) {
		pr_err("can't find %d usecase id\n", index);
		ret = -ENODEV;
		goto err;
	}

	desc = kzalloc(sizeof(struct llcc_slice_desc), GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto err;
	}

	desc->llcc_slice_id = llcc_data_ptr->slice_id;
	desc->llcc_slice_size = llcc_data_ptr->max_cap;
	desc->dev = &pdev->dev;

	return desc;

err:
	put_device(dev);
	return ERR_PTR(ret);
}

static void dump_llcc(void)
{
	struct device *dev;
	struct llcc_drv_data *drv;
	struct platform_device *pdev;
	const struct llcc_slice_config *cfg;

	dev = bus_find_device_by_name(&platform_bus_type, NULL, PLATFORM_DEVICE_NAME);
	if (!dev) {
		pr_err("failed to find llcc device.\n");
		return;
	}

	pdev = to_platform_device(dev);
	drv = platform_get_drvdata(pdev);
	if (!drv) {
		pr_err("cannot find platform driver data\n");
		return;
	}

	cfg = drv->slice_data;

	pr_info("llcc slices:");
	while (cfg) {
		pr_info("[%2d] %s %s",
			cfg->usecase_id,
			cfg->name,
			test_bit(cfg->slice_id, drv->llcc_slice_map) ? "activated" : ""
		);

		if (cfg->usecase_id >= 22) break;

		cfg++;
	}
}

static int llcc_ctrl_init(void)
{
	struct llcc_slice_desc *desc;

	pr_info("enter\n");

	if (enable > 0) {
		pr_info("enable %d\n", enable);
		desc = llcc_slice_get_entry(enable);
		if (IS_ERR_OR_NULL(desc))
			goto leave;
		llcc_slice_activate(desc);
	} else if (disable > 0) {
		pr_info("disable %d\n", disable);
		desc = llcc_slice_get_entry(disable);
		if (IS_ERR_OR_NULL(desc))
			goto leave;
		llcc_slice_deactivate(desc);
	} else if (dump > 0) {
		dump_llcc();
		goto leave;
	} else {
		goto leave;
	}

	put_device(desc->dev);
	kfree(desc);

leave:
	pr_info("leave\n");
	return 0;
}

static void llcc_ctrl_exit(void)
{
	pr_info("exit\n");
	return;
}

module_init(llcc_ctrl_init);
module_exit(llcc_ctrl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("0x1Fu");
