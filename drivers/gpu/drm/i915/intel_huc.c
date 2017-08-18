/*
 * Copyright © 2016-2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */
#include <linux/firmware.h>
#include "i915_drv.h"
#include "intel_uc.h"

/**
 * DOC: HuC Firmware
 *
 * Motivation:
 * GEN9 introduces a new dedicated firmware for usage in media HEVC (High
 * Efficiency Video Coding) operations. Userspace can use the firmware
 * capabilities by adding HuC specific commands to batch buffers.
 *
 * Implementation:
 * The same firmware loader is used as the GuC. However, the actual
 * loading to HW is deferred until GEM initialization is done.
 *
 * Note that HuC firmware loading must be done before GuC loading.
 */

#define BXT_HUC_FW_MAJOR 01
#define BXT_HUC_FW_MINOR 07
#define BXT_BLD_NUM 1398

#define SKL_HUC_FW_MAJOR 01
#define SKL_HUC_FW_MINOR 07
#define SKL_BLD_NUM 1398

#define KBL_HUC_FW_MAJOR 02
#define KBL_HUC_FW_MINOR 00
#define KBL_BLD_NUM 1810

#define HUC_FW_PATH(platform, major, minor, bld_num) \
	"i915/" __stringify(platform) "_huc_ver" __stringify(major) "_" \
	__stringify(minor) "_" __stringify(bld_num) ".bin"

#define I915_SKL_HUC_UCODE HUC_FW_PATH(skl, SKL_HUC_FW_MAJOR, \
	SKL_HUC_FW_MINOR, SKL_BLD_NUM)
MODULE_FIRMWARE(I915_SKL_HUC_UCODE);

#define I915_BXT_HUC_UCODE HUC_FW_PATH(bxt, BXT_HUC_FW_MAJOR, \
	BXT_HUC_FW_MINOR, BXT_BLD_NUM)
MODULE_FIRMWARE(I915_BXT_HUC_UCODE);

#define I915_KBL_HUC_UCODE HUC_FW_PATH(kbl, KBL_HUC_FW_MAJOR, \
	KBL_HUC_FW_MINOR, KBL_BLD_NUM)
MODULE_FIRMWARE(I915_KBL_HUC_UCODE);

/**
 * huc_ucode_xfer() - DMA's the firmware
 * @dev_priv: the drm_i915_private device
 *
 * Transfer the firmware image to RAM for execution by the microcontroller.
 *
 * Return: 0 on success, non-zero on failure
 */
static int huc_ucode_xfer(struct drm_i915_private *dev_priv)
{
	struct intel_uc_fw *huc_fw = &dev_priv->huc.fw;
	struct i915_vma *vma;
	unsigned long offset = 0;
	u32 size;
	int ret;

	ret = i915_gem_object_set_to_gtt_domain(huc_fw->obj, false);
	if (ret) {
		DRM_DEBUG_DRIVER("set-domain failed %d\n", ret);
		return ret;
	}

	vma = i915_gem_object_ggtt_pin(huc_fw->obj, NULL, 0, 0,
				PIN_OFFSET_BIAS | GUC_WOPCM_TOP);
	if (IS_ERR(vma)) {
		DRM_DEBUG_DRIVER("pin failed %d\n", (int)PTR_ERR(vma));
		return PTR_ERR(vma);
	}

	intel_uncore_forcewake_get(dev_priv, FORCEWAKE_ALL);

	/* init WOPCM */
	I915_WRITE(GUC_WOPCM_SIZE, intel_guc_wopcm_size(dev_priv));
	I915_WRITE(DMA_GUC_WOPCM_OFFSET, GUC_WOPCM_OFFSET_VALUE |
			HUC_LOADING_AGENT_GUC);

	/* Set the source address for the uCode */
	offset = guc_ggtt_offset(vma) + huc_fw->header_offset;
	I915_WRITE(DMA_ADDR_0_LOW, lower_32_bits(offset));
	I915_WRITE(DMA_ADDR_0_HIGH, upper_32_bits(offset) & 0xFFFF);

	/* Hardware doesn't look at destination address for HuC. Set it to 0,
	 * but still program the correct address space.
	 */
	I915_WRITE(DMA_ADDR_1_LOW, 0);
	I915_WRITE(DMA_ADDR_1_HIGH, DMA_ADDRESS_SPACE_WOPCM);

	size = huc_fw->header_size + huc_fw->ucode_size;
	I915_WRITE(DMA_COPY_SIZE, size);

	/* Start the DMA */
	I915_WRITE(DMA_CTRL, _MASKED_BIT_ENABLE(HUC_UKERNEL | START_DMA));

	/* Wait for DMA to finish */
	ret = wait_for((I915_READ(DMA_CTRL) & START_DMA) == 0, 100);

	DRM_DEBUG_DRIVER("HuC DMA transfer wait over with ret %d\n", ret);

	/* Disable the bits once DMA is over */
	I915_WRITE(DMA_CTRL, _MASKED_BIT_DISABLE(HUC_UKERNEL));

	intel_uncore_forcewake_put(dev_priv, FORCEWAKE_ALL);

	/*
	 * We keep the object pages for reuse during resume. But we can unpin it
	 * now that DMA has completed, so it doesn't continue to take up space.
	 */
	i915_vma_unpin(vma);

	return ret;
}

/**
 * intel_huc_select_fw() - selects HuC firmware for loading
 * @huc:	intel_huc struct
 */
void intel_huc_select_fw(struct intel_huc *huc)
{
	struct drm_i915_private *dev_priv = huc_to_i915(huc);

	huc->fw.path = NULL;
	huc->fw.fetch_status = INTEL_UC_FIRMWARE_NONE;
	huc->fw.load_status = INTEL_UC_FIRMWARE_NONE;
	huc->fw.type = INTEL_UC_FW_TYPE_HUC;

	if (i915.huc_firmware_path) {
		huc->fw.path = i915.huc_firmware_path;
		huc->fw.major_ver_wanted = 0;
		huc->fw.minor_ver_wanted = 0;
	} else if (IS_SKYLAKE(dev_priv)) {
		huc->fw.path = I915_SKL_HUC_UCODE;
		huc->fw.major_ver_wanted = SKL_HUC_FW_MAJOR;
		huc->fw.minor_ver_wanted = SKL_HUC_FW_MINOR;
	} else if (IS_BROXTON(dev_priv)) {
		huc->fw.path = I915_BXT_HUC_UCODE;
		huc->fw.major_ver_wanted = BXT_HUC_FW_MAJOR;
		huc->fw.minor_ver_wanted = BXT_HUC_FW_MINOR;
	} else if (IS_KABYLAKE(dev_priv)) {
		huc->fw.path = I915_KBL_HUC_UCODE;
		huc->fw.major_ver_wanted = KBL_HUC_FW_MAJOR;
		huc->fw.minor_ver_wanted = KBL_HUC_FW_MINOR;
	} else {
		DRM_ERROR("No HuC firmware known for platform with HuC!\n");
		return;
	}
}

/**
 * intel_huc_init_hw() - load HuC uCode to device
 * @huc: intel_huc structure
 *
 * Called from guc_setup() during driver loading and also after a GPU reset.
 * Be note that HuC loading must be done before GuC loading.
 *
 * The firmware image should have already been fetched into memory by the
 * earlier call to intel_huc_init(), so here we need only check that
 * is succeeded, and then transfer the image to the h/w.
 *
 * Return:	non-zero code on error
 */
int intel_huc_init_hw(struct intel_huc *huc)
{
	struct drm_i915_private *dev_priv = huc_to_i915(huc);
	int err;

	if (huc->fw.fetch_status == INTEL_UC_FIRMWARE_NONE)
		return 0;

	DRM_DEBUG_DRIVER("%s fw status: fetch %s, load %s\n",
		huc->fw.path,
		intel_uc_fw_status_repr(huc->fw.fetch_status),
		intel_uc_fw_status_repr(huc->fw.load_status));

	if (huc->fw.fetch_status == INTEL_UC_FIRMWARE_SUCCESS &&
	    huc->fw.load_status == INTEL_UC_FIRMWARE_FAIL)
		return -ENOEXEC;

	huc->fw.load_status = INTEL_UC_FIRMWARE_PENDING;

	switch (huc->fw.fetch_status) {
	case INTEL_UC_FIRMWARE_FAIL:
		/* something went wrong :( */
		err = -EIO;
		goto fail;

	case INTEL_UC_FIRMWARE_NONE:
	case INTEL_UC_FIRMWARE_PENDING:
	default:
		/* "can't happen" */
		WARN_ONCE(1, "HuC fw %s invalid fetch_status %s [%d]\n",
			huc->fw.path,
			intel_uc_fw_status_repr(huc->fw.fetch_status),
			huc->fw.fetch_status);
		err = -ENXIO;
		goto fail;

	case INTEL_UC_FIRMWARE_SUCCESS:
		break;
	}

	err = huc_ucode_xfer(dev_priv);
	if (err)
		goto fail;

	huc->fw.load_status = INTEL_UC_FIRMWARE_SUCCESS;

	DRM_DEBUG_DRIVER("%s fw status: fetch %s, load %s\n",
		huc->fw.path,
		intel_uc_fw_status_repr(huc->fw.fetch_status),
		intel_uc_fw_status_repr(huc->fw.load_status));

	return 0;

fail:
	if (huc->fw.load_status == INTEL_UC_FIRMWARE_PENDING)
		huc->fw.load_status = INTEL_UC_FIRMWARE_FAIL;

	DRM_ERROR("Failed to complete HuC uCode load with ret %d\n", err);

	return err;
}

/**
 * intel_guc_auth_huc() - authenticate ucode
 * @dev_priv: the drm_i915_device
 *
 * Triggers a HuC fw authentication request to the GuC via intel_guc_action_
 * authenticate_huc interface.
 */
void intel_guc_auth_huc(struct drm_i915_private *dev_priv)
{
	struct intel_guc *guc = &dev_priv->guc;
	struct intel_huc *huc = &dev_priv->huc;
	struct i915_vma *vma;
	int ret;
	u32 data[2];

	if (huc->fw.load_status != INTEL_UC_FIRMWARE_SUCCESS)
		return;

	vma = i915_gem_object_ggtt_pin(huc->fw.obj, NULL, 0, 0,
				PIN_OFFSET_BIAS | GUC_WOPCM_TOP);
	if (IS_ERR(vma)) {
		DRM_ERROR("failed to pin huc fw object %d\n",
				(int)PTR_ERR(vma));
		return;
	}

	/* Specify auth action and where public signature is. */
	data[0] = INTEL_GUC_ACTION_AUTHENTICATE_HUC;
	data[1] = guc_ggtt_offset(vma) + huc->fw.rsa_offset;

	ret = intel_guc_send(guc, data, ARRAY_SIZE(data));
	if (ret) {
		DRM_ERROR("HuC: GuC did not ack Auth request %d\n", ret);
		goto out;
	}

	/* Check authentication status, it should be done by now */
	ret = intel_wait_for_register(dev_priv,
				HUC_STATUS2,
				HUC_FW_VERIFIED,
				HUC_FW_VERIFIED,
				50);

	if (ret) {
		DRM_ERROR("HuC: Authentication failed %d\n", ret);
		goto out;
	}

out:
	i915_vma_unpin(vma);
}

