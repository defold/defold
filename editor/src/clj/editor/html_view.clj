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
            [util.http-server :as http-server])
  (:import [java.net URI URLDecoder]
           [javafx.beans.value ChangeListener]
           [javafx.concurrent Worker$State]
           [javafx.scene Parent]
           [javafx.scene.control Tab]
           [javafx.scene.web WebEngine WebEvent WebView]
           [org.w3c.dom Document Element NodeList]
           [org.w3c.dom.events Event EventListener EventTarget]))

(set! *warn-on-reflection* true)

(defn- make-resource-server [project]
  (http-server/start!
    (http-server/router-handler
      {"{*path}"
       {"GET" (fn [{:keys [path]}]
                (if-let [node (project/get-resource-node project path)]
                  (let [resource (g/node-value node :resource)]
                    (if (resource/has-view-type? resource :html)
                      (let [body (g/node-value node :html)]
                        (if-let [error (g/error? body)]
                          (http-server/response 500 (format "Couldn't render %s: %s\n"
                                                            (resource/resource->proj-path resource)
                                                            (or (:message error) "Unknown reason")))
                          (http-server/response 200 {"content-type" "text/html; charset=utf-8"} body)))
                      (http-server/response 200 resource)))
                  (do (log/warn :message (format "Cannot find resource for %s" path))
                      http-server/not-found)))}})
    :host "127.0.0.1"))

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

(g/defnk produce-update-web-view! [^WebView web-view resource-node server-url _node-id html]
  (when resource-node
    (let [web-engine (.getEngine web-view)
          resource (g/node-value resource-node :resource)]
      (.load web-engine (str server-url (resource/proj-path resource))))))

(defn- handle-location-change! [url project view-id new-location]
  (if-let [new-resource-node
           (when (string/starts-with? new-location url)
             (let [resource-path (URLDecoder/decode (subs new-location (count url)))
                   resource-node (project/get-resource-node project resource-path)]
               (when-let [resource (g/node-value resource-node :resource)]
                 (when (resource/has-view-type? resource :html)
                   resource-node))))]
    (g/transact [(g/connect new-resource-node :html view-id :html)
                 (view/connect-resource-node view-id new-resource-node)])
    (log/warn :message (format "Moving to non-local url or missing resource: %s" new-location))))

(g/defnode WebViewNode
  (inherits view/WorkbenchView)
  (property web-view WebView)
  (property server-url g/Str)
  (input html g/Str)
  (output update-web-view g/Any :cached produce-update-web-view!)) 

(defn make-view
  [graph ^Parent parent html-node opts]
  (let [project (or (:project opts) (project/get-project html-node))
        web-view (make-web-view project)
        web-engine (.getEngine web-view)
        server (make-resource-server project)
        server-url (http-server/url server)
        view-id (g/make-node! graph WebViewNode :web-view web-view :server-url server-url)
        ^Tab tab (:tab opts)
        repainter (ui/->timer 30 "update-web-view!" (fn [_ _ _]
                                                      (when (.isSelected tab)
                                                        (g/node-value view-id :update-web-view))))]

    (.addListener (.locationProperty web-engine)
                  ^ChangeListener
                  (ui/change-listener _ _ new-location (handle-location-change! server-url project view-id new-location)))

    (ui/children! parent [web-view])
    (ui/fill-control web-view)

    (ui/context! web-view :browser {:web-engine web-engine} nil)

    (doto web-engine
      (.setOnAlert (ui/event-handler ev
                     (dialogs/make-info-dialog
                       {:title "Alert"
                        :icon :icon/circle-info
                        :header (.getData ^WebEvent ev)})))
      (.setUserStyleSheetLocation (str (io/resource "markdown.css"))))
    (ui/timer-start! repainter)
    (ui/on-closed! tab (fn [_]
                         (ui/timer-stop! repainter)
                         (http-server/stop! server 0)))
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
