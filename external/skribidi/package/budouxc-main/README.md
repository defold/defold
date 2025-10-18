BudouX implementation in C
===

This is a simple implementation of [BudouX](https://google.github.io/budoux/) word boundary breaking for east asian languages.
The code tries to be a faithful implementation of the original algorithm, and uses the models from the original project.

Usage
--

```C
char const sentence[] = "私はその人を常に先生と呼んでいた。\n"
                             "だからここでもただ先生と書くだけで本名は打ち明けない。\n"
                             "これは世間を憚かる遠慮というよりも、その方が私にとって自然だからである。";


// Init iterator, does not allocate, no need to tear down.
boundary_iterator_t iter = boundary_iterator_init_ja_utf8(sentence, -1);

// Iterate and pring all ranges.
int32_t range_start = 0, range_end = 0;
while (boundary_iterator_next(&iter, &range_start, &range_end)) {
    for (int i = range_start; i < range_end; i++)
        printf("%c", sentence[i]);
    printf("|");
}

```
Outputs:
```
私は|その|人を|常に|先生と|呼んで|いた。|
だから|ここでも|ただ先生と|書くだけで|本名は|打ち明けない。|
これは|世間を|憚かる|遠慮と|いうよりも、|その方が|私に|とって|自然だからである。|
```

Models
--
The `models` folder contains json files from BudouX project. They have been converted to C headers using the `covert.py` script:
```python
python .\convert.py .\models\zh-hant.json zh_hant model_zh_hant.h
```

Similar Project
--
- [budoux-c](https://github.com/oov/budoux-c/tree/main)