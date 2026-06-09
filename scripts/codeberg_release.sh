#!/bin/bash
set -e

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <version> <path_to_notes.md> <path_to_firmware.bin>"
    echo "Example: $0 v1.7.0 release_notes.md .pio/build/dottie_esp32s3/firmware.bin"
    exit 1
fi

VERSION=$1
NOTES_FILE=$2
FIRMWARE_FILE=$3

TOKEN_FILE="$HOME/.config/tea/config.yml"
if [ ! -f "$TOKEN_FILE" ]; then
    echo "Fehler: $TOKEN_FILE nicht gefunden. Gitea/Codeberg-Token wird benötigt."
    exit 1
fi

# Simples Auslesen des Tokens aus der yml
TOKEN=$(grep "token:" "$TOKEN_FILE" | head -n 1 | awk '{print $2}')

if [ -z "$TOKEN" ]; then
    echo "Fehler: Kein Token in $TOKEN_FILE gefunden."
    exit 1
fi

jq -n --arg name "$VERSION" --arg tag "$VERSION" --rawfile body "$NOTES_FILE" '{"name": $name, "tag_name": $tag, "body": $body}' > /tmp/release_payload_$$.json

echo "Erstelle Release via Codeberg API ($VERSION)..."
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "https://codeberg.org/api/v1/repos/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition/releases" \
  -H "Authorization: token $TOKEN" \
  -H "Content-Type: application/json" \
  -d @/tmp/release_payload_$$.json)

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | sed '$d')

if [ "$HTTP_CODE" != "201" ]; then
  if [ "$HTTP_CODE" == "409" ]; then
    echo "Release existiert bereits, suche ID für Upload..."
    REL_ID=$(curl -s -X GET "https://codeberg.org/api/v1/repos/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition/releases/tags/$VERSION" -H "Authorization: token $TOKEN" | jq -r .id)
  else
    echo "Fehler: HTTP Code $HTTP_CODE"
    echo "$BODY"
    rm -f /tmp/release_payload_$$.json
    exit 1
  fi
else
  REL_ID=$(echo "$BODY" | jq -r .id)
fi

echo "Lade Firmware-Asset hoch..."
curl -s -X POST "https://codeberg.org/api/v1/repos/diwou/Dottie_-_The_Matrix_Clock_Extended_Edition/releases/$REL_ID/assets?name=$(basename $FIRMWARE_FILE)" \
  -H "Authorization: token $TOKEN" \
  -F "attachment=@$FIRMWARE_FILE"

rm -f /tmp/release_payload_$$.json
echo ""
echo "Upload für $VERSION erfolgreich abgeschlossen."
