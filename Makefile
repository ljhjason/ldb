PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	cd lib && $(MAKE) win32-debug
	cd ldb && $(MAKE) win32-debug

win32-release:
	cd lib && $(MAKE) win32-release
	cd ldb && $(MAKE) win32-release

linux-debug:
	cd lib && $(MAKE) linux-debug
	cd ldb && $(MAKE) linux-debug

linux-release:
	cd lib && $(MAKE) linux-release
	cd ldb && $(MAKE) linux-release

cleanall:
	cd lib && $(MAKE) cleanall
	cd ldb && $(MAKE) cleanall

