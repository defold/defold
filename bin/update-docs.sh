mkdir -p $DYNAMO_HOME/doc
mkdir -p $DYNAMO_HOME/doc/html
./bin/markdown.pl doc/README.txt > $DYNAMO_HOME/doc/html/README.html
cp -v ./doc/doxygen.h $DYNAMO_HOME/include
( cat share/Doxyfile ; echo "INPUT=$DYNAMO_HOME/include" ; echo "OUTPUT_DIRECTORY=$DYNAMO_HOME/doc" ; echo "EXTRACT_ALL=YES" ) | doxygen -
