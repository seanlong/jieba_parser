/*
 * PotgreSQL FTS parser extension based on Jieba
 * Copyright (c) 2014, Long Xiang(seanlong@outlook.com)
 * Copyright (c) 2007-2011, PostgreSQL Global Development Group
 */

extern "C" {

#include "postgres.h"

#include "catalog/pg_type.h"
#include "fmgr.h"
#include "tsearch/ts_type.h"
#include "utils/array.h"
#include "utils/builtins.h"

}

#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE
#include <Python.h>

#include <memory>
#include <string>
#include <vector>

extern "C" {

PG_MODULE_MAGIC;

typedef struct {
	int			lexid;
	char	   *alias;
	char	   *descr;
} LexDescr;

/*
 * prototypes
 */
PG_FUNCTION_INFO_V1(jiebaprs_start);
Datum		jiebaprs_start(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(jiebaprs_getlexeme);
Datum		jiebaprs_getlexeme(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(jiebaprs_end);
Datum		jiebaprs_end(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(jiebaprs_lextype);
Datum		jiebaprs_lextype(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(jieba_rank);
Datum   jieba_rank(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(jieba_extract_tags);
Datum jieba_extract_tags(PG_FUNCTION_ARGS);

}

namespace {

PyObject* ImportJiebaModule() {
  Py_Initialize();

  PyObject* name = PyString_FromString("jieba");
  PyObject* module = PyImport_Import(name);
  assert(module);
  Py_DECREF(name);

  char method[] = "initialize";
  PyObject* ret = PyObject_CallMethod(module, method, NULL);
  assert(ret);
  Py_DECREF(ret);

  return module;
}

struct ParserState {
  ParserState(PyObject* token_generator)
      : token_gen_(token_generator) {
  }

  ~ParserState() {
    ClearTokens();
    Py_DECREF(token_gen_);
  }

  char* NextToken() {
    PyObject* item = PyIter_Next(token_gen_);
    if (!item)
      return NULL;
    PyObject* str = PyUnicode_AsUTF8String(item);
    Py_DECREF(item);
    token_strs_.push_back(str);
    return PyString_AsString(str);
  }

  void ClearTokens() {
    auto iter = token_strs_.begin();
    for (; iter != token_strs_.end(); ++iter)
      Py_DECREF(*iter);
    token_strs_.clear();
  }

  PyObject* token_gen_;
  std::vector<PyObject*> token_strs_;
};

class JiebaParser {
 public:
  JiebaParser()
      : module_(NULL),
        cutter_(NULL) {
    PyObject* module = ImportJiebaModule();
    cutter_ = PyObject_GetAttrString(module, "cut_for_search");
    assert(cutter_);
    Py_DECREF(module);
  }

  ~JiebaParser() {
    Py_DECREF(cutter_);
    Py_DECREF(module_);
    Py_Finalize();
  }

  ParserState* Cut(const char* buffer, int length) {
    PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, PyUnicode_DecodeUTF8(buffer, length, NULL));
    ParserState* pst = new ParserState(PyObject_CallObject(cutter_, args));
    Py_DECREF(args);
    return pst;
  }

 private:
  PyObject* module_;
  PyObject* cutter_;
} jieba_parser;

class JiebaExtractor {
 public:
  JiebaExtractor()
      : tagger_(NULL),
        module_(NULL) {
    module_ = ImportJiebaModule();
    PyObject* analyse_name  = PyString_FromString("jieba.analyse");
    PyObject* analyse_module = PyImport_Import(analyse_name);
    assert(analyse_module);
    Py_DECREF(analyse_name);

    tagger_ = PyObject_GetAttrString(analyse_module, "extract_tags");
    assert(tagger_);
    Py_DECREF(analyse_module);
  }

  ~JiebaExtractor() {
    Py_DECREF(tagger_);
    Py_DECREF(module_);
    Py_Finalize();
  }

  std::unique_ptr<std::vector<std::string>>
  Tag(unsigned short top_num, const char* buffer, int length) {
    PyObject* kws = PyDict_New();
    PyDict_SetItemString(kws, "topK", PyInt_FromLong(top_num)); 
    PyObject* args = PyTuple_New(1);
    PyTuple_SetItem(args, 0, PyUnicode_DecodeUTF8(buffer, length, NULL));
    PyObject* tags = PyObject_Call(tagger_, args, kws);
    Py_DECREF(kws);
    Py_DECREF(args);

    assert(PyList_Check(tags));
    Py_ssize_t size = PyList_Size(tags);
    std::unique_ptr<std::vector<std::string>> ret(
        new std::vector<std::string>());
    for (int i = 0; i < size; ++i) {
      PyObject* item = PyList_GetItem(tags, i);  // borrowed reference
      PyObject* str = PyUnicode_AsUTF8String(item);  // new reference
      ret->push_back(PyString_AsString(str));  // internal buffer of string
      Py_DECREF(str);
    }
    return std::move(ret);
  }

 private:
  PyObject* tagger_;
  PyObject* module_;
} jieba_extractor_;

}

Datum jiebaprs_start(PG_FUNCTION_ARGS)
{
  char* buffer = PG_GETARG_POINTER(0);
  int len = PG_GETARG_INT32(1);
  PG_RETURN_POINTER(jieba_parser.Cut(buffer, len)); 
}

Datum jiebaprs_getlexeme(PG_FUNCTION_ARGS) {
  ParserState* pst = (ParserState*)PG_GETARG_POINTER(0);
	char** t = (char**)PG_GETARG_POINTER(1);
	int* tlen = (int*)PG_GETARG_POINTER(2);
  int type = 3;

  char* token = pst->NextToken();
  if (token) {
    *t = token;
    *tlen = strlen(token);
  } else {
    type = 0;
    *tlen = 0;
  }

	PG_RETURN_INT32(type);
}

Datum jiebaprs_end(PG_FUNCTION_ARGS) {
  ParserState* pst = (ParserState*)PG_GETARG_POINTER(0);
  delete pst;
  PG_RETURN_VOID();
}

Datum jiebaprs_lextype(PG_FUNCTION_ARGS) {
	LexDescr* descr = (LexDescr*)palloc(sizeof(LexDescr) * 2);
  descr[0].lexid = 3;
  descr[0].alias = pstrdup("word");
  descr[0].descr = pstrdup("Word");
  descr[1].lexid = 0;

	PG_RETURN_POINTER(descr);
}

Datum jieba_rank(PG_FUNCTION_ARGS) {
  TSVector txt = PG_GETARG_TSVECTOR(0);
  TSQuery query = PG_GETARG_TSQUERY(1);

  //TODO return rank based on td-idf.
  float res = 0.0;

  PG_FREE_IF_COPY(txt, 0);
  PG_FREE_IF_COPY(query, 1);
  PG_RETURN_FLOAT4(res);
}

Datum jieba_extract_tags(PG_FUNCTION_ARGS) {
  text* t = PG_GETARG_TEXT_P(0);
  uint16 top_num = PG_GETARG_UINT16(1);
  char* raw_text = text_to_cstring(t);
  auto tags = jieba_extractor_.Tag(top_num, raw_text, strlen(raw_text));

  Datum* tag_datums = (Datum*)palloc(tags->size() * sizeof(Datum));
  auto iter = tags->begin();
  for (int i = 0; iter != tags->end(); ++iter, ++i)
    tag_datums[i] = CStringGetTextDatum(iter->c_str());

  //fprintf(stderr, "%d %p %d\n", __LINE__, tag_datums, tags->size());
  ArrayType* array = construct_array(
      tag_datums, tags->size(), TEXTOID, -1, false, 'i');
  pfree(tag_datums);
  PG_RETURN_POINTER(array);
}
