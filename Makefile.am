ACLOCAL_AMFLAGS = -I m4 -I m4/libpoke
SUBDIRS = jitter gl maps pickles gl-libpoke libpoke poke utils \
          doc man testsuite etc po

EXTRA_DIST = INSTALL.generic DEPENDENCIES

noinst_SCRIPTS = run

# Support for generating a cyclomatic complexity report using the
# pmccabe tool.

CYCLO_SRCS = ${top_srcdir}/poke/*.[ch] $(top_srcdir)/libpoke/*.[ch]

cyclo-$(PACKAGE).html: $(CYCLO_SRCS)
	$(PMCCABE) $(CYCLO_SRCS) \
		| sort -nr \
		| $(AWK) -f ${top_srcdir}/build-aux/pmccabe2html \
			-v lang=html -v name="$(PACKAGE_NAME)" \
			-v vcurl="https://git.savannah.gnu.org/gitweb/?p=$(PACKAGE).git;a=blob;f=%FILENAME%;hb=HEAD" \
			-v url="https://www.jemarch.net/poke" \
			-v css=${top_srcdir}/build-aux/pmccabe.css \
			-v cut_dir=${top_srcdir}/ \
			> $@-tmp
	mv $@-tmp $@

# Update the HACKING file from the contents of etc/hacking.org.
# This requires Emacs.

update-hacking:
	emacs ${srcdir}/etc/hacking.org --batch -f org-ascii-export-to-ascii --kill
	mv -f ${srcdir}/etc/hacking.txt ${srcdir}/HACKING

.PHONY = update-hacking
