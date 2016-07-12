(ns editor.targets
  (:require [clj-time
             [core :as t]
             [format :as f]]
            [clojure
             [string :as str]
             [xml :as xml]]
            [editor.ui :as ui])
  (:import [com.dynamo.upnp DeviceInfo SSDP]
           [java.io ByteArrayInputStream ByteArrayOutputStream]
           [java.net URL URLConnection]
           [javafx.scene Parent Scene]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality Stage]
           org.apache.commons.io.IOUtils))

(set! *warn-on-reflection* true)

(def ^:private targets (atom #{}))
(def ^:private descriptions (atom {}))
(def ^:private last-search (atom 0))
(def ^:private running (atom true))
(def ^:private event-log (atom []))

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

(def ^:const local-target
  {:name "Local"
   :url  "http://localhost:8001"})

(defn- append-event-log [message]
  (swap! event-log (fn [xs]
                     (take 512 (conj xs (format "%s: %s"
                                                (f/unparse (f/formatters :mysql) (t/now))
                                                message))))))

(defn- update-targets! [devices]
  (let [res (reduce (fn [{:keys [blacklist targets] :as acc} ^DeviceInfo device]
                      (let [loc                 (.get (.headers device) "LOCATION")
                            ^URL url            (try (URL. loc)
                                                     (catch Exception _
                                                       (append-event-log (format "[%s] not a valid URL" loc))))
                            ^String description (and url (not (contains? blacklist (.getHost url)))
                                                     (or (get @descriptions loc)
                                                         (try (http-get url)
                                                              (catch Exception _
                                                                (append-event-log (format "[%s] error getting XML description" loc))))))
                            desc                (try (xml/parse (ByteArrayInputStream. (.getBytes description)))
                                                     (catch Exception _
                                                       (append-event-log (format "[%s] error parsing XML description" loc))))
                            target              (desc->target desc)]

                        (when-not target
                          (append-event-log (format "[%s] not a Defold target" url)))

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
    (let [found-targets (:targets res)]
      (when (not-empty found-targets)
        (append-event-log (str "Found engine(s) " (into '() found-targets))))

      (when (or
             ;; We found new/different engines
             (and (not-empty found-targets)
                  (not= found-targets @targets))

             ;; We didn't find any engines (but we had atleast one in the list)
             (and (empty? found-targets)
                  (not= @targets #{local-target})))
        (ui/invalidate-menus!))

      (reset! targets (or (not-empty found-targets) #{local-target})))))

(defn- targets-worker []
  (when-let [ssdp-service (SSDP. append-event-log)]
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

(defn make-target-log-dialog []
  (let [root        ^Parent (ui/load-fxml "target-log.fxml")
        stage       (Stage.)
        scene       (Scene. root)
        controls    (ui/collect-controls root ["message" "ok" "clear"])
        get-message (fn [log] (apply str (interpose "\n" log)))]
    (ui/title! stage "Engine Target Event Log")
    (ui/text! (:message controls) (get-message @event-log))
    (ui/on-action! (:ok controls) (fn [_] (.close stage)))
    (ui/on-action! (:clear controls) (fn [_] (reset! event-log [])))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
      (ui/event-handler event
                        (let [code (.getCode ^KeyEvent event)]
                          (when (= code KeyCode/ESCAPE)
                            (.close stage)))))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)

    (add-watch event-log :dialog (fn [_ _ _ log]
                                   (ui/text! (:message controls) (get-message log))))

    (ui/show-and-wait! stage)
    (remove-watch event-log :dialog)))
