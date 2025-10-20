;; Copyright 2020-2025 The Defold Foundation
;; Licensed under the Defold License version 1.0

(ns editor.library-min-version-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.library :as library])
  (:import [com.dynamo.bob.archive EngineVersion]
           [java.io File]
           [java.util.zip ZipEntry ZipOutputStream]))

(defn- write-zip-entry [^ZipOutputStream zos ^String name ^bytes data]
  (doto zos
    (.putNextEntry (ZipEntry. name))
    (.write data)
    (.closeEntry))
  zos)

(defn- make-lib-zip ^File [^String dir-name ^String min-version]
  (let [f (fs/create-temp-file! "lib-" ".zip")]
    (with-open [zos (-> f io/output-stream ZipOutputStream.)]
      (write-zip-entry zos "game.project"
                       (.getBytes (format "[library]\ninclude_dirs=%s\ndefold_min_version=%s\n" dir-name min-version) "UTF-8"))
      (.putNextEntry zos (ZipEntry. (str dir-name "/")))
      (.closeEntry zos)
      (write-zip-entry zos (str dir-name "/file.in") (.getBytes "ok" "UTF-8")))
    f))

(defn- higher-version-than [^String _v]
  ;; Use a very high version to avoid ambiguity across environments.
  "999.0.0")

(deftest validate-updated-libraries-stale-min-version
  (let [current EngineVersion/version
        too-high (higher-version-than current)
        ok (make-lib-zip "ok" current)
        bad (make-lib-zip "bad" too-high)
        states [{:status :stale :new-file ok :uri (java.net.URI. "https://example.com/ok.zip")}
                {:status :stale :new-file bad :uri (java.net.URI. "https://example.com/bad.zip")}]
        validated (library/validate-updated-libraries states)
        [ok-state bad-state] validated]
    (is (not= :error (:status ok-state)))
    (is (= :error (:status bad-state)))
    (is (= :defold-min-version (:reason bad-state)))
    (is (= too-high (:required bad-state)))
    (is (= current (:current bad-state)))))

(deftest validate-updated-libraries-uptodate-min-version
  (let [current EngineVersion/version
        too-high (higher-version-than current)
        ok (make-lib-zip "ok" current)
        bad (make-lib-zip "bad" too-high)
        states [{:status :up-to-date :file ok :uri (java.net.URI. "https://example.com/ok.zip")}
                {:status :up-to-date :file bad :uri (java.net.URI. "https://example.com/bad.zip")}]
        validated (library/validate-updated-libraries states)
        [ok-state bad-state] validated]
    (is (not= :error (:status ok-state)))
    (is (= :error (:status bad-state)))
    (is (= :defold-min-version (:reason bad-state)))
    (is (= too-high (:required bad-state)))
    (is (= current (:current bad-state)))))
