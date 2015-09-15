(ns editor.handler-test
  (:require [clojure.test :refer :all]
            [editor.handler :as handler]))

(defn fixture [f]
  (with-redefs [handler/*handlers* (atom {})]
   (f)))

(use-fixtures :each fixture)

(deftest run
  (handler/defhandler :open :global
    (enabled? [instances] (every? #(= % :foo) instances))
    (run [instances] 123))
  (are [inst exp] (= exp (handler/enabled? :open [{:name :global :env {:instances [inst]}}] {}))
       :foo true
       :bar false)
  (is (= 123 (handler/run :open [{:name :global :env {:instances [:foo]}}] {}))))

(deftest context
  (handler/defhandler :c1 :global
    (enabled? [] true)
    (run [] :c1))
  (handler/defhandler :c2 :local
    (enabled? [] true)
    (run [] :c2))

  (is (handler/enabled? :c1 [{:name :global :env {}}] {}))
  (is (not (handler/enabled? :c1 [{:name :local :env {}}] {})))
  (is (handler/enabled? :c2 [{:name :local :env {}}] {}))
  (is (not (handler/enabled? :c2 [{:name :global :env {}}] {}))))
