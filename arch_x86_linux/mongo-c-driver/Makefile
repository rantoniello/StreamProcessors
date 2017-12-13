LIBNAME:= libmongoc
BUILD_DIR:= $(shell pwd)/build_dir
SOURCE_DIR:= $(shell pwd)/../../3rdplibs/mongo-c-driver
PREFIX:= $(INSTALL_DIR)$(CROSS_ARCH)

CONFIGURE_PARAMS= --srcdir=$(SOURCE_DIR) --prefix=$(PREFIX) --with-zlib=no --enable-ssl=no --enable-sasl=no

all: install

config:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then \
		cd $(BUILD_DIR); \
		$(SOURCE_DIR)/configure $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	cd $(BUILD_DIR); make install;

clean:
	@rm -fr $(BUILD_DIR)
