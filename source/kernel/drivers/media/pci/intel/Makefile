# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the IPU drivers
#
obj-$(CONFIG_IPU_BRIDGE) += ipu-bridge.o
obj-y	+= ipu3/
obj-y	+= ivsc/
# Keep uninitialized-value diagnostics enabled, including in the CSS library.
subdir-ccflags-y += $(call cc-disable-warning, unused-parameter)
subdir-ccflags-y += $(call cc-disable-warning, implicit-fallthrough)
subdir-ccflags-y += $(call cc-disable-warning, missing-field-initializers)
subdir-ccflags-y += $(call cc-disable-warning, int-conversion)
subdir-ccflags-y += $(call cc-disable-warning, enum-conversion)
subdir-ccflags-$(CONFIG_VIDEO_INTEL_IPU_WERROR) += -Werror

obj-$(CONFIG_VIDEO_INTEL_IPU6)	+= ipu6/
obj-$(CONFIG_VIDEO_INTEL_IPU4)  += ipu4/
obj-$(CONFIG_VIDEO_INTEL_IPU4P)	+= ipu4/
