PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	cd lxnet && $(MAKE) win32-debug
	cd lua-5.1.4 && $(MAKE) ansi

win32-release:
	cd lxnet && $(MAKE) win32-release
	cd lua-5.1.4 && $(MAKE) ansi

linux-debug:
	cd lxnet && $(MAKE) linux-debug
	cd lua-5.1.4 && $(MAKE) linux

linux-release:
	cd lxnet && $(MAKE) linux-release
	cd lua-5.1.4 && $(MAKE) linux

cleanall:
	cd lxnet && $(MAKE) cleanall
	cd lua-5.1.4 && $(MAKE) clean
