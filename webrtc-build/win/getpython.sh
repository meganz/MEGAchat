set -e
if [[ -z "$1" ]]; then
    echo "Usage getpython.sh <path/to/install>"
    exit 1
fi

owndir="$1"
CHROME_INFRA_URL=https://storage.googleapis.com/chrome-infra

if [ -d "$owndir/python276_bin" ]; then
    echo Python 2.7.6 already installed
    exit 0
fi

echo Installing python 2.7.6...
PYTHON_URL=$CHROME_INFRA_URL/python276_bin.zip
if [ -f "$owndir/python276.zip" ]; then
    rm -v "$owndir/python276.zip"
fi

echo Fetching python 2.7.6 from $PYTHON_URL
wget -q "$PYTHON_URL" -O "$owndir/python276_bin.zip"
echo "Extracting python zip..."
unzip -q -d "$owndir" "$owndir/python276_bin.zip"
chmod -R a+x $owndir/python276_bin
