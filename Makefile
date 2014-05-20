# contrib/test_parser/Makefile

MODULE_big = jieba_parser
OBJS = jieba_parser.o

EXTENSION = jieba_parser
DATA = jieba_parser--1.0.sql jieba_parser--unpackaged--1.0.sql

REGRESS = jieba_parser

PY_CPPFLAGS = -I/usr/include/python2.7 -I/usr/include/python2.7 -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -g -fstack-protector --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security 

PY_LDFLAGS =  -L/usr/lib/python2.7/config -lpthread -ldl -lutil -lm -lpython2.7 -Xlinker -export-dynamic -Wl,-O1 -Wl,-Bsymbolic-functions

PG_CPPFLAGS = $(PY_CPPFLAGS) --std=c++0x
SHLIB_LINK = -lstdc++ $(PY_LDFLAGS)

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
