CREATE EXTENSION jieba_parser;

-- make test configuration using parser

CREATE TEXT SEARCH CONFIGURATION jiebacfg (PARSER = jiebaparser);

ALTER TEXT SEARCH CONFIGURATION jiebacfg ADD MAPPING FOR word WITH simple;

-- ts_parse

SELECT * FROM ts_parse('jiebaparser', '小明硕士毕业于中国科学院计算所，后在日本京都大学深造');

SELECT to_tsvector('jiebacfg','我来到北京清华大学');

SELECT to_tsquery('jiebacfg', '清华');

SELECT to_tsvector('jiebacfg', '我来到北京清华大学') @@ to_tsquery('jiebacfg', '清华');

SELECT jieba_extract_tags('被称为“相声演员”的罗永浩，依然以调侃开场：先调侃了一下延迟开始所造成的“经济损失”，紧接着矛头直接指向眼下手机市场上的硬件大战，以及“跑分大战”；看上去，小米和雷军再次躺枪。', 5);
