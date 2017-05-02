(ns util.luaj-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [util.luaj :as luaj])
  (:import [java.io FileNotFoundException]
           [clojure.lang ExceptionInfo]))

(defn- setup-state []
  (let [is-factory (reify luaj/InputStreamFactory
                     (->input-stream [this filename]
                       (some-> filename
                         io/resource
                         io/input-stream)))]
    (luaj/->state is-factory)))

(deftest basic-test []
  (let [state (setup-state)
        script (luaj/load-script state "resources/lua/basic_test.lua")
        res (luaj/run-script script)]
    (is (= {:string "value" :boolean true :int 1 :double 1.1} (luaj/get-value state "table")))
    (is (= [1 2 "value"] (luaj/get-value state "array")))
    (is (= {1 1, 2 2, :string "value"} (luaj/get-value state "mixed_table")))
    (let [f (luaj/get-value state "add_func")]
      (is (= 3 (f [1 2]))))
    (let [f (luaj/get-value state "add_func_array_arg")]
      (is (= 3 (f [[1 2]]))))
    (let [f (luaj/get-value state "add_func_table_arg")]
      (is (= 3 (f [{:a 1 :b 2}]))))
    (is (nil? res))))

(deftest load-script-not-found []
  (let [state (setup-state)]
    (is (thrown? FileNotFoundException (luaj/load-script state "resources/lua/not_found.lua")))))

(deftest load-script-syntax-error []
  (let [state (setup-state)]
    (is (thrown? ExceptionInfo (luaj/load-script state "resources/lua/syntax_error.lua")))))

(deftest runtime-error []
  (let [state (setup-state)
        script (luaj/load-script state "resources/lua/rt_error.lua")]
    (is (thrown? RuntimeException (luaj/run-script script)))))
