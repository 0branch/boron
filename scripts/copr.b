#!/usr/bin/boron -sp
/*
	Copr - Compile Program v0.2.0
	Copyright 2021 Karl Robillard
	Documentation is at http://urlan.sourceforge.net/copr.html
*/

bsd: linux: macx: mingw: sun: unix: win32: none

~copr~: context [
project_file: %project.b
target-os: none
cli_options: ""
verbose: 2

archive:
show_help:
do_clean:
dry_run:
clear_caches:
debug_mode: false
build_env:
jobs: none
;sub-projects: []

~execute~: :execute		; Conflict with boron option; put options in context?

/*
	The cache-file contains the file-inf & job-list tables.
	file-inf: [
		size date filename [depend-file ...]	; four values per entry
		...
	]
	job-list: [
		source output [summary command ...]		; three values per entry
		...
	]
*/
file-inf: []
job-list: []
file-id-map: make hash-map! 64	; Maps filename to file-inf slices.

benv: context [
	asm: "as"
	cc:  "cc -c -pipe -Wall -W -Wdeclaration-after-statement"
	c++: "c++ -c -pipe -Wall -W"
	link_c: "cc"
	link_c++: "c++"
	obj_suffix: %.o
	lib_prefix: %lib
	lib_suffix: %.a
	shlib_suffix: %.so
	debug: "-g -DDEBUG"
	debug-link: ""
	release: "-O3 -DNDEBUG"
	opengl-link: "-lGL"
	sys_console:
	sys_windows: ""
	qt_inc: %/usr/include/qt5
	qt_c++: "-fPIC"
	qt_release: "-DQT_NO_DEBUG"
	moc: "moc-qt5"
	qrc: "rcc-qt5"
]

compile-rules: [
	ext-map: [
		%.c     rule-c
		%.cpp   rule-c++
		%.cxx   rule-c++
		%.s     rule-asm
		%.asm   rule-asm
		%.qrc   rule-qrc
	]

	src-base:
	src-file:
	out-file: none
	job-output: []

	append-obj: does [
		if eq? obj_suffix find/last out-file '.' [
			append-pair obj-args ' ' out-file
		]
	]

	; Emit job-list entry for a source file.
	emit: func [rule src /extern has-gen has-c++] [
		either rule [
			set-files rule src
		][
			rule: prepare-rule src
		]
		if eq? 'gen_dir rule/2/1 [
			has-gen: true
		]
		if same? rule rule-c++ [
			has-c++: true
		]

		clear job-output
		append-obj
		do third rule

		either chain: get pick rule 5 [
			push-rule rule

			set-files chain out-file
			append-obj
			push-rule chain
		][
			push-rule rule
		]
		finish-job src job-output
	]

	emit-moc: func [src] [emit rule-moc src]

	push-rule: func [rule] [
		push-command [first rule ' ' src-file] pick rule 4
		append job-output out-file
	]

	set-files: func [rule src /extern src-base src-file out-file] [
		ext: find/last src '.'
		src-file: src
		src-base: slice second split-path src ext
		out-file: rejoin second rule
	]

	prepare-rule: func [src /extern src-base src-file out-file] [
		ext: find/last src '.'
		ifn rule: select ext-map ext [
			error join "No compile rule defined for " ext
		]
		rule: get rule
		src-file: src
		src-base: slice second split-path src ext
		out-file: rejoin second rule
		rule
	]

	; These rules are bound to target (copy of clean-target) & benv.
	; Rule fields: [verb  out-file  cache_dependencies  command  chain]

	rule-asm: [
		"Assemble" [
			obj_dir src-base obj_suffix
		][
			;update-info-include src-file include_paths defines
		][
			asm ' ' src-file opt_asm
			ne-string select custom_flags src-file
			" -o " out-file
		]
		none
	]
	rule-c: [
		"Compile" [
			obj_dir src-base obj_suffix
		][
			update-info-include src-file include_paths defines
		][
			cc ' ' src-file ' ' either debug_mode debug release
			inc-args opt_compile
			ne-string select custom_flags src-file
			" -o " out-file
		]
		none
	]
	rule-c++: [
		"Compile" [
			obj_dir src-base obj_suffix
		][
			update-info-include src-file include_paths defines
		][
			c++ ' ' src-file ' ' either debug_mode debug release
			inc-args opt_compile opt_compile_cxx
			ne-string select custom_flags src-file
			" -o " out-file
		]
		none
	]
	rule-moc: [
		"Qt moc" [
			gen_dir %moc_ src-base %.cpp
		]
			none
		[
			moc ' ' src-file " -o " out-file
		]
		rule-c++
	]
	rule-qrc: [
		"Qt resource" [
			gen_dir src-base %.cpp
		][
			dep: make block! 16
			parse read/text src-file [some[
				thru "<file" thru '>' tok: to "</file>" :tok (
					append dep tok: to-file tok
					cache-info tok none
				)
			]]
			cache-info src-file dep
		][
			qrc ' ' src-file
			ne-string select custom_flags src-file
			" -o " out-file
		]
		rule-c++
	]

	rule-blocks: reduce [rule-asm rule-c rule-c++ rule-moc rule-qrc]
]

forall args [
	switch first args [
		"-a" [archive: true]
		"-h" [show_help: true]
		"-c" [do_clean: true]
		"-d" [debug_mode: true]
		"-e" [build_env: second ++ args]
		"-j" [
			jobs: to-int second ++ args
			if lt? jobs 2 [jobs: none]
			if gt? jobs 6 [jobs: 6]
		]
		"-r" [dry_run: true  ~execute~: func [cmd] [0]]
		;"-s" [error "Show statistics not implemented"]
		"-t" [target-os: to-word second ++ args]
		"-v" [verbose: to-int second ++ args]
		"--clear" [clear_caches: true]
		[
			either find a1: first args ':' [
				append-pair cli_options a1 '^/'
			][
				project_file: to-file a1
			]
		]
	]
]

; Show help after parsing args to get any project_file.
if show_help [
	context [
		usage: {copr version 0.2.0

Copr Options:
  -a              Archive source files.
  -c              Clean up (remove) previously built files & project cache.
  -d              Build in debug mode.         (default is release)
  -e <env_file>   Override build environment.
  -h              Print this help and quit
  -j <count>      Use specified number of job threads.  (1-6, default is 1)
  -r              Do a dry run and only print commands.
  -t <os>         Set target operating system. (default is auto-detected)
  -v <level>      Set verbosity level.         (0-4, default is 2)
  --clear         Remove caches of all projects.
  <project>       Specify project file         (default is project.b)
  <opt>:<value>   Set project option

Project Options:}

		either all [
			exists? project_file
			opt: select load project_file 'options
		][
			parse opt [some[
				tok:
				set-word! (
					append usage rejoin ["^/  " pw: to-text first tok ':']
				)
				| string! (
					append usage join skip "               " size? pw first tok
				)
				| skip
			]]
		][
			append usage "^/  none"
		]
		prin terminate usage '^/'
	]
	quit
]

cache-dir: rejoin any [
	select [
		Windows [terminate getenv "LOCALAPPDATA" '\' %copr\ ]
		Darwin  [terminate getenv "HOME" '/' %Library/Caches/copr/]
	] environs/os
	[terminate getenv "HOME" '/' %.cache/copr/]
	;[%/tmp/copr/]
]

if clear_caches [
	if exists? cache-dir [
		foreach f read cache-dir [
			if eq? 16 size? f [
				delete join cache-dir f
			]
		]
	]
	quit
]

ifn exists? project_file [
	print [project_file "does not exist; Please specify project file."]
	quit/return 66	; EX_NOINPUT
]

active-env: func [blk] [do blk]

ifn target-os [
	switch target-os: environs/os [
		darwin  [target-os: 'macx]
		windows [target-os: 'win32]
	]
]
set in binding? 'unix target-os :active-env
if find [bsd linux sun] target-os [unix: :active-env]

if benv-target: select [
	macx [
		shlib_suffix: %.dylib
		opengl-link:  "-framework OpenGL"
		qt_c++: none
		moc: "$(QTDIR)/bin/moc"
		qrc: "$(QTDIR)/bin/rcc"
	]
	mingw [
		asm: "x86_64-w64-mingw32-as"
		cc:  "x86_64-w64-mingw32-gcc -c -pipe -Wall -W -Wdeclaration-after-statement"
		c++: "x86_64-w64-mingw32-g++ -c -pipe -Wall -W"
		link_c: "x86_64-w64-mingw32-gcc"
		link_c++: "x86_64-w64-mingw32-g++"
		opengl-link: "-lopengl32"
		sys_windows: " -mwindows"
		qt_inc: %/usr/x86_64-w64-mingw32/sys-root/mingw/include/qt5
		qt_c++: none
		moc: "/usr/x86_64-w64-mingw32/bin/qt5/moc"
		qrc: "/usr/x86_64-w64-mingw32/bin/qt5/rcc"
	]
	win32 [
		asm: "ml64.exe"
		cc:  "cl.exe /nologo -W3 -D_CRT_SECURE_NO_WARNINGS"
		c++: join cc " /EHsc"
		link_c: link_c++: "link.exe /nologo"
		lib_prefix: ""
		lib_suffix: %.lib
		shlib_suffix: %.dll
		debug:		"/MDd -Zi -DDEBUG"
		debug-link: "/DEBUG"
		release:	"/MD -O2 -DNDEBUG"
		opengl-link: "-lopengl32"
		sys_console: " /subsystem:console"
		sys_windows: " /subsystem:windows"
		qt_inc: "$(QTDIR)\include"
		qt_c++: none
		moc: "$(QTDIR)\bin\moc.exe"
		qrc: "$(QTDIR)\bin\rcc.exe"
	]
] target-os [
	do bind benv-target benv
]

if build_env [
	do bind/secure load build_env benv
]

;-------------------------------
; Include Parser

context [
	dtok:
	use-include: none
	inc-stack: []

	space: charset { ^-}
	set 'define-symbol charset
		{_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz}

	if-case: func [enable /extern use-include] [
		if last inc-stack [use-include: enable]
	]
	push-if: func [enable] [
		append inc-stack use-include
		if-case enable
	]

	def-exp: [
		some space opt "defined(" dtok: some define-symbol :dtok thru '^/'
	]

	set 'get-includes func [
		code defs inc-blk
		/extern dtok use-include ~qobject
	][
		use-include: true
		clear inc-stack
		defs: copy defs
		parse code [any[
			some space
		  | '#' any space [
				"include" some space '"' dtok: to '"' :dtok (
					if use-include [append inc-blk copy dtok]
				)
			  | "define"  def-exp   (if use-include [append defs dtok])
			  | "ifdef"   def-exp   (push-if find defs dtok)
			  | "ifndef"  def-exp   (push-if not find defs dtok)
			  | "if"      def-exp   (push-if either eq? dtok "1"
										[true] [find defs dtok])
			  | "else"    thru '^/' (if-case not use-include)
			  | "elif"    def-exp   (if-case find defs dtok)
			  | "endif"   thru '^/' (use-include: pop inc-stack)
			]
		  | "/*" thru "*/"
		  | "Q_OBJECT" thru '^/' (~qobject: true)
		  | thru '^/'
		]]
	]
]

append-pair: func [series a b] [append append series a b]
new-block: does [make block! 0]

; Read into an established buffer to avoid mallocs.
~rbuf: make string! 16384
read-buf: func [file] [
	read/into file ~rbuf
	~rbuf	; Read can return none
]


debug-inc-print: either eq? verbose 4 [func [blk] [print blk]] none

find_include_file: func [
	; Checks if a file exists in the current directory or any set of paths.
	file  file!/string!
	paths block!
][
	debug-inc-print [' ' file]
	if exists? file [return file]

	forall paths [
		path: to-file paths/1
		term-dir path
		full: join path file

		debug-inc-print ["   " full]
		if exists? full [return full]
	]
	none
]

included_files: func [
   /* Returns a block of the files which are included in a C/C++ file.
	  Adds    #include "file.h"
	  Ignores #include <file.h>
   */
	file	file!/string!	; File to check for #include statements
	paths	block!			; Paths to check for included files
	defs	block!			; Defined C pre-processor symbols
][
	out: new-block

	ifn exists? file [
		print ["included_files:" file "not found"]
		return out
	]

	get-includes read-buf file defs out
	blk: intersect out out   ; Removes duplicates.

	if :debug-inc-print [print ["Finding include files for" file] probe blk]

	; Get path of each included file. Drop and complain if not found.
	clear out
	forall blk [
		tmp: find_include_file first blk paths
		either tmp [
			append out tmp
		][
			print rejoin
				["included_files: " first blk " (from " file ") not found!"]
		]
	]
	out
]

;-------------------------------
; Private Helpers

compile-data: context [
	; Program for the entire project; referenced by job-list entries.
	cmd-store: make block! 64

	inc-args: make string! 128
	obj-args: copy inc-args
	lib-args: copy inc-args
	has-gen:
	has-c++: false

	tcmd: none			; command block of current target.
	moc-files: []

	reset-data: does [
		clear inc-args
		clear obj-args
		clear lib-args
		has-gen:
		has-c++: false
		clear moc-files
		tcmd: tail cmd-store
	]

	; Low-level poke (or append) to cache file-inf & file-id-map.
	cache-info: func [file dep] [
		either fid: pick file-id-map file [
			poke fid 4 dep
		][
			either info: info? file [
				info: slice info 1,3
			][
				; Dummy value. File may be generated later.
				info: reduce [0 now/date file]
			]
			append file-inf mark-sol info
			append/block file-inf dep
			fid: slice skip tail file-inf -4 4
			poke file-id-map third info fid
		]
	]

	update-info-include: func [
		file inc_paths defs
		/local it
		/extern ~qobject
	][
		ifn inc_paths [inc_paths: []]
		ifn defs [defs: []]
		~qobject: false
		depend: included_files file inc_paths defs
		if ~qobject [append moc-files file]
		either empty? depend [
			pval: none
		][
			more: new-block
			foreach it depend [
				append more update-info-include it inc_paths defs
			]
			pval: depend: union depend more
		]
		cache-info file pval
		depend
	]

	; None to Empty functions.
	ne-string: func [val] [either val val ""]

	finish-job: func [src out] [
		if block? out [
			out: either eq? 1 size? out [
				first out
			][
				copy out
			]
		]
		append/block append job-list mark-sol src out
		append/block job-list slice tcmd tail cmd-store
		set 'tcmd tail cmd-store
	]

	push-command: func [summary com] [
		append-pair cmd-store
			either summary [mark-sol rejoin summary] none
			mark-sol rejoin com
	]

	stringify: func [blk str-out flag /local it] [
		if blk [
			foreach it blk [append-pair str-out flag it]
		]
	]

	set 'compile-target func [
		tname tfile spec block! link-cmd block!
		/extern name output_file
		/local f
	][
		tc: create-target spec
		ifn tc/source_files [error join "No source specified for " tname]
		reset-data

		bind compile-rules/rule-blocks tc

		do bind [
			name: tname
			output_file: tfile

			stringify include_paths inc-args " -I"
			stringify link_paths    lib-args " -L"
			stringify link_libs     lib-args " -l"

			foreach f source_files [
				compile-rules/emit none f
			]
			ifn empty? moc-files [
				foreach f intersect moc-files moc-files [
					compile-rules/emit-moc f
				]
			]
			do bind bind link-cmd compile-data tc

			ifn dry_run [
				if all [obj_dir not exists? obj_dir] [
					make-dir/all obj_dir
				]
				if all [has-gen gen_dir not exists? gen_dir] [
					make-dir/all gen_dir
				]
			]
		] tc
		vprint 4 [mold tc]
	]

	set 'gen func [output dep commands /local it] [
		reset-data
		cache-info dep none
		foreach it split trim commands '^/' [
			push-command ["Build " output] [trim it]
		]
		finish-job dep output
	]
]

;-------------------------------
; Project Commands

do-any: func [file] [
	if exists? file [do file]
]

; Spec is a table of [name: value "Help"] triplets.
set 'options func [spec block!] [
	do spec
	do-any %project.config

	; Validate command line options.
	w1: words-of context spec
	w2: collect set-word! to-block cli_options
	ifn empty? w2: difference w2 w1 [
		error join "Invalid project options: " mold w2
	]
	do cli_options
]

default_block: none
set 'default func [blk] [
	set 'default_block blk
]

exe-link: bind [
	push-command ["Link " output_file] [
		either has-c++ link_c++ link_c
		either cfg_console sys_console sys_windows
		either debug_mode debug-link ""
		opt_link obj-args lib-args " -o " output_file
	]
	finish-job 'exe-job output_file
] benv

; Build commands for an executable binary.
set 'exe func [basename spec] [
	outf: basename
	if any [:win32 :mingw] [outf: join basename %.exe]
	compile-target basename outf spec exe-link
]

; Build commands for a static library.
set 'lib func [basename spec] [
	outf: rejoin [benv/lib_prefix basename benv/lib_suffix]
	compile-target basename outf spec [
		; TODO: Support win32, etc.
		either empty? link_libs [
			push-command ["Archive " output_file] [
				"ar rc " output_file opt_link obj-args
			]
		][
			; Concatenate other libraries.
			push-command none [
				"ld -Ur" obj-args lib-args
				" -o " ne-string obj_dir name %lib.o
			]
			push-command ["Archive " output_file] [
				"ar rc " output_file ' ' ne-string obj_dir name %lib.o
			]
		]
		push-command none ["ranlib " output_file]
		ifn debug_mode [
			push-command none ["strip -d " output_file]
		]
		finish-job 'library-job output_file
	]
]

; Build commands for a shared library.
set 'shlib func [basename spec] [
	if block? basename [
		version: second basename
		basename: first basename
	]
	outf: rejoin [benv/lib_prefix basename benv/shlib_suffix]
	link-cmd: exe-link

	either :unix [
		if version [
			lib_base: outf
			lib_m: rejoin [lib_base '.' first version]
			outf: rejoin [
				; Handles any number of version elements.
				lib_base '.' construct mold version [',' '.']
			]
			link-cmd: append copy slice exe-link -3 [
				push-command none ["ln -sf " output_file ' ' lib_m]
				push-command none ["ln -sf " output_file ' ' lib_base]
				finish-job 'library-job reduce [output_file lib_m lib_base]
			]
		]
	][
		version: none
	]

	compile-target basename outf
		append copy spec bind [
			unix [
				cflags "-fPIC"
				lflags "-shared"
				if version [
					lflags join "-Wl,-soname," lib_m
				]
			]
			macx [
				lflags "-dynamiclib"
				lflags join "-install_name @rpath/" output_file
			]
			win32 [
				;cflags "-MT"
				lflags "/DLL"
			]
		] target-func
		link-cmd
]

set 'sub-project func [def block!] [
	parse def [some[
		tok:
		file! (lpath: terminate first tok '/')
	  | word! file!		; e.g. lib %my-module
	  | string! (lconfig: trim/lines first tok)
	]]

	ifn lpath [error "Invalid sub-project"]
	/*
	if lconfig [
		write join lpath %project.config lconfig
	]
	eval-project lpath
	*/
	vprint 1 ["Sub-project" lpath "ignored!"]
]

; m2 compatibility
set 'rule :gen

;-------------------------------

vprint: func [level msg] [
	ifn lt? verbose level [print msg]
]

clean-target: [
	name:
	output_file:
	output_dir:
	defines: none

	obj_dir: %.copr/obj/
	gen_dir: %.copr/

	include_paths:
	link_paths:
	link_libs:
	source_files: none

	opt_asm:
	opt_compile:
	opt_compile_cxx:
	opt_link: none

	custom_flags: []		; Pairs of files & flags.

	cfg_console:
	command: none
]

target-collect: make context clean-target [
	white: charset " ^-^/"
	non-white: complement copy white
	parse-white: func [str string!] [
		blk: make block! 8
		parse str [any[
			any white tok: some non-white :tok (append blk tok)
		]]
		if empty? blk [append blk str]
		blk
	]

	+blk: func ['member list] [
		ifn block? blk: get member [
			set member blk: new-block
		]
		append blk either string? list
			[parse-white list]
			[reduce list]
	]

	+blk1: func ['member item] [
		ifn block? blk: get member [
			set member blk: new-block
		]
		append blk item
	]

	+opt: func [options str] [
		append-pair options ' ' trim/lines str
	]

	add_def: func [str] [
		parse str [some[
			thru "-D" tok: some define-symbol :tok (+blk1 defines tok)
		]]
	]

	; Re-direct clean-target values to this context.
	reset: does bind clean-target self

	set 'create-target func [spec /local str] [
		; Fill target-collect.
		reset
		foreach str [opt_asm opt_compile opt_compile_cxx opt_link] [
			set str make string! 80
		]
		do default_block
		do spec

		; Transfer values to new target.
		tc: context clean-target
		set words-of tc values-of self
		tc
	]
]

target-func: context bind [
	into:		func [dir] [set 'output_dir term-dir dir]
	objdir:		func [dir] [set 'obj_dir    term-dir dir]

	aflags:		func [str string!] [+opt opt_asm  str]
	cflags:		func [str string!] [+opt opt_compile str		add_def str]
	cxxflags:	func [str string!] [+opt opt_compile_cxx str	add_def str]
	lflags:		func [str string!] [+opt opt_link str]

	console:	does [cfg_console: true]
	opengl:		does [lflags benv/opengl-link]  ; Using OpenGL libraries.

	; m2 compatibility
	debug:		none	;does [debug_mode: true]
	release:	none	;does [debug_mode: false]

	include-define: func [str string!] [+blk defines str]

	include_from: func [list string!/file!/block!] [
		+blk include_paths list
	]

	sources: func [arg block! /flags fstr] [
		+blk source_files arg
		if flags [
			sstr: join ' ' fstr
			forall arg [
				append-pair custom_flags first arg sstr
			]
		]
	]

	sources_from: func [path files block! /local it] [
		terminate path '/'
		+blk source_files map it copy files [join path it]
	]

	libs: func [list string!/file!/block!] [
		+blk link_libs list
	]

	libs_from: func [dir list] [
		+blk1 link_paths dir
		+blk link_libs list		; libs list
	]

	~qt-module: func [name] pick [[
		include_from join benv/qt_inc name
		libs join "Qt5" skip name 3
	][
		include_from rejoin ["/Library/Frameworks" name ".framework/Headers"]
		lflags join "-framework " next name
	]] none? :macx

	qt: func [modules block!] [
		cxxflags benv/qt_release
		if opt: benv/qt_c++ [cxxflags opt]

		include_from benv/qt_inc

		~qt-module "/QtCore"
		if find modules 'widgets [
			ifn find modules 'gui [~qt-module "/QtGui"]
		]
		foreach it modules [
			iname: select [
				gui			 "/QtGui"
				widgets		 "/QtWidgets"
				network		 "/QtNetwork"
				concurrent	 "/QtConcurrent"
				opengl		 "/QtOpenGL"
				printsupport "/QtPrintSupport"
				svg			 "/QtSvg"
				sql			 "/QtSql"
				xml			 "/QtXml"
				core		 ignore
			] it
			switch type? iname [
				string! [~qt-module iname]
				none!   [error join "Invalid Qt module " it]
			]
		]
	]
] target-collect

compile-rules: context bind bind compile-rules benv compile-data

;-------------------------------
; Load or create cache

; Change to project dir if needed.
set [path project_file] split-path project_file
if path [change-dir path]

csum: checksum join current-dir project_file
cache-file: join cache-dir slice to-text csum 2,16

if do_clean [
	if exists? cache-file [delete cache-file]
	do bind load project_file target-func
	; delete target (obj_dir gen_dir)?
	foreach [src out cmd] job-list [
		either block? out [
			forall out [try [delete first out]]
		][
			try [delete out]
		]
	]
	quit
]

;info-size: :second
info-time: :third

;cinf-size: :first
cinf-time: :second
cinf-file: :third

current-map:  make hash-map! 64
outdated-map: make hash-map! 64
cache-modified: false

either all [
	exists? cache-file
	gt? info-time info? cache-file info-time info? project_file
][
	vprint 2 ["Loading cache" cache-file]
	set [file-inf job-list] load cache-file

	; Mark files as current or not.
	it: file-inf
	forall it [
		if outi: info? file: cinf-file it [
			which-map: either eq? info-time outi cinf-time it
				[current-map]
				[outdated-map]
			poke which-map file slice it 4
		]

		it: skip it 3
	]
][
	vprint 2 ["Evaluating" project_file]

	; NOTE: The project_file is bound to target-func before evaluation so
	; that the words can be overridden inside the project itself.
	do bind load project_file target-func

	cache-modified: true
	ifn dry_run [
		ifn exists? cache-dir [make-dir cache-dir]
	]
]

if archive [
	tmp: new-block
	foreach [src out cmd] job-list [
		ifn word? src [
			append tmp src
			if dep: select file-inf src [
				forall dep [
					append tmp first dep
				]
			]
		]
	]

	append-pair clear ~rbuf "tar czf project.tar.gz " project_file
	foreach src intersect tmp tmp [
		append-pair ~rbuf ' ' src
	]

	either dry_run [print ~rbuf] [~execute~ ~rbuf]
	quit
]

vprint 3 [mold file-inf mold job-list]

;-------------------------------
; Gather commands needed to rebuild modified sources and/or missing outputs.

; There is no formal tracking of dependencies between targets.
; These functions determine the library dependencies.
lib-list: new-block
record-built-lib: func [file] [
	; Convert library path to a link command.
	if block? file [file: first file]	; For versioned shlib
	parse to-string file [any [thru '/'] opt "lib" name: to '.' :name]
	append lib-list join " -l" name
]
requires-built-lib: func [cmds /local it] [
	; Assuming exe-link command.  TODO: Handle libs.
	if string? cmds: last cmds [
		foreach it lib-list [
			if find cmds it [return true]
		]
	]
	false
]

outdated?: func [src] [
	ifn fentry: pick current-map src [
		return true
	]
	if dep: pick fentry 4 [
		forall dep [
			ifn pick current-map first dep [
				return true
			]
		]
	]
	false
]

outputs-missing?: func [out] [
	if block? out [
		forall out [
			ifn exists? first out [return true]
		]
		return false
	]
	not exists? out
]

dep-changed: false
update-inf: none
build-jobs: make block! 64		; job-list slices

record-job: func [it] [
	append/block build-jobs slice it 3
]

jlist: job-list
either cache-modified [
	forall jlist [
		record-job jlist
		jlist: skip jlist 2
	]
][
	forall jlist [			; (src out cmds)
		src: first  jlist
		out: second jlist
		either word? src [
			if any [
				dep-changed
				requires-built-lib third jlist	;cmds
				outputs-missing? out
			][
				dep-changed: false
				if eq? 'library-job src [
					record-built-lib out
				]
				record-job jlist
			]
		][
			if any [outdated? src outputs-missing? out] [
				record-job jlist
				dep-changed: true
			]
		]
		jlist: skip jlist 2
	]

	update-inf: func [src] [
		if all [
			not word? src
			fentry: pick outdated-map src
			srci: info? src
		][
			change fentry slice srci 1,2		; Update size & time.
			remove/key outdated-map src
			set 'cache-modified true
		]
	]
]

if empty? build-jobs [
	vprint 1 "All targets are up to date"
	quit
]

;-------------------------------
; Run jobs

switch verbose [
	0 [
		report: none
		report-fail: [print [/*target/name*/ summary "failed!"]]
	]
	1 [
		cmd-count: 0
		foreach it build-jobs [
			cmd-count: add cmd-count div size? third it 2
		]
		cmd-done: 0
		report: [
			if summary [
				print format [-2 "% " 74] [
					div mul 100 cmd-done cmd-count
					summary
				]
			]
			++ cmd-done
		]
		report-fail: [print cmd]
	][
		report: [print cmd]
		report-fail: none
	]
]

rc: 0
start-time: now
either all [jobs not dry_run] [
	vprint 2 ["Running" jobs "jobs"]
	next-worker:
	worker-ports: []
	job-record: []		; List of jobs sent to each worker.
	loop jobs [
		append/block job-record new-block
		append worker-ports thread/port {{
			while [string? val: read thread-port] [
				count: 0
				foreach cmd split val '^/' [
					++ count
					ifn zero? rc: execute cmd [break]
				]
				write thread-port to-coord [rc count]
				ifn zero? rc [break]
			]
		}}
	]

	workers-busy?: does [
		foreach it job-record [
			ifn empty? it [return true]
		]
		false
	]

	wait-all-jobs: func [/extern rc summary cmd] [
		while [workers-busy?] [
			wp: wait worker-ports
			either port? wp [
				status: read wp

				worker-it: find worker-ports wp
				records: pick job-record index? worker-it
				it: records/1/3
				loop second status [
					summary: first ++ it
					cmd:     first ++ it
					do report
				]
				either zero? first status [
					update-inf records/1/1
					remove records
				][
					rc: first status
					do report-fail
					close wp		; Join with exited worker thread.
					remove skip job-record sub index? worker-it 1
					remove worker-it
				]
			][
				print "Wait worker-ports returned none!"
				;throw 70		; EX_SOFTWARE
			]
		]
	]

	foreach job build-jobs [
		if word? first job [
			wait-all-jobs		; Finish target dependencies.
		]

		ifn records: pick job-record index? next-worker [
			break
		]
		append/block records job
		wp: first ++ next-worker
		if tail? next-worker [next-worker: worker-ports]

		; All commands of a job are run serialized on one thread.
		worker-cmds: make string! 128
		foreach [summary cmd] third job [
			append-pair worker-cmds cmd '^/'
		]
		write wp worker-cmds
	]
	wait-all-jobs

	; Quit and join with worker threads.
	forall worker-ports [close first worker-ports]
][
	catch [
		foreach job build-jobs [
			foreach [summary cmd] third job [
				do report
				ifn zero? rc: ~execute~ cmd [
					do report-fail
					throw rc
				]
			]
			update-inf first job
		]
	]
]

ifn dry_run [
	foreach fentry values-of outdated-map [
		if srci: info? cinf-file fentry [
			change fentry slice srci 1,2		; Update size & time.
			cache-modified: true
		]
	]
	if cache-modified [
		write cache-file serialize reduce [file-inf job-list]
	]
	if zero? rc [
		rt: to-string sub now start-time
		vprint 1 ["Build time:" slice rt skip find rt '.' 3]
	]
]
quit/return rc
]
