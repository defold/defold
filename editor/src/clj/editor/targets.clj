(ns editor.targets
  (:require [clojure.string :as str]
            [clojure.xml :as xml]
            [clojure.java.io :as io]
            [editor.process :as process]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.util :as util])
  (:import [clojure.lang ExceptionInfo]
           [com.dynamo.upnp DeviceInfo SSDP SSDP$Logger]
           [java.io ByteArrayInputStream ByteArrayOutputStream IOException]
           [java.net InetAddress MalformedURLException NetworkInterface SocketTimeoutException URL URLConnection]
           [java.util UUID]
           [javafx.scene Parent Scene]
           [javafx.scene.control TextArea]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality]))

(set! *warn-on-reflection* true)

(defonce ^:private launched-targets (atom []))
(defonce ^:private ssdp-targets (atom []))
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

(defn kill-launched-target! [target]
  (let [^Process process (:process target)]
    (when (.isAlive process)
      (.destroy process))))

(defn kill-launched-targets! []
  (doseq [launched-target @launched-targets]
    (kill-launched-target! launched-target))
  (reset! launched-targets []))

(def ^:private kill-lingering-launched-targets-hook
  (Thread. (fn [] (kill-launched-targets!))))

(defn- invalidate-target-menu! []
  (ui/invalidate-menubar-item! ::target))

(defn launched-target? [target]
  (contains? target :process))

(defn remote-target? [target]
  (not (launched-target? target)))

(defn add-launched-target! [target]
  (assert (launched-target? target))
  (let [launched-target (assoc target
                               :local-address "127.0.0.1"
                               :id (str (UUID/randomUUID)))]
    (kill-launched-targets!)
    (swap! launched-targets conj launched-target)
    (invalidate-target-menu!)
    (process/watchdog! (:process launched-target)
                       (fn []
                         (swap! launched-targets (partial remove #(= (:id %) (:id launched-target))))
                         (invalidate-target-menu!)))
    launched-target))

(defn update-launched-target! [target target-info]
  (let [old @launched-targets]
    (reset! launched-targets
            (map (fn [launched-target]
                   (if (= (:id launched-target) (:id target))
                     (merge launched-target target-info)
                     launched-target))
                 old))
    (when (not= old @launched-targets)
      (invalidate-target-menu!))))

(defn launched-targets? []
  (seq @launched-targets))

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
  {:targets-atom ssdp-targets
   :log-fn log
   :fetch-url-fn http-get
   :on-targets-changed-fn invalidate-target-menu!})

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
              name (tag->val :friendlyName tags)
              url (tag->val :defold:url tags)
              target {:name name
                      :url url
                      :id url
                      :log-port (tag->val :defold:logPort tags)
                      :address address
                      :local-address local-address}]
          (when (not-any? nil? (vals target))
            target))))
    (catch ExceptionInfo e
      (.getMessage e))))

(defn- local-target? [target]
  (or (= (:address target) (:local-address target))
      (launched-target? target)))

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
        {external-targets false local-targets true} (group-by local-target? targets)
        targets (into [] (comp cat (distinct)) [(sort-by :name util/natural-order local-targets)
                                                (sort-by :name util/natural-order external-targets)])]
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
    (reset! worker (future (targets-worker)))
    (doto (Runtime/getRuntime)
      (.removeShutdownHook kill-lingering-launched-targets-hook)
      (.addShutdownHook kill-lingering-launched-targets-hook))))

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

(defn all-targets []
  (concat @launched-targets @ssdp-targets))

(defn make-target-log-dialog []
  (let [root        ^Parent (ui/load-fxml "target-log.fxml")
        stage       (doto (ui/make-stage)
                      (.initOwner (ui/main-stage)))
        scene       (Scene. root)
        controls    (ui/collect-controls root ["message" "ok" "clear" "restart"])
        get-message (fn [log] (apply str (interpose "\n" log)))]
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
  (let [target-address (prefs/get-prefs prefs "selected-target-id" nil)]
    (first (filter #(= (:id %) target-address) (all-targets)))))

(defn controllable-target? [target]
  (when (:url target) target))

(defn select-target! [prefs target]
  (prefs/set-prefs prefs "selected-target-id" (:id target))
  target)

(defn- url-string [url-string]
  (try
    (if (nil? url-string)
      "engine service not available"
      (let [url (URL. url-string)
            host (.getHost url)
            port (.getPort url)]
        (str host (when (not= port -1) (str ":" port)))))
    (catch Exception _
      "invalid host")))

(defn target-label [target]
  (format "%s - %s" (str (if (local-target? target) "Local " "") (:name target)) (url-string (:url target))))

(defn- target-option [target]
  {:label     (target-label target)
   :command   :target
   :check     true
   :user-data target})

(def ^:private separator {:label :separator})

(handler/defhandler :target :global
  (run [user-data prefs]
    (when user-data
      (select-target! prefs (if (= user-data :new-local-engine) nil user-data))))
  (state [user-data prefs]
         (let [selected-target (selected-target prefs)]
           (or (= user-data selected-target)
               (and (nil? selected-target)
                    (= user-data :new-local-engine)))))
  (options [user-data prefs]
           (when-not user-data
             (let [launched-options (mapv target-option @launched-targets)
                   ssdp-options (mapv target-option @ssdp-targets)]
               (cond
                 (seq launched-options)
                 (into launched-options (concat [separator] ssdp-options))

                 :else
                 (into [{:label "New Local Engine" :check true :command :target :user-data :new-local-engine} separator] ssdp-options))))))

(defn- locate-device [ip port]
  (when (not-empty ip)
    (let [port (or port "8001")
          inet-addr (InetAddress/getByName ip)
          n-ifs (SSDP/getMCastInterfaces)
          device (when-let [^NetworkInterface n-if (first (filter (fn [^NetworkInterface n-if] (.isReachable inet-addr n-if SSDP/SSDP_MCAST_TTL timeout)) n-ifs))]
                   (when-let [^InetAddress local-address (first (SSDP/getIPv4Addresses n-if))]
                     {:address ip
                      :local-address (.getHostAddress local-address)
                      :headers {"LOCATION" (format "http://%s:%s/upnp" ip port)}}))]
      (if device
        device
        (throw (ex-info (format "'%s' could not be reached from this host" ip) {}))))))

(handler/defhandler :target-ip :global
  (run [prefs]
    (ui/run-later
      (loop [manual-ip+port (dialogs/make-target-ip-dialog (prefs/get-prefs prefs "manual-target-ip+port" "") nil)]
        (when (some? manual-ip+port)
          (prefs/set-prefs prefs "manual-target-ip+port" manual-ip+port)
            (let [[manual-ip port] (str/split manual-ip+port #":")
                  device (try
                           (locate-device manual-ip port)
                           (catch Exception e (.getMessage e)))
                  target (when (not (string? device))
                           (device->target update-targets-context device))
                  error-msg (or (and (string? target) target)
                                (and (string? device) device))]
              (if error-msg
                (recur (dialogs/make-target-ip-dialog manual-ip+port error-msg))
                (do
                  (reset! manual-device device)
                  (select-target! prefs target)
                  (invalidate-target-menu!)))))))))

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
