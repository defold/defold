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
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as str]
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
  (:import [com.dynamo.discovery MDNS MDNS$Logger MDNSServiceInfo]
           [java.io ByteArrayOutputStream]
           [java.net InetAddress NetworkInterface URL URLConnection]
           [java.util UUID]))

(set! *warn-on-reflection* true)

(defonce ^:private launched-targets (atom []))
(defonce ^:private mdns-targets (atom []))
;; We cache the selected target in an atom to avoid garbage from parsing prefs.
;; Must clear when launched-targets or mdns-targets change.
(defonce ^:private selected-target-atom (atom ::undefined))
(defonce ^:private manual-device (atom nil))
(defonce ^:private last-search (atom 0))
(defonce ^:private running (atom false))
(defonce ^:private worker (atom nil))
(defonce ^:private event-log (atom []))
(defonce ^:private mdns-service (atom nil))

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

(defn- http-get-json [^URL url]
  (json/read-str (http-get url) :key-fn keyword))

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
  {:targets-atom mdns-targets
   :log-fn log
   :on-targets-changed-fn invalidate-target-menu!})

(defn- device->target [device]
  (let [address (:address device)
        port (:port device)
        local-address (or (:local-address device) address)
        name (or (:name device) (:instance-name device) address)
        port-str (some-> port str)
        url (when (and (some? address) (some? port-str))
              (format "http://%s:%s" address port-str))
        id (or (:id device) (:service-name device) url)]
    (when (and id name url address local-address)
      (cond-> {:name name
               :url url
               :id id
               :address address
               :local-address local-address}
        (:log-port device)
        (assoc :log-port (:log-port device))))))

(defn- manual-target-device [ip port local-address info]
  (let [log-port (some-> (:log_port info) str)]
    (cond-> {:id (format "manual-%s:%s" ip port)
             :name (format "%s:%s" ip port)
             :instance-name ip
             :address ip
             :local-address local-address
             :port (Integer/parseInt port)}
      log-port
      (assoc :log-port log-port))))

(defn- local-target? [target]
  (or (= (:address target) (:local-address target))
      (launched-target? target)))

(defn update-targets! [{:keys [targets-atom on-targets-changed-fn]} devices]
  (let [devices (if-let [manual-device @manual-device]
                  (conj devices manual-device)
                  devices)
        old-targets @targets-atom
        targets (->> devices
                  (pmap device->target)
                  (filter some?))
        {external-targets false local-targets true} (group-by local-target? targets)
        targets (into [] (comp cat (distinct)) [(sort-by :url local-targets)
                                                (sort-by :url external-targets)])]
    (reset! targets-atom targets)
    (when (not= targets old-targets)
      (clear-selected-target-hint!)
      (on-targets-changed-fn))))

(defn- search-interval [^MDNS mdns]
  (if (.isConnected mdns)
    search-interval-connected
    search-interval-disconnected))

(defn- device->map [^MDNSServiceInfo device]
  {:id (.id device)
   :name (.instanceName device)
   :service-name (.serviceName device)
   :host (.host device)
   :address (.address device)
   :local-address (.localAddress device)
   :port (.port device)
   :log-port (.logPort device)
   :txt (.txt device)
   :expires (.expires device)})

(defn- devices [^MDNS mdns]
  (map device->map (.getDevices mdns)))

(defn- targets-worker []
  (let [mdns-service' (MDNS. (reify MDNS$Logger
                               (log [this msg] (log msg))))]
    (try
      (if (.setup mdns-service')
        (do
          (reset! mdns-service mdns-service')
          (while @running
            (Thread/sleep update-interval)
            (let [now      (System/currentTimeMillis)
                  search?  (>= now (+ @last-search (search-interval mdns-service')))
                  changed? (.update mdns-service' search?)]
              (when search?
                (reset! last-search now))
              (when (or search? changed?)
                (update-targets! update-targets-context (devices mdns-service'))))))
        (do
          (reset! running false)))
      (catch Exception e
        (prn e))
      (finally
        (.dispose mdns-service')
        (reset! mdns-service nil)))))

(defn- update! []
  (when-let [^MDNS mdns @mdns-service]
    (update-targets! update-targets-context (devices mdns))))

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
  (concat [{:id :all-launched-targets}] @launched-targets @mdns-targets))

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
            mdns-options (mapv target-option @mdns-targets)]
        (cond
          (seq launched-options)
          (if (> (count launched-options) 1)
            (into [{:label (localization/message "command.run.select-target.option.all-launched-instances")
                    :check true
                    :command :run.select-target
                    :user-data {:id :all-launched-targets}}]
                  (concat launched-options
                          [separator]
                          mdns-options))
            (into launched-options (concat [separator] mdns-options)))

          :else
          (into [{:label (localization/message "command.run.select-target.option.new-local-engine")
                  :check true
                  :command :run.select-target
                  :user-data :new-local-engine}
                 separator]
                mdns-options))))))

(defn- locate-device [ip port]
  (when (not-empty ip)
    (let [port (or port "8001")
          inet-addr (InetAddress/getByName ip)
          n-ifs (MDNS/getMCastInterfaces)
          device (when-let [^NetworkInterface n-if (first (filter (fn [^NetworkInterface n-if] (.isReachable inet-addr n-if MDNS/MDNS_MCAST_TTL timeout)) n-ifs))]
                   (when-let [^InetAddress local-address (first (MDNS/getIPv4Addresses n-if))]
                     (let [info-url (URL. (format "http://%s:%s/info" ip port))
                           info (http-get-json info-url)]
                       (manual-target-device ip port (.getHostAddress local-address) info))))]
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
                         (device->target device))
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
