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
