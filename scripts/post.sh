
echo "HTTP POST /image unsigned-image0.bin"
make clean sign
curl -X POST \
  --data-binary @/dist/unsigned-image0.bin \
  -H "Content-Type: application/octet-stream" \
  http://192.168.0.140:3000/image
