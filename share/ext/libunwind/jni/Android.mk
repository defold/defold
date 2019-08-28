LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libunwind_defold

LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += \
	-UNDEBUG \
	-DHAVE_CONFIG_H \
	-DNDEBUG \
	-D_GNU_SOURCE \
	-Wno-unused-parameter \
	-Wno-inline-asm \
	-Wno-header-guard \
	-Wno-absolute-value

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../src \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../include/tdep-arm

LOCAL_SRC_FILES := \
	../src/mi/init.c \
    ../src/mi/flush_cache.c \
    ../src/mi/mempool.c \
    ../src/mi/strerror.c \
    ../src/mi/backtrace.c \
    ../src/mi/dyn-cancel.c \
    ../src/mi/dyn-info-list.c \
    ../src/mi/dyn-register.c \
    ../src/mi/map.c \
    ../src/mi/Lmap.c \
    ../src/mi/Ldyn-extract.c \
    ../src/mi/Lfind_dynamic_proc_info.c \
    ../src/mi/Lget_proc_info_by_ip.c \
    ../src/mi/Lget_proc_name.c \
    ../src/mi/Lput_dynamic_unwind_info.c \
    ../src/mi/Ldestroy_addr_space.c \
    ../src/mi/Lget_reg.c \
    ../src/mi/Lset_reg.c \
    ../src/mi/Lget_fpreg.c \
    ../src/mi/Lset_fpreg.c \
    ../src/mi/Lset_caching_policy.c \
    ../src/mi/Gdyn-extract.c \
    ../src/mi/Gdyn-remote.c \
    ../src/mi/Gfind_dynamic_proc_info.c \
    ../src/mi/Gget_accessors.c \
    ../src/mi/Gget_proc_info_by_ip.c \
    ../src/mi/Gget_proc_name.c \
    ../src/mi/Gput_dynamic_unwind_info.c \
    ../src/mi/Gdestroy_addr_space.c \
    ../src/mi/Gget_reg.c \
    ../src/mi/Gset_reg.c \
    ../src/mi/Gget_fpreg.c \
    ../src/mi/Gset_fpreg.c \
    ../src/mi/Gset_caching_policy.c \
    ../src/dwarf/Lexpr.c \
    ../src/dwarf/Lfde.c \
    ../src/dwarf/Lparser.c \
    ../src/dwarf/Lpe.c \
    ../src/dwarf/Lstep_dwarf.c \
    ../src/dwarf/Lfind_proc_info-lsb.c \
    ../src/dwarf/Lfind_unwind_table.c \
    ../src/dwarf/Gexpr.c \
    ../src/dwarf/Gfde.c \
    ../src/dwarf/Gfind_proc_info-lsb.c \
    ../src/dwarf/Gfind_unwind_table.c \
    ../src/dwarf/Gparser.c \
    ../src/dwarf/Gpe.c \
    ../src/dwarf/Gstep_dwarf.c \
    ../src/dwarf/global.c \
    ../src/os-common.c \
    ../src/os-linux.c \
    ../src/Los-common.c \
    ../src/ptrace/_UPT_accessors.c \
    ../src/ptrace/_UPT_access_fpreg.c \
    ../src/ptrace/_UPT_access_mem.c \
    ../src/ptrace/_UPT_access_reg.c \
    ../src/ptrace/_UPT_create.c \
    ../src/ptrace/_UPT_destroy.c \
    ../src/ptrace/_UPT_find_proc_info.c \
    ../src/ptrace/_UPT_get_dyn_info_list_addr.c \
    ../src/ptrace/_UPT_put_unwind_info.c \
    ../src/ptrace/_UPT_get_proc_name.c \
    ../src/ptrace/_UPT_reg_offset.c \
    ../src/ptrace/_UPT_resume.c


ifeq ($(TARGET_ARCH_ABI),$(filter $(TARGET_ARCH_ABI),armeabi armeabi-v7a))
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../include/tdep-arm

LOCAL_SRC_FILES += \
	../src/elf32.c \
    ../src/arm/dl-iterate-phdr.c \
	../src/arm/is_fpreg.c \
	../src/arm/regname.c \
	../src/arm/Gcreate_addr_space.c \
	../src/arm/Gget_proc_info.c \
	../src/arm/Gget_save_loc.c \
	../src/arm/Gglobal.c \
	../src/arm/Ginit.c \
	../src/arm/Ginit_local.c \
	../src/arm/Ginit_remote.c \
	../src/arm/Gregs.c \
	../src/arm/Gresume.c \
	../src/arm/Gstep.c \
	../src/arm/Lcreate_addr_space.c \
	../src/arm/Lget_proc_info.c \
	../src/arm/Lget_save_loc.c \
	../src/arm/Lglobal.c \
	../src/arm/Linit.c \
	../src/arm/Linit_local.c \
	../src/arm/Linit_remote.c \
	../src/arm/Lregs.c \
	../src/arm/Lresume.c \
	../src/arm/Lstep.c \
	../src/arm/getcontext.S \
	../src/arm/Gis_signal_frame.c \
	../src/arm/Gex_tables.c \
	../src/arm/Lis_signal_frame.c \
	../src/arm/Lex_tables.c
endif

ifeq ($(TARGET_ARCH_ABI),$(filter $(TARGET_ARCH_ABI),arm64-v8a))
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../include/tdep-arm

LOCAL_SRC_FILES += \
    ../src/elf64.c \
    ../src/aarch64/is_fpreg.c \
    ../src/aarch64/regname.c \
    ../src/aarch64/Gcreate_addr_space.c \
    ../src/aarch64/Gget_proc_info.c \
    ../src/aarch64/Gget_save_loc.c \
    ../src/aarch64/Gglobal.c \
    ../src/aarch64/Ginit.c \
    ../src/aarch64/Ginit_local.c \
    ../src/aarch64/Ginit_remote.c \
    ../src/aarch64/Gregs.c \
    ../src/aarch64/Gresume.c \
    ../src/aarch64/Gstep.c \
    ../src/aarch64/Lcreate_addr_space.c \
    ../src/aarch64/Lget_proc_info.c \
    ../src/aarch64/Lget_save_loc.c \
    ../src/aarch64/Lglobal.c \
    ../src/aarch64/Linit.c \
    ../src/aarch64/Linit_local.c \
    ../src/aarch64/Linit_remote.c \
    ../src/aarch64/Lregs.c \
    ../src/aarch64/Lresume.c \
    ../src/aarch64/Lstep.c \
    ../src/aarch64/Gis_signal_frame.c \
    ../src/aarch64/Lis_signal_frame.c
endif

LOCAL_ADDITIONAL_DEPENDENCIES := \
	$(LOCAL_PATH)/Android.mk

LOCAL_STATIC_LIBRARIES := m dl

include $(BUILD_STATIC_LIBRARY)
