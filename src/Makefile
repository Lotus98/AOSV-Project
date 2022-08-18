.PHONY: all clean

all:
	cd dev && $(MAKE) all && $(MAKE) tidy && $(MAKE) install
	cd lib && $(MAKE) all && $(MAKE) tidy
	cd usr && $(MAKE)

clean:
	cd dev && $(MAKE) uninstall && $(MAKE) clean
	cd lib && $(MAKE) clean
	cd usr && $(MAKE) clean
