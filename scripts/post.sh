
echo "\nHTTP POST /image unsigned-image0.bin\n"

make sign
curl -X POST \
  --data-binary @./dist/unsigned-image0.bin \
  -H "Content-Type: application/octet-stream" \
  http://192.168.0.140:3000/image
