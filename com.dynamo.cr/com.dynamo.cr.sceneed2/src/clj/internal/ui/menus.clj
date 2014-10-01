(ns internal.ui.menus
  (:require [dynamo.project :as p]
            [internal.system :as sys]
            [internal.ui.handlers :as handlers]
            [dynamo.ui :refer [defcommand defhandler]])
  (:import [org.eclipse.core.commands ExecutionEvent]))


(defn make-menu-item [command]
  [(.getName command) (.getId command)])

; com.dynamo.cr.menu-items.edit-menu
(defn collect-items
  []
  (let [commands (handlers/commands-in-category "com.dynamo.cr.menu-items.EDIT")]
    (into [] (map make-menu-item commands))))

;; collect-items' returned data structure:
;; '(label command-id & [mnemonic image-id disabled-image-id])
