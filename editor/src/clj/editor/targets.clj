;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.targets
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [clojure.xml :as xml]
            [editor.console :as console]
            [editor.dialogs :as dialogs]
            [editor.engine :as engine]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [clojure.lang ExceptionInfo]
           [com.dynamo.upnp DeviceInfo SSDP SSDP$Logger]
           [java.io ByteArrayInputStream ByteArrayOutputStream IOException]
           [java.net InetAddress MalformedURLException NetworkInterface SocketTimeoutException URL URLConnection]
           [java.util UUID]))

(set! *warn-on-reflection* true)

(defonce ^:private launched-targets (atom []))
(defonce ^:private ssdp-targets (atom []))
;; We cache the selected target in an atom to avoid garbage from parsing prefs.
;; Must clear when launched-targets or ssdp-targets change.
(defonce ^:private selected-target-atom (atom ::undefined))
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

(defn- clear-selected-target-hint! []
  (reset! selected-target-atom ::undefined))

(defn kill-launched-target! [target]
  (let [^Process process (:process target)]
    (when (.isAlive process)
      (.destroy process))))

(defn kill-launched-targets! []
  (doseq [launched-target @launched-targets]
    (kill-launched-target! launched-target))
  (reset! launched-targets [])
  (clear-selected-target-hint!))

(def ^:private kill-lingering-launched-targets-hook
  (Thread. (fn [] (kill-launched-targets!))))

(defn- invalidate-target-menu! []
  (ui/invalidate-menubar-item! ::target))

(defn launched-target? [target]
  (contains? target :process))

(defn remote-target? [target]
  (not (launched-target? target)))

(defn add-launched-target! [instance-index target]
  (assert (launched-target? target))
  (let [launched-target (assoc target
                          :local-address "127.0.0.1"
                          :id (str (UUID/randomUUID))
                          :instance-index instance-index)]
    (when (= instance-index 0)
      (kill-launched-targets!))
    (swap! launched-targets conj launched-target)
    (clear-selected-target-hint!)
    (invalidate-target-menu!)
    (process/on-exit! (:process launched-target)
                      (fn []
                        (swap! launched-targets (partial remove #(= (:id %) (:id launched-target))))
                        (clear-selected-target-hint!)
                        (invalidate-target-menu!)))
    launched-target))

(defn- find-by-id [targets id]
  (some #(when (= (:id %) id) %) targets))

(defn- url-watcher [id callback]
  (fn [key ref _ targets]
    (let [target (find-by-id targets id)
          url (:url target)]
      (when (or url (nil? target))
        (remove-watch ref key))
      (when url
        (callback url)))))

(defn when-url [id callback]
  (when-let [target (find-by-id @launched-targets id)]
    (if-let [url (:url target)]
      (callback url)
      (add-watch launched-targets [::url id callback] (url-watcher id callback)))))

(defn update-launched-target! [target target-info]
  (let [old @launched-targets
        result-target (volatile! nil)]
    (reset! launched-targets
            (map (fn [launched-target]
                   (if (= (:id launched-target) (:id target))
                     (let [rt (merge launched-target target-info)]
                       (vreset! result-target rt)
                       rt)
                     launched-target))
                 old))
    (when (not= old @launched-targets)
      (clear-selected-target-hint!)
      (invalidate-target-menu!))
    @result-target))

(defn launched-targets? []
  (seq @launched-targets))

(defn all-launched-targets []
  @launched-targets)

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
        targets (into [] (comp cat (distinct)) [(sort-by :url local-targets)
                                                (sort-by :url external-targets)])]
    (doseq [error errors]
      (log-fn error))
    (reset! targets-atom targets)
    (when (not= targets old-targets)
      (clear-selected-target-hint!)
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
  (concat [{:id :all-launched-targets}] @launched-targets @ssdp-targets))

(defn selected-target [prefs]
  (swap! selected-target-atom
         (fn [selected-target]
           (if (not= ::undefined selected-target)
             selected-target
             (let [target-id (prefs/get prefs [:run :selected-target-id])]
               (find-by-id (all-targets) target-id))))))

(defn controllable-target? [target]
  (some? (:url target)))

(defn all-launched-targets? [target]
  (= :all-launched-targets (:id target)))

(defn- show-error-message [exception workspace]
  (ui/run-later
    (notifications/show!
      (workspace/notifications workspace)
      {:type :error
       :id ::target-connection-error
       :message (localization/message
                  "notification.targets.selected-target-unavailable.error"
                  {"error" (or (ex-message exception) (.getSimpleName (class exception)))})})))

(defn select-target! [prefs target]
  (reset! selected-target-atom target)
  (prefs/set! prefs [:run :selected-target-id] (:id target))
  (let [log-stream (engine/get-log-service-stream target)]
    (when log-stream
      (console/set-log-service-stream log-stream)))
  target)

(defn- url-message
  "Returns localization MessagePattern (or string)"
  [url-string]
  (try
    (if (nil? url-string)
      (localization/message "engine.url.unavailable")
      (let [url (URL. url-string)
            host (.getHost url)
            port (.getPort url)]
        (str host (when (not= port -1) (str ":" port)))))
    (catch Exception _
      (localization/message "engine.url.invalid-host"))))

(defn target-message
  "Returns localization MessagePattern (or string)"
  [target]
  (let [{:keys [url name]} target
        name-message (if (local-target? target)
                       (localization/message "engine.name.local" {"name" name})
                       name)]
    (if (some? url)
      (localization/message "engine.name.with-url" {"name" name-message "url" (url-message url)})
      name-message)))

(defn target-menu-item-message
  "Returns localization MessagePattern (or string)"
  [target]
  (let [instance-index (:instance-index target)
        target-message (target-message target)]
    (if (or (nil? instance-index) (= instance-index 0))
      target-message
      (localization/message "engine.name.with-instance" {"name" target-message "instance" instance-index}))))

(defn- target-option [target]
  {:label     (target-menu-item-message target)
   :command   :run.select-target
   :check     true
   :user-data target})

(def ^:private separator {:label :separator})

(handler/defhandler :run.select-target :global
  (run [user-data prefs workspace]
    (when user-data
      (try
        (select-target! prefs (if (= user-data :new-local-engine) nil user-data))
        (catch Exception e
          (show-error-message e workspace)))))
  (state [user-data prefs]
    (let [selected-target (selected-target prefs)]
      (or (= user-data selected-target)
          (= (:id user-data) (:id selected-target) :all-launched-targets)
          (and (nil? selected-target)
               (= user-data :new-local-engine)))))
  (options [user-data]
    (when-not user-data
      (let [launched-options (mapv target-option @launched-targets)
            ssdp-options (mapv target-option @ssdp-targets)]
        (cond
          (seq launched-options)
          (if (> (count launched-options) 1)
            (into [{:label (localization/message "command.run.select-target.option.all-launched-instances")
                    :check true
                    :command :run.select-target
                    :user-data {:id :all-launched-targets}}]
                  (concat launched-options
                          [separator]
                          ssdp-options))
            (into launched-options (concat [separator] ssdp-options)))

          :else
          (into [{:label (localization/message "command.run.select-target.option.new-local-engine")
                  :check true
                  :command :run.select-target
                  :user-data :new-local-engine}
                 separator]
                ssdp-options))))))

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

(handler/defhandler :run.set-target-ip :global
  (run [prefs localization]
    (ui/run-later
      (loop [manual-ip+port (dialogs/make-target-ip-dialog (prefs/get prefs [:run :manual-target-ip+port]) nil localization)]
        (when (some? manual-ip+port)
          (prefs/set! prefs [:run :manual-target-ip+port] manual-ip+port)
          (let [[manual-ip port] (str/split manual-ip+port #":")
                device (try
                         (locate-device manual-ip port)
                         (catch Exception e (.getMessage e)))
                target (when (not (string? device))
                         (device->target update-targets-context device))
                error-msg (or (and (string? target) target)
                              (and (string? device) device))]
            (if error-msg
              (recur (dialogs/make-target-ip-dialog manual-ip+port error-msg localization))
              (do
                (reset! manual-device device)
                (select-target! prefs target)
                (invalidate-target-menu!)))))))))

(handler/defhandler :run.show-target-log :global
  (run [localization]
    (dialogs/make-target-log-dialog event-log #(reset! event-log []) restart localization)))

(handler/defhandler :run.stop :global
  (enabled? [app-view] (launched-targets?))
  (active? [] true)
  (run []
       (kill-launched-targets!)))

(handler/register-menu! ::menubar :editor.defold-project/targets
  [{:label (localization/message "command.run.select-target")
    :id ::target
    :on-submenu-open update!
    :command :run.select-target
    :expand true}
   {:label (localization/message "command.run.stop")
    :command :run.stop}
   {:label (localization/message "command.run.set-instance-count")
    :command :run.set-instance-count
    :expand true}
   {:label (localization/message "command.run.set-target-ip")
    :command :run.set-target-ip}
   {:label (localization/message "command.run.show-target-log")
    :command :run.show-target-log}])
