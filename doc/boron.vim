" Vim syntax file
" Language:		Boron
" Maintainer:	Karl Robillard <wickedsmoke@users.sf.net>
" Filenames:	*.b
" Last Change:	21 Nov 2019
" URL:			http://urlan.sf.net/boron


" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" We are case insensitive
syn case ignore

if version < 600
  set iskeyword=@,48-57,?,!,.,+,-,*,&,\|,=,_,~
else
  setlocal iskeyword=@,48-57,?,!,.,+,-,*,&,\|,=,_,~
endif

" Yer TODO highlighter
syn keyword	boronTodo	contained TODO FIXME

" Comments
syn match       boronComment	";.*$" contains=boronTodo
syn region		boronCommentB	start="/\*" end="\*/" contains=boronTodo,boronCommentB


" Words
syn match       boronSetWord    "\a\k*:"
syn match       boronGetWord    ":\a\k*"
syn match       boronLitWord    "'\a\k*"
syn match       boronWord       "\a\k*"
"syn match       boronWordPath   "[^[:space:]]/[^[:space]]"ms=s+1,me=e-1

" Booleans
syn keyword     boronBoolean    true false on off yes no

" Values
" Integers
syn match       boronInteger    "\<[+-]\=\d\+\('\d*\)*\>"
" Decimals
syn match       boronDecimal    "[+-]\=\(\d\+\('\d*\)*\)\=[,.]\d*\(e[+-]\=\d\+\)\="
syn match       boronDecimal    "[+-]\=\d\+\('\d*\)*\(e[+-]\=\d\+\)\="
" Hex number
syn match       boronInteger    "\$\x\+"
syn match       boronInteger    "0x\x\+"
" Time
syn match       boronTime       "[+-]\=\(\d\+\('\d*\)*\:\)\{1,2}\d\+\('\d*\)*\([.,]\d\+\)\=\([AP]M\)\=\>"
syn match       boronTime       "[+-]\=:\d\+\([.,]\d*\)\=\([AP]M\)\=\>"
" Dates
" DD-MMM-YY & YYYY format
syn match       boronDate       "\d\{1,2}\([/-]\)\(Jan\|Feb\|Mar\|Apr\|May\|Jun\|Jul\|Aug\|Sep\|Oct\|Nov\|Dec\)\1\(\d\{2}\)\{1,2}\>"
" DD-month-YY & YYYY format
syn match       boronDate       "\d\{1,2}\([/-]\)\(January\|February\|March\|April\|May\|June\|July\|August\|September\|October\|November\|December\)\1\(\d\{2}\)\{1,2}\>"
" DD-MM-YY & YY format
syn match       boronDate       "\d\{1,2}\([/-]\)\d\{1,2}\1\(\d\{2}\)\{1,2}\>"
" YYYY-MM-YY format
syn match       boronDate       "\d\{4}-\d\{1,2}-\d\{1,2}\>"
" DD.MM.YYYY format
syn match       boronDate       "\d\{1,2}\.\d\{1,2}\.\d\{4}\>"
" Strings
syn region      boronString     oneline start=+"+ skip=+^"+ end=+"+ contains=boronSpecialChar
syn region      boronString     start="{" end="}" skip="{[^}]*}" contains=boronSpecialChar
syn region      boronString     start="{{\+\n" end="}}\+\n" contains=boronSpecialChar
" Binary
syn region      boronBinary     start="\d*#{" end="}" contains=boronComment
" File
syn match       boronFile       "%\(\k\+/\)*\k\+[/]\=" contains=boronSpecialChar
syn region      boronFile       oneline start=+%"+ end=+"+ contains=boronSpecialChar
" URLs
syn match	boronURL	"http://\k\+\(\.\k\+\)*\(:\d\+\)\=\(/\(\k\+/\)*\(\k\+\)\=\)*"
syn match	boronURL	"file://\k\+\(\.\k\+\)*/\(\k\+/\)*\k\+"
syn match	boronURL	"ftp://\(\k\+:\k\+@\)\=\k\+\(\.\k\+\)*\(:\d\+\)\=/\(\k\+/\)*\k\+"
syn match	boronURL	"mailto:\k\+\(\.\k\+\)*@\k\+\(\.\k\+\)*"
" Issues
syn match	boronIssue	"#\(\d\+-\)*\d\+"
" Tuples
syn match	boronTuple	"\(\d\+\.\)\{2,}"
" Characters
syn match   boronChar   "'\^\=.'"
"syn match   boronChar   "'\S\+'"

syn match	boronSpecialChar contained "\^[^[:space:][]"
syn match	boronSpecialChar contained "%\d\+"


" Operators
" Math operators
"syn match       boronMathOperator  "\(\*\{1,2}\|+\|-\|/\{1,2}\)"
syn keyword      boronMathFunction  add div mul sub mod
"syn keyword     boronMathFunction  abs absolute add arccosine arcsine arctangent cosine
"syn keyword     boronMathFunction  divide exp log-10 log-2 log-e max maximum min
"syn keyword     boronMathFunction  minimum multiply negate power random remainder sine
"syn keyword     boronMathFunction  square-root subtract tangent
" Binary operators
syn keyword     boronBinaryOperator complement and or xor
" Logic operators
"syn match       boronLogicOperator "[<>=]=\="
"syn match       boronLogicOperator "<>"
syn keyword     boronLogicOperator not
syn keyword     boronLogicFunction all any
syn keyword     boronLogicFunction head? tail? empty? exists? series?
syn keyword     boronLogicFunction same? equal? ne? gt? lt? zero?
syn keyword     boronLogicFunction unset? datatype? none? logic? char? int?
syn keyword     boronLogicFunction double? time? date? coord? vec3?
syn keyword     boronLogicFunction word? lit-word? set-word? get-word? option?
syn keyword     boronLogicFunction binary? bitset? string? file? vector?
syn keyword     boronLogicFunction block? paren? path? lit-path? set-path?
syn keyword     boronLogicFunction context? hash-map? error? func? cfunc?

" Datatypes
syn keyword     boronType	unset! datatype! none! logic! char! int!
syn keyword     boronType	double! time! date! coord! vec3!
syn keyword     boronType	word! lit-word! set-word! get-word! option!
syn keyword     boronType	binary! bitset! string! file! vector!
syn keyword     boronType	block! paren! path! lit-path! set-path!
syn keyword     boronType	context! hash-map! error! func! cfunc!
syn keyword     boronStatement	type?

" Control statements
syn keyword     boronStatement		catch try throw halt quit return break
syn keyword     boronConditional	if ifn either case
syn keyword     boronRepeat			forall foreach loop while

" Series statements
syn keyword     boronStatement  make do does func ++ -- reduce parse
syn keyword     boronStatement  append change clear copy find first free head
syn keyword     boronStatement  insert join last next prev pick poke remove
syn keyword     boronStatement  second select skip sort tail third trim appair

" Context
syn keyword     boronStatement  binding? bind unbind infuse set get in
syn keyword     boronStatement  value? words-of values-of

" I/O statements
syn keyword     boronStatement  print probe read load save write
syn keyword     boronOperator   size?

" Debug statement
"syn keyword     boronStatement  help probe trace

" Constants
syn keyword     boronConstant   none


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_boron_syntax_inits")
  if version < 508
    let did_boron_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink boronTodo     		Todo

  HiLink boronStatement 	Statement
" HiLink boronSetWord		Label
  HiLink boronSetWord		Identifier
  HiLink boronGetWord		Identifier
  HiLink boronLitWord		Constant
  HiLink boronConditional	Conditional
  HiLink boronRepeat		Repeat

  HiLink boronOperator		Operator
  HiLink boronLogicOperator boronOperator
  HiLink boronLogicFunction boronLogicOperator
  HiLink boronMathOperator	boronOperator
  HiLink boronMathFunction 	boronMathOperator
  HiLink boronBinaryOperator boronOperator
  HiLink boronBinaryFunction boronBinaryOperator

  HiLink boronType     		Type

" HiLink boronWord		Keyword
  HiLink boronOpcode   	Operator
" HiLink boronWordPath	boronWord
  HiLink boronFunction	Function

  HiLink boronChar         Character
  HiLink boronSpecialChar  SpecialChar
  HiLink boronString	   String

  HiLink boronNumber    Number
  HiLink boronInteger   boronNumber
  HiLink boronDecimal   boronNumber
  HiLink boronTime      boronNumber
  HiLink boronDate      boronNumber
  HiLink boronBinary    boronNumber
  HiLink boronFile      boronString
  HiLink boronURL       boronString
  HiLink boronIssue     boronNumber
  HiLink boronTuple     boronNumber
  HiLink boronFloat     Float
  HiLink boronBoolean   Boolean

  HiLink boronConstant  Constant

  HiLink boronComment   Comment
  HiLink boronCommentB  Comment

  HiLink boronError	    Error

  delcommand HiLink
endif


let b:current_syntax = "boron"

" vim: ts=4
