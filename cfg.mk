manual_title=GNU Poke
old_NEWS_hash=d41d8cd98f00b204e9800998ecf8427e
po_file=does-not-exist
export _gl_TS_headers = *.h

local-checks-to-skip =                  \
   sc_tight_scope \
   sc_prohibit_gnu_make_extensions

sc_jemarchism_lets:
	@prohibit='(^|[ ])[Ll]ets( |$$)'				\
	in_vc_files='$(texinfo_suffix_re_)'				\
	halt='found use of "lets" in Texinfo source'			\
	  $(_sc_search_regexp)

sc_jemarchism_file_fd:
	@prohibit='FILE \*fd[,;]'                        \
	exclude=cfg.mk                                   \
	halt='do not use FILE *fd, use FILE *fp instead' \
	  $(_sc_search_regexp)

sc_rockdabootism_missing_space:
	@prohibit='[a-z]+\('                    \
	in_vc_files='\.[chl]$$'                 \
	exclude="([a-z]+\(3\)\.|poke\(wo\)men)" \
	halt='missing space before ('           \
	$(_sc_search_regexp)

sc_unitalicised_ie:
	@prohibit='i\.e\.'				  \
	in_vc_files='$(texinfo_suffix_re_)'		  \
	exclude='@i{i\.e\.}'                              \
	halt='found unitalicised "i.e." in Texinfo source.  Use @i{i.e.}' \
	  $(_sc_search_regexp)

sc_unitalicised_etc:
	@prohibit='\betc\b[^/]'				  \
	in_vc_files='$(texinfo_suffix_re_)'		  \
	exclude='@i{etc}'                              \
	halt='found unitalicised "etc" in Texinfo source.  Use @i{etc}' \
	  $(_sc_search_regexp)

sc_tabs_in_source:
	@prohibit='	'                    \
	in_vc_files='\.[chly]$$'                 \
	halt='found tabs in source.  Use spaces instead.'           \
	$(_sc_search_regexp)

sc_tests_listed_in_makefile_am:
	@find $(top_srcdir)/testsuite/ -name '*.pk' \
           | sed -e 's#^.*testsuite/##g' | sort > in-files
	@egrep -e '\.pk *\\$$' $(top_srcdir)/testsuite/Makefile.am \
           | sed -e 's/ *\\$$//' -e 's/^ *//' | sort  > in-makefile
	@cmp -s in-files in-makefile || \
          { diff -u in-files in-makefile; \
            msg='missing or extra tests in EXTRA_DIST' $(_sc_say_and_exit) }

sc_pvm_wrappers:
	@nm $(top_builddir)/libpoke/libpvmjitter_la-pvm-vm2.o \
           | grep 'U ' | awk '{ print $$2; }' \
           | sort | uniq | grep -v _GLOBAL_OFFSET_TABLE_ > globals-list
	@awk '/^wrapped-functions/,/^end/ { print $$1; } /^wrapped-globals/,/^end/ { print $$1; }' $(top_srcdir)/libpoke/pvm.jitter \
           | sort | uniq | grep -v wrapped-functions | grep -v wrapped-globals \
           | grep -v end > wrapped-list
	@cmp -s globals-list wrapped-list || \
          { diff -u globals-list wrapped-list; \
            msg='found non-wrapped globals in pvm.jitter' $(_sc_say_and_exit) }

sc_recfix_poke_rec:
	@$(RECFIX) $(top_srcdir)/etc/poke.rec || \
            { msg='integrity problems found in etc/poke.rec' $(_sc_say_and_exit) }

update-copyright-env = UPDATE_COPYRIGHT_HOLDER="Jose E. Marchesi"

gendocs_options_ = -I $(abs_builddir)/doc/

# Override the following two (non-consecutive) lines in maint.mk:
#   gl_public_submodule_commit ?= public-submodule-commit
#   submodule-checks ?= no-submodule-changes public-submodule-commit
# That default prevents us from using commits from non-main branches in
# submodules; this is notably a problem with Jitter, which keeps specific
# public branches for poke releases; but according to the test in maint.mk
# those branches do not count as "public".
submodule-checks = no-submodule-changes
gl_public_submodule_commit =

# Do not run the style check for indentation.
INDENT_SOURCES =
