.PHONY: create_folder

ROOT_LOC := $(CURDIR)
ROOT_KCONFIG := $(ROOT_LOC)/Kconfig
KCONFIG_CONFIG_LOC := $(ROOT_LOC)/configs

ifeq ("$(origin DEF)", "command line")
DEFCONFIG_FILE := $(KCONFIG_CONFIG_LOC)/$(DEF)
else
DEFCONFIG_FILE := $(KCONFIG_CONFIG_LOC)/logger_defconfig
endif

KCONFIG_CONFIG := $(KCONFIG_CONFIG_LOC)/.config
KCONFIG_CONFIG_OUTPUT_LOC := $(KCONFIG_CONFIG_LOC)/generated
export KCONFIG_CONFIG

menuconfig: create_folder
	menuconfig $(ROOT_KCONFIG)
	cd $(KCONFIG_CONFIG_OUTPUT_LOC) && genconfig $(ROOT_KCONFIG) && mv config.h logger_conf.h

savedefconfig:
	cp $(KCONFIG_CONFIG_LOC)/.config $(DEFCONFIG_FILE)

defconfig: create_folder
	cd configs && defconfig --kconfig $(ROOT_KCONFIG) $(DEFCONFIG_FILE)
	cd $(KCONFIG_CONFIG_OUTPUT_LOC) && genconfig $(ROOT_KCONFIG) && mv config.h logger_conf.h

create_folder:
	mkdir -p configs/generated
