(ns editor.ui.html-popup
  (:require [clojure.string :as string]
            [editor.ui :as ui])
  (:import (javafx.beans.value ChangeListener)
           (javafx.css Styleable)
           (javafx.scene Node)
           (javafx.scene.control PopupControl Skin)
           (javafx.scene.layout Region)
           (javafx.scene.text FontSmoothingType)
           (javafx.scene.web WebEngine WebErrorEvent WebEvent WebView)
           (netscape.javascript JSObject)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-web-view
  ^WebView []
  (let [web-view (WebView.)
        web-engine (.getEngine web-view)]
    (ui/add-style! web-view "html-popup-web-view")
    (.setFocusTraversable web-view false)
    (.setContextMenuEnabled web-view false)
    (.setFontSmoothingType web-view FontSmoothingType/LCD)
    (.setJavaScriptEnabled web-engine false)
    (.setOnAlert web-engine (ui/event-handler event (println "HTML Popup Alert: " (.getData ^WebEvent event))))
    (.setOnError web-engine (ui/event-handler event (println "HTML Popup Error: " (.getMessage ^WebErrorEvent event))))
    web-view))

(defn web-view
  ^WebView [^PopupControl html-popup]
  (.. html-popup getSkin getNode))

(defn web-engine
  ^WebEngine [^PopupControl html-popup]
  (.getEngine (web-view html-popup)))

(defn- make-element-size-js
  ^String [element-id]
  (string/join "\n" [(str "var element = document.getElementById('" element-id "');")
                     "var style = getComputedStyle(element);"
                     "['width', 'height'].reduce(function (result, key) {"
                     "  result[key] = parseFloat(style[key]);"
                     "  return result;"
                     "}, {});"]))

(defn- element-size [^WebEngine web-engine ^String element-size-js]
  (let [^JSObject js-obj (.executeScript web-engine element-size-js)
        width (Math/ceil (.getMember js-obj "width"))
        height (Math/ceil (.getMember js-obj "height"))]
    [width height]))

(def ^:private html-popup-size-js (make-element-size-js "html-popup"))

(defn load-content [^PopupControl html-popup ^String content loaded-fn!]
  (let [web-view (web-view html-popup)
        web-engine (.getEngine web-view)
        document-listener (reify ChangeListener
                            (changed [this observable old-document new-document]
                              (when (some? new-document)
                                (.removeListener observable this)
                                (let [[_width height] (element-size web-engine html-popup-size-js)]
                                  (.setPrefHeight web-view height)
                                  (loaded-fn! (.getPrefWidth web-view) height)))))]
    (.addListener (.documentProperty web-engine) document-listener)
    (.loadContent web-engine (str "<div id='html-popup'>" content "</div>") "text/html")))

(defn make-html-popup
  ^PopupControl [^Styleable owner]
  (let [web-view (make-web-view)
        popup (proxy [PopupControl] []
                (getStyleableParent [] owner))
        *skinnable (atom popup)
        popup-skin (reify Skin
                     (getSkinnable [_] @*skinnable)
                     (getNode [_] web-view)
                     (dispose [_] (reset! *skinnable nil)))]
    (doto popup
      (.setSkin popup-skin)
      (.setConsumeAutoHidingEvents true)
      (.setAutoHide true)
      (.setAutoFix true)
      (.setHideOnEscape true))))
