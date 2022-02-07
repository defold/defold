
BASIS_DIR=$1

if [ -z $BASIS_DIR ]; then
    echo "You must pass in the directory where to find the basis_universal repo"
    exit 1
fi

# from the script dir

cp -v -r $BASIS_DIR/encoder/ ./encoder
cp -v -r $BASIS_DIR/transcoder/ ./transcoder

git apply -v defold.patch