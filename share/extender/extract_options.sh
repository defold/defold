# Use this to extract compiler flags from a text file

FLAGS=$(grep '^-' $1 | grep -v '#')


echo -n "allowedFlags: ["
for f in $FLAGS
do
	echo -n "\"$f\","
done

echo "]"
