(ns potemkin.namespaces-test
  (:require
   [potemkin :as p]
   [clojure.test :refer :all]
   [clojure.repl :as repl]
   [clojure.string :as str]
   [potemkin.imports-test :as i]))

(p/import-macro i/multi-arity-macro)
(p/import-macro i/multi-arity-macro p/alt-macro-name)
(p/import-fn i/multi-arity-fn)
(p/import-fn i/multi-arity-fn p/alt-name)
(p/import-fn i/protocol-function)
(p/import-fn i/inlined-fn)
(p/import-def i/some-value)

(defn drop-lines [n s]
  (->> s str/split-lines (drop n) (interpose "\n") (apply str)))

(defmacro out= [& args]
  `(= ~@(map (fn [x] `(with-out-str ~x)) args)))

(defmacro rest-out= [& args]
  `(= ~@(map (fn [x] `(drop-lines 2 (with-out-str ~x))) args)))

(deftest test-import-macro
  (is (out= (source i/multi-arity-macro) (source multi-arity-macro)))
  (is (rest-out= (doc i/multi-arity-macro) (doc multi-arity-macro)))
  (is (out= (source i/multi-arity-macro) (source alt-macro-name)))
  (is (rest-out= (doc i/multi-arity-macro) (doc alt-macro-name))))

(deftest test-import-fn
  (is (= 1 (inlined-fn 1)))
  (is (= 1 (apply inlined-fn [1])))
  (is (out= (source i/multi-arity-fn) (source multi-arity-fn)))
  (is (rest-out= (doc i/multi-arity-fn) (doc multi-arity-fn)))
  (is (out= (source i/multi-arity-fn) (source alt-name)))
  (is (rest-out= (doc i/multi-arity-fn) (doc alt-name)))
  (is (rest-out= (doc i/protocol-function) (doc protocol-function))))

(deftest test-points-to-the-value-after-reload
  (is (= 1 some-value))
  (require 'potemkin.imports-test :reload)
  (is (= 1 some-value)))
