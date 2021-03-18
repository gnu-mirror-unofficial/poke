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

sc_recfix_poke_rec:
	@$(RECFIX) $(top_srcdir)/etc/poke.rec || \
            { msg='integrity problems found in etc/poke.rec' $(_sc_say_and_exit) }

update-copyright-env = UPDATE_COPYRIGHT_HOLDER="Jose E. Marchesi"

gendocs_options_ = -I $(abs_builddir)/doc/
