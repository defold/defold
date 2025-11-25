;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.lsp
  (:require [clojure.core.async :as a :refer [<! >!]]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.error-reporting :as error-reporting]
            [editor.lsp.async :as lsp.async]
            [editor.lsp.server :as lsp.server]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.system :as system]
            [editor.ui :as ui]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair]]
            [util.fn :as fn])
  (:import [editor.code.data CursorRange]
           [java.util.regex Pattern]
           [sun.nio.fs Globs]))

(set! *warn-on-reflection* true)

(defn- sorted-resource-diagnostics [state resource]
  (into (sorted-set)
        (comp
          (filter #(= :running (:status %)))
          (mapcat #(get-in % [:diagnostics resource :items])))
        (vals (:server->server-state state))))

(defn- combined-resource-diagnostics [state resource]
  ;; Take resource diagnostics from all servers (meaning there might be
  ;; intersections in diagnostic ranges) and combine them to a sorted vector
  ;; of non-intersecting diagnostic regions, merging them as necessary
  (let [;; 1. Get all diagnostics and collect them to a sorted map from cursor
        ;;    to a vector of ranges that either start or end at this cursor
        cursor->cursor-ranges (util/group-into
                                (sorted-map)
                                []
                                key
                                val
                                (eduction
                                  (filter #(= :running (:status %)))
                                  (mapcat #(get-in % [:diagnostics resource :items]))
                                  (mapcat (fn [^CursorRange cursor-range]
                                            (pair
                                              (pair (.-from cursor-range) cursor-range)
                                              (pair (.-to cursor-range) cursor-range))))
                                  (vals (:server->server-state state))))]
    ;; 2. Convert collected map by going through every cursor in order and:
    ;;    - add a non-empty batch of current cursors as a single merged cursor
    ;;    - prepare for next step:
    ;;      * remove from the batch all the regions that end at current step
    ;;      * add to the batch all the regions that start at current step
    (:results
      (reduce-kv (fn [{:keys [batch cursor] :as acc} new-cursor new-cursor-ranges]
                   (-> acc
                       (cond-> (pos? (count batch))
                               (update
                                 :results conj
                                 ;; merge all intersecting ranges to a single range
                                 (assoc (data/->CursorRange cursor new-cursor)
                                   :type :diagnostic
                                   :hoverable true
                                   :severity (transduce
                                               (map :severity)
                                               (completing (partial max-key {:error 4 :warning 3 :information 2 :hint 1}))
                                               :hint
                                               batch)
                                   :messages (into [] (map :message) batch))))
                       (assoc :cursor new-cursor
                              :batch (into
                                       ;; remove all regions that end at current step
                                       (filterv #(not= new-cursor (.-to ^CursorRange %)) batch)
                                       ;; add all regions that start at current step
                                       (filter #(= new-cursor (.-from ^CursorRange %)))
                                       new-cursor-ranges))))
                 {:batch []
                  :results []}
                 cursor->cursor-ranges))))

(defn- set-view-node-diagnostics-tx [state resource view-node]
  {:pre [(= view-node (get-in state [:resource->view-node resource]))]}
  (g/set-property view-node :diagnostics (combined-resource-diagnostics state resource)))

(defn- combined-completion-trigger-characters [state resource-language]
  (into #{}
        (comp
          (filter
            (fn [[{:keys [languages]} {:keys [status]}]]
              (and (= :running status) (contains? languages resource-language))))
          (mapcat #(-> % val :capabilities :completion :trigger-characters)))
        (:server->server-state state)))

(defn- set-view-node-completion-trigger-characters-tx [state resource view-node]
  {:pre [(= view-node (get-in state [:resource->view-node resource]))]}
  (g/set-property
    view-node
    :completion-trigger-characters
    (combined-completion-trigger-characters state (resource/language resource))))

(s/def ::language string?)
(s/def ::languages (s/coll-of ::language :kind set? :min-count 1))
(s/def ::launcher #(satisfies? lsp.server/Launcher %))
(s/def ::pattern string?)
(s/def ::watched-file (s/keys :req-un [::pattern]))
(s/def ::watched-files (s/coll-of ::watched-file :distinct true :min-count 1))
(s/def ::server (s/keys :req-un [::languages ::launcher]
                        :opt-un [::watched-files]))
(s/def ::resource resource/resource?)

(defn- on-diagnostics-published [server resource diagnostics-result]
  {:pre [(s/assert ::server server)
         (s/assert ::resource resource)
         (s/assert ::lsp.server/diagnostics-result diagnostics-result)]}
  (fn [state]
    (let [state (assoc-in state [:server->server-state server :diagnostics resource] diagnostics-result)]
      (when-let [view-node (get-in state [:resource->view-node resource])]
        (ui/run-later (g/transact (set-view-node-diagnostics-tx state resource view-node))))
      state)))

(defn- apply-full-or-unchanged-resource-diagnostics [state server [resource result]]
  (case (:type result)
    :full ((on-diagnostics-published server resource (:result result)) state)
    :unchanged (update-in state
                          [:server->server-state server :diagnostics resource]
                          (fn [diagnostics-result]
                            (let [result-id (:result-id result)]
                              (if diagnostics-result
                                (assoc diagnostics-result :result-id result-id)
                                ;; we want to enforce the invariant that :items are always
                                ;; present, so if there were no diagnostics, and the server
                                ;; responded with "unchanged", we specify the diagnostic
                                ;; items as empty
                                {:items [] :result-id result-id}))))))

(defn- apply-full-or-unchanged-workspace-diagnostics [state server result]
  (reduce #(apply-full-or-unchanged-resource-diagnostics %1 server %2) state result))

(defn- capability-open-close? [capabilities]
  (-> capabilities :text-document-sync :open-close))

(defn refresh-completion-trigger-characters-for-languages-tx [state languages]
  (for [[resource view-node] (:resource->view-node state)
        :let [language (resource/language resource)]
        :when (contains? languages language)]
    (set-view-node-completion-trigger-characters-tx state resource view-node)))

(defn- on-server-initialized [server capabilities]
  {:pre [(s/assert ::server server)
         (s/assert ::lsp.server/capabilities capabilities)]}
  (fn [{:keys [server->server-state] :as state}]
    (let [{:keys [languages]} server
          {:keys [in]} (get server->server-state server)
          state (update-in state [:server->server-state server] assoc
                           :capabilities capabilities
                           :status :running)]
      (doseq [[resource {:keys [lines]}] (:resource->open-state state)
              :let [language (resource/language resource)]
              :when (and (contains? languages language)
                         (capability-open-close? capabilities))]
        (a/put! in (lsp.server/open-text-document resource lines)))
      (when (:completion capabilities)
        (ui/run-later
          (g/transact
            (refresh-completion-trigger-characters-for-languages-tx state languages))))
      state)))

(defprotocol ServerResponse
  (server-response-value [response])
  (server-response-update-state [response server state]))

(extend-protocol ServerResponse
  nil
  (server-response-value [response] response)
  (server-response-update-state [_ _ state] state)
  Object
  (server-response-value [response] response)
  (server-response-update-state [_ _ state] state))

(defn- with-update-state-on-response
  ([state-updater]
   (with-update-state-on-response identity state-updater))
  ([result-transformer state-updater]
   (fn [initial-result]
     (let [result (result-transformer initial-result)]
       (reify ServerResponse
         (server-response-value [_] result)
         (server-response-update-state [_ server state]
           (state-updater state server result)))))))

(defn- on-server-response [server responses-ch response]
  (fn [{:keys [requests] :as state}]
    ;; might be absent if the request is cancelled due to a timeout
    (if (contains? requests responses-ch)
      (let [remaining-responses (dec (requests responses-ch))]
        (cond
          (:error response) (when-not (Boolean/getBoolean "defold.tests")
                              (log/warn :message "Language server responded with error" :server server :error (:error response)))
          (some? (:result response)) (a/put! responses-ch (server-response-value (:result response))))
        (let [state (if (zero? remaining-responses)
                      (do (a/close! responses-ch)
                          (-> state
                              (update :requests dissoc responses-ch)
                              (update-in [:server->server-state server :requests] dissoc responses-ch)))
                      (-> state
                          (assoc-in [:requests responses-ch] remaining-responses)
                          (update-in [:server->server-state server :requests]
                                     (fn [state-requests]
                                       (let [remaining-server-responses (dec (state-requests responses-ch))]
                                         (if (zero? remaining-server-responses)
                                           (dissoc state-requests responses-ch)
                                           (assoc state-requests responses-ch remaining-server-responses)))))))]
          (if (:error response)
            state
            (server-response-update-state (:result response) server state))))
      state)))

(s/def ::chan lsp.async/chan?)
(s/def :editor.lsp.server-state/diagnostics (s/nilable (s/map-of ::resource ::lsp.server/diagnostics-result)))
(s/def :editor.lsp.server-state/capabilities ::lsp.server/capabilities)
(s/def :editor.lsp.server-state/requests (s/map-of lsp.async/chan? pos-int?))
(s/def :editor.lsp.server-state/status #{:initializing :running :down})
(s/def :editor.lsp.server-state/in ::chan)
(s/def :editor.lsp.server-state/out ::chan)
(s/def ::server-state
  (s/keys :req-un [:editor.lsp.server-state/in
                   :editor.lsp.server-state/out
                   :editor.lsp.server-state/status
                   :editor.lsp.server-state/requests]
          :opt-un [:editor.lsp.server-state/capabilities
                   :editor.lsp.server-state/diagnostics]))

(defn- dispose-server-state! [state server {:keys [in out diagnostics] :as server-state}]
  {:pre [(s/assert ::server-state server-state)]}
  (ui/run-later
    (g/transact
      (concat
        (for [resource (keys diagnostics)
              :let [view-node (get-in state [:resource->view-node resource])]
              :when view-node]
          (set-view-node-diagnostics-tx state resource view-node))
        (refresh-completion-trigger-characters-for-languages-tx state (:languages server)))))
  (a/close! in)
  (a/close! out))

(defn- cleanup-requests-of-removed-server [state server-state]
  (update state :requests (fn [state-requests]
                            (reduce-kv
                              (fn [acc responses-ch remaining-server-responses]
                                {:pre [(contains? acc responses-ch)]}
                                (let [remaining-responses (- (get acc responses-ch) remaining-server-responses)]
                                  (if (zero? remaining-responses)
                                    (do (a/close! responses-ch) (dissoc acc responses-ch))
                                    (assoc acc responses-ch remaining-responses))))
                              state-requests
                              (:requests server-state)))))

(defn- remove-server!
  "Remove server due to a request (removed from state)"
  [state server]
  (let [{:keys [out] :as server-state} (get-in state [:server->server-state server])
        state (-> state
                  (update :channels (partial filterv #(not= out %)))
                  (update :server->server-state dissoc server)
                  (update :server-out->server dissoc out)
                  (cleanup-requests-of-removed-server server-state))]
    (dispose-server-state! state server server-state)
    state))

(defn- fail-server!
  "Stop server due to a server error, but keep it in state as down

  This results in [[set-servers]] not trying to restart the server if it fails"
  [state ch]
  (let [server (get-in state [:server-out->server ch])
        {:keys [out] :as server-state} (get-in state [:server->server-state server])
        state (-> state
                  (update :channels (partial filterv #(not= out %)))
                  (assoc-in [:server->server-state server :status] :down)
                  (update :server-out->server dissoc out)
                  (cleanup-requests-of-removed-server server-state))]
    (dispose-server-state! state server server-state)
    state))

(defn- add-server! [project state {:keys [launcher] :as server}]
  (let [;; Use sliding input as a protection against slow servers that consume
        ;; messages too slowly: we degrade by skipping messages, prioritising
        ;; newer ones
        input (a/chan (a/sliding-buffer 4096))
        ;; Use sliding output buffer as a protection against chatty servers that
        ;; post too many notifications that we can't keep up with: we degrade by
        ;; skipping messages, prioritising newer ones
        output (a/chan (a/sliding-buffer 4096))]
    (lsp.server/make project launcher input output
                     :on-initialized (partial on-server-initialized server)
                     :on-publish-diagnostics (partial on-diagnostics-published server)
                     :on-response (partial on-server-response server))
    (-> state
        (update :channels conj output)
        (update :server->server-state assoc server {:in input :out output :status :initializing :requests {}})
        (update :server-out->server assoc output server))))

(defn- resource-open? [state resource]
  (contains? (:resource->open-state state) resource))

(defn- resource-viewed? [state resource]
  (contains? (:resource->view-node state) resource))

(defn- resource-polled? [state resource]
  (contains? (:polled-resources state) resource))

(defn- interesting-resource? [state resource]
  (or (resource-open? state resource)
      (resource-polled? state resource)))

(defn- text-sync-kind [capabilities]
  {:post [(s/assert ::lsp.server/change %)]}
  (-> capabilities :text-document-sync :change))

(defn- capability-sync-text? [capabilities]
  (and (capability-open-close? capabilities)
       (not= :none (text-sync-kind capabilities))))

(defn- notify-interested-servers!
  [state resource & {:keys [message-fn message capabilities-pred]
                     :or {capabilities-pred any?}}]
  {:pre [(not= (some? message-fn) (some? message))]}
  (let [message-fn (or message-fn (constantly message))
        language (resource/language resource)]
    (doseq [[{:keys [languages]} {:keys [in status capabilities]}] (:server->server-state state)
            :when (and (= status :running)
                       (contains? languages language)
                       (capabilities-pred capabilities))]
      (a/put! in (message-fn capabilities)))))

(defn- open-resource! [state resource lines]
  {:pre [(not (resource-open? state resource))]}
  (notify-interested-servers! state resource
                              :capabilities-pred capability-open-close?
                              :message (lsp.server/open-text-document resource lines))
  (assoc-in state [:resource->open-state resource] {:lines lines :version 0}))

(defn- did-change! [state resource new-lines]
  {:pre [(resource-open? state resource)]}
  (let [{:keys [lines version]} (get-in state [:resource->open-state resource])]
    (if (= lines new-lines)
      state
      (let [new-version (inc version)
            full-text-change-delay (delay (lsp.server/full-text-document-change resource new-lines new-version))
            incremental-change-delay (delay
                                       (if-let [incremental-diff (data/get-incremental-diff lines new-lines)]
                                         (lsp.server/incremental-document-change resource incremental-diff new-version)
                                         @full-text-change-delay))]
        (notify-interested-servers!
          state resource
          :capabilities-pred capability-sync-text?
          :message-fn (fn [capabilities]
                        (case (text-sync-kind capabilities)
                          :incremental @incremental-change-delay
                          :full @full-text-change-delay)))
        (assoc-in state [:resource->open-state resource] {:lines new-lines :version new-version})))))

(defn- close-resource! [state resource]
  {:pre [(resource-open? state resource)]}
  (notify-interested-servers! state resource
                              :capabilities-pred capability-open-close?
                              :message (lsp.server/close-text-document resource))
  (update state :resource->open-state dissoc resource))

(defn- ensure-resource-open! [state resource lines]
  (if (resource-open? state resource)
    (did-change! state resource lines)
    (open-resource! state resource lines)))

(defn- sync-modified-lines-of-existing-node! [state resource old-source-value new-lines]
  (let [resource-type (resource/resource-type resource)
        dirty (resource-node/dirty-save-value? new-lines old-source-value resource-type)]
    (if-not dirty
      (cond
        ;; viewed implies open
        (resource-viewed? state resource) (did-change! state resource new-lines)
        (resource-open? state resource) (close-resource! state resource)
        :else state)
      ;; dirty:
      ;; - ensure polled
      ;; - ensure open (notify servers of open or text change if already open)
      (-> state
          (update :polled-resources conj resource)
          (ensure-resource-open! resource new-lines)))))

(defn- remove-resource-diagnostics [state resource]
  (update state :server->server-state (fn [server->server-state]
                                        (reduce #(update-in %1 [%2 :diagnostics] dissoc resource)
                                                server->server-state
                                                (keys server->server-state)))))

(defn- sync-resource-state!
  ([state resource]
   (lsp.async/with-auto-evaluation-context evaluation-context
     (sync-resource-state! state resource evaluation-context)))
  ([{:keys [project get-resource-node] :as state} resource evaluation-context]
   (let [resource-node (get-resource-node project resource evaluation-context)]
     (if (or (nil? resource-node)
             (resource-io/file-not-found-error?
               (-> resource-node (g/node-value :_output-jammers evaluation-context) vals first)))
       ;; deleted:
       ;; - ensure not polled
       ;; - close if was open but not viewed
       (let [state (-> state
                       (update :polled-resources disj resource)
                       (remove-resource-diagnostics resource))]
         (cond-> state
                 ;; Deleted but still viewed? Keep it until the view closes,
                 ;; since the view controls open-view/close-view, and we want
                 ;; to preserve the "viewed implies open" invariant
                 (and (resource-open? state resource) (not (resource-viewed? state resource)))
                 (close-resource! resource)))
       ;; exists, check if clean or dirty
       (let [source-value (g/node-value resource-node :source-value evaluation-context)
             lines (g/node-value resource-node :lines evaluation-context)]
         (sync-modified-lines-of-existing-node! state resource source-value lines))))))

(s/def ::new-servers (s/coll-of ::server :kind set?))

(defn set-servers [new-servers]
  {:pre [(s/assert ::new-servers new-servers)]}
  (fn [{:keys [server->server-state project] :as state}]
    (let [old-servers (set (keys server->server-state))
          to-remove (set/difference old-servers new-servers)
          to-add (set/difference new-servers old-servers)]
      (as-> state $
            (reduce (partial add-server! project) $ to-add)
            (reduce remove-server! $ to-remove)))))

(def ^:private dev (system/defold-dev?))
(defonce ^:private running-lsps (when dev (atom {} :meta {:type ::running-lsps})))

(defn make
  "Create LSP manager that can be used for interacting with multiple LSP servers

  Args:
    project              the project node id
    get-resource-node    fn of 3 args - project, resource and evaluation
                         context - that should return current resource node id
                         for the specified resource

  Returns a manager object that can be used to submit LSP manager commands (see
  other public fns in this ns)"
  [project get-resource-node]
  {:pre [(g/node-id? project) (ifn? get-resource-node)]}
  (let [in (a/chan 128)
        state-vol (volatile! nil)]
    (a/go-loop [state {:in in
                       ;; in + server output channels, for performance
                       :channels [in]
                       :project project
                       :get-resource-node get-resource-node
                       ;; server->server-state is a map from server to server state, where
                       ;; server is:
                       ;; {:languages #{"lang"}
                       ;;  :launcher {:command ["shell-command"]}
                       ;;  ; optional:
                       ;;  :watched-files [{:pattern "**/*.json"}]}
                       ;; and server state is:
                       ;; {:in ch
                       ;;  :out ch
                       ;;  :status :initializing|:running|:down
                       ;;  ; appears when diagnostics are pulled or published
                       ;;  :diagnostics {resource {:items [diagnostic]
                       ;;                          ; might appear on publish, might appear on pull, a version of an open document
                       ;;                          :version n
                       ;;                          ; appears only on diagnostics pull
                       ;;                          :result-id "result-id-string"}}
                       ;;  ; map of responses-chans awaiting responses from this server to number of remaining responses,
                       ;;  ; appears when requests arrive
                       ;;  :requests {a/chan 12}
                       ;;  ; appears on initialization
                       ;;  :capabilities {:text-document-sync {:open-close boolean
                       ;;                                      :change :none|:full|:incremental}}}
                       :server->server-state {}
                       ;; map form server's :out ch to server, for performance
                       :server-out->server {}
                       ;; map from responses-ch to a number of remaining responses
                       :requests {}
                       ;; set of resources we monitor for changes
                       :polled-resources #{}
                       ;; map of resource to open view id
                       :resource->view-node {}
                       ;; inverse :resource->view-node, a map from view node id
                       ;; to resource, used for performance
                       :view-node->resource {}
                       ;; map {resource {:lines ["code..."] :version int}},
                       ;; indicates that the resource is open
                       ;; invariant: every viewed resource must be open
                       :resource->open-state {}}]
      (vreset! state-vol state)
      (when dev (swap! running-lsps assoc in state))
      (let [[value ch] (a/alts! (:channels state))]
        ;; close the manager input => stop the manager
        (if (and (nil? value) (= ch in))
          (try
            (when dev (swap! running-lsps dissoc in))
            ((set-servers #{}) state)
            (catch Throwable e (error-reporting/report-exception! e)))
          (recur
            (try
              (cond
                ;; on message from input channel, update the state
                (= ch in)
                (value state)

                ;; on server channel close, remove it from inputs, but keep the state as failed
                (nil? value)
                (fail-server! state ch)

                ;; on server message, update the state
                :else
                (value state))
              (catch Throwable e
                (error-reporting/report-exception! e)
                state))))))
    (fn the-lsp
      ([] @state-vol)
      ([x] (a/put! in x)))))

(defn set-servers!
  "Notify the LSP manager that we want these servers to be running"
  [lsp new-servers]
  (lsp (set-servers new-servers)))

(defn- do-open-view [state view-node resource lines]
  (if (resource-viewed? state resource)
    state
    (let [state (-> state
                    (update :resource->view-node assoc resource view-node)
                    (update :view-node->resource assoc view-node resource)
                    (ensure-resource-open! resource lines))]
      (ui/run-later
        (g/transact
          (concat
            (set-view-node-diagnostics-tx state resource view-node)
            (set-view-node-completion-trigger-characters-tx state resource view-node))))
      state)))

(defn open-view!
  "Notify the LSP manager that a view for a resource is open"
  [lsp view-node resource lines]
  (lsp #(do-open-view % view-node resource lines)))

(defn- do-close-view [state view-node]
  (let [resource (get-in state [:view-node->resource view-node])]
    (if (resource-viewed? state resource)
      (-> state
          (update :resource->view-node dissoc resource)
          (update :view-node->resource dissoc view-node)
          ;; will close if clean or removed, keep open if dirty
          (sync-resource-state! resource))
      state)))

(defn close-view!
  "Notify the LSP manager that a view is closed"
  [lsp view-node]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn [state] (do-close-view state view-node))))

(defn notify-lines-modified!
  "Notify the LSP manager about new lines of a resource node"
  [lsp resource old-source-value new-lines]
  (lsp (fn notify-lines-modified [state]
         (cond-> state
                 (resource/file-resource? resource)
                 (sync-modified-lines-of-existing-node! resource old-source-value new-lines)))))

(defn check-if-polled-resources-are-modified!
  "Notify the LSP manager that some previously modified resources might change

  This can happen e.g. on undo/redo"
  [lsp]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn check-if-polled-resources-are-modified [{:keys [polled-resources] :as state}]
         (lsp.async/with-auto-evaluation-context evaluation-context
           (reduce #(sync-resource-state! %1 %2 evaluation-context) state polled-resources)))))

(let [method (-> Globs
                 (.getDeclaredMethod "toUnixRegexPattern" (into-array Class [String]))
                 (doto (.setAccessible true)))]
  (defn- make-glob-pattern
    ^Pattern [s]
    (re-pattern (.invoke method nil (into-array Object [s])))))

(defn- notify-files-changed! [state change-type->resources]
  {:pre [(every? #{:created :changed :deleted} (keys change-type->resources))]}
  (let [watcher->server-ins
        (util/group-into
          {} []
          key val
          (eduction
            (mapcat
              (fn [[server {:keys [in]}]]
                (eduction
                  (map #(pair % in))
                  (:watched-files server))))
            (:server->server-state state)))

        server-in->changes
        (util/group-into
          {} []
          key val
          (for [[{:keys [pattern]} server-ins] watcher->server-ins
                :let [re (make-glob-pattern pattern)]
                [change-type resources] change-type->resources
                resource resources
                :when (.matches (.matcher re (resource/proj-path resource)))
                in server-ins]
            (pair in (pair resource change-type))))]
    (doseq [[server-in changes] server-in->changes]
      (a/put! server-in (lsp.server/watched-file-change changes)))))

(defn touch-resources! [lsp resources]
  (lsp #(doto % (notify-files-changed! {:changed resources}))))

(defn apply-resource-changes!
  "Notify the LSP manager about resource sync"
  [lsp {:keys [added removed changed moved]}]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn apply-resource-changes [{:keys [project get-resource-node] :as state}]
         (notify-files-changed! state {:deleted removed :changed changed :created added})
         (let [interesting-added (into []
                                       (keep (fn [[from to]]
                                               (when (interesting-resource? state from)
                                                 to)))
                                       moved)
               interesting-removed (filterv #(interesting-resource? state %) removed)
               interesting-changed (filterv #(interesting-resource? state %) changed)
               viewed-moved (filterv #(resource-viewed? state (first %)) moved)]
           (lsp.async/with-auto-evaluation-context evaluation-context
             (as-> state $
                   (reduce #(sync-resource-state! %1 %2 evaluation-context) $ interesting-added)
                   (reduce #(sync-resource-state! %1 %2 evaluation-context) $ interesting-removed)
                   (reduce #(sync-resource-state! %1 %2 evaluation-context) $ interesting-changed)
                   (reduce (fn [acc [from to]]
                             (let [view-node (get-in acc [:resource->view-node from])
                                   resource-node (get-resource-node project to evaluation-context)]
                               (-> acc
                                   (do-close-view view-node)
                                   (do-open-view view-node to (g/node-value resource-node :lines evaluation-context)))))
                           $ viewed-moved)))))))

(defn get-graph-lsp
  "Given a project's graph id, return the LSP manager"
  ([graph-id]
   (get-graph-lsp (g/now) graph-id))
  ([basis graph-id]
   (g/graph-value basis graph-id :lsp)))

(defn get-node-lsp
  "Given a node id in a project graph, return the LSP manager"
  ([node]
   (get-node-lsp (g/now) node))
  ([basis node]
   (get-graph-lsp basis (g/node-id->graph-id node))))

(defn- cancel-requests [responses-ch servers]
  (fn [{:keys [server->server-state requests] :as state}]
    ;; The request might be absent if all servers responded before the timeout.
    (if (contains? requests responses-ch)
      (do
        (a/close! responses-ch)
        (reduce
          (fn [state server]
            ;; The server might be absent if it died before the timeout
            (if (contains? server->server-state server)
              ;; Also, the server might be restarted by now, which means it's
              ;; not guaranteed that it will have responses-ch in its requests
              ;; map, but it's not a problem for dissoc
              (update-in state [:server->server-state server :requests] dissoc responses-ch)
              state))
          (assoc state :requests (dissoc requests responses-ch))
          servers))
      state)))

(defn- send-requests! [state responses-ch & {:keys [requests requests-fn capabilities-pred language timeout-ms]
                                             :or {capabilities-pred any?}}]
  {:pre [(not= (some? requests) (some? requests-fn))
         (pos-int? timeout-ms)
         (not (contains? (:requests state) responses-ch))]}
  (let [requests-fn (or requests-fn (constantly requests))
        server->requests (->> state
                              :server->server-state
                              (eduction
                                (mapcat
                                  (fn [[{:keys [languages] :as server} {:keys [status capabilities] :as server-state}]]
                                    (when (and (= :running status)
                                               (capabilities-pred capabilities)
                                               (or (nil? language) (contains? languages language)))
                                      (eduction
                                        (map #(pair server %))
                                        (requests-fn server server-state))))))
                              (util/group-into {} [] key val))]
    (if (zero? (count server->requests))
      (do (a/close! responses-ch) state)
      (do
        (doseq [[server requests] server->requests
                request requests]
          (a/put! (get-in state [:server->server-state server :in]) (lsp.server/finalize-request request responses-ch)))
        (a/go
          (<! (a/timeout timeout-ms))
          (>! (:in state) (cancel-requests responses-ch (keys server->requests))))
        (reduce-kv (fn [state server requests]
                     (assoc-in state [:server->server-state server :requests responses-ch] (count requests)))
                   (assoc-in state [:requests responses-ch] (transduce (map count) + 0 (vals server->requests)))
                   server->requests)))))

(defn- notify-workspace-diagnostics-callback! [resources callback]
  (fn [state]
    (callback
      (let [results (into {}
                          (keep (fn [resource]
                                  (let [diagnostics (sorted-resource-diagnostics state resource)]
                                    (when (pos? (count diagnostics))
                                      (pair resource diagnostics)))))
                          resources)]
        (when (pos? (count results))
          results)))
    state))

(defn pull-workspace-diagnostics! [lsp results-callback & {:keys [timeout-ms]
                                                           :or {timeout-ms 60000}}]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn [{:keys [project] :as state}]
         (let [responses-ch (a/chan)
               language->resources (delay
                                     (lsp.async/with-auto-evaluation-context evaluation-context
                                       (let [workspace (g/node-value project :workspace evaluation-context)]
                                         (->> (g/node-value workspace :resource-list evaluation-context)
                                              (eduction
                                                (filter #(and (resource/file-resource? %)
                                                              (not= "plaintext" (resource/language %)))))
                                              (group-by resource/language)))))]
           (a/go
             (let [diagnosed-resources (keys (<! (a/into {} responses-ch)))]
               (>! (:in state) (notify-workspace-diagnostics-callback! diagnosed-resources results-callback))))
           (send-requests!
             state
             responses-ch
             :requests-fn (fn [server server-state]
                            (case (-> server-state :capabilities :pull-diagnostics)
                              :workspace [(lsp.server/pull-workspace-diagnostics
                                            (into {}
                                                  (keep (fn [[resource {:keys [result-id]}]]
                                                          (when result-id
                                                            [resource result-id])))
                                                  (:diagnostics server-state))
                                            (with-update-state-on-response
                                              apply-full-or-unchanged-workspace-diagnostics))]
                              :text-document (eduction
                                               (mapcat @language->resources)
                                               (map (fn [resource]
                                                      (lsp.server/pull-document-diagnostics
                                                        resource
                                                        (get-in server-state [:diagnostics resource :result-id])
                                                        (with-update-state-on-response
                                                          #(pair resource %)
                                                          apply-full-or-unchanged-resource-diagnostics))))
                                               (:languages server))))
             :timeout-ms timeout-ms
             :capabilities-pred #(not= :none (:pull-diagnostics %)))))))

(defn goto-definition! [lsp resource cursor result-callback & {:keys [timeout-ms]
                                                               :or {timeout-ms 3000}}]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn [state]
         (let [ch (a/chan 1 cat)]
           (a/go (result-callback (<! (a/into [] ch))))
           (send-requests! state ch
                           :requests [(lsp.server/goto-definition resource cursor)]
                           :capabilities-pred :goto-definition
                           :language (resource/language resource)
                           :timeout-ms timeout-ms)))))

(defn find-references! [lsp resource cursor result-callback & {:keys [timeout-ms]
                                                               :or {timeout-ms 3000}}]
  ;; bound-fn only needed for tests to pick up the test system
  (lsp (bound-fn [state]
         (let [ch (a/chan 1 cat)]
           (a/go (result-callback (<! (a/into [] ch))))
           (send-requests! state ch
                           :requests [(lsp.server/find-references resource cursor)]
                           :capabilities-pred :find-references
                           :language (resource/language resource)
                           :timeout-ms timeout-ms)))))

(defn has-language-servers-running-for-language? [lsp language]
  (->> (lsp)
       :server->server-state
       (some (fn [[server server-state]]
               (and (= :running (:status server-state))
                    (contains? (:languages server) language))))
       boolean))

(defn request-completions!
  "Request completions for a specific cursor position in the text resource

  Args:
    lsp                the LSP object
    resource           text resource
    cursor             the document position cursor for completions
    context            extra information for completion request, a map with
                       following keys:
                         :trigger-kind         required, either :invoked,
                                               :trigger-character or
                                               :trigger-for-incomplete-completions
                         :trigger-character    needed only when :trigger-kind is
                                               :trigger-character, a single
                                               character string
    result-callback    callback that will be called on the background thread,
                       will receive a completion result map with the following
                       keys:
                         :complete    boolean, indicating whether further typing
                                      should trigger another completion request
                         :items       vector of completions

  Kv-args:
    :timeout-ms    timeout before giving up on requests and returning the
                   results so far"
  [lsp resource cursor context result-callback & {:keys [timeout-ms]
                                                  :or {timeout-ms 1000}}]
  (if (and (resource/file-resource? resource)
           (resource/editable? resource))
    (lsp (bound-fn [state]
           (let [ch (a/chan 1)]
             (a/go (result-callback (<! (a/reduce
                                          (fn [acc {:keys [complete items]}]
                                            (-> acc
                                                (update :complete fn/and complete)
                                                (update :items into items)))
                                          {:complete true
                                           :items []}
                                          ch))))
             (send-requests!
               state ch
               :capabilities-pred :completion
               :language (resource/language resource)
               :timeout-ms timeout-ms
               :requests-fn (fn [_ {:keys [out]}]
                              [(lsp.server/completion
                                 resource
                                 cursor
                                 context
                                 #(vary-meta % assoc :editor.lsp.server-state/out out))])))))
    ;; Don't ask the servers about dependency (zip) resources
    (do (result-callback {:complete true :items []}) nil)))

(defn capability-resolve-completions? [capability]
  (-> capability :completion :resolve boolean))

(defn resolve-completion! [lsp completion result-callback & {:keys [timeout-ms]
                                                             :or {timeout-ms 5000}}]
  (if-let [completion-out (:editor.lsp.server-state/out (meta completion))]
    (lsp (bound-fn [state]
           (let [ch (a/chan 1)]
             (a/go (result-callback (or (<! ch) completion)))
             (send-requests!
               state ch
               :capabilities-pred capability-resolve-completions?
               :requests-fn (fn [_ {:keys [out]}]
                              (when (= out completion-out)
                                [(lsp.server/resolve-completion
                                   completion
                                   #(vary-meta % dissoc :editor.lsp.server-state/out))]))
               :timeout-ms timeout-ms))))
    (result-callback completion)))

(defn hover! [lsp resource cursor result-callback & {:keys [timeout-ms]
                                                     :or {timeout-ms 1000}}]
  (if (resource/file-resource? resource)
    (lsp (bound-fn [state]
           (let [ch (a/chan 1 cat)]
             (a/go (result-callback (<! (a/into [] ch))))
             (send-requests!
               state ch
               :capabilities-pred :hover
               :language (resource/language resource)
               :timeout-ms timeout-ms
               :requests [(lsp.server/hover resource cursor)]))))
    (do (result-callback []) nil)))

(defn prepare-rename [lsp resource cursor result-callback & {:keys [timeout-ms]
                                                             :or {timeout-ms 1000}}]
  (if (and (resource/file-resource? resource)
           (resource/editable? resource))
    (lsp (bound-fn [state]
           (let [ch (a/chan 1 (take 1))]
             (a/go (result-callback (<! ch)))
             (send-requests!
               state ch
               :capabilities-pred :rename
               :language (resource/language resource)
               :timeout-ms timeout-ms
               :requests-fn (fn [_ {:keys [out]}]
                              [(lsp.server/prepare-rename
                                 resource
                                 cursor
                                 #(vary-meta % assoc
                                             :editor.lsp.server-state/out out
                                             :resource resource
                                             :cursor cursor))])))))
    (do (result-callback nil) nil)))

(defn rename [lsp prepared-range new-name result-callback & {:keys [timeout-ms]
                                                             :or {timeout-ms 5000}}]
  (if-let [{:keys [resource cursor] rename-out :editor.lsp.server-state/out} (meta prepared-range)]
    (lsp (bound-fn [state]
           (let [ch (a/chan 1)]
             (a/go (result-callback (<! ch)))
             (send-requests!
               state ch
               :requests-fn (fn [_ {:keys [out]}]
                              (when (= out rename-out)
                                [(lsp.server/rename resource cursor new-name)]))
               :timeout-ms timeout-ms))))
    (throw (IllegalArgumentException. "Expected a prepared range" {:range prepared-range}))))

(comment
  (val (first @running-lsps))
  ;; Restart all servers:
  ((g/graph-value 1 :lsp) (fn [state]
                            (let [servers (set (keys (:server->server-state state)))]
                              (-> state
                                  ((set-servers #{}))
                                  ((set-servers servers))))))
  ;; Stop all LSP servers
  (set-servers! (g/graph-value 1 :lsp) #{})
  ;; Pull diagnostics
  (pull-workspace-diagnostics! (g/graph-value 1 :lsp) tap>)
  ;; Start json LSP server (install: npm install -g vscode-json-languageserver)
  (set-servers!
    (g/graph-value 1 :lsp)
    #{{:languages #{"json" "jsonc"}
       :launcher {:command ["/opt/homebrew/bin/vscode-json-languageserver" "--stdio"]}}
      {:languages #{"lua"}
       :watched-files [{:pattern "**/.luacheckrc"}]
       :launcher {:command ["build/plugins/lsp-lua-language-server/plugins/bin/arm64-macos/bin/lua-language-server" "--configpath=build/plugins/lsp-lua-language-server/plugins/share/config.json"]}}
      #_{:languages #{"lua"}
         :instance-id 2
         :watched-files [{:pattern "**/.luacheckrc"}]
         :launcher {:command ["build/plugins/lsp-lua-language-server/plugins/bin/arm64-macos/bin/lua-language-server" "--configpath=build/plugins/lsp-lua-language-server/plugins/share/config.json"]}}
      #_{:languages #{"lua"}
         :watched-files [{:pattern "**/.luacheckrc"}]
         :launcher {:command ["/Users/vlaaad/Projects/vscode-luacheck/lua_language_server" "--"]}}}))