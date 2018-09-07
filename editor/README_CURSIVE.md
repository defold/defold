## Installing Cursive

1. Download and install **IntelliJ IDEA Community Edition** from here: https://www.jetbrains.com/idea/download.

2. Start IntelliJ and customize it to your liking. You might want to disable some of the plugins that you know for sure you will not use.

3. On the Welcome Screen, click the **Configure** button and choose **Plugins** from the dropdown menu that appears. You can also reach the Plugins dialog from **Settings > Plugins** within IntelliJ.

4. In the plugins window, press **Browse Repositories...** and search for "Cursive". Install Cursive using the green install button, then close the Browse Repositories window and the Settings window. Restart IntelliJ when it prompts you to.

## Configuring Cursive preferences

Once the IntelliJ restarts, we should make some changes to the Cursive preferences to make it easier to work with. On the Welcome Screen, click the **Configure** button and choose **Plugins** from the dropdown menu and select the **Languages & Frameworks > Clojure** page.

1. Enable **Rainbow parentheses**. This colors parentheses and brackets in the editor so it will be easier to distinguish between nested scopes.

2. Disable **Highlight unresolved symbols**. For some reason Cursive is not able to resolve all the symbols in the editor project. When enabled, this clutters up the editor with a lot of noisy highlighting that doesn't really help.

3. Disable **Load out-of-date file dependencies transitively**. By default, Cursive will automatically reload any edited files that are referenced from the file you're reloading. This can cause type errors if a referenced file re-defines a Java class. It is better to manually refresh only the changed forms, because once you trigger a type error the editor needs to be restarted to get it back on track.

4. Disable **Add forms send from the editor to REPL history**. It is preferable to have the REPL history only contain the evaluations you sent from the REPL window when experimenting.

## Setting up the Defold Editor project

Time to open the Defold Editor project. The first time you open it, you'll need to configure some things.

1. Choose **Open** from the Welcome screen and select the `defold/editor` directory. Choose **Open as Project** when prompted.

2. You need to choose the target Java SDK for the project. Choose **File > Project Structure** from the menu bar and go to the **Project Settings > Project** page.

3. Under the Project SDK section, choose Java version **1.8.0_102** from the dropdown. If that's not available from the dropdown, you'll need to tell IntelliJ where to find it. In that case you must browse for the JDK folder by clicking the **New...** button. Close the Project settings dialog when you're done.

4. Select **Edit Configurations...** from the dropdown in the toolbar of the main IntelliJ window to open the Run/Debug Configurations dialog.

5. First we'll create a configuration that compiles and runs the Defold editor. Click the plus button in the top left of the dialog and select **Leiningen**. Name the configuration `lein run`. Type `-o run` in the Arguments field. The `-o` flag allows Leiningen to start the project in offline mode. Otherwise it will fail to start while updating dependencies in case there is no internet connection. If you want to use the debugger, check **Use trampoline**, but be aware that if a fatal error occurs during startup, you'll need to uncheck it to see the error.

6. Next, we'll create a configuration the connects a REPL to the running editor. You'll select and run this configuration after you've started the editor to connect to it. Click the plus button in the top left of the dialog and select **Clojure REPL > Remote**. Name the configuration `connect repl`. Select **Use Leiningen REPL port** and uncheck **Activate tool window** to prevent the Output panel from opening when we run this. The REPL view will capture standard output, so there is no need for it.

7. Optionally you can also create a configuration with a standalone REPL. This lets you run code and tests without starting the editor, and is sometimes useful when fixing failing unit tests. Click the plus button in the top left of the dialog and select **Clojure REPL > Local**. Name the configuration `standalone repl`. Select **Run nREPL with Leiningen** and uncheck **Activate tool window** for this one as well.

8. Press **OK** to close the dialog. You should now be able to select the `lein run` configuration and launch the editor from the toolbar. Once you see the Defold Welcome screen, you can select `connect repl` and run that task to connect to the running editor. From the REPL you can evaluate code, redefine functions, or experiment with function calls while the editor is running.

## Recommended keyboard shortcuts

The defaults for these might depend on the keymap you selected when installing IntelliJ, but here are some keyboard shortcuts you might find useful when working with Clojure. You can customize the bindings for these from the **Keymap** page in the Preferences dialog.

#### Add Selection for Next Occurrence
Suggested shortcut: **Cmd+D**. Adds the word around the cursor to the selection, or adds the next occurrence of the selected word to the selection.

#### Extend Selection
Suggested shortcut: **Cmd+W**. Extends the selection to encompass the entire form directly after or around the cursor. This is very useful when moving code around, or when you want to insert a form directly after another.

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
