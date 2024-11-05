include hardware/qcom/display/config/display-modules.mk
include vendor/qcom/opensource/mm-drivers/mm_driver_product.mk
include vendor/qcom/opensource/display-drivers/display_driver_product.mk

# In the HY22 vendor build, the image did not contain the display prebuilds
ifeq (,$(wildcard $(QCPATH)/display))
-include $(QCPATH)/techpack/artifacts/display/$(TARGET_BOARD_PLATFORM)/prebuilt.mk
endif

ifneq (,$(wildcard $(QCPATH)/display))
include $(QCPATH)/display/config/display-vendor-modules.mk
endif

# Add display-tests, display FEAT, display EXT deliverables as new phony
# target allowing to separate vendor, FEAT, EXT and test modules cleanly.
# This will allow to generate test, FEAT, and EXT modules only when
# sources are available.

.PHONY: display_tp display_tp_hal display_tp_dlkm display_tp_tests display_tp_feat display_tp_ext

display_tp: display_tp_hal display_tp_dlkm display_tp_tests display_tp_feat display_tp_ext

display_tp_hal: $(DISPLAY_MODULES_HARDWARE) $(DISPLAY_MODULES_VENDOR)

display_tp_dlkm: $(DISPLAY_MODULES_DRIVER) $(DISPLAY_MM_DRIVER)

display_tp_tests: $(DISPLAY_MODULES_TEST)

display_tp_feat: $(DISPLAY_MODULES_FEAT)

display_tp_ext: $(DISPLAY_MODULES_EXT)

$(warning "Display Techpack configuration TARGET_USES_QMAA  = $(TARGET_USES_QMAA)")
$(warning "Display Techpack configuration TARGET_USES_QMAA_OVERRIDE_DISPLAY  = $(TARGET_USES_QMAA_OVERRIDE_DISPLAY)")
$(warning "Display Techpack configuration TARGET_IS_HEADLESS  = $(TARGET_IS_HEADLESS)")
$(warning "Display Techpack configuration DISPLAY_MODULES_FEAT   = $(DISPLAY_MODULES_FEAT)")
$(warning "Display Techpack configuration DISPLAY_MODULES_EXT   = $(DISPLAY_MODULES_EXT)")
$(warning "Display Techpack configuration DISPLAY_MODULES_VENDOR   = $(DISPLAY_MODULES_VENDOR)")
