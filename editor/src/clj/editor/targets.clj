(ns editor.targets
  (:require [clojure.string :as str]
            [clojure.xml :as xml]
            [clojure.java.io :as io]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.util :as util])
  (:import [clojure.lang ExceptionInfo]
           [com.dynamo.upnp DeviceInfo SSDP SSDP$Logger]
           [java.io ByteArrayInputStream ByteArrayOutputStream IOException]
           [java.net InetAddress MalformedURLException NetworkInterface SocketTimeoutException URL URLConnection]
           [javafx.scene Parent Scene]
           [javafx.scene.control TextArea]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality]))

(set! *warn-on-reflection* true)

(defonce ^:const local-target
  {:name "Local"
   :url  "http://127.0.0.1:8001"
   :address "127.0.0.1"
   ;; :local-address must be an ipv4 address. dmengine
   ;; will resolve "localhost" as an ipv6 address and not find
   ;; the editor web server.
   :local-address "127.0.0.1"})

(defonce ^:private targets (atom [local-target]))
(defonce ^:private manual-device (atom nil))
(defonce ^:private last-search (atom 0))
(defonce ^:private running (atom false))
(defonce ^:private worker (atom nil))
(defonce ^:private event-log (atom []))
(defonce ^:private ssdp-service (atom nil))
(defonce ^:private defold-upnp-attrs {:xmlns:defold "urn:schemas-defold-com:DEFOLD-1-0", :xmlns "urn:schemas-upnp-org:device-1-0"})

(def ^:const search-interval-disconnected (* 5 1000))
(def ^:const search-interval-connected (* 30 1000))
(def ^:const update-interval 1000)
(def ^:const timeout 200)
(def ^:const max-log-entries 512)

(defn- http-get [^URL url]
  (let [conn   ^URLConnection (doto (.openConnection url)
                                (.setRequestProperty "Connection" "close")
                                (.setConnectTimeout timeout)
                                (.setReadTimeout timeout))
        input  (.getInputStream conn)
        output (ByteArrayOutputStream.)]
    (try
      (io/copy input output)
      (.toString output "UTF8")
      (finally
        (.close input)))))

(defn- tag->val [tag tags]
  (->> tags
       (filter #(= tag (:tag %)))
       first
       :content
       first))

(defn- log [message]
  (swap! event-log (fn [xs]
                     (if (not= (last xs) message)
                       (let [discard (max 0 (inc (- (count xs) max-log-entries)))]
                         (-> xs
                           (conj message)
                           (subvec discard)))
                       xs)))
  nil)

(def ^:private update-targets-context
  {:targets-atom targets
   :log-fn log
   :fetch-url-fn http-get
   :on-targets-changed-fn #(ui/invalidate-menubar-item! ::target)})

(defn- device->target [{:keys [fetch-url-fn]} device]
  ;; The reason we try/throw/catch is so that this function can run through pmap,
  ;; yet return sensible error messages in case of failure for later logging
  (try
    (when-let [location (get-in device [:headers "LOCATION"])]
      (let [url (try
                  (URL. location)
                  (catch MalformedURLException e
                    (throw (ex-info (format "[%s] not a valid URL: %s" location (.getMessage e)) {} e))))
            response (try
                       (fetch-url-fn url)
                       (catch SocketTimeoutException e
                         (throw (ex-info (format "[%s] timed out getting XML description: %s" location (.getMessage e)) {} e)))
                       (catch IOException e
                         (throw (ex-info (format "[%s] error getting XML description: %s" location (.getMessage e)) {} e))))
            desc (try
                   (xml/parse (ByteArrayInputStream. (.getBytes ^String response)))
                   (catch Exception e
                     (throw (ex-info (format "[%s] error parsing XML description: %s" location (.getMessage e)) {} e))))]
        (when (not= (:attrs desc) defold-upnp-attrs)
          (throw (ex-info (format "[%s] invalid UPNP attributes: %s" location (:attrs desc)) {})))
        (let [tags (->> desc :content (filter #(= :device (:tag %))) first :content)
              address (:address device)
              local-address (:local-address device)
              name (if (= address local-address)
                     "Local"
                     (tag->val :friendlyName tags))
              target {:name name
                      :url (tag->val :defold:url tags)
                      :log-port (tag->val :defold:logPort tags)
                      :address address
                      :local-address local-address}]
          (when (not-any? nil? (vals target))
            target))))
    (catch ExceptionInfo e
      (.getMessage e))))

(defn- local-target? [target]
  (= (:address target) (:local-address target)))

(defn update-targets! [{:keys [targets-atom log-fn on-targets-changed-fn] :as context} devices]
  (let [devices (if-let [manual-device @manual-device]
                  (conj devices manual-device)
                  devices)
        old-targets @targets-atom
        target-by-address (into {} (map (fn [t] [(:address t) t]) old-targets))
        targets-result (pmap (fn [device] (device->target context device)) devices)
        targets (->> targets-result
                  (map (fn [device result]
                         (if (string? result)
                           (get target-by-address (:address device))
                           result))
                    devices)
                  (filter some?))
        errors (filter string? targets-result)
        local-target (or (first (filter local-target? targets)) local-target)
        external-targets (remove local-target? targets)
        targets (vec (distinct (cons local-target (sort-by :name util/natural-order external-targets))))]
    (doseq [error errors]
      (log-fn error))
    (reset! targets-atom targets)
    (when (not= targets old-targets)
      (on-targets-changed-fn))))

(defn- search-interval [^SSDP ssdp]
  (if (.isConnected ssdp)
    search-interval-connected
    search-interval-disconnected))

(defn- device->map [^DeviceInfo device]
  {:address (.address device)
   :local-address (.localAddress device)
   :headers (.headers device)
   :expires (.expires device)})

(defn- defold-service? [device]
  (let [server (get (:headers device) "SERVER" "")]
    (= server SSDP/SSDP_SERVER_IDENTIFIER)))

(defn- devices [^SSDP ssdp]
  (filter defold-service? (map device->map (.getDevices ssdp))))

(defn- targets-worker []
  (let [ssdp-service' (SSDP. (reify SSDP$Logger
                               (log [this msg] (log msg))))]
    (try
      (if (.setup ssdp-service')
        (do
          (reset! ssdp-service ssdp-service')
          (while @running
            (Thread/sleep update-interval)
            (let [now      (System/currentTimeMillis)
                  search?  (>= now (+ @last-search (search-interval ssdp-service')))
                  changed? (.update ssdp-service' search?)]
              (when search?
                (reset! last-search now))
              (when (or search? changed?)
                (update-targets! update-targets-context (devices ssdp-service'))))))
        (do
          (reset! running false)))
      (catch Exception e
        (prn e))
      (finally
        (.dispose ssdp-service')
        (reset! ssdp-service nil)))))

(defn- update! []
  (when-let [^SSDP ss @ssdp-service]
    (update-targets! update-targets-context (devices ss))))

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
        stage       (doto (ui/make-stage)
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
    (ui/on-closed! stage (fn [_]
                           (remove-watch event-log :dialog)))
    (ui/show! stage)))

(defn selected-target [prefs]
  (let [target-address (prefs/get-prefs prefs "selected-target-address" nil)
        targets (get-targets)]
    (or (first (filter #(= target-address (:address %)) targets))
      (first targets))))

(defn- select-target! [prefs target]
  (prefs/set-prefs prefs "selected-target-address" (:address target)))

(handler/defhandler :target :global
  (run [user-data prefs]
    (when user-data
      (select-target! prefs user-data)))
  (state [user-data prefs]
         (let [selected-target (selected-target prefs)]
           (= user-data selected-target)))
  (options [user-data prefs]
           (when-not user-data
             (let [targets (get-targets)]
               (mapv (fn [target]
                       {:label     (format "%s (%s)" (:name target) (:address target))
                        :command   :target
                        :check     true
                        :user-data target})
                 targets)))))

(defn- locate-device [ip]
  (when (not-empty ip)
    (let [inet-addr (InetAddress/getByName ip)
          n-ifs (SSDP/getMCastInterfaces)
          device (when-let [^NetworkInterface n-if (first (filter (fn [^NetworkInterface n-if] (.isReachable inet-addr n-if SSDP/SSDP_MCAST_TTL timeout)) n-ifs))]
                   (when-let [^InetAddress local-address (first (SSDP/getIPv4Addresses n-if))]
                     {:address ip
                      :local-address (.getHostAddress local-address)
                      :headers {"LOCATION" (format "http://%s:8001/upnp" ip)}}))]
      (if device
        device
        (throw (ex-info (format "'%s' could not be reached from this host" ip) {}))))))

(handler/defhandler :target-ip :global
  (run [prefs]
    (ui/run-later
      (loop [manual-ip (dialogs/make-target-ip-dialog (prefs/get-prefs prefs "manual-target-ip" "") nil)]
        (when (some? manual-ip)
          (prefs/set-prefs prefs "manual-target-ip" manual-ip)
          (let [device (try
                         (locate-device manual-ip)
                         (catch Exception e (.getMessage e)))]
            (if (string? device)
              (recur (dialogs/make-target-ip-dialog manual-ip device))
              (do
                (reset! manual-device device)
                (prefs/set-prefs prefs "selected-target-address" (not-empty manual-ip))
                (ui/invalidate-menubar-item! ::target)))))))))

(handler/defhandler :target-log :global
  (run []
    (ui/run-later (make-target-log-dialog))))

(ui/extend-menu ::menubar :editor.defold-project/project-end
                [{:label "Target"
                  :id ::target
                  :on-submenu-open update!
                  :command :target}
                 {:label "Enter Target IP"
                  :command :target-ip}
                 {:label "Target Discovery Log"
                  :command :target-log}])
