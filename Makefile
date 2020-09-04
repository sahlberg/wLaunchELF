#.SILENT:

EE_BIN = BOOT-UNC.ELF
EE_BIN_PKD = BOOT.ELF
EE_OBJS = main.o pad.o config.o elf.o draw.o loader_elf.o filer.o \
	poweroff_irx.o iomanx_irx.o filexio_irx.o ps2atad_irx.o DEV9_irx.o NETMAN_irx.o ps2ip_irx.o\
	SMAP_irx.o ps2hdd_irx.o ps2fs_irx.o usbd_irx.o usbhdfsd_irx.o mcman_irx.o mcserv_irx.o\
	cdfs_irx.o vmc_fs_irx.o ps2kbd_irx.o\
	hdd.o hdl_rpc.o hdl_info_irx.o editor.o timer.o jpgviewer.o icon.o lang.o\
	font_uLE.o makeicon.o chkesr.o sior_irx.o allowdvdv_irx.o
EE_INCS := -I$(PS2DEV)/gsKit/include -I$(PS2SDK)/ports/include

EE_LDFLAGS := -L$(PS2DEV)/gsKit/lib -L$(PS2SDK)/ports/lib -s
EE_LIBS = -lgskit -ldmakit -ljpeg -lpad -lmc -lhdd -lkbd -lm \
	-lcdvd -lfileXio -lpatches -lpoweroff -ldebug -lsior
EE_CFLAGS := -mno-gpopt -G0

BIN2S = $(PS2SDK)/bin/bin2s

.PHONY: all run reset clean rebuild format format-check

all: githash.h $(EE_BIN_PKD)

$(EE_BIN_PKD): $(EE_BIN)
	ps2-packer $< $@

reset: clean
	ps2client -h 192.168.0.10 reset

format:
	find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -i

format-check:
	@! find . -type f -a \( -iname \*.h -o -iname \*.c \) | xargs clang-format -style=file -output-replacements-xml | grep "<replacement " >/dev/null

githash.h:
	printf '#ifndef ULE_VERDATE\n#define ULE_VERDATE "' > $@ && \
	git show -s --format=%cd --date=local | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@
	printf '#ifndef GIT_HASH\n#define GIT_HASH "' >> $@ && \
	git rev-parse --short HEAD | tr -d "\n" >> $@ && \
	printf '"\n#endif\n' >> $@

DEV9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2c $< DEV9_irx.c DEV9_irx

NETMAN_irx.c: $(PS2SDK)/iop/irx/netman.irx
	bin2c $< NETMAN_irx.c NETMAN_irx

SMAP_irx.c: $(PS2SDK)/iop/irx/smap.irx
	bin2c $< SMAP_irx.c SMAP_irx

mcman_irx.s: $(PS2SDK)/iop/irx/mcman.irx
	$(BIN2S) $< $@ mcman_irx

mcserv_irx.s: $(PS2SDK)/iop/irx/mcserv.irx
	$(BIN2S) $< $@ mcserv_irx

usbd_irx.s: $(PS2SDK)/iop/irx/usbd.irx
	$(BIN2S) $< $@ usbd_irx

usbhdfsd_irx.s: $(PS2SDK)/iop/irx/usbhdfsd.irx
	$(BIN2S) $< $@ usb_mass_irx

cdfs_irx.s: $(PS2SDK)/iop/irx/cdfs.irx
	$(BIN2S) $< $@ cdfs_irx

poweroff_irx.s: $(PS2SDK)/iop/irx/poweroff.irx
	$(BIN2S) $< $@ poweroff_irx

iomanx_irx.s: $(PS2SDK)/iop/irx/iomanX.irx
	$(BIN2S) $< $@ iomanx_irx

filexio_irx.s: $(PS2SDK)/iop/irx/fileXio.irx
	$(BIN2S) $< $@ filexio_irx

ps2ip_irx.s: $(PS2SDK)/iop/irx/ps2ip.irx
	$(BIN2S) $< $@ ps2ip_irx

ps2atad_irx.s: $(PS2SDK)/iop/irx/ps2atad.irx
	$(BIN2S) $< $@ ps2atad_irx

ps2hdd_irx.s: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	$(BIN2S) $< $@ ps2hdd_irx

ps2fs_irx.s: $(PS2SDK)/iop/irx/ps2fs.irx
	$(BIN2S) $< $@ ps2fs_irx

hdl_info/hdl_info.irx: hdl_info
	$(MAKE) -C $<

hdl_info_irx.s: hdl_info/hdl_info.irx
	$(BIN2S) $< $@ hdl_info_irx

vmc_fs/vmc_fs.irx: vmc_fs
	$(MAKE) -C $<

vmc_fs_irx.s: vmc_fs/vmc_fs.irx
	$(BIN2S) $< $@ vmc_fs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

loader_elf.s: loader/loader.elf
	$(BIN2S) $< $@ loader_elf

ps2kbd_irx.s: $(PS2SDK)/iop/irx/ps2kbd.irx
	$(BIN2S) $< $@ ps2kbd_irx

sior_irx.s: $(PS2SDK)/iop/irx/sior.irx
	$(BIN2S) $< $@ sior_irx

AllowDVDV/AllowDVDV.irx: AllowDVDV
	$(MAKE) -C $<

allowdvdv_irx.s: AllowDVDV/AllowDVDV.irx
	$(BIN2S) $< $@ allowdvdv_irx

clean:
	$(MAKE) -C hdl_info clean
	$(MAKE) -C loader clean
	$(MAKE) -C vmc_fs clean
	$(MAKE) -C AllowDVDV clean
	rm -f githash.h *.s $(EE_OBJS) $(EE_BIN) $(EE_BIN_PKD)

rebuild: clean all

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
