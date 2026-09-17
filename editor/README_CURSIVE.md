## Installing Cursive

1. Download and install **IntelliJ IDEA Community Edition** from here: https://www.jetbrains.com/idea/download.

2. Start IntelliJ and customize it to your liking. You might want to disable some of the plugins that you know for sure you will not use.

3. On the Welcome Screen, select **Plugins** from the menu on the right. You can also reach the plugin manager from the **Preferences** window within IntelliJ.

4. In the plugin manager, select the **Marketplace** tab and type "Cursive" in the search box. Install Cursive using the green **Install** button. Restart IntelliJ when it prompts you to.

## Configuring Cursive preferences

Once the IntelliJ restarts, we should make some changes to the Cursive preferences to make it easier to work with. You are not required to change these preferences, but we recommend you do, especially if this is your first time using Clojure.

On the Welcome Screen, select **Customize** from the menu on the right and click the **All settings...** link at the bottom. Inside the **Preferences** window that appears, navigate to the **Languages & Frameworks > Clojure** page.

1. Enable **Rainbow parentheses**. This colors parentheses and brackets in the editor so it will be easier to distinguish between nested scopes.

2. Disable **Load out-of-date file dependencies transitively**. By default, Cursive will automatically reload any edited files that are referenced from the file you're reloading. This can cause type errors if a referenced file re-defines a Java class. It is better to manually refresh only the changed forms, because once you trigger a type error the editor needs to be restarted to get it back on track.

3. Disable **Add forms sent from the editor to REPL history**. It is preferable to have the REPL history only contain the evaluations you sent from the REPL window when experimenting.

## Setting up the Defold Editor project

Time to open the Defold Editor project. The first time you open it, you'll need to configure some things.

1. Choose **Open** from the Welcome screen and select the `defold/editor` directory. Choose to trust the project when prompted.

2. Open the **Project** sidebar on the left of the IDE window and locate the `project.clj` file at root level. Right-click it and select **Add as Leiningen Project** from the context menu. The "Stub Generation Required" notification that might appear can be safely ignored.

3. You need to choose the target Java SDK for the project. Choose **File > Project Structure** from the menu bar and go to the **Project Settings > Project** page.

4. Under the Project SDK section, choose Java version **25** from the dropdown. If that's not available from the dropdown, you'll need to tell IntelliJ where to find it. In that case you must browse for the JDK folder by selecting **Add JDK...** from the dropdown.

5. Go to the **Project Settings > Modules** page and navigate to the Clojure tab. Uncheck the **Create stubs if required** checkbox. This gets rid of the "Stub Generation Required" notification whenever you reopen the project. Click **OK** to close the Project settings dialog when you're done.

6. Select **Edit Configurations...** from the **Run Configuration** dropdown in the toolbar (this likely says "Current File" before you change it) of the main IntelliJ window to open the Run/Debug Configurations dialog.

7. Click the plus button in the top left of the dialog and select **Clojure REPL > Remote**. Name the configuration `connect repl` and select **nREPL** for the **Connection type**. Under **Connection details**, select **Use port from REPL file** and select the `defold-editor` project from the **Project** dropdown. Make sure **Use standard port file** is selected, then click **OK** to close the dialog.

8. From a command-line prompt, `cd` into the `defold/editor` directory and type `lein run` to start the editor. Once you reach the Welcome window, run the `connect repl` configuration from the IntelliJ toolbar to connect to the running process. From here, you can evaluate expressions, redefine functions, run tests, and interact with the running editor from the REPL window.

9. Alternatively, you can type `lein repl` to quickly start a process you can connect to where you're able to run tests and play around with the code without starting the editor. Type `exit` to terminate it from the command-line.

## Recommended keyboard shortcuts

The defaults for these might depend on the keymap you selected when installing IntelliJ, but here are some keyboard shortcuts you might find useful when working with Clojure. You can customize the bindings for these from the **Keymap** page in the Preferences dialog.

#### Add Selection for Next Occurrence
Suggested shortcut: **Cmd+D**. Adds the word around the cursor to the selection, or adds the next occurrence of the selected word to the selection.

#### Extend Selection
Suggested shortcut: **Cmd+E**. Extends the selection to encompass the entire form directly after or around the cursor. This is very useful when moving code around, or when you want to insert a form directly after another.

#### Auto-Indent Lines
Suggested shortcut: **Cmd+Alt+I**. Fixes the indentation on the selected lines.

#### Load file in REPL
Suggested shortcut: **Cmd+R** followed by **F**. Reloads the entire file in the connected REPL.

#### Send top form to REPL
Suggested shortcut: **Cmd+R** followed by **D**. Finds the outermost form around the cursor and sends it to the connected REPL. Typically used to redefine a function or similar at runtime.

#### Switch REPL NS to current file
Suggested shortcut: **Cmd+R** followed by **S**. Changes the REPL prompt to evaluate in the namespace of the current file. Useful when you want to test functions or evaluate expressions in the context of the current file.

#### Run test under caret in REPL
Suggested shortcut: **Cmd+R** followed by **T**. Runs the test under the cursor and flags it with the result directly in the editor.

#### Run tests in current NS in REPL
Suggested shortcut: **Cmd+R** followed by **Cmd+T**. Runs all the tests in the file in the same way.

#### Clear all Test markers
Suggested shortcut: **Cmd+R** followed by **C**. Clears inline results from tests. Useful when you have lots of failing tests cluttering up the editor.

#### Interrupt Current Evaluation
Suggested shortcut: **Cmd+R** followed by **Cmd+C**. Cancels the last long-running REPL evaluation, so you don't need to restart the editor in case you write a non-terminating loop or something.
