(ns editor.targets
  (:require [clojure
             [string :as str]
             [xml :as xml]]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs])
  (:import [com.dynamo.upnp DeviceInfo SSDP SSDP$Logger]
           [java.io ByteArrayInputStream ByteArrayOutputStream IOException]
           [java.net MalformedURLException SocketTimeoutException URL URLConnection]
           [javafx.scene Parent Scene]
           [javafx.scene.control TextArea]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality]
           org.apache.commons.io.IOUtils))

(set! *warn-on-reflection* true)

(defonce ^:const local-target
  {:name "Local"
   :url  "http://localhost:8001"
   ;; :local-address must be an ipv4 address. dmengine
   ;; will resolve "localhost" as an ipv6 address and not find
   ;; the editor web server.
   :local-address "127.0.0.1"})

(defonce ^:private targets (atom #{local-target}))
(defonce ^:private blacklist (atom #{}))
(defonce ^:private descriptions (atom {}))
(defonce ^:private last-search (atom 0))
(defonce ^:private running (atom false))
(defonce ^:private worker (atom nil))
(defonce ^:private event-log (atom []))
(defonce ^:private ssdp-service (atom nil))

(def ^:const search-interval-disconnected (* 5 1000))
(def ^:const search-interval-connected (* 30 1000))
(def ^:const timeout 200)
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

(defn- desc->target [desc local-address]
  (when-let [tags (and (= {:xmlns:defold "urn:schemas-defold-com:DEFOLD-1-0", :xmlns "urn:schemas-upnp-org:device-1-0"}
                          (:attrs desc))
                       (->> desc :content (filter #(= :device (:tag %))) first :content))]
    (when (some->> tags (tag->val :manufacturer) str/lower-case (= "defold"))
      (let [target {:name     (tag->val :friendlyName tags)
                    :model    (tag->val :modelName tags)
                    :udn      (tag->val :UDN tags)
                    :url      (tag->val :defold:url tags)
                    :log-port (tag->val :defold:logPort tags)
                    :local-address local-address}]
        (when (not-any? nil? (vals target))
          target)))))

(defn- log [message]
  (swap! event-log (fn [xs]
                     (if (not= (last xs) message)
                       (let [discard (max 0 (inc (- (count xs) max-log-entries)))]
                         (-> xs
                           (conj message)
                           (subvec discard)))
                       xs)))
  nil)

(defn- process-devices [{:keys [blacklist-atom descriptions-atom fetch-url-fn log-fn]} devices]
  (reduce (fn [{:keys [targets blacklist descriptions]} ^DeviceInfo device]
            (let [loc        (.get (.headers device) "LOCATION")
                  ^URL url   (try (URL. loc)
                                  (catch MalformedURLException _
                                    (log-fn (format "[%s] not a valid URL" loc))
                                    nil))
                  maybe-desc (and url
                                  (not (contains? blacklist url))
                                  (or (get descriptions url)
                                      (let [fetch-result (try (or (fetch-url-fn url)
                                                                  (throw (NullPointerException. (format "The supplied fetch-url-fn broke the contract by returning nil for %s" url))))
                                                              (catch SocketTimeoutException _
                                                                (log-fn (format "[%s] timed out getting XML description" loc))
                                                                ::socket-timeout)
                                                              (catch IOException _
                                                                (log-fn (format "[%s] error getting XML description" loc))
                                                                ::fetch-error))]
                                        (if-not (string? fetch-result)
                                          fetch-result
                                          (try (xml/parse (ByteArrayInputStream. (.getBytes ^String fetch-result)))
                                               (catch Exception _
                                                 (log-fn (format "[%s] error parsing XML description" loc))
                                                 ::parse-error))))))
                  target     (and (map? maybe-desc)
                                  (desc->target maybe-desc (.localAddress device)))]

              (when (and (not target) (not= ::socket-timeout maybe-desc))
                (log-fn (format "[%s] not a Defold target" loc)))

              {:targets      (if target
                               (conj targets target)
                               targets)
               :blacklist    (if (and url (not target) (not= ::socket-timeout maybe-desc))
                               (conj blacklist url)
                               blacklist)
               :descriptions (if (map? maybe-desc)
                               (assoc descriptions url maybe-desc)
                               descriptions)}))
          {:targets #{}
           :blacklist @blacklist-atom
           :descriptions @descriptions-atom}
          devices))

(def ^:private update-targets-context
  {:targets-atom targets
   :blacklist-atom blacklist
   :descriptions-atom descriptions
   :log-fn log
   :fetch-url-fn http-get
   :on-targets-changed-fn ui/invalidate-menus!})

(defn update-targets! [{:keys [targets-atom descriptions-atom blacklist-atom log-fn on-targets-changed-fn] :as context} devices]
  (let [{found-targets :targets
         updated-blacklist :blacklist
         updated-descriptions :descriptions} (process-devices context devices)]
    (when (not-empty found-targets)
      (log-fn (format "Found engine(s) [%s]" (str/join "," (mapv (fn [t] (let [url (URL. (:url t))]
                                                                           (format "%s (%s)" (:name t) (.getHost url)))) found-targets)))))
    (let [old-targets @targets-atom]
      (reset! descriptions-atom updated-descriptions)
      (reset! blacklist-atom updated-blacklist)
      (reset! targets-atom (or (not-empty found-targets) #{local-target}))
      (when (or
              ;; We found new/different engines
              (and (not-empty found-targets)
                   (not= found-targets old-targets))

              ;; We didn't find any engines (but we had at least one in the list)
              (and (empty? found-targets)
                   (not= old-targets #{local-target})))
        (on-targets-changed-fn)))))

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
              (when search?
                (reset! last-search now))
              (when (or search? changed?)
                (update-targets! update-targets-context (.getDevices ssdp-service'))))))
        (do
          (reset! running false)))
      (catch Exception e
        (prn e))
      (finally
        (.dispose ssdp-service')
        (reset! ssdp-service nil)))))

(defn update! []
  (when-let [^SSDP ss @ssdp-service]
    (update-targets! update-targets-context (.getDevices ss))
    (reset! last-search (System/currentTimeMillis))))

(defn start []
  (when (not @running)
    (reset! running true)
    (reset! blacklist #{})
    (reset! descriptions {})
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

(ui/extend-menu ::menubar :editor.defold-project/project-end
                [{:label "Target"
                  :on-submenu-open update!
                  :command :target}
                 {:label "Enter Target IP"
                  :command :target-ip}
                 {:label "Target Discovery Log"
                  :command :target-log}])
