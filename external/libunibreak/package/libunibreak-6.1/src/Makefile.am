include_HEADERS = \
	unibreakbase.h \
	unibreakdef.h \
	linebreak.h \
	linebreakdef.h \
	eastasianwidthdef.h \
	graphemebreak.h \
	wordbreak.h
lib_LTLIBRARIES = libunibreak.la

libunibreak_la_CFLAGS = -W -Wall
libunibreak_la_LDFLAGS = -no-undefined -version-info 6:1:0
libunibreak_la_SOURCES = \
	unibreakbase.c \
	unibreakdef.c \
	linebreak.c \
	linebreakdata.c \
	linebreakdef.c \
	eastasianwidthdef.c \
	emojidef.c \
	graphemebreak.c \
	wordbreak.c

EXTRA_DIST = \
	extract_data.sed \
	expand_single.sed \
	linebreakdata1.tmpl \
	wordbreakdata.c \
	graphemebreakdata.c \
	indicconjunctbreakdata.c \
	eastasianwidthdata.c \
	emojidata.c \
	Makefile.gcc \
	Makefile.msvc \
	generate_linebreakdata.py \
	generate_word_break.py \
	generate_grapheme_break.py \
	generate_east_asian_width.py \
	generate_extended_pictographic.py \
	generate_indic_conjunct_break.py \
	unicode_data_property.py \
	wordbreakdef.h \
	graphemebreakdef.h \
	emojidef.h \
	indicconjunctbreakdef.h \
	test_skips.h \
	LineBreakTest.txt \
	WordBreakTest.txt \
	GraphemeBreakTest.txt

install-exec-hook:
	- [ -f $(DESTDIR)/${libdir}/libunibreak.a ] && \
	rm -f $(DESTDIR)/${libdir}/liblinebreak.a && \
	${LN_S} libunibreak.a $(DESTDIR)/${libdir}/liblinebreak.a

distclean-local:
	rm -f LineBreak.txt WordBreakData.txt GraphemeBreakData.txt DerivedCoreProperties.txt EastAsianWidth.txt emoji-data.txt


LineBreak.txt:
	wget http://unicode.org/Public/UNIDATA/LineBreak.txt

WordBreakProperty.txt:
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakProperty.txt

GraphemeBreakProperty.txt:
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/GraphemeBreakProperty.txt

DerivedCoreProperties.txt:
	wget https://www.unicode.org/Public/UNIDATA/DerivedCoreProperties.txt

EastAsianWidth.txt:
	wget https://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt

emoji-data.txt:
	wget https://www.unicode.org/Public/UNIDATA/emoji/emoji-data.txt


linebreakdata: LineBreak.txt
	sed -E -n -f ${srcdir}/extract_data.sed LineBreak.txt | \
	sed -E -f ${srcdir}/expand_single.sed | \
	${srcdir}/generate_linebreakdata.py > tmp.c
	head -2 LineBreak.txt > tmp.txt
	cat ${srcdir}/linebreakdata1.tmpl tmp.txt tmp.c > ${srcdir}/linebreakdata.c
	rm tmp.txt tmp.c

wordbreakdata: WordBreakProperty.txt
	${srcdir}/generate_word_break.py > ${srcdir}/wordbreakdata.c

graphemebreakdata: GraphemeBreakProperty.txt
	${srcdir}/generate_grapheme_break.py > ${srcdir}/graphemebreakdata.c

eastasianwidthdata: EastAsianWidth.txt
	${srcdir}/generate_east_asian_width.py > ${srcdir}/eastasianwidthdata.c

indicconjunctbreakdata: DerivedCoreProperties.txt
	${srcdir}/generate_indic_conjunct_break.py > ${srcdir}/indicconjunctbreakdata.c

emojidata: emoji-data.txt
	${srcdir}/generate_extended_pictographic.py > ${srcdir}/emojidata.c

update-test:
	rm -f WordBreakTest.txt GraphemeBreakTest.txt
	rm -f ${srcdir}/WordBreakTest.txt ${srcdir}/GraphemeBreakTest.txt
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/WordBreakTest.txt
	wget http://www.unicode.org/Public/UNIDATA/auxiliary/GraphemeBreakTest.txt
	[ -f ${srcdir}/WordBreakTest.txt ] || mv -f WordBreakTest.txt GraphemeBreakTest.txt ${srcdir}


# Tests

check_PROGRAMS = tests

tests_SOURCES = tests.c
tests_CPPFLAGS = -I$(srcdir)
tests_LDADD = libunibreak.la
tests_DEPENDENCIES = libunibreak.la

check: $(check_PROGRAMS)
	@[ -f LineBreakTest.txt ]     || $(LN_S) ${srcdir}/LineBreakTest.txt .
	@[ -f WordBreakTest.txt ]     || $(LN_S) ${srcdir}/WordBreakTest.txt .
	@[ -f GraphemeBreakTest.txt ] || $(LN_S) ${srcdir}/GraphemeBreakTest.txt .
	./tests line
	./tests word
	./tests grapheme
