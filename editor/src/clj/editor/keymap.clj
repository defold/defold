;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.keymap
  (:refer-clojure :exclude [empty])
  (:require [editor.os :as os]
            [editor.prefs :as prefs]
            [internal.util :as util]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [javafx.event EventHandler]
           [javafx.scene Scene]
           [javafx.scene.input KeyCharacterCombination KeyCode KeyCodeCombination KeyCombination KeyCombination$ModifierValue KeyEvent]))

(set! *warn-on-reflection* true)

(def platform->default-key-bindings
  ;; We should generally avoid adding shortcuts that can also be
  ;; interpreted as typable text. This includes "A", "Shift+E" but
  ;; also combinations like Alt+Ctrl+2 because on windows Alt+Ctrl is
  ;; an alternative way to enter AltGR and AltGR+2 is @ on some layouts -
  ;; something you may want to type in the code editor.
  ;; The function key-binding-data->keymap attempts to catch this type
  ;; of mistake.
  ;; Note that specifying Shortcut key (which is Meta on macOS and Ctrl on
  ;; Windows and Linux) is not allowed.
  {:macos [["A" :edit.add-embedded-component]
           ["A" :scene.free-camera.left]
           ["Alt+Backspace" :code.delete-previous-word]
           ["Alt+Delete" :code.delete-next-word]
           ["Alt+Down" :code.goto-line-end]
           ["Alt+Down" :edit.reorder-down]
           ["Alt+Left" :code.goto-previous-word]
           ["Alt+F9" :breakpoints.edit-selected]
           ["Alt+F9" :debugger.edit-breakpoint]
           ["Alt+Meta+E" :code.select-next-occurrence]
           ["Alt+Meta+F" :code.replace-text]
           ["Alt+Meta+G" :code.replace-next]
           ["Alt+Right" :code.goto-next-word]
           ["Alt+Space" :code.show-completions]
           ["Alt+Up" :code.goto-line-start]
           ["Alt+Up" :edit.reorder-up]
           ["Alt+W" :scene.free-camera.walking-mode]
           ["Backspace" :code.delete-previous-char]
           ["Backspace" :edit.delete]
           ["Ctrl+A" :code.goto-line-start]
           ["Ctrl+E" :code.goto-line-end]
           ["Ctrl+I" :code.reindent]
           ["Ctrl+Meta+H" :scene.visibility.toggle-component-guides]
           ["Ctrl+R" :file.open-recent]
           ["Ctrl+Space" :code.show-completions]
           ["D" :scene.free-camera.right]
           ["Delete" :edit.delete]
           ["Delete" :code.delete-next-char]
           ["Delete" :breakpoints.remove-selected]
           ["Down" :scene.move-down]
           ["E" :scene.free-camera.up]
           ["E" :scene.select-rotate-tool]
           ["End" :code.goto-line-end]
           ["Esc" :code.escape]
           ["F" :scene.frame-selection]
           ["F1" :help.open-documentation]
           ["F10" :debugger.step-over]
           ["F11" :debugger.step-into]
           ["F2" :edit.rename]
           ["F5" :debugger.start]
           ["F5" :debugger.continue]
           ["F6" :window.toggle-left-pane]
           ["F7" :window.toggle-bottom-pane]
           ["F8" :window.toggle-right-pane]
           ["F9" :debugger.toggle-breakpoint]
           ["F12" :code.goto-definition]
           ["Home" :code.goto-line-text-start]
           ["Left" :scene.move-left]
           ["Meta+'+'" :code.zoom.increase]
           ["Meta+Add" :code.zoom.increase]
           ["Meta+'-'" :code.zoom.decrease]
           ["Meta+Subtract" :code.zoom.decrease]
           ["Meta+0" :code.zoom.reset]
           ["Meta+A" :code.select-all]
           ["Meta+B" :project.build]
           ["Meta+M" :project.build-html5]
           ["Meta+C" :edit.copy]
           ["Meta+Comma" :app.preferences]
           ["Meta+D" :code.select-next-occurrence]
           ["Meta+Down" :code.goto-file-end]
           ["Meta+E" :scene.visibility.toggle-selection]
           ["Meta+F" :edit.find]
           ["Meta+G" :code.find-next]
           ["Meta+G" :scene.visibility.toggle-grid]
           ["Meta+L" :code.goto-line]
           ["Meta+Left" :code.goto-line-text-start]
           ["Meta+N" :file.new]
           ["Meta+O" :file.open-selected]
           ["Meta+P" :file.open]
           ["Meta+Q" :app.quit]
           ["Meta+R" :run.hot-reload]
           ["Meta+Right" :code.goto-line-end]
           ["Meta+S" :file.save-all]
           ["Meta+'/'" :code.toggle-comment]
           ["Meta+T" :scene.stop]
           ["Meta+U" :project.rebundle]
           ["Meta+J" :run.stop]
           ["Meta+Up" :code.goto-file-start]
           ["Meta+V" :edit.paste]
           ["Meta+W" :window.tab.close]
           ["Meta+X" :edit.cut]
           ["Meta+Z" :edit.undo]
           ["Page Down" :code.page-down]
           ["Page Up" :code.page-up]
           ["Period" :scene.realign-camera]
           ["Q" :scene.free-camera.down]
           ["R" :scene.select-scale-tool]
           ["Right" :scene.move-right]
           ["S" :scene.free-camera.backward]
           ["Shift+A" :edit.add-secondary-embedded-component]
           ["Shift+Alt+Down" :code.select-line-end]
           ["Shift+Alt+Left" :code.select-previous-word]
           ["Shift+Alt+Right" :code.select-next-word]
           ["Shift+Alt+Up" :code.select-line-start]
           ["Shift+Back Quote" :scene.free-camera.activate]
           ["Shift+Backspace" :code.delete-previous-char]
           ["Shift+Ctrl+A" :code.select-line-start]
           ["Shift+Ctrl+E" :code.select-line-end]
           ["Shift+Ctrl+Left" :code.select-previous-word]
           ["Shift+Ctrl+Right" :code.select-next-word]
           ["Shift+Down" :scene.move-down-major]
           ["Shift+Down" :code.select-down]
           ["Shift+E" :scene.select-erase-tool]
           ["Shift+End" :code.select-line-end]
           ["Shift+F9" :debugger.toggle-breakpoint-enabled]
           ["Shift+F11" :debugger.step-out]
           ["Shift+F12" :code.show-references]
           ["Shift+Home" :code.select-line-text-start]
           ["Shift+Left" :scene.move-left-major]
           ["Shift+Left" :code.select-left]
           ["Shift+Meta+B" :project.clean-build]
           ["Shift+Meta+D" :code.duplicate-selection]
           ["Shift+Meta+M" :project.clean-build-html5]
           ["Shift+Meta+Down" :code.select-file-end]
           ["Shift+Meta+E" :scene.visibility.show-last-hidden]
           ["Shift+Meta+F" :file.search]
           ["Shift+Meta+G" :code.find-previous]
           ["Shift+Meta+I" :scene.visibility.toggle-filters]
           ["Shift+Meta+L" :code.split-selection-into-lines]
           ["Shift+Meta+Left" :code.select-line-text-start]
           ["Shift+Meta+R" :project.reload-editor-scripts]
           ["Shift+Meta+Right" :code.select-line-end]
           ["Shift+Meta+T" :file.reopen-recent]
           ["Shift+Meta+Up" :code.select-file-start]
           ["Shift+Meta+W" :window.tab.close-all]
           ["Shift+Meta+Y" :file.load-external-changes]
           ["Shift+Meta+Z" :edit.redo]
           ["Shift+Page Down" :code.select-page-down]
           ["Shift+Page Up" :code.select-page-up]
           ["Shift+Right" :scene.move-right-major]
           ["Shift+Right" :code.select-right]
           ["Shift+Tab" :code.tab-backward]
           ["Shift+Up" :code.select-up]
           ["Shift+Up" :scene.move-up-major]
           ["Space" :scene.play]
           ["Space" :scene.toggle-tile-palette]
           ["Tab" :code.tab-forward]
           ["Up" :scene.move-up]
           ["W" :scene.free-camera.forward]
           ["W" :scene.select-move-tool]
           ["X" :scene.flip-brush-horizontally]
           ["Y" :scene.flip-brush-vertically]
           ["Z" :scene.rotate-brush-90-degrees]]
   :win32 [["A" :edit.add-embedded-component]
           ["A" :scene.free-camera.left]
           ["Alt+Down" :edit.reorder-down]
           ["Alt+F9" :breakpoints.edit-selected]
           ["Alt+F9" :debugger.edit-breakpoint]
           ["Alt+R" :file.open-recent]
           ["Alt+Up" :edit.reorder-up]
           ["Alt+W" :scene.free-camera.walking-mode]
           ["Backspace" :code.delete-previous-char]
           ["Ctrl+'+'" :code.zoom.increase]
           ["Ctrl+Add" :code.zoom.increase]
           ["Ctrl+'-'" :code.zoom.decrease]
           ["Ctrl+Subtract" :code.zoom.decrease]
           ["Ctrl+0" :code.zoom.reset]
           ["Ctrl+A" :code.select-all]
           ["Ctrl+B" :project.build]
           ["Ctrl+M" :project.build-html5]
           ["Ctrl+Backspace" :code.delete-previous-word]
           ["Ctrl+C" :edit.copy]
           ["Ctrl+Comma" :app.preferences]
           ["Ctrl+D" :code.select-next-occurrence]
           ["Ctrl+Delete" :code.delete-next-word]
           ["Ctrl+E" :scene.visibility.toggle-selection]
           ["Ctrl+End" :code.goto-file-end]
           ["Ctrl+F" :edit.find]
           ["Ctrl+G" :code.find-next]
           ["Ctrl+G" :scene.visibility.toggle-grid]
           ["Ctrl+H" :code.replace-text]
           ["Ctrl+H" :scene.visibility.toggle-component-guides]
           ["Ctrl+Home" :code.goto-file-start]
           ["Ctrl+I" :code.reindent]
           ["Ctrl+L" :code.goto-line]
           ["Ctrl+Left" :code.goto-previous-word]
           ["Ctrl+N" :file.new]
           ["Ctrl+O" :file.open-selected]
           ["Ctrl+P" :file.open]
           ["Ctrl+Q" :app.quit]
           ["Ctrl+R" :run.hot-reload]
           ["Ctrl+Right" :code.goto-next-word]
           ["Ctrl+S" :file.save-all]
           ["Ctrl+'/'" :code.toggle-comment]
           ["Ctrl+Space" :code.show-completions]
           ["Ctrl+T" :scene.stop]
           ["Ctrl+U" :project.rebundle]
           ["Ctrl+J" :run.stop]
           ["Ctrl+V" :edit.paste]
           ["Ctrl+W" :window.tab.close]
           ["Ctrl+X" :edit.cut]
           ["Ctrl+Z" :edit.undo]
           ["D" :scene.free-camera.right]
           ["Delete" :edit.delete]
           ["Delete" :code.delete-next-char]
           ["Delete" :breakpoints.remove-selected]
           ["Down" :scene.move-down]
           ["E" :scene.free-camera.up]
           ["E" :scene.select-rotate-tool]
           ["End" :code.goto-line-end]
           ["Esc" :code.escape]
           ["F" :scene.frame-selection]
           ["F1" :help.open-documentation]
           ["F10" :debugger.step-over]
           ["F11" :debugger.step-into]
           ["F2" :edit.rename]
           ["F5" :debugger.start]
           ["F5" :debugger.continue]
           ["F6" :window.toggle-left-pane]
           ["F7" :window.toggle-bottom-pane]
           ["F8" :window.toggle-right-pane]
           ["F9" :debugger.toggle-breakpoint]
           ["F12" :code.goto-definition]
           ["Home" :code.goto-line-text-start]
           ["Left" :scene.move-left]
           ["Page Down" :code.page-down]
           ["Page Up" :code.page-up]
           ["Period" :scene.realign-camera]
           ["Q" :scene.free-camera.down]
           ["R" :scene.select-scale-tool]
           ["Right" :scene.move-right]
           ["S" :scene.free-camera.backward]
           ["Shift+A" :edit.add-secondary-embedded-component]
           ["Shift+Back Quote" :scene.free-camera.activate]
           ["Shift+Backspace" :code.delete-previous-char]
           ["Shift+Ctrl+B" :project.clean-build]
           ["Shift+Ctrl+D" :code.duplicate-selection]
           ["Shift+Ctrl+M" :project.clean-build-html5]
           ["Shift+Ctrl+E" :scene.visibility.show-last-hidden]
           ["Shift+Ctrl+End" :code.select-file-end]
           ["Shift+Ctrl+F" :file.search]
           ["Shift+Ctrl+G" :code.find-previous]
           ["Shift+Ctrl+H" :code.replace-next]
           ["Shift+Ctrl+Home" :code.select-file-start]
           ["Shift+Ctrl+I" :scene.visibility.toggle-filters]
           ["Shift+Ctrl+L" :code.split-selection-into-lines]
           ["Shift+Ctrl+Left" :code.select-previous-word]
           ["Shift+Ctrl+R" :project.reload-editor-scripts]
           ["Shift+Ctrl+Right" :code.select-next-word]
           ["Shift+Ctrl+T" :file.reopen-recent]
           ["Shift+Ctrl+W" :window.tab.close-all]
           ["Shift+Ctrl+Y" :file.load-external-changes]
           ["Shift+Ctrl+Z" :edit.redo]
           ["Shift+Down" :scene.move-down-major]
           ["Shift+Down" :code.select-down]
           ["Shift+E" :scene.select-erase-tool]
           ["Shift+End" :code.select-line-end]
           ["Shift+F9" :debugger.toggle-breakpoint-enabled]
           ["Shift+F11" :debugger.step-out]
           ["Shift+F12" :code.show-references]
           ["Shift+F5" :debugger.stop]
           ["Shift+Home" :code.select-line-text-start]
           ["Shift+Left" :scene.move-left-major]
           ["Shift+Left" :code.select-left]
           ["Shift+Page Down" :code.select-page-down]
           ["Shift+Page Up" :code.select-page-up]
           ["Shift+Right" :scene.move-right-major]
           ["Shift+Right" :code.select-right]
           ["Shift+Tab" :code.tab-backward]
           ["Shift+Up" :code.select-up]
           ["Shift+Up" :scene.move-up-major]
           ["Space" :scene.play]
           ["Space" :scene.toggle-tile-palette]
           ["Tab" :code.tab-forward]
           ["Up" :scene.move-up]
           ["W" :scene.free-camera.forward]
           ["W" :scene.select-move-tool]
           ["X" :scene.flip-brush-horizontally]
           ["Y" :scene.flip-brush-vertically]
           ["Z" :scene.rotate-brush-90-degrees]]
   :linux [["A" :edit.add-embedded-component]
           ["A" :scene.free-camera.left]
           ["Alt+Down" :edit.reorder-down]
           ["Alt+F9" :breakpoints.edit-selected]
           ["Alt+F9" :debugger.edit-breakpoint]
           ["Alt+R" :file.open-recent]
           ["Alt+Up" :edit.reorder-up]
           ["Alt+W" :scene.free-camera.walking-mode]
           ["Backspace" :code.delete-previous-char]
           ["Ctrl+'+'" :code.zoom.increase]
           ["Ctrl+Add" :code.zoom.increase]
           ["Ctrl+'-'" :code.zoom.decrease]
           ["Ctrl+Subtract" :code.zoom.decrease]
           ["Ctrl+0" :code.zoom.reset]
           ["Ctrl+A" :code.select-all]
           ["Ctrl+B" :project.build]
           ["Ctrl+M" :project.build-html5]
           ["Ctrl+Backspace" :code.delete-previous-word]
           ["Ctrl+C" :edit.copy]
           ["Ctrl+Comma" :app.preferences]
           ["Ctrl+D" :code.select-next-occurrence]
           ["Ctrl+Delete" :code.delete-next-word]
           ["Ctrl+E" :scene.visibility.toggle-selection]
           ["Ctrl+End" :code.goto-file-end]
           ["Ctrl+F" :edit.find]
           ["Ctrl+G" :code.find-next]
           ["Ctrl+G" :scene.visibility.toggle-grid]
           ["Ctrl+H" :code.replace-text]
           ["Ctrl+H" :scene.visibility.toggle-component-guides]
           ["Ctrl+Home" :code.goto-file-start]
           ["Ctrl+I" :code.reindent]
           ["Ctrl+L" :code.goto-line]
           ["Ctrl+Left" :code.goto-previous-word]
           ["Ctrl+N" :file.new]
           ["Ctrl+O" :file.open-selected]
           ["Ctrl+P" :file.open]
           ["Ctrl+Q" :app.quit]
           ["Ctrl+R" :run.hot-reload]
           ["Ctrl+Right" :code.goto-next-word]
           ["Ctrl+S" :file.save-all]
           ["Ctrl+'/'" :code.toggle-comment]
           ["Ctrl+Space" :code.show-completions]
           ["Ctrl+T" :scene.stop]
           ["Ctrl+U" :project.rebundle]
           ["Ctrl+J" :run.stop]
           ["Ctrl+V" :edit.paste]
           ["Ctrl+W" :window.tab.close]
           ["Ctrl+X" :edit.cut]
           ["Ctrl+Z" :edit.undo]
           ["D" :scene.free-camera.right]
           ["Delete" :edit.delete]
           ["Delete" :code.delete-next-char]
           ["Delete" :breakpoints.remove-selected]
           ["Down" :scene.move-down]
           ["E" :scene.free-camera.up]
           ["E" :scene.select-rotate-tool]
           ["End" :code.goto-line-end]
           ["Esc" :code.escape]
           ["F" :scene.frame-selection]
           ["F1" :help.open-documentation]
           ["F10" :debugger.step-over]
           ["F11" :debugger.step-into]
           ["F2" :edit.rename]
           ["F5" :debugger.start]
           ["F5" :debugger.continue]
           ["F6" :window.toggle-left-pane]
           ["F7" :window.toggle-bottom-pane]
           ["F8" :window.toggle-right-pane]
           ["F9" :debugger.toggle-breakpoint]
           ["F12" :code.goto-definition]
           ["Home" :code.goto-line-text-start]
           ["Left" :scene.move-left]
           ["Page Down" :code.page-down]
           ["Page Up" :code.page-up]
           ["Period" :scene.realign-camera]
           ["Q" :scene.free-camera.down]
           ["R" :scene.select-scale-tool]
           ["Right" :scene.move-right]
           ["S" :scene.free-camera.backward]
           ["Shift+A" :edit.add-secondary-embedded-component]
           ["Shift+Back Quote" :scene.free-camera.activate]
           ["Shift+Backspace" :code.delete-previous-char]
           ["Shift+Ctrl+B" :project.clean-build]
           ["Shift+Ctrl+D" :code.duplicate-selection]
           ["Shift+Ctrl+M" :project.clean-build-html5]
           ["Shift+Ctrl+E" :scene.visibility.show-last-hidden]
           ["Shift+Ctrl+End" :code.select-file-end]
           ["Shift+Ctrl+F" :file.search]
           ["Shift+Ctrl+G" :code.find-previous]
           ["Shift+Ctrl+H" :code.replace-next]
           ["Shift+Ctrl+Home" :code.select-file-start]
           ["Shift+Ctrl+I" :scene.visibility.toggle-filters]
           ["Shift+Ctrl+L" :code.split-selection-into-lines]
           ["Shift+Ctrl+Left" :code.select-previous-word]
           ["Shift+Ctrl+R" :project.reload-editor-scripts]
           ["Shift+Ctrl+Right" :code.select-next-word]
           ["Shift+Ctrl+T" :file.reopen-recent]
           ["Shift+Ctrl+W" :window.tab.close-all]
           ["Shift+Ctrl+Y" :file.load-external-changes]
           ["Shift+Ctrl+Z" :edit.redo]
           ["Shift+Down" :scene.move-down-major]
           ["Shift+Down" :code.select-down]
           ["Shift+E" :scene.select-erase-tool]
           ["Shift+End" :code.select-line-end]
           ["Shift+F9" :debugger.toggle-breakpoint-enabled]
           ["Shift+F11" :debugger.step-out]
           ["Shift+F12" :code.show-references]
           ["Shift+F5" :debugger.stop]
           ["Shift+Home" :code.select-line-text-start]
           ["Shift+Left" :scene.move-left-major]
           ["Shift+Left" :code.select-left]
           ["Shift+Page Down" :code.select-page-down]
           ["Shift+Page Up" :code.select-page-up]
           ["Shift+Right" :scene.move-right-major]
           ["Shift+Right" :code.select-right]
           ["Shift+Tab" :code.tab-backward]
           ["Shift+Up" :code.select-up]
           ["Shift+Up" :scene.move-up-major]
           ["Space" :scene.play]
           ["Space" :scene.toggle-tile-palette]
           ["Tab" :code.tab-forward]
           ["Up" :scene.move-up]
           ["W" :scene.free-camera.forward]
           ["W" :scene.select-move-tool]
           ["X" :scene.flip-brush-horizontally]
           ["Y" :scene.flip-brush-vertically]
           ["Z" :scene.rotate-brush-90-degrees]]})

(def ^:private typable-truth-table
  "Only act on key pressed events that look like textual input, and
  skip what is likely shortcut combinations.

  Transcribed, and altered, from javafx TextInputControlBehaviour/defaultKeyTyped. See:

  http://grepcode.com/file/repo1.maven.org/maven2/net.java.openjfx.backport/openjfx-78-backport/1.8.0-ea-b96.1/com/sun/javafx/scene/control/behavior/TextInputControlBehavior.java#TextInputControlBehavior.defaultKeyTyped%28javafx.scene.input.KeyEvent%29

  Original:

        if (event.isControlDown() || event.isAltDown() || (isMac() && event.isMetaDown())) {
            if (!((event.isControlDown() || isMac()) && event.isAltDown())) return;
        }

  Transcribed:

        (not (and (or control-down? alt-down? (and mac? meta-down?))
                  (not (and (or control-down? mac?) alt-down?))))

  Simplified:

    (or (and control-down? alt-down?)
        (and mac? alt-down?)
        (and (not mac?) (not control-down?) (not alt-down?))
        (and (not control-down?) (not alt-down?) (not meta-down?)))

  Truth table:

  MAC   CTRL  ALT   META     RESULT
  false false false false => true
  false false false true  => true  -- Haven't been able to repro META on non-mac, assume not typable if even possible
  false false true  false => false
  false false true  true  => false
  false true  false false => false
  false true  false true  => false
  false true  true  false => true
  false true  true  true  => true  -- As above
  true  false false false => true
  true  false false true  => false
  true  false true  false => true
  true  false true  true  => true  -- No other tested macOS app reacts like this, but seems you can use cmd (Meta) as modifier in keyboard input sources
  true  true  false false => false
  true  true  false true  => false
  true  true  true  false => true  -- As above
  true  true  true  true  => true  -- As above

  This does not seem right. We probably want something like that:

    MAC   CTRL  ALT   META   TYPABLE"
  {[false false false false] true
   [false false false true]  false   ;; Now treated as possibly shortcut
   [false false true  false] false
   [false false true  true]  false
   [false true  false false] false
   [false true  false true]  false
   [false true  true  false] true
   [false true  true  true]  false   ;; As above
   [true  false false false] true
   [true  false false true]  false
   [true  false true  false] true
   [true  false true  true]  false   ;; Now treated as possibly shortcut
   [true  true  false false] false
   [true  true  false true]  false
   [true  true  true  false] true    ;; As above
   [true  true  true  true]  false}) ;; Now treated as possibly shortcut

(def ^:private key-code->typable-char-strings
  ;; Based on javafx.scene.input.KeyCodeCombination/getSingleChar
  ;; Additions: SPACE
  ;; Deletions: ENTER, LEFT, UP, RIGHT, DOWN
  {KeyCode/ADD "+"
   KeyCode/AMPERSAND "&"
   KeyCode/ASTERISK "*"
   KeyCode/AT "@"
   KeyCode/BACK_QUOTE "`"
   KeyCode/BACK_SLASH "\\"
   KeyCode/BRACELEFT "{"
   KeyCode/BRACERIGHT "}"
   KeyCode/CIRCUMFLEX "^"
   KeyCode/CLOSE_BRACKET "]"
   KeyCode/COLON ":"
   KeyCode/COMMA ","
   KeyCode/DECIMAL "."
   KeyCode/DIGIT0 "0"
   KeyCode/DIGIT1 "1"
   KeyCode/DIGIT2 "2"
   KeyCode/DIGIT3 "3"
   KeyCode/DIGIT4 "4"
   KeyCode/DIGIT5 "5"
   KeyCode/DIGIT6 "6"
   KeyCode/DIGIT7 "7"
   KeyCode/DIGIT8 "8"
   KeyCode/DIGIT9 "9"
   KeyCode/DIVIDE "/"
   KeyCode/DOLLAR "$"
   KeyCode/EQUALS "="
   KeyCode/EURO_SIGN "€"
   KeyCode/EXCLAMATION_MARK "!"
   KeyCode/GREATER ">"
   KeyCode/LEFT_PARENTHESIS "("
   KeyCode/LESS "<"
   KeyCode/MINUS "-"
   KeyCode/MULTIPLY "*"
   KeyCode/NUMBER_SIGN "#"
   KeyCode/OPEN_BRACKET "["
   KeyCode/PERIOD "."
   KeyCode/PLUS "+"
   KeyCode/QUOTE "\""
   KeyCode/RIGHT_PARENTHESIS ")"
   KeyCode/SEMICOLON ";"
   KeyCode/SLASH "/"
   KeyCode/SPACE " "
   KeyCode/SUBTRACT "-"
   KeyCode/UNDERSCORE "_"})

(defn typable?
  "Returns true if the input is typable (on a specific OS)

  Args:
    key    either KeyCombination or KeyEvent
    os     target os keyword (:linux, :macos or :win32), defaults to host os"
  ([key-data-source]
   (typable? key-data-source (os/os)))
  ([key os]
   (let [mac (= os :macos)]
     (condp instance? key
       KeyCombination
       (let [^KeyCombination key key]
         (and (typable-truth-table
                [mac
                 (= KeyCombination$ModifierValue/DOWN (.getControl key))
                 (= KeyCombination$ModifierValue/DOWN (.getAlt key))
                 (= KeyCombination$ModifierValue/DOWN (.getMeta key))])
              (= 1 (count
                     (condp instance? key
                       KeyCodeCombination
                       (let [code (.getCode ^KeyCodeCombination key)]
                         (or (key-code->typable-char-strings code)
                             (.getName code)))

                       KeyCharacterCombination
                       (.getCharacter ^KeyCharacterCombination key))))))

       KeyEvent
       (let [^KeyEvent key key]
         (typable-truth-table
           [mac
            (.isControlDown key)
            (.isAltDown key)
            (.isMetaDown key)]))))))

(def empty
  {;; KeyCombination->#{command}
   :shortcut->commands {}
   ;; command->#{KeyCombination}
   :command->shortcuts {}
   ;; command->KeyCombination->{:type ...}
   :command->shortcut->warnings {}})

(def ^:private set-conj (fnil conj #{}))

(defn- update-removing-empty
  ([m k f v]
   (let [v (f (get m k) v)]
     (if (coll/empty? v)
       (dissoc m k)
       (assoc m k v))))
  ([m k f v1 v2 v3]
   (let [v (f (get m k) v1 v2 v3)]
     (if (coll/empty? v)
       (dissoc m k)
       (assoc m k v)))))

(defn from
  "Make a keymap based on another keymap

  Args:
    parent     the parent keymap (see [[empty]], [[default]])
    os         target os keyword (:macos, :linux or :win32), defaults to host os
    changes    map of command keyword keys and map vals with the following keys:
                 :add       set of new shortcuts (strings like \"Meta+A\") to
                            add
                 :remove    set of existing shortcuts (strings) to remove"
  ([parent changes]
   (from parent (os/os) changes))
  ([parent os changes]
   (-> (reduce-kv
         (fn [acc command {:keys [add remove]}]
           (let [acc (reduce
                       (fn [acc s]
                         (let [shortcut (KeyCombination/valueOf s)]
                           (if (-> acc :shortcut->commands (get shortcut) (contains? command))
                             (-> acc
                                 (update :shortcut->commands update-removing-empty shortcut disj command)
                                 (update :command->shortcuts update-removing-empty command disj shortcut)
                                 (update :command->shortcut->warnings update-removing-empty command dissoc shortcut)
                                 (as-> $
                                       (reduce
                                         (fn [acc warning]
                                           (case (:type warning)
                                             :conflict (update acc :command->shortcut->warnings
                                                               update-removing-empty (:command warning)
                                                               update-removing-empty shortcut
                                                               disj {:type :conflict :command command})
                                             acc))
                                         $
                                         (-> acc :command->shortcut->warnings (get command) (get shortcut)))))
                             acc)))
                       acc
                       remove)
                 acc (reduce
                       (fn [acc s]
                         (let [shortcut (KeyCombination/valueOf s)]
                           (-> acc
                               (update-in [:shortcut->commands shortcut] set-conj command)
                               (update-in [:command->shortcuts command] set-conj shortcut)
                               (cond->
                                 (= KeyCombination$ModifierValue/DOWN (.getShortcut shortcut))
                                 (update-in [:command->shortcut->warnings command shortcut] set-conj {:type :shortcut-modifier})

                                 (not= s (.getName shortcut))
                                 (update-in [:command->shortcut->warnings command shortcut] set-conj {:type :non-canonical-name})

                                 (typable? shortcut os)
                                 (update-in [:command->shortcut->warnings command shortcut] set-conj {:type :typable})

                                 (contains? (:shortcut->commands acc) shortcut)
                                 (as-> $
                                       (reduce
                                         (fn [acc conflicting-command]
                                           (-> acc
                                               (update-in [:command->shortcut->warnings conflicting-command shortcut] set-conj {:type :conflict :command command})
                                               (update-in [:command->shortcut->warnings command shortcut] set-conj {:type :conflict :command conflicting-command})))
                                         $
                                         (disj (get (:shortcut->commands acc) shortcut) command)))))))
                       acc
                       add)]
             acc))
         parent
         changes))))

(def ^:private ^{:arglists '([os])} default-keymap
  (fn/memoize
    (fn [os]
      (from
        empty
        os
        (-> {}
            (util/group-into #{} #(% 1) #(% 0) (platform->default-key-bindings os))
            (update-vals (fn [s] {:add s})))))))

(defn default
  "Returns default keymap (for a given os)"
  ([]
   (default (os/os)))
  ([os]
   (default-keymap os)))

(defn from-prefs
  "Returns a keymap with changes taken from prefs"
  ([prefs]
   (from-prefs @prefs/global-state prefs))
  ([prefs-state prefs]
   (let [os (os/os)]
     (from (default-keymap os) os (prefs/get prefs-state prefs [:window :keymap])))))

(defn warnings
  ([keymap]
   (:command->shortcut->warnings keymap))
  ([keymap command]
   (get (warnings keymap) command))
  ([keymap command shortcut]
   {:pre [(or (string? shortcut) (instance? KeyCombination shortcut))]}
   (let [shortcut (if (string? shortcut) (KeyCombination/valueOf shortcut) shortcut)]
     (get (warnings keymap command) shortcut))))

(defn shortcuts
  "Returns a non-empty set of KeyCombination shortcuts for a command (or nil)"
  [keymap command]
  (get (:command->shortcuts keymap) command))

(defn commands
  "Returns a non-empty set of commands or nil

  When provided a shortcut, returns commands or nil for the shortcut"
  ([keymap]
   (coll/not-empty (into #{} (keys (:command->shortcuts keymap)))))
  ([keymap shortcut]
   {:pre [(or (string? shortcut) (instance? KeyCombination shortcut))]}
   (let [shortcut (if (string? shortcut) (KeyCombination/valueOf shortcut) shortcut)]
     (get (:shortcut->commands keymap) shortcut))))

(defn display-text
  "Return a display text string for one of its shortcuts (or not-found value)"
  [keymap command not-found]
  (if-let [s (shortcuts keymap command)]
    (.getDisplayText ^KeyCombination (first s))
    not-found))

(defn install!
  "Install a keymap on a scene, replacing any predefined accelerators"
  [keymap ^Scene scene execute-fn]
  (when-let [old-handler (.get (.getProperties scene) ::keymap)]
    (.removeEventHandler scene KeyEvent/KEY_PRESSED old-handler))
  (let [{:keys [shortcut->commands]} keymap
        new-handler (reify EventHandler
                      (handle [_ e]
                        (reduce-kv
                          (fn [_ ^KeyCombination shortcut commands]
                            (when (.match shortcut e)
                              (.consume e)
                              (reduced (execute-fn commands))))
                          nil
                          shortcut->commands)))]
    (.put (.getProperties scene) ::keymap new-handler)
    (.addEventHandler scene KeyEvent/KEY_PRESSED new-handler))
  nil)

(defn shortcut-display-text
  "Default display text of a shortcut"
  [^KeyCombination shortcut]
  (.getDisplayText shortcut))

(defn shortcut-distinct-display-text
  "Display text for a shortcut that takes shortcut type into account"
  ([shortcut]
   (shortcut-distinct-display-text shortcut (os/os)))
  ([^KeyCombination shortcut os]
   (condp instance? shortcut
     KeyCodeCombination (.getDisplayText shortcut)
     KeyCharacterCombination
     (let [sb (StringBuilder.)
           control (= KeyCombination$ModifierValue/DOWN (.getControl shortcut))
           alt (= KeyCombination$ModifierValue/DOWN (.getAlt shortcut))
           shift (= KeyCombination$ModifierValue/DOWN (.getShift shortcut))
           meta (= KeyCombination$ModifierValue/DOWN (.getMeta shortcut))
           shortcut-modifier (= KeyCombination$ModifierValue/DOWN (.getShortcut shortcut))]
       (if (= os :macos)
         (cond-> sb
                 control (.append "⌃")
                 alt (.append "⌥")
                 shift (.append "⇧")
                 (or meta shortcut-modifier) (.append "⌘"))
         (cond-> sb
                 (or control shortcut-modifier) (.append "Ctrl+")
                 alt (.append "Alt+")
                 shift (.append "Shift+")
                 meta (.append "Meta+")))
       (let [char-str (.getCharacter ^KeyCharacterCombination shortcut)]
         (if (= "'" char-str)
           (.append sb "\"'\"")
           (.append sb (str "'" char-str "'"))))
       (.toString sb)))))

(defn shortcut-filterable-text
  "User-typable shortcut text, uses Cmd/Win instead of Meta depending on the os"
  ([^KeyCombination shortcut]
   (shortcut-filterable-text shortcut (os/os)))
  ([^KeyCombination shortcut os]
   (let [sb (StringBuilder.)
         control (= KeyCombination$ModifierValue/DOWN (.getControl shortcut))
         alt (= KeyCombination$ModifierValue/DOWN (.getAlt shortcut))
         shift (= KeyCombination$ModifierValue/DOWN (.getShift shortcut))
         meta (= KeyCombination$ModifierValue/DOWN (.getMeta shortcut))
         shortcut-modifier (= KeyCombination$ModifierValue/DOWN (.getShortcut shortcut))]
     ;; See https://github.com/microsoft/vscode/blob/7ec68793e7d971def8c2ea3f669b8f85ebc726a4/src/vs/base/common/keybindingLabels.ts#L130-L152
     (case os
       :macos (cond-> sb
                      control (.append "Ctrl+")
                      alt (.append "Alt+")
                      shift (.append "Shift+")
                      (or meta shortcut-modifier) (.append "Cmd+"))
       :win32 (cond-> sb
                      (or control shortcut-modifier) (.append "Ctrl+")
                      alt (.append "Alt+")
                      shift (.append "Shift+")
                      meta (.append "Win+"))
       :linux (cond-> sb
                      (or control shortcut-modifier) (.append "Ctrl+")
                      alt (.append "Alt+")
                      shift (.append "Shift+")
                      meta (.append "Meta+")))
     (condp instance? shortcut
       KeyCharacterCombination
       (.append sb (.getCharacter ^KeyCharacterCombination shortcut))

       KeyCodeCombination
       (let [code (.getCode ^KeyCodeCombination shortcut)]
         (.append sb (or (when (= KeyCode/SPACE code) (.getName code))
                         (key-code->typable-char-strings code)
                         (.getName code)))))
     (.toString sb))))

(defn shortcut-key-codes
  "Returns a set of KeyCodes for a command's shortcuts, ignoring modifiers"
  [shortcuts]
  (coll/into-> shortcuts #{}
    (filter #(instance? KeyCodeCombination %))
    (map #(.getCode ^KeyCodeCombination %))))
