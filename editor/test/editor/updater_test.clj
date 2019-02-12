(ns editor.updater-test
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.updater :as updater]
            [ring.adapter.jetty :as jetty]
            [ring.util.response :as response])
  (:import [ch.qos.logback.classic Level Logger]
           [com.defold.editor Platform]
           [java.io File]
           [java.util Timer]
           [org.slf4j LoggerFactory]))

(set! *warn-on-reflection* true)

(def ^:private test-port
  23232)

(defn- error-log-level-fixture [f]
  (let [root-logger ^Logger (LoggerFactory/getLogger Logger/ROOT_LOGGER_NAME)
        level (.getLevel root-logger)]
    (.setLevel root-logger Level/ERROR)
    (f)
    (.setLevel root-logger level)))

(defn- test-urls-fixture [f]
  (with-redefs [updater/download-url
                (fn [sha1 ^Platform platform]
                  (format "http://localhost:%s/editor2/%s/editor2/Defold-%s.zip"
                          test-port sha1 (.getPair platform)))

                updater/update-url
                (fn [channel]
                  (format "http://localhost:%s/editor2/channels/%s/update-v2.json"
                          test-port channel))]
    (f)))

(use-fixtures :once error-log-level-fixture)

(use-fixtures :once test-urls-fixture)

(defn make-handler-resources [channel sha1]
  {(format "/editor2/channels/%s/update-v2.json" channel)
   (response/response (json/write-str {:sha1 sha1}))

   (format "/editor2/%s/editor2/Defold-%s.zip" sha1 (.getPair (Platform/getHostPlatform)))
   (response/resource-response "test-update.zip")})

(defn- make-resource-handler [channel sha1]
  (let [resources (make-handler-resources channel sha1)]
    (fn [request]
      (get resources (:uri request) (response/not-found "404")))))

(defn- list-files [dir]
  (->> (file-seq (io/file dir))
       (filter #(.isFile ^File %))
       (map #(.getName ^File %))
       (apply hash-set)))

(defmacro with-server [channel sha1 & body]
  `(let [server# (jetty/run-jetty (make-resource-handler ~channel ~sha1)
                                  {:port ~test-port :join? false})]
     (try
       ~@body
       (finally
         (.stop server#)))))

(defn make-updater [channel sha1]
  (#'updater/make-updater
    channel
    sha1
    sha1
    (Platform/getHostPlatform)
    (io/file ".")
    (io/file "no-launcher")))

(deftest no-update-on-client-when-no-update-on-server
  (with-server "test" "1"
    (let [updater (make-updater "test" "1")]
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater)))
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest has-update-on-client-when-has-update-on-server
  (with-server "test" "2"
    (let [updater (make-updater "test" "1")]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater))))))

(deftest no-update-on-client-when-server-has-update-on-different-channel
  (with-server "alpha" "2"
    (let [updater (make-updater "beta" "1")]
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest can-download-and-extract-update
  (with-server "test" "2"
    (let [updater (make-updater "test" "1")
          ^File update-sha1-file @#'updater/update-sha1-file
          ^File update-dir @#'updater/update-dir]
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file)
      (#'updater/check! updater)
      @(updater/download-and-extract! updater)
      (is (.exists update-dir))
      (is (.exists update-sha1-file))
      (is (= #{"extracted-file.txt"} (list-files update-dir)))
      (fs/delete-directory! update-dir)
      (fs/delete! update-sha1-file))))

(deftest throws-if-zip-is-missing-on-server
  (with-server "test" "2"
    (let [updater (#'updater/make-updater
                    "test"
                    "1"
                    "1"
                    Platform/JsWeb
                    (io/file ".")
                    (io/file "no-launcher"))]
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater)))
      (is (false? @(updater/download-and-extract! updater))))))

(deftest client-has-update-after-check-when-update-appears-on-server
  (let [updater (make-updater "test" "1")]
    (with-server "test" "1"
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))
    (with-server "test" "2"
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater))))))

(deftest no-new-update-is-reported-after-extracting
  (let [updater (make-updater "test" "1")]
    (with-server "test" "2"
      (#'updater/check! updater)
      (is (true? (updater/can-download-update? updater)))
      @(updater/download-and-extract! updater)
      (#'updater/check! updater)
      (is (false? (updater/can-download-update? updater))))))

(deftest update-timer-performs-checks-automatically
  (let [updater (make-updater "test" "1")
        timer ^Timer (#'updater/start-timer! updater 10 10)]
    (try
      (with-server "test" "1"
        (Thread/sleep 1000)
        (is (false? (updater/can-download-update? updater))))
      (with-server "test" "2"
        (Thread/sleep 1000)
        (is (true? (updater/can-download-update? updater))))
      (finally
        (.cancel timer)
        (.purge timer)))))
