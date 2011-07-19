export PATH=~/local/bin:$PATH

mkdir -p war/site
asciidoc -a data-uri -o war/site/tutorial01.html site/tutorial01.txt 
asciidoc -a data-uri -o war/site/tutorial02.html site/tutorial02.txt

