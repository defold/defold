Be concise.
Offensive/fail-fast coding style.
Keep requires sorted.
Use variables and keywords with "?" suffix only to denote predicate functions, never boolean values.
Use transducers instead of lazy sequences where possible. See src/clj/util/coll.clj for alternatives of core clojure functions that use reduce instead of seq. See src/clj/util/eduction.clj for using eductions instead of unrealized sequences. 
Use git in read-only mode (e.g., status, diff), never stage/commit anything.
