(ns editor.targets
  (:require [clojure
             [string :as str]
             [xml :as xml]]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs])
  (:import [com.dynamo.upnp DeviceInfo SSDP SSDP$Logger]
           [java.io ByteArrayInputStream ByteArrayOutputStream]
           [java.net URL URLConnection]
           [javafx.scene Parent Scene]
           [javafx.scene.control TextArea]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality Stage]
           org.apache.commons.io.IOUtils))

(set! *warn-on-reflection* true)

(defonce ^:const local-target
  {:name "Local"
   :url  "http://localhost:8001"})
(defonce ^:private targets (atom #{local-target}))
(defonce ^:private descriptions (atom {}))
(defonce ^:private last-search (atom 0))
(defonce ^:private running (atom false))
(defonce ^:private active-ips (atom nil))
(defonce ^:private worker (atom nil))
(defonce ^:private event-log (atom []))
(defonce ^:private ssdp-service (atom nil))

(def ^:const search-interval-disconnected (* 5 1000))
(def ^:const search-interval-connected (* 30 1000))
(def ^:const timeout 2000)
(def ^:const max-log-entries 512)

(defn- http-get [^URL url]
  (let [conn   ^URLConnection (doto (.openConnection url)
                                (.setRequestProperty "Connection" "close")
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

(defn- log [message]
  (swap! event-log (fn [xs]
                     (if (not= (last xs) message)
                       (let [discard (max 0 (inc (- (count xs) max-log-entries)))]
                         (-> xs
                           (conj message)
                           (subvec discard)))
                       xs))))

(defn- update-targets! [devices]
  (let [res (reduce (fn [{:keys [blacklist targets] :as acc} ^DeviceInfo device]
                      (let [loc                 (.get (.headers device) "LOCATION")
                            ^URL url            (try (URL. loc)
                                                     (catch Exception _
                                                       (log (format "[%s] not a valid URL" loc))))
                            ^String description (and url (not (contains? blacklist (.getHost url)))
                                                     (or (get @descriptions loc)
                                                         (try (http-get url)
                                                              (catch Exception _
                                                                (log (format "[%s] error getting XML description" loc))))))
                            desc                (try (xml/parse (ByteArrayInputStream. (.getBytes description)))
                                                     (catch Exception _
                                                       (log (format "[%s] error parsing XML description" loc))))
                            target              (desc->target desc)]

                        (when-not target
                          (log (format "[%s] not a Defold target" url)))

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
        (log (format "Found engine(s) [%s]" (str/join "," (mapv (fn [t] (let [url (URL. (:url t))]
                                                                          (format "%s (%s)" (:name t) (.getHost url)))) found-targets)))))

      (when (or
             ;; We found new/different engines
             (and (not-empty found-targets)
                  (not= found-targets @targets))

             ;; We didn't find any engines (but we had atleast one in the list)
             (and (empty? found-targets)
                  (not= @targets #{local-target})))
        (ui/invalidate-menus!))

      (reset! targets (or (not-empty found-targets) #{local-target})))))

(defn- search-interval [^SSDP ssdp]
  (if (.isConnected ssdp)
    search-interval-connected
    search-interval-disconnected))

(defn- targets-worker []
  (let [ssdp-service' (SSDP. (reify SSDP$Logger
                               (log [this msg] (log msg))))]
    (try
      (if (.setup ssdp-service')
        (do
          (reset! ssdp-service ssdp-service')
          (while @running
            (Thread/sleep 200)
            (let [now      (System/currentTimeMillis)
                  search?  (>= now (+ @last-search (search-interval ssdp-service')))
                  changed? (.update ssdp-service' search?)]
              (reset! active-ips (.getIPs ssdp-service'))
              (when search?
                (reset! last-search now))
              (when (or search? changed?)
                (update-targets! (.getDevices ssdp-service'))))))
        (do
          (reset! running false)
          (reset! active-ips nil)))
      (catch Exception e
        (prn e))
      (finally
        (.dispose ssdp-service')
        (reset! ssdp-service nil)))))

(defn update! []
  (when-let [^SSDP ss @ssdp-service]
    (update-targets! (.getDevices ss))
    (reset! last-search (System/currentTimeMillis))))

(defn start []
  (when (not @running)
    (reset! running true)
    (reset! worker (future (targets-worker)))))

(defn stop []
  (reset! running false)
  (when-let [f @worker]
    @f
    (reset! worker nil))
  nil)

(defn restart []
  (log "Restarting service")
  (stop)
  (start))

(defn get-targets []
  @targets)

(defn make-target-log-dialog []
  (let [root        ^Parent (ui/load-fxml "target-log.fxml")
        stage       (doto (Stage.)
                      (.setAlwaysOnTop true)
                      (.initOwner (ui/main-stage)))
        scene       (Scene. root)
        controls    (ui/collect-controls root ["message" "ok" "clear" "restart"])
        get-message (fn [log] (apply str (interpose "\n" log)))]
    (dialogs/observe-focus stage)
    (ui/title! stage "Target Discovery Log")
    (ui/text! (:message controls) (get-message @event-log))
    (ui/on-action! (:ok controls) (fn [_] (.close stage)))
    (ui/on-action! (:clear controls) (fn [_] (reset! event-log [])))
    (ui/on-action! (:restart controls) (fn [_] (restart)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
      (ui/event-handler event
                        (let [code (.getCode ^KeyEvent event)]
                          (when (= code KeyCode/ESCAPE)
                            (.close stage)))))

    (.initModality stage Modality/NONE)
    (.setScene stage scene)

    (add-watch event-log :dialog (fn [_ _ _ log]
                                   (ui/run-later (let [ta ^TextArea (:message controls)
                                                       old-text (ui/text ta)
                                                       new-text (get-message log)]
                                                   (when (and (empty? (.getSelectedText ta)) (not= old-text new-text))
                                                     (let [left (.getScrollLeft ta)]
                                                       (ui/text! ta new-text)
                                                       (.setScrollTop ta Double/MAX_VALUE)
                                                       (.deselect ta)
                                                       (.setScrollLeft ta left)))))))
    (ui/on-hiding! stage (fn [_]
                           (remove-watch event-log :dialog)))
    (ui/show! stage)))

(defn current-ip []
  (first @active-ips))
