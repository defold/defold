(ns editor.handler-test
  (:require [clojure.test :refer :all]
            [editor.handler :as handler]))

(defn fixture [f]
  (with-redefs [handler/*handlers* (atom {})]
   (f)))

(use-fixtures :each fixture)

(defn- enabled? [command command-contexts user-data]
  (some-> (handler/active command command-contexts user-data)
    handler/enabled?))

(defn- run [command command-contexts user-data]
  (some-> (handler/active command command-contexts user-data)
    handler/run))

(deftest run-test
  (handler/defhandler :open :global
    (enabled? [instances] (every? #(= % :foo) instances))
    (run [instances] 123))
  (are [inst exp] (= exp (enabled? :open [{:name :global :env {:instances [inst]}}] {}))
       :foo true
       :bar false)
  (is (= 123 (run :open [{:name :global :env {:instances [:foo]}}] {}))))

(deftest context
  (handler/defhandler :c1 :global
    (enabled? [] true)
    (run [] :c1))
  (handler/defhandler :c2 :local
    (enabled? [] true)
    (run [] :c2))

  (is (enabled? :c1 [{:name :global :env {}}] {}))
  (is (not (enabled? :c1 [{:name :local :env {}}] {})))
  (is (enabled? :c2 [{:name :local :env {}}] {}))
  (is (not (enabled? :c2 [{:name :global :env {}}] {}))))
