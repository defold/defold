(ns basement
  (:require [editor.ui :as ui]))

(comment

  (def property-update-debouncer (display-debouncer 100 refresh-property-page))

  (signal property-update-debouncer)
  (cancel property-update-debouncer)

  )
