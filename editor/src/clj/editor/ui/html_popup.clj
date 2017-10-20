(ns editor.ui.html-popup
  (:require [editor.ui :as ui])
  (:import (javafx.css Styleable)
           (javafx.scene.control PopupControl Skin)
           (javafx.scene.text FontSmoothingType)
           (javafx.scene.web WebErrorEvent WebEvent WebView WebEngine)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- make-web-view []
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

(defn load-content
  ([^PopupControl html-popup ^String content]
   (.loadContent (web-engine html-popup) content))
  ([^PopupControl html-popup ^String content ^String content-type]
   (.loadContent (web-engine html-popup) content content-type)))

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
