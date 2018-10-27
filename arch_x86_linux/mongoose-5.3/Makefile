LIBNAME:= mongoose-5.3
BUILD_DIR:= $(shell pwd)/build_dir
SOURCE_DIR:= $(shell pwd)/../../3rdplibs/$(LIBNAME)
PREFIX:= $(INSTALL_DIR)$(CROSS_ARCH)

all: install

config:
	mkdir -p $(PREFIX)
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/include
	mkdir -p $(BUILD_DIR)

build: config
	if [ ! -f $(PREFIX)/lib/libmongoose-5.3.so ] ; then \
		cp -a $(SOURCE_DIR)/* $(BUILD_DIR)/; \
		cd $(BUILD_DIR)/examples; \
		$(CC) -c $(CFLAGS) -L$(PREFIX)/lib -I$(PREFIX)/include -Werror -fpic $(BUILD_DIR)/mongoose.c; \
		$(CC) -shared -Bdynamic $(CFLAGS) -o libmongoose-5.3.so mongoose.o; \
	fi

install: build
	cp $(BUILD_DIR)/examples/libmongoose-5.3.so $(PREFIX)/lib/;
	cp $(BUILD_DIR)/mongoose.h $(PREFIX)/include/mongoose-5.3.h;

clean:
	@rm -fr $(BUILD_DIR)
	@rm -f $(PREFIX)/lib/libmongoose-5.3.so
	@rm -f $(PREFIX)/include/mongoose-5.3.h
