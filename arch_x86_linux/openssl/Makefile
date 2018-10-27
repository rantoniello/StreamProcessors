LIBNAME:= openssl
BUILD_DIR:= $(shell pwd)/build_dir
SOURCE_DIR:= $(shell pwd)/../../3rdplibs/openssl
PREFIX:= $(INSTALL_DIR)$(CROSS_ARCH)

CONFIGURE_PARAMS= --prefix=${PREFIX}
CONFIGURE_PARAMS+= shared

all: install

config:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	mkdir -p $(BUILD_DIR)
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then \
		cp -a $(SOURCE_DIR)/* $(BUILD_DIR); \
		cd $(BUILD_DIR); \
		./config $(CONFIGURE_PARAMS); \
	fi

build: config
	cd $(BUILD_DIR); make

install: build
	if [ ! -f $(PREFIX)/lib/libssl.so ] ; then cd $(BUILD_DIR); make install; fi

clean:
	rm -rf $(BUILD_DIR)
	@rm -rf $(LIB_DIR)/libcrypto* $(LIB_DIR)/libssl* $(LIB_DIR)/pkgconfig/libcrypto* $(LIB_DIR)/pkgconfig/libssl* $(LIB_DIR)/pkgconfig/openssl*
	@rm -rf $(INCLUDE_DIR)/openssl
	@rm -f $(INSTALL_DIR)/bin/openssl $(INSTALL_DIR)/bin/c_rehash
	@rm -rf $(INSTALL_DIR)/ssl
