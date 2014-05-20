-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION jieba_parser" to load this file. \quit

CREATE FUNCTION jiebaprs_start(internal, int4)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jiebaprs_getlexeme(internal, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jiebaprs_end(internal)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jiebaprs_lextype(internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE TEXT SEARCH PARSER jiebaparser (
    START    = jiebaprs_start,
    GETTOKEN = jiebaprs_getlexeme,
    END      = jiebaprs_end,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = jiebaprs_lextype
);

CREATE FUNCTION jieba_rank(tsvector, tsquery)
RETURNS float4
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION jieba_extract_tags(text, int)
RETURNS text array
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
