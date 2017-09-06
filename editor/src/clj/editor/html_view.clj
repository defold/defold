(ns editor.html-view
  (:require
   [clojure.core.protocols :as p]
   [clojure.java.io :as io]
   [clojure.string :as string]
   [dynamo.graph :as g]
   [editor.defold-project :as project]
   [editor.dialogs :as dialogs]
   [editor.handler :as handler]
   [editor.resource :as resource]
   [editor.ui :as ui]
   [editor.view :as view]
   [editor.workspace :as workspace])
  (:import
   (java.net URI URLDecoder)
   (javafx.scene Node Parent)
   (javafx.scene.layout GridPane Priority ColumnConstraints)
   (javafx.scene.web WebEngine WebView)
   (javafx.concurrent Worker Worker$State)
   (org.w3c.dom Document Element NodeList)
   (org.w3c.dom.events Event EventListener EventTarget)))

(set! *warn-on-reflection* true)


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

(defn- update-attribute!
  [^Element elem attr f & args]
  (when-some [v (.getAttribute elem attr)]
    (.setAttribute elem attr (apply f v args))))


;;--------------------------------------------------------------------
;; view

(defn- query-params->map
  [params]
  (when (not (string/blank? params))
    (into {}
          (map (fn [param]
                 (let [[k v] (string/split param #"=")]
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

(defmethod url->command :default
  [^URI uri _]
  {:command (keyword (.getHost uri))})

(defn- dispatch-url!
  [project ^URI uri]
  (when-some [{:keys [command user-data]} (url->command uri {:project project})]
    (when-let [handler-ctx (handler/active command (ui/contexts (.getScene (ui/main-stage))) user-data)]
      (when (handler/enabled? handler-ctx)
        (handler/run handler-ctx)))))

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch java.net.URISyntaxException _
        nil))))

(defn- anchor-url
  ^URI [^Element anchor-element]
  (some-> anchor-element (.getAttribute "href") string->url))

(defn- handle-click!
  [project ^Event ev]
  (when-some [url (anchor-url (.getTarget ev))]
    (dispatch-url! project url)))

(defn- hijack-defold-links
  [^Document doc project]
  (let [handler (reify EventListener
                  (handleEvent [_this ev]
                    (.preventDefault ev)
                    (handle-click! project ev)))]
    (run! (fn [^Element anchor-element]
            (when-some [url (anchor-url anchor-element)]
              (when (= "defold" (.getScheme url))
                (.addEventListener ^EventTarget anchor-element "click" handler true))))
          (.getElementsByTagName doc "a"))))

(defn- on-load-succeeded
  [^WebEngine web-engine project]
  (when-some [doc (.getDocument web-engine)]
    (doto doc
      (hijack-defold-links project))))

(defn- on-load-failed
  [^WebEngine web-engine]
  (let [load-worker (.getLoadWorker web-engine)]
    (when-some [ex (.getException load-worker)]
      (dialogs/make-alert-dialog (str (.getMessage load-worker)
                                      ": "
                                      (.getMessage ex))))))

(defn- inject-base-url
  [html html-resource]
  ;; NOTE: This is a slight hack to enable project-directory relative
  ;; URLs in the html. By adding a <base> tag to head, with a href of
  ;; the directory of the resource being displayed, any relative link
  ;; in document (img src, link href etc) will be resolved using this
  ;; base href. This allows us to easily reference resource in project
  ;; from html.
  (let [base-url (str "file://" (resource/parent-proj-path (resource/abs-path html-resource)) "/")]
    (string/replace-first html #"\<head\>"
                          (format "<head><base href=\"%s\"/>" base-url))))

(g/defnk produce-html
  [html html-resource]
  (if (and html html-resource)
    (inject-base-url html html-resource)
    html))

(g/defnk update-web-view
  [html ^WebView web-view]
  (doto (.getEngine web-view)
    (.setUserStyleSheetLocation (str (io/resource "markdown.css")))
    (.loadContent html)))

(g/defnode WebViewNode
  (inherits view/WorkbenchView)

  (input html g/Any)
  (input html-resource g/Any)

  (property web-view WebView)

  (output html g/Any :cached produce-html)
  (output refresh g/Any :cached update-web-view))

(defn- make-web-view
  [project]
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

(defn make-view
  [graph ^Parent parent html-node opts]
  (let [project (or (:project opts)
                    (project/get-project html-node))
        web-view (make-web-view project)
        view-id (g/make-node! graph WebViewNode :web-view web-view)
        repainter (ui/->timer 1 "update-web-view" (fn [_ dt] (g/node-value view-id :refresh)))]
    (ui/children! parent [web-view])
    (ui/fill-control web-view)
    (g/transact
      (concat
        (g/connect html-node :html view-id :html)
        (g/connect html-node :resource view-id :html-resource)))
    (g/node-value view-id :refresh)
    (ui/timer-start! repainter)
    (when-some [^Tab tab (:tab opts)]
      (ui/timer-stop-on-closed! tab repainter))
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :html
                                :label "Web"
                                :make-view-fn make-view))

(comment

  (ui/run-later (g/transact (register-view-types 0)))

  )
