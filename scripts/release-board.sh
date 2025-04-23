
echo "\nRelease factory build for 'esp32s3_devkitc-esp32s3-procpu'"

python3 scripts/util_binary.py -i dist/bootloader.bin -m dist/signed_image.bin -o dist/bootimage.bin
curl -X POST \
  --data-binary @./dist/bootimage.bin \
  -H "Content-Type: application/octet-stream" \
  -H "Authorization: 12345" \
  http://192.168.0.140:3000/factory/image/esp32s3_devkitc-esp32s3-procpu
