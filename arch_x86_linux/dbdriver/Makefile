BUILD_DIR:= $(shell pwd)/build_dir
SOURCE_DIR:= $(shell pwd)/../../dbdriver
PREFIX:= $(INSTALL_DIR)$(CROSS_ARCH)

CONFIGURE_PARAMS= --prefix=$(PREFIX) --srcdir=$(SOURCE_DIR)

CFLAGS+=-D_CONFIG_FILE_DIR="\"$(PREFIX)/etc/stream_procs/stream_procs.conf\""

all: build

config:
	@mkdir -p $(PREFIX)
	@mkdir -p $(PREFIX)/bin
	@mkdir -p $(BUILD_DIR)
	@if [ ! -f $(BUILD_DIR)/Makefile ] ; then \
		cd $(BUILD_DIR); \
		$(SOURCE_DIR)/configure $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

clean:
	@if [ -d $(BUILD_DIR) ] ; then \
		cd $(BUILD_DIR); make clean; \
	fi
	@rm -rf $(BUILD_DIR)
