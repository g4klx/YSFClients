SUBDIRS = YSFGateway YSFParrot YSFReflector
CLEANDIRS = $(SUBDIRS:%=clean-%)
INSTALLDIRS = $(SUBDIRS:%=install-%)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean: $(CLEANDIRS)

$(CLEANDIRS): 
	$(MAKE) -C $(@:clean-%=%) clean

install: $(INSTALLDIRS)

$(INSTALLDIRS): 
	$(MAKE) -C $(@:install-%=%) install

.PHONY: $(SUBDIRS) $(CLEANDIRS) $(INSTALLDIRS)
