PLATS = win32-debug win32-release linux-debug linux-release
none:
	@echo "Please choose a platform:"
	@echo " $(PLATS)"

win32-debug:
	cd contrib/lib && $(MAKE) win32-debug
	cd tool/ldb && $(MAKE) win32-debug

win32-release:
	cd contrib/lib && $(MAKE) win32-release
	cd tool/ldb && $(MAKE) win32-release

linux-debug:
	cd contrib/lib && $(MAKE) linux-debug
	cd tool/ldb && $(MAKE) linux-debug

linux-release:
	cd contrib/lib && $(MAKE) linux-release
	cd tool/ldb && $(MAKE) linux-release

cleanall:
	cd contrib/lib && $(MAKE) cleanall
	cd tool/ldb && $(MAKE) cleanall

