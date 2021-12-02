# Installation

## Visual Studio Code

Although there are other tools out there like [Cursive](README_CURSIVE.md)
we recommend using `Visual Studio Code` and its plugin `Calva` since both tools are free.


Install the latest version of [VSCode](https://code.visualstudio.com/download)


## Calva plugin

The `Calva` plugin is an addon for VSCode that will add Clojure support to the IDE.

Select "View -> Extensions" and search for `Calva`


# Setup

_Make sure you have the editor [built and setup](README_BUILD.md) at least once._

* Open the `Visual Studio Code` app in the `defold/editor` folder
* Open the command palette (<kbd>CMD+SHIFT+P</kbd>kbd>) in VSCode
* Choose `Calva: Start a project REPL and Connect (aka Jack-in`
* Choose `Leiningen`
* Choose `No alias`
* In "Pick any profiles to launch with", choose `dev` and `local-repl`

The editor should now launch with the project selection dialog
