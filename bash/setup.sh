usage()
{
    echo "usage: -s <int> | source directory"
}

if [ "$1" = "-h" ] || [ $# = 0 ]; then
    usage
    exit 1
fi

while getopts s: flag
do
    case "${flag}" in
        s) source=${OPTARG};;
    esac
done

SOURCE_DIR=$source
ENGINE_NAME="sayuri"

cp "$SOURCE_DIR/build/$ENGINE_NAME" "."
cp -r "$SOURCE_DIR/train/torch" "."
