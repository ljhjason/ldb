PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	cd lxnet && $(MAKE) win32-debug
	cd lua && $(MAKE) ansi
	cd luacjson && $(MAKE) win32-debug

win32-release:
	cd lxnet && $(MAKE) win32-release
	cd lua && $(MAKE) ansi
	cd luacjson && $(MAKE) win32-release

linux-debug:
	cd lxnet && $(MAKE) linux-debug
	cd lua && $(MAKE) linux
	cd luacjson && $(MAKE) linux-debug

linux-release:
	cd lxnet && $(MAKE) linux-release
	cd lua && $(MAKE) linux
	cd luacjson && $(MAKE) linux-release

cleanall:
	cd lxnet && $(MAKE) cleanall
	cd lua && $(MAKE) clean
