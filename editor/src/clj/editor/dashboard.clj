(ns editor.dashboard
  (:require [editor.client :as client]
            [editor.error-reporting :as error-reporting]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import (clojure.lang Atom)
           (java.util.prefs Preferences)
           (javafx.beans.binding Bindings)
           (javafx.beans.property SimpleObjectProperty)
           (javafx.scene.control TextField)
           (javafx.scene.input Clipboard ClipboardContent)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^:private last-used-email-prefs-key "login-last-used-email")

(defn- sign-in-state [^SimpleObjectProperty sign-in-state-property]
  (.get sign-in-state-property))

(defn- set-sign-in-state! [^SimpleObjectProperty sign-in-state-property sign-in-state]
  (assert (case sign-in-state (:not-signed-in :browser-open :login-fields :signed-in) true false))
  (.set sign-in-state-property sign-in-state))

;; -----------------------------------------------------------------------------
;; Dashboard client
;; -----------------------------------------------------------------------------

(defn dashboard-client? [value]
  (and (map? value)
       (instance? Atom (:sign-in-response-server-atom value))
       (instance? SimpleObjectProperty (:sign-in-state-property value))
       (instance? Preferences (:prefs value))))

(defn make-dashboard-client [prefs]
  (let [signed-in? (with-open [client (client/make-client prefs)]
                     (login/logged-in? prefs client))
        sign-in-state (if signed-in? :signed-in :not-signed-in)
        sign-in-response-server-atom (atom nil)
        sign-in-state-property (doto (SimpleObjectProperty.)
                                 (set-sign-in-state! sign-in-state))]
    {:sign-in-response-server-atom sign-in-response-server-atom
     :sign-in-state-property sign-in-state-property
     :prefs prefs}))

(defn shutdown-dashboard-client! [{:keys [sign-in-response-server-atom] :as _dashboard-client}]
  (when-some [sign-in-response-server (preset! sign-in-response-server-atom nil)]
    (login/stop-server-now! sign-in-response-server)))

(defn- start-sign-in-response-server!
  "Starts a server that awaits an oauth response and sets the sign-in state
  depending on the response. Returns the started server instance."
  [{:keys [prefs sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (let [client (client/make-client prefs)
        server-opts {:on-success (fn [exchange-info]
                                   (client/destroy-client! client)
                                   (ui/run-later
                                     (login/stop-server-soon! (preset! sign-in-response-server-atom nil))
                                     (login/set-prefs-from-successful-login! prefs exchange-info)
                                     (set-sign-in-state! sign-in-state-property :signed-in)))
                     :on-error (fn [exception]
                                 (client/destroy-client! client)
                                 (ui/run-later
                                   (login/stop-server-soon! (preset! sign-in-response-server-atom nil))
                                   (set-sign-in-state! sign-in-state-property :not-signed-in)
                                   (error-reporting/report-exception! exception)))}]
    (swap! sign-in-response-server-atom
           (fn [old-sign-in-response-server]
             (when (some? old-sign-in-response-server)
               (login/stop-server-now! old-sign-in-response-server))
             (login/make-server client server-opts)))))

(defn- begin-sign-in-with-browser!
  "Open a sign-in page in the system-configured web browser. Start a server that
  awaits the oauth response redirect from the browser."
  [{:keys [sign-in-state-property] :as dashboard-client}]
  (assert (= :not-signed-in (sign-in-state sign-in-state-property)))
  (let [sign-in-response-server (start-sign-in-response-server! dashboard-client)
        sign-in-page-url (login/login-page-url sign-in-response-server)]
    (set-sign-in-state! sign-in-state-property :browser-open)
    (ui/open-url sign-in-page-url)))

(defn- cancel-sign-in!
  "Cancel the sign-in process and return to the :not-signed-in screen."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (assert (case (sign-in-state sign-in-state-property) (:browser-open :login-fields) true false))
  (login/stop-server-now! (preset! sign-in-response-server-atom nil))
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn- copy-sign-in-url!
  "Copy the sign-in url from the sign-in response server to the clipboard.
  If the sign-in response server is not running, does nothing."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client} ^Clipboard clipboard]
  (assert (= :browser-open (sign-in-state sign-in-state-property)))
  (when-some [sign-in-response-server @sign-in-response-server-atom]
    (let [sign-in-page-url (login/login-page-url sign-in-response-server)]
      (.setContent clipboard (doto (ClipboardContent.)
                               (.putString sign-in-page-url)
                               (.putUrl sign-in-page-url))))))

(defn- sign-out!
  "Sign out the active user from the Defold dashboard."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client}]
  (assert (= :signed-in (sign-in-state sign-in-state-property)))
  (login/logout prefs)
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn- compare-project-names [p1 p2]
  (.compareToIgnoreCase ^String (:name p1) ^String (:name p2)))

(defn fetch-projects [{:keys [prefs] :as _dashboard-client}]
  (with-open [client (client/make-client prefs)]
    (let [project-info-list (client/cr-get client ["projects" -1] Protocol$ProjectInfoList)]
      (sort compare-project-names (:projects project-info-list)))))

;; -----------------------------------------------------------------------------
;; UI elements
;; -----------------------------------------------------------------------------

(defn configure-sign-in-ui-elements! [dashboard-client root]
  (assert (dashboard-client? dashboard-client))
  (let [sign-in-state-property ^SimpleObjectProperty (:sign-in-state-property dashboard-client)
        prefs (:prefs dashboard-client)]
    (ui/with-controls root [state-login-fields state-not-signed-in state-browser-open]

      ;; E-mail login fields state.
      (ui/bind-presence! state-login-fields (Bindings/equal :login-fields sign-in-state-property))
      (ui/with-controls state-login-fields [cancel-button ^TextField email-field forgot-password-button password-field submit-button]
        (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
        (ui/on-action! submit-button (fn [_] (cancel-sign-in! dashboard-client)))
        (ui/observe sign-in-state-property
                    (fn [_ _ sign-in-state]
                      (when (= :login-fields sign-in-state)
                        (ui/text! email-field (prefs/get-prefs prefs last-used-email-prefs-key ""))))))

      ;; Not signed in state.
      (ui/bind-presence! state-not-signed-in (Bindings/equal :not-signed-in sign-in-state-property))
      (ui/with-controls state-not-signed-in [create-account-button sign-in-with-browser-button]
        (ui/on-action! sign-in-with-browser-button (fn [_] (begin-sign-in-with-browser! dashboard-client)))
        (ui/on-action! create-account-button (fn [_] (ui/open-url "https://www.defold.com"))))

      ;; Browser open awaiting Google sign-in state.
      (ui/bind-presence! state-browser-open (Bindings/equal :browser-open sign-in-state-property))
      (ui/with-controls state-browser-open [cancel-button copy-sign-in-url-button]
        (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
        (ui/on-action! copy-sign-in-url-button (fn [_] (copy-sign-in-url! dashboard-client (Clipboard/getSystemClipboard))))))))

(defn configure-sign-out-button! [dashboard-client sign-out-button]
  (assert (dashboard-client? dashboard-client))
  (let [sign-in-state-property ^SimpleObjectProperty (:sign-in-state-property dashboard-client)]
    (ui/bind-presence! sign-out-button (Bindings/equal :signed-in sign-in-state-property))
    (ui/on-action! sign-out-button (fn [_] (sign-out! dashboard-client)))))
