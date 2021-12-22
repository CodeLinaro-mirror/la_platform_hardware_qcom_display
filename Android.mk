ifneq ($(TARGET_DISABLE_DISPLAY),true)

display-hals += gralloc

include $(call all-named-subdir-makefiles,$(display-hals))

endif #TARGET_DISABLE_DISPLAY
