(ns editor.updater-test
 (:require [clojure.test :refer :all]
           [clojure.java.io :as io]
           [ring.adapter.jetty :as jetty]
           [clojure.data.json :as json]
           [editor.fs :as fs]
           [editor.updater :as updater])
 (:import [com.defold.editor Updater]
          [org.slf4j LoggerFactory]
          [ch.qos.logback.classic Level]
          [ch.qos.logback.classic Logger]))

(defn- error-log-level-fixture [f]
  (let [root-logger (LoggerFactory/getLogger Logger/ROOT_LOGGER_NAME)
        level (.getLevel root-logger)]
    (.setLevel root-logger Level/ERROR)
    (f)
    (.setLevel root-logger level)))

(use-fixtures :once error-log-level-fixture)

(defn- temp-dir []
  (-> (fs/create-temp-directory!)))

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
    (if-let [r (get @resources uri)]
      {:status 200
       :body (if (fn? r) (r) r)}
      {:status 404
       :body "404"}))))

(defn make-handler-resources [port sha1 jar-sha1]
  {(format "/%s/manifest.json" sha1)
                 (make-manifest sha1)

                 "/update.json"
                 (fn [] (json/write-str {"url" (format "http://localhost:%d/%s" @port sha1)}))

                 (format "/%s/config" sha1)
                 "config"

                 (format "/%s/defold-%s.jar" sha1 jar-sha1)
                 "a jar"})

(defn- make-resource-handler [port sha1 jar-sha1]
  (make-handler (atom (make-handler-resources port sha1 jar-sha1))))

(defn- list-files [dir]
  (->> (file-seq (io/file dir))
    (filter #(.isFile %))
    (map #(.getName %))
    (apply hash-set)))

(defn- update-url [port]
  (format "http://localhost:%d" port))

(deftest raw-updater-test
  ; no update
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "1" "1")
                                {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        updater (Updater. (format "http://localhost:%d" @port))]

    (let [pending-update (.check updater "1")]
      (is (= nil pending-update)))
    (let [pending-update (.check updater "1")]
      (is (= nil pending-update)))
   (.stop server))

  ; update from 1 -> 2, installed
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "2" "2")
                                {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        res-dir (temp-dir)
        updater (Updater. (format "http://localhost:%d" @port))]
    (let [pending-update (.check updater "1")]
      (is (some? pending-update))
      (is (= "2" (.sha1 pending-update)))
      (is (= #{} (list-files res-dir)))
      (.install pending-update res-dir)
      (is (= #{"config" "defold-2.jar"} (list-files res-dir))))
    (let [pending-update (.check updater "2")]
      (is (= nil pending-update))
      (is (= #{"config" "defold-2.jar"} (list-files res-dir))))
    (.stop server))

  ; update from 1 -> 2, not installed
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "2" "2")
                 {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        updater (Updater. (format "http://localhost:%d" @port))]
    (let [pending-update (.check updater "1")]
      (is (some? pending-update))
      (is (= "2" (.sha1 pending-update))))
    (let [pending-update (.check updater "2")]
      (is (= nil pending-update)))
   (.stop server))

  ; jar is missing
  (let [port (atom nil)
        server (jetty/run-jetty (make-resource-handler port "2" "XXXXX") {:port 0 :join? false})
        _ (reset! port (-> server .getConnectors first .getLocalPort))
        updater (Updater. (format "http://localhost:%d" @port))]
    (is (thrown? Exception (.check updater "1")))
    (.stop server)))

(defmacro test-updater [initial-sha1 sha1 jar-sha1 & body]
  `(let [port# (atom nil)
         resources# (atom (make-handler-resources port# ~sha1 ~jar-sha1))
         server# (jetty/run-jetty (make-handler resources#)
                                  {:port 0 :join? false})
         ~'_ (reset! port# (-> server# .getConnectors first .getLocalPort))
         ~'res-dir (temp-dir)
         ~'set-update-version! (fn [sha1'# jar-sha1'#] (reset! resources# (make-handler-resources port# sha1'# jar-sha1'#)))
         ~'ctxt (updater/make-update-context (update-url @port#) ~initial-sha1)]
     ~@body
     (.stop server#)))

(deftest updater-test
  (test-updater "1" "1" "1"
                (updater/check! ctxt)
                (is (= nil (updater/pending-update ctxt)))
                (updater/check! ctxt)
                (is (= nil (updater/pending-update ctxt))))
  (test-updater "1" "2" "2"
    (updater/check! ctxt)
    (is (= "2" (updater/pending-update ctxt)))
    (is (= #{} (list-files res-dir)))
    (updater/install-pending-update! ctxt res-dir)
    (is (= #{"config" "defold-2.jar"} (list-files res-dir)))
    (is (= nil (updater/pending-update ctxt)))
    (updater/check! ctxt)
    (is (= nil (updater/pending-update ctxt))))

  (test-updater "1" "2" "2"
                (is (some? (updater/check! ctxt)))
                (is (= "2" (updater/pending-update ctxt)))
                (is (nil? (updater/check! ctxt))))

  (test-updater "1" "2" "missing-jar-sha1"
                (is (thrown? Exception (updater/check! ctxt))))

  (test-updater "1" "2" "2"
                (updater/check! ctxt)
                (is (= "2" (updater/pending-update ctxt)))

                (set-update-version! "3" "3")
                (updater/check! ctxt)
                (is (= "3" (updater/pending-update ctxt)))

                (set-update-version! "2" "2")
                (updater/check! ctxt)
                (is (= "2" (updater/pending-update ctxt)))

                (set-update-version! "1" "1")
                (updater/check! ctxt)
                (is (= "1" (updater/pending-update ctxt)))))

(deftest updater-timer-test
  (test-updater "1" "2" "2"
                (let [timer (updater/start-update-timer! ctxt 10 10)] ; damn fast!
                  (try
                    (Thread/sleep 1000)
                    (is (= "2" (updater/pending-update ctxt)))
                    (set-update-version! "3" "3")
                    (Thread/sleep 1000)
                    (is (= "3" (updater/pending-update ctxt)))
                    (finally
                      (updater/stop-update-timer! timer))))))
