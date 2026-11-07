# SPDX-License-Identifier: GPL-2.0-only

GCOV_PROFILE := y
obj-$(CONFIG_HYBRIDSWAP_ZRAM) += zram.o

obj-$(CONFIG_CRYPTO_ZSTDN) += zstd/
obj-$(CONFIG_CRYPTO_LZ4K) += lz4k/

zram-y	:=	zcomp.o zram_drv.o
zram-$(CONFIG_KCOMPRESSD)	+=  kcompressd.o
zram-$(CONFIG_HYBRIDSWAP) += hybridswap/hybridmain.o
zram-$(CONFIG_HYBRIDSWAP_SWAPD) += hybridswap/hybridswapd.o
# onyx: hybridswapd.c calls the panel notifier wrappers defined here
zram-$(CONFIG_HYBRIDSWAP_SWAPD) += hybridswap/display.o
zram-$(CONFIG_HYBRIDSWAP_CORE) += hybridswap/hybridswap.o

obj-$(CONFIG_HYBRID_ZSMALLOC)	+= hybrid_zsmalloc.o
