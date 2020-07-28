// Load Gulp Plugins
var gulp = require('gulp'),
		sass = require('gulp-sass'),
		rename = require('gulp-rename');

// Styles
gulp.task('styles', function() {
	return gulp.src('stylesheets/main.sass')
		.pipe(sass({style: 'expanded', quiet: true, cacheLocation: '.sass-cache'}))
		.pipe(rename('editor.css'))
		.pipe(gulp.dest('../resources'))
});

// Default task
gulp.task('default', function() {
	gulp.start('styles');
});

// Watch
gulp.task('watch', function() {
	// Watch .scss files
	gulp.watch('stylesheets/**/*', ['styles']);
});