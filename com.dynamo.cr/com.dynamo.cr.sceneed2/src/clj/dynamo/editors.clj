(ns dynamo.editors
  (:require [clojure.core.async :refer [chan dropping-buffer]]
            [dynamo.ui :as ui]
            [internal.ui.editors :as ie]))

(set! *warn-on-reflection* true)

(defprotocol Editor
  (init [this site])
  (create-controls [this parent])
  (save [this file monitor])
  (dirty? [this])
  (save-as-allowed? [this])
  (set-focus [this]))

(defn open-part
  [behavior & {:as opts}]
  (ie/open-part behavior opts))

(doseq [[v doc]
        {*ns*
         "This namespace allows you to create new editors. It
supplies a protocol that any new editor must implement."

         #'Editor
         "Any new editor must implement this protocol."

         #'init
         "The first function invoked to set up an editor.
Receives the 'editor site'."

         #'create-controls
         "The second function in the editor lifecycle. This
function must create the GUI for the editor. If you're
editing scenes using the scene graph and your own node types,
you can just call dynamo.editors/create-scenegraph-controls."

         #'save
         "Save the current state of the editor to a file. For
good user experience, you should update the progress monitor
(it is an IProgressMonitor passed through from the Workbench)."

         #'dirty?
         "Return true if there are unsaved changes."

         #'save-as-allowed?
         "Return true if the contents of this editor could be
saved with a new file name"

         #'set-focus
         "Called when this editor receives focus. Should
in turn set focus on the most appropriate control."

         #'open-part
         "Creates an Eclipse view for the given behavior. The
behavior must support dynamo.types/MessageTarget.

Allowed options are:

:label - the visible label for the part
:closeable - if true, the part can be closed (defaults to true)
:id - the part ID by which Eclipse will know it"}]
  (alter-meta! v assoc :doc doc))
