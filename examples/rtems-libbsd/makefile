#
#  RTEMS_MAKEFILE_PATH is typically set in an environment variable
#

PGM=${ARCH}/rtems_libbsd.exe

# C source names
CSRCS =  main.c \
		 posix_main.c
CSRCS += src/rtems_media.c \
		 src/rtems_network.c \
		 src/rtems_usb_mouse.c \
		 src/rtems_usb_keyboard.c \
		 src/rtems_wlan.c

COBJS = $(CSRCS:%.c=${ARCH}/%.o)

# Libraries
LIBS =

include $(RTEMS_MAKEFILE_PATH)/Makefile.inc
include $(RTEMS_CUSTOM)
include $(PROJECT_ROOT)/make/leaf.cfg

OBJS= $(COBJS) $(CXXOBJS) $(ASOBJS)


# override RTEMS build function `bsp-link-c` to link libbsd and application inlcudes
#   take cares on the order of -l
define bsp-link-c
	$(LINK.c) $(CPU_CFLAGS) $(AM_CFLAGS) $(AM_LDFLAGS) \
	    -o $(basename $@)$(EXEEXT) $(LINK_OBJS) \
		-ltelnetd -lftpd -lftpfs -ldebugger -lbsd -lz -lm \
		$(LINK_LIBS) \
		-Wl,-Map,"$(basename $@).map"
endef

define default-bsp-post-link
#	$(RTEMS_CPU)-rtems$(RTEMS_API)-objdump -D $@ > $(basename $@).asm
	$(SIZE) $@
endef

subdir:
	@mkdir -p ${ARCH}/src

all: subdir ${ARCH} $(PGM)
	@echo "BSP: $(RTEMS_MAKEFILE_PATH)"

ifeq ($(OS),Windows_NT)
USR_BOOT_DIR		?= D:\\tftpboot
else
USR_BOOT_DIR		?= /mnt/d/tftpboot
endif

image: all
	@cp ${PGM} $(USR_BOOT_DIR)/rtems.exe -f
	@$(OBJCOPY) -O binary ${USR_BOOT_DIR}/rtems.exe ${USR_BOOT_DIR}/rtems.bin
	@ls $(USR_BOOT_DIR)/rtems.* -lh

$(PGM): $(OBJS)
	$(make-exe)