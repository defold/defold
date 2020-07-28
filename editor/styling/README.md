# Setup
To build the editor css, gulp is used in combination with SASS to compile the CSS file from many smaller files.

1. First run `npm install` in the /styling folder
2. To build the CSS file, either run `gulp` to build it once or `gulp watch` to watch for changes within the .scss files beneath the folder stylesheets and build it on every save.

# Editor styling
The old CSS has been divided into multiple files and grouped into `base`, `mixins`, `components` and `modules`.
* Base are things like palette and typography.
* Mixins contain SASS mixins.
* Components are JavaFX componenents that are reused on several places within the Editor
* Modules are specific parts of the editor that need to override default component behaviour