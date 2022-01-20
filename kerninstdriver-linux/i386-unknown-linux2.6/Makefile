KERNEL_VERSION  = $(shell uname -r)
KERNEL_SOURCE 	= /lib/modules/$(KERNEL_VERSION)/build

SRC_DIR		= $(PWD)/../src

default:
	make -C $(KERNEL_SOURCE) M=$(SRC_DIR) modules

install:
	make -C $(KERNEL_SOURCE) M=$(SRC_DIR) modules_install
	./install_script

clean:
	/bin/rm -f $(SRC_DIR)/kerninst.ko $(SRC_DIR)/kerninst.mod.c $(SRC_DIR)/*.o
