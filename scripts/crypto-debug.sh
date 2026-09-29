#!/bin/bash
set -euo pipefail
Lunar="${1:-./build/src/lunar/lunar}"
Work="${RUNNER_TEMP:-/tmp}/okra-debug"
rm -rf "$Work"
mkdir -p "$Work"

"$Lunar" key generate --private "$Work/m.key" --public "$Work/m.pub" > /dev/null
"$Lunar" key generate --private "$Work/d.key" --public "$Work/d.pub" > /dev/null

MID="$("$Lunar" key id --key "$Work/m.pub" 2>/dev/null)"
DID="$("$Lunar" key id --key "$Work/d.pub" 2>/dev/null)"
echo "MID=$MID"
echo "DID=$DID"
echo "MPUB=$(sed -n 's/^key=//p' "$Work/m.pub")"
echo "DPUB=$(sed -n 's/^key=//p' "$Work/d.pub")"

cat > "$Work/m.cert" <<CERT
keyid: $MID
role: master
name: okra-master
public: $(sed -n 's/^key=//p' "$Work/m.pub")
issued: 2026-01-01
expires:
certifier:
signature:
CERT

"$Lunar" keyring add --keyring "$Work/ring" --cert "$Work/m.cert" 2>&1 || true
echo "RING after master:"
cat "$Work/ring" 2>/dev/null || echo "(none)"

"$Lunar" key certify --master "$Work/m.key" --developer "$Work/d.pub" --name alice --out "$Work/d.cert" 2>&1 || true
echo "CERT:"
cat "$Work/d.cert" 2>/dev/null

sed -i "s/^certifier:.*/certifier: $MID/" "$Work/d.cert"
Out="$("$Lunar" keyring add --keyring "$Work/ring" --cert "$Work/d.cert" 2>&1 || true)"
echo "ADD DEV OUTPUT: $Out"
echo "RING after dev:"
cat "$Work/ring" 2>/dev/null || echo "(none)"

echo "payload" > "$Work/test.txt"
"$Lunar" key sign --key "$Work/d.key" --file "$Work/test.txt" --out "$Work/test.sig" 2>&1 || true
echo "SIG:"
cat "$Work/test.sig" 2>/dev/null
echo "VERIFY:"
"$Lunar" key verify --keyring "$Work/ring" --file "$Work/test.txt" --sig "$Work/test.sig" 2>&1 || true