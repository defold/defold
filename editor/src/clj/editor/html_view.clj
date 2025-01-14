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

(ns editor.html-view
  (:require [clojure.core.protocols :as p]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.http-server :as http-server]
            [util.http-util :as http-util])
  (:import [java.net URI URLDecoder]
           [javafx.scene Parent]
           [javafx.scene.control Tab]
           [javafx.scene.web WebEngine WebView WebEvent]
           [javafx.concurrent Worker$State]
           [org.w3c.dom Document Element NodeList]
           [org.w3c.dom.events Event EventListener EventTarget]))

(set! *warn-on-reflection* true)

;; TODO: stop on closing, restart with new project id on new project.
(defonce http-server (atom nil))

(def ^:private known-content-types
  {"jpg"  "image/jpeg"
   "jpeg" "image/jpeg"
   "png"  "image/png"
   "js"   "application/javascript"
   "css"  "text/css"})

(defn- resource-handler [project request]
  (if-let [node (project/get-resource-node project (:url request))]
    (let [resource      (g/node-value node :resource)
          resource-type (resource/resource-type resource)
          resource-ext  (resource/type-ext resource)
          view-types    (map :id (:view-types resource-type))]
      (cond
        (some #{:html} view-types)
        (let [body (g/node-value node :html)]
          (if-let [error (g/error? body)]
            {:code    500
             :headers {"Content-Type" "text/html; charset=utf-8"}
             :body    (format "Couldn't render %s: %s\n"
                              (resource/resource->proj-path resource)
                              (or (:message error) "Unknown reason"))}
            {:code    200
             :headers {"Content-Type" "text/html; charset=utf-8"}
             :body    body}))

        (contains? known-content-types resource-ext)
        {:code    200
         :headers {"Content-Type" (known-content-types resource-ext)}
         :body    resource}

        :else
        (do (log/warn :message (format "Unknown content-type %s for %s" resource-ext resource))
            http-util/not-found-response)))
    (do (log/warn :message (format "Cannot find resource for %s" (:url request)))
        http-util/not-found-response)))

(defn- get-http-server!
  [project]
  (when-not @http-server
    (let [http-handlers {"/", (partial resource-handler project)}
          server (http-server/->server "127.0.0.1" 0 http-handlers)]
      (http-server/start! server)
      (reset! http-server server)))
  @http-server)

;; make NodeList reducible

(extend-protocol p/CollReduce
  NodeList
  (coll-reduce
    ([coll f]
     (let [n (.getLength coll)]
       (if (zero? n)
         (f)
         (loop [ret nil
                i 0]
           (if (< i n)
             (let [ret (f ret (.item coll i))]
               (if (reduced? ret)
                 @ret
                 (recur ret (inc i))))
             ret)))))
    ([coll f val]
     (let [n (.getLength coll)]
       (loop [ret val
              i 0]
         (if (< i n)
           (let [ret (f ret (.item coll i))]
             (if (reduced? ret)
               @ret
               (recur ret (inc i))))
           ret))))))

;;--------------------------------------------------------------------
;; view

(defn- query-params->map
  [params]
  (when (not (string/blank? params))
    (into {}
          (map (fn [param]
                 (let [[k ^String v] (string/split param #"=")]
                   [(keyword k) (URLDecoder/decode v "UTF-8")])))
          (string/split params #"&"))))

(defmulti url->command (fn [^URI uri dispatch-args] (.getHost uri)))

(defmethod url->command "open"
  [^URI uri {:keys [project]}]
  (let [params (query-params->map (.getQuery uri))
        proj-path (:path params)
        resource-id (project/get-resource-node project proj-path)
        resource (g/node-value resource-id :resource)]
    {:command   :open
     :user-data {:resources [resource]}}))

(defmethod url->command "add-dependency"
  [^URI uri {:keys [project]}]
  (let [params (query-params->map (.getQuery uri))
        dep-url (:url params)]
    {:command   :add-dependency
     :user-data {:dep-url dep-url}}))

(defmethod url->command :default
  [^URI uri _]
  {:command (keyword (.getHost uri))})

(defn dispatch-url!
  [project ^URI uri]
  (when-some [{:keys [command user-data]} (url->command uri {:project project})]
    (ui/execute-command (ui/contexts (ui/main-scene)) command user-data)))

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch java.net.URISyntaxException _
        nil))))

(defn- anchor-href [^Element anchor-element]
  (.getAttribute anchor-element "href"))

(defn- anchor-url
  ^URI [^Element anchor-element]
  (some-> anchor-element anchor-href string->url))


(defn- handle-defold-click!
  [project ^Event ev]
  (when-some [url (anchor-url (.getCurrentTarget ev))]
    (dispatch-url! project url)))

(defn- hijack-defold-links!
  [^Document doc project]
  (let [handler (reify EventListener
                  (handleEvent [_this ev]
                    (.preventDefault ev)
                    (handle-defold-click! project ev)))]
    (run! (fn [^Element anchor-element]
            (when-some [url (anchor-url anchor-element)]
              (when (= "defold" (.getScheme url))
                (.addEventListener ^EventTarget anchor-element "click" handler true))))
          (.getElementsByTagName doc "a"))))

(defn- open-external-ahrefs-in-browser!
  [^Document doc]
  (let [handler (reify EventListener
                  (handleEvent [_this ev]
                    (.preventDefault ev)
                    (when-some [url (anchor-url (.getCurrentTarget ev))]
                      (ui/open-url url))))]
    (run! (fn [^Element anchor-element]
            (when-some [url (anchor-url anchor-element)]
              (when (and (not= "defold" (.getScheme url))
                         (some? (.getHost url)))
                (.addEventListener ^EventTarget anchor-element "click" handler true))))
          (.getElementsByTagName doc "a"))))

(defn- on-load-succeeded
  [^WebEngine web-engine project]
  (when-some [doc (.getDocument web-engine)]
    (doto doc
      (hijack-defold-links! project)
      (open-external-ahrefs-in-browser!))))

(defn- on-load-failed
  [^WebEngine web-engine]
  (let [load-worker (.getLoadWorker web-engine)]
    (when-some [ex (.getException load-worker)]
      (dialogs/make-info-dialog
        {:title "Could not load page"
         :icon :icon/triangle-error
         :header (.getMessage load-worker)
         :content (.getMessage ex)}))))

(g/defnode WebViewNode
  (inherits view/WorkbenchView)
  (property web-view WebView))

(defn- make-web-view
   ^WebView [project]
  (let [web-view (WebView.)
        web-engine (.getEngine web-view)
        load-worker (.getLoadWorker web-engine)]
    (ui/observe (.stateProperty load-worker)
                (fn [_ old new]
                  (condp = new
                    Worker$State/SUCCEEDED (on-load-succeeded web-engine project)
                    Worker$State/FAILED (on-load-failed web-engine)
                    nil)))
    web-view))

(defn- resource-url [http-server resource]
  (when resource
    (str (http-server/url http-server)
         (resource/proj-path resource))))

(defn- load-resource! [^WebEngine web-engine project resource]
  (let [url         (resource-url (get-http-server! project) resource)
        current-url (.getLocation web-engine)]
    (when (not= url current-url)
      (.load web-engine url))))

(defn- update-web-view!
  "Loads the url corresponding to the current resource.

  This will open the resource at the new location if we've moved it.

  If we somehow manage to browse away from the project to an external
  page, this will somewhat annoyingly bring us back."
  [view-id project]
  (let [web-view ^WebView (g/node-value view-id :web-view)
        web-engine (.getEngine web-view)]
    (when-let [resource-node (g/node-value view-id :resource-node)]
      (load-resource! web-engine project (g/node-value resource-node :resource)))))

(defn- handle-location-change! [project view-id new-location]
  (if-let [new-resource-node
           (if (string/starts-with? new-location (http-server/url (get-http-server! project)))
             (let [resource-path (subs new-location (count (http-server/url (get-http-server! project))))]
               (project/get-resource-node project resource-path))
             nil)]
    (g/transact (view/connect-resource-node view-id new-resource-node))
    (log/warn :message (format "Moving to non-local url or missing resource: %s" new-location))))

(defn make-view
  [graph ^Parent parent html-node opts]
  (let [project       (or (:project opts) (project/get-project html-node))
        web-view      (make-web-view project)
        web-engine    (.getEngine web-view)
        view-id       (g/make-node! graph WebViewNode :web-view web-view)
        repainter     (ui/->timer 1 "update-web-view!" (fn [_ _ _] (update-web-view! view-id project)))]

    (.addListener (.locationProperty web-engine)
                  (ui/change-listener _ _ new-location (handle-location-change! project view-id new-location)))

    (ui/children! parent [web-view])
    (ui/fill-control web-view)

    (ui/context! web-view :browser {:web-engine web-engine} nil)

    (doto web-engine
      (.setOnAlert (ui/event-handler ev
                     (dialogs/make-info-dialog
                       {:title "Alert"
                        :icon :icon/circle-info
                        :header (.getData ^WebEvent ev)})))
      (.setUserStyleSheetLocation (str (io/resource "markdown.css")))
      (load-resource! project (g/node-value html-node :resource)))

    (ui/timer-start! repainter)
    (when-some [^Tab tab (:tab opts)]
      (ui/timer-stop-on-closed! tab repainter))
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :html
                                :label "Web"
                                :make-view-fn make-view))

(handler/defhandler :reload :browser
  (run [^WebEngine web-engine]
    (.reload web-engine)))

(handler/register-menu! ::menubar :editor.app-view/edit-end
  [{:command :reload :label "Reload"}])
