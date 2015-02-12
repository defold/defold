(ns internal.ui.menus
  (:require [internal.ui.handlers :as handlers])
  (:import [org.eclipse.core.commands ExecutionEvent]))

(def SCENE-MENU "com.dynamo.cr.menus.scene")
(def EDIT-MENU  "com.dynamo.cr.menus.edit")

(def ^:private menu-configs (atom {SCENE-MENU ["Scene" "com.dynamo.cr.menu-items.scene"]
                                   EDIT-MENU  ["Edit"  "com.dynamo.cr.menu-items.edit"]}))

;; get-menu-config data structure:
;; '(label category-id)
(defn get-menu-config
  [menu-id]
  (get @menu-configs menu-id))

(defn get-menu-ids
  []
  (keys @menu-configs))

(defn get-category-ids
  [menu-ids]
  (prn "requesting " menu-ids)
  (map #((second (get @menu-configs %))) menu-ids))

(defn make-menu-item [^org.eclipse.core.commands.Command command]
  [(.getName command) (.getId command)])
;; collect-category-items' returned data structure:
;; '(label command-id & [mnemonic image-id disabled-image-id])

(defn collect-category-items
  [category]
  (let [commands (handlers/commands-in-category category)]
    (into [] (map make-menu-item commands))))

(defn collect-menu-items
  "given a menu-id, returns a vec of:\n
  '(label command-id & [mnemonic image-id disabled-image-id])"
  [menu-id]
  (if-let [menu-cfg (get @menu-configs menu-id)]
    (collect-category-items (second menu-cfg))
    []))
