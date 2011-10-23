export PATH=~/local/bin:$PATH

mkdir -p war/site
asciidoc -a data-uri -o war/site/tutorial_falling_box.html site/tutorial_falling_box.txt
asciidoc -a data-uri -o war/site/tutorial02.html site/tutorial02.txt
asciidoc -a data-uri -o war/site/blog.html site/blog.txt
