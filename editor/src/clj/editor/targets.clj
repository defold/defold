(ns editor.targets
  (:require [clojure.string :as str]
            [clojure.xml :as xml])
  (:import [com.dynamo.upnp DeviceInfo SSDP]
           [java.net URL URLConnection]
           [java.io ByteArrayOutputStream ByteArrayInputStream]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def ^:private targets (atom #{}))
(def ^:private descriptions (atom {}))
(def ^:private last-search (atom 0))
(def ^:private running (atom true))

(def ^:const search-interval (* 60 1000))
(def ^:const timeout 2000)

(defn- http-get [^URL url]
  (let [conn   ^URLConnection (doto (.openConnection url)
                                (.setRequestProperty "Conenction" "close")
                                (.setConnectTimeout timeout)
                                (.setReadTimeout timeout))
        input  (.getInputStream conn)
        output (ByteArrayOutputStream.)]
    (IOUtils/copy input output)
    (.close input)
    (.close output)
    (.toString output "UTF8")))

(defn- tag->val [tag tags]
  (->> tags
       (filter #(= tag (:tag %)))
       first
       :content
       first))

(defn- desc->target [desc]
  (when-let [tags (and (= {:xmlns:defold "urn:schemas-defold-com:DEFOLD-1-0", :xmlns "urn:schemas-upnp-org:device-1-0"}
                          (:attrs desc))
                       (->> desc :content (filter #(= :device (:tag %))) first :content))]
    (when (= "defold" (str/lower-case (tag->val :manufacturer tags)))
      {:name     (tag->val :friendlyName tags)
       :model    (tag->val :modelName tags)
       :udn      (tag->val :UDN tags)
       :url      (tag->val :defold:url tags)
       :log-port (tag->val :defold:logPort tags)})))

(def local-target
  {:name "localhost"
   :url  "http://localhost:8001"})

(defn- update-targets! [devices]
  (let [res (reduce (fn [{:keys [blacklist targets] :as acc} ^DeviceInfo device]
                      (let [loc                 (.get (.headers device) "LOCATION")
                            ^URL url            (try (URL. loc) (catch Exception _))
                            ^String description (and url (not (contains? blacklist (.getHost url)))
                                                     (or (get @descriptions loc)
                                                         (try (http-get url) (catch Exception _))))
                            desc                (try (xml/parse (ByteArrayInputStream. (.getBytes description)))
                                                     (catch Exception _))
                            target              (desc->target desc)]

                        (when desc
                          (swap! descriptions assoc loc description))

                        {:targets   (if target
                                      (conj targets target)
                                      targets)
                         :blacklist (if (and url (not desc))
                                      (conj blacklist (.getHost url))
                                      blacklist)}))
                    {:blacklist #{} :targets #{}}
                    devices)]
    (reset! targets (:targets res))))

(defn- targets-worker []
  (when-let [ssdp-service (SSDP.)]
    (try
      (println "Starting targets service")
      (while @running
        (Thread/sleep 100)
        (let [now      (System/currentTimeMillis)
              search?  (>= now (+ @last-search search-interval))
              changed? (.update ssdp-service search?)]
          (when search?
            (reset! last-search now))
          (when (and search? changed?)
            (update-targets! (.getDevices ssdp-service)))))
      (catch Exception e
        (println e))
      (finally
        (.dispose ssdp-service)
        (println "Stopping targets service")))))

(defn update! []
  (reset! last-search 0))

(defn start []
  (future (targets-worker)))

(defn stop []
  (reset! running false))

(defn get-targets []
  @targets)
