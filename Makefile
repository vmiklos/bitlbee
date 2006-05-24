###########################
## Makefile for BitlBee  ##
##                       ##
## Copyright 2002 Lintux ##
###########################

### DEFINITIONS

-include Makefile.settings

# Program variables
objects = account.o bitlbee.o crypting.o help.o ini.o ipc.o irc.o irc_commands.o nick.o query.o root_commands.o set.o storage.o storage_text.o url.o user.o util.o
headers = account.h bitlbee.h commands.h conf.h config.h crypting.h help.h ini.h ipc.h irc.h log.h nick.h query.h set.h sock.h storage.h url.h user.h protocols/http_client.h protocols/md5.h protocols/nogaim.h protocols/proxy.h protocols/sha.h protocols/ssl_client.h
subdirs = protocols

ifeq ($(ARCH),Windows)
objects += win32.o
else
objects += unix.o conf.o log.o
endif

# Expansion of variables
subdirobjs = $(foreach dir,$(subdirs),$(dir)/$(dir).o)
CFLAGS += -Wall

all: $(OUTFILE)
	$(MAKE) -C doc

uninstall: uninstall-bin uninstall-doc 
	@echo -e '\nmake uninstall does not remove files in '$(DESTDIR)$(ETCDIR)', you can use make uninstall-etc to do that.\n'

install: install-bin install-doc
	@if ! [ -d $(DESTDIR)$(CONFIG) ]; then echo -e '\nThe configuration directory $(DESTDIR)$(CONFIG) does not exist yet, don'\''t forget to create it!'; fi
	@if ! [ -e $(DESTDIR)$(ETCDIR)/bitlbee.conf ]; then echo -e '\nNo files are installed in '$(DESTDIR)$(ETCDIR)' by make install. Run make install-etc to do that.'; fi
	@echo

.PHONY:   install   install-bin   install-etc   install-doc \
        uninstall uninstall-bin uninstall-etc uninstall-doc \
        all clean distclean tar $(subdirs)

Makefile.settings:
	@echo
	@echo Run ./configure to create Makefile.settings, then rerun make
	@echo

clean: $(subdirs)
	rm -f *.o $(OUTFILE) core utils/bitlbeed encode decode

distclean: clean $(subdirs)
	rm -f Makefile.settings config.h
	find . -name 'DEADJOE' -o -name '*.orig' -o -name '*.rej' -o -name '*~' -exec rm -f {} \;

install-doc:
	$(MAKE) -C doc install

uninstall-doc:
	$(MAKE) -C doc uninstall

install-bin:
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 0755 $(OUTFILE) $(DESTDIR)$(BINDIR)/$(OUTFILE)

uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(OUTFILE)

install-dev:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	install -m 0644 $(headers) $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(PCDIR)
	install -m 0644 bitlbee.pc $(DESTDIR)$(PCDIR)

uninstall-dev:
	rm -f $(foreach hdr,$(headers),$(DESTDIR)$(INCLUDEDIR)/$(hdr))
	-rmdir $(DESTDIR)$(INCLUDEDIR)
	rm -f $(DESTDIR)$(PCDIR)/bitlbee.pc

install-etc:
	mkdir -p $(DESTDIR)$(ETCDIR)
	install -m 0644 motd.txt $(DESTDIR)$(ETCDIR)/motd.txt
	install -m 0644 bitlbee.conf $(DESTDIR)$(ETCDIR)/bitlbee.conf

uninstall-etc:
	rm -f $(DESTDIR)$(ETCDIR)/motd.txt
	rm -f $(DESTDIR)$(ETCDIR)/bitlbee.conf
	-rmdir $(DESTDIR)$(ETCDIR)

tar:
	fakeroot debian/rules clean || make distclean
	x=$$(basename $$(pwd)); \
	cd ..; \
	tar czf $$x.tar.gz --exclude=debian --exclude=.bzr $$x

$(subdirs):
	@$(MAKE) -C $@ $(MAKECMDGOALS)

$(objects): %.o: %.c
	@echo '*' Compiling $<
	@$(CC) -c $(CFLAGS) $< -o $@

$(objects): Makefile Makefile.settings config.h

$(OUTFILE): $(objects) $(subdirs)
	@echo '*' Linking $(OUTFILE)
	@$(CC) $(objects) $(subdirobjs) -o $(OUTFILE) $(LFLAGS) $(EFLAGS)
ifndef DEBUG
	@echo '*' Stripping $(OUTFILE)
	@-$(STRIP) $(OUTFILE)
endif

encode: crypting.c
	$(CC) crypting.c protocols/md5.c $(CFLAGS) -o encode -DCRYPTING_MAIN $(CFLAGS) $(EFLAGS) $(LFLAGS)

decode: encode
	cp encode decode

ctags: 
	ctags `find . -name "*.c"` `find . -name "*.h"`
