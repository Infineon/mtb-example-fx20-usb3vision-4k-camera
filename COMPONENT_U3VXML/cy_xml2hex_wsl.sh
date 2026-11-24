#!/bin/bash
# Steps to generate xml to c source file
# 1. Install WSL environment in windows:
#   - Open PowerShell in administrator mode,
#   - Run: $wsl --install
# 2. Open WSL
#   - Install 'dos2unix' and 'zip' pakages in WSL
#     - sudo apt install zip
#     - sudo apt install dos2unix
#
# 3. Navigate to the folder contains *.xml file
#   - Ex: C drive navigation in WSL: cd /mnt/c
# 4: convert *.sh file as unix executable
#   - Run: $sudo dos2unix cy_xml2hex.sh
# 5. Run script to convert *.xml file into source array
#   - Run: $./cy_xml2hex.sh cy_u3v.xml
# OUTPUT C Source code:
#   - Folder named with DATE. 
#     - Folder has copy of xml file
#     - a C source file which contains an hex array, with the SHA1 sum and length
#     - a *.zip file
# Actions:
#   - Copy generated hex array and sha1 from C source file to project source file "cy_u3v_xml.c"
#   - Copy generated hex array length from C source file to project source file "cy_u3v_xml.h"

DATE=$(date "+%d%b%y")
DATETIME=$(date "+%d%b%y_%H%M%S")
XML_PATH=$(wslpath $1)
XML_NAME=$(basename $XML_PATH) #extract file name from path
OUTPUT_DIRECTORY=$DATE"_xmlgen"
ZIP_FILENAME=$XML_NAME".zip"
OUTPUT_C_FILENAME=$XML_NAME"_"$DATETIME".c"

#Exit if any command fails
set -e

# Keep track of the last executed command
trap 'last_command=$current_command; current_command=$BASH_COMMAND' DEBUG
# Echo an error message before exiting
trap 'catch $? $LINENO' EXIT

catch() {
  if [ "$1" != "0" ]; then
    # error handling goes here
    echo -e "\033[0;31m ${last_command} command failed on line $2 \033[0m"
  fi
}

if grep -qEi "(Microsoft|WSL)" /proc/version &> /dev/null ; then
    echo "WSL found. Continuing..."
else
    echo "Script needs WSL to be installed. Exiting.."
    exit 1
fi

echo "*************************************"
echo "  Convert XML to hex for U3V Camera"
echo "*************************************"

echo "Copying input xml file at $XML_PATH to $OUTPUT_DIRECTORY"
mkdir -p $OUTPUT_DIRECTORY #no error if already existing
cp -v $XML_PATH $OUTPUT_DIRECTORY/$XML_NAME

cd $OUTPUT_DIRECTORY
zip $ZIP_FILENAME $XML_NAME
xxd -i $ZIP_FILENAME > $OUTPUT_C_FILENAME
echo "/*-----SHA1SUM-----*/" >> $OUTPUT_C_FILENAME
sha1sum $ZIP_FILENAME >> $OUTPUT_C_FILENAME
echo  >> $OUTPUT_C_FILENAME
sha1sum $ZIP_FILENAME | awk -v FS="" '{printf("#define U3V_XML_SHA1_HASH ")}{for (i=1;i<41;i=i+2){printf("%s","0x"$i$(i+1)",")}}' >> $OUTPUT_C_FILENAME
echo "Num of bytes in sha1sum (Valid count:20) =" 
sha1sum $ZIP_FILENAME | awk -v FS="" '{for (i=1;i<41;i=i+2){print("%s","0x"$i$(i+1)",")}}'| wc --lines 

echo "*************************************"
echo "                DONE!"
echo "*************************************"