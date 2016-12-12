(ns editor.updater-test
 (:require [clojure.test :refer :all]
           [clojure.java.io :as io]
           [ring.adapter.jetty :as jetty]
           [clojure.data.json :as json])
 (:import [com.defold.editor Updater]
          [java.nio.file Files]
          [java.nio.file.attribute FileAttribute]
          [org.slf4j LoggerFactory]
          [ch.qos.logback.classic Level]
          [ch.qos.logback.classic Logger]))

(.setLevel (LoggerFactory/getLogger Logger/ROOT_LOGGER_NAME) Level/WARN)

(defn- temp-dir []
  (-> (Files/createTempDirectory nil (into-array FileAttribute [])) .toFile .getAbsolutePath))

(defn- make-manifest [sha1]
  (json/write-str {"version" "2.0.1",
                   "sha1" sha1,
                   "config" "config",
                   "packages" [{"platform" "*",
                                "url" (format "defold-%s.jar" sha1),
                                "type" "jar",
                                "action" "copy"}]}))

(defn make-handler [resources]
  (fn [request]
    (let [uri (:uri request)]
    (if-let [r (get resources uri)]
      {:status 200
       :body (if (fn? r) (r) r)}
      {:status 404
       :body "404"}))))

(defn- make-resource-handler [port sha1 jar-sha1]
  (make-handler {(format "/%s/manifest.json" sha1)
                 (make-manifest sha1)
                 "/update.json" (fn [] (json/write-str {"url" (format "http://localhost:%d/%s" @port sha1)}))
                 (format "/%s/config" sha1) "config" (format "/%s/defold-%s.jar" sha1 jar-sha1) "a jar"}))

(defn- list-files [dir]
  (->> (file-seq (io/file dir))
    (filter #(.isFile %))
    (map #(.getName %))
    (apply hash-set)))

(deftest updater-test
  ; no update
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "1" "1")
                                {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        res-dir (temp-dir)
        updater (Updater. (format "http://localhost:%d" @port) res-dir "1")]

    (let [pending-update (.check updater)]
      (is (= nil pending-update))
      (is (= #{} (list-files res-dir))))
    (let [pending-update (.check updater)]
      (is (= nil pending-update))
      (is (= #{} (list-files res-dir))))
   (.stop server))

  ; update from 1 -> 2, installed
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "2" "2")
                                {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        res-dir (temp-dir)
        updater (Updater. (format "http://localhost:%d" @port) res-dir "1")]
    (let [pending-update (.check updater)]
      (is (some? pending-update))
      (is (= "2" (.sha1 pending-update)))
      (is (= #{} (list-files res-dir)))
      (.install pending-update)
      (is (= #{"config" "defold-2.jar"} (list-files res-dir))))
    (let [pending-update (.check updater)]
      (is (= nil pending-update))
      (is (= #{"config" "defold-2.jar"} (list-files res-dir))))
    (.stop server))

  ; update from 1 -> 2, not installed
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "2" "2")
                 {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        res-dir (temp-dir)
        updater (Updater. (format "http://localhost:%d" @port) res-dir "1")]
    (let [pending-update (.check updater)]
      (is (some? pending-update))
      (is (= "2" (.sha1 pending-update)))
      (is (= #{} (list-files res-dir))))
    (let [pending-update (.check updater)]
      (is (= nil pending-update))
      (is (= #{} (list-files res-dir))))
   (.stop server))

  ; jar is missing
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "1" "XXXXX") {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        res-dir (temp-dir)
        updater (Updater. (format "http://localhost:%d" @port) res-dir "1")]
    (try (.check updater)
      (catch Exception _))
    (is (= #{} (list-files res-dir)))
   (.stop server)))
