#!/bin/bash
set -euo pipefail

Lunar="${1:-./build/src/lunar/lunar}"
Work="${RUNNER_TEMP:-/tmp}/okra-sign"
rm -rf "$Work"
mkdir -p "$Work"

Failures=0

Report() {
	if [ "$1" = "ok" ]; then
		echo "PASS $2"
	else
		echo "FAIL $2"
		Failures=$((Failures + 1))
	fi
}

PublicOf() {
	sed -n 's/^key=//p' "$1" | head -1
}

Ring="$Work/keyring.pub"
MasterKey="$Work/master.key"
MasterPub="$Work/master.pub"
DevKey="$Work/dev.key"
DevPub="$Work/dev.pub"
Cert="$Work/dev.cert"
Payload="$Work/payload.txt"
Sig="$Work/payload.txt.sig"

"$Lunar" key generate --kind private --out "$MasterKey" > /dev/null
"$Lunar" key generate --kind public --out "$MasterPub" > /dev/null
MasterId="$("$Lunar" key id --key "$MasterPub" 2>/dev/null || echo master)"

{
	echo "keyid: $MasterId"
	echo "role: master"
	echo "name: okra-master"
	echo "public: $(PublicOf "$MasterPub")"
	echo "issued: 2026-01-01"
	echo "expires:"
	echo "certifier:"
	echo "signature:"
} > "$Work/master.cert"

Out="$("$Lunar" keyring add --keyring "$Ring" --cert "$Work/master.cert" 2>&1 || true)"
if [ -f "$Ring" ] && grep -q "okra-master" "$Ring"; then
	Report ok "master key added to keyring"
else
	Report fail "master key added to keyring"
	echo "$Out"
fi

"$Lunar" key generate --kind private --out "$DevKey" > /dev/null
"$Lunar" key generate --kind public --out "$DevPub" > /dev/null

Out="$("$Lunar" key certify --master "$MasterKey" --developer "$DevPub" --name alice --out "$Cert" 2>&1 || true)"
if [ -f "$Cert" ] && grep -q "signature:" "$Cert"; then
	Report ok "developer certificate created"
else
	Report fail "developer certificate created"
	echo "$Out"
fi

sed -i "s/^certifier:.*/certifier: $MasterId/" "$Cert"
"$Lunar" keyring add --keyring "$Ring" --cert "$Cert" > /dev/null 2>&1 || true
if grep -q "alice" "$Ring"; then
	Report ok "certified developer accepted"
else
	Report fail "certified developer accepted"
fi

"$Lunar" key generate --kind private --out "$Work/rogue.key" > /dev/null
"$Lunar" key generate --kind public --out "$Work/rogue.pub" > /dev/null
{
	echo "keyid: rogueid"
	echo "role: developer"
	echo "name: mallory"
	echo "public: $(PublicOf "$Work/rogue.pub")"
	echo "issued: 2026-01-01"
	echo "expires:"
	echo "certifier: $MasterId"
	echo "signature: AAAA"
} > "$Work/rogue.cert"
Out="$("$Lunar" keyring add --keyring "$Ring" --cert "$Work/rogue.cert" 2>&1 || true)"
if echo "$Out" | grep -qi "not signed by any known master"; then
	Report ok "uncertified developer rejected"
else
	Report fail "uncertified developer rejected"
	echo "$Out"
fi

printf 'okra release payload\n' > "$Payload"
Out="$("$Lunar" key sign --key "$DevKey" --file "$Payload" --out "$Sig" 2>&1 || true)"
if [ -f "$Sig" ] && grep -q "signature:" "$Sig"; then
	Report ok "file signed"
else
	Report fail "file signed"
	echo "$Out"
fi

Out="$("$Lunar" key verify --keyring "$Ring" --file "$Payload" --sig "$Sig" 2>&1 || true)"
if echo "$Out" | grep -qi "signature valid"; then
	Report ok "valid signature accepted"
else
	Report fail "valid signature accepted"
	echo "$Out"
fi

printf 'tampered\n' > "$Payload"
Out="$("$Lunar" key verify --keyring "$Ring" --file "$Payload" --sig "$Sig" 2>&1 || true)"
if echo "$Out" | grep -qi "verification failed"; then
	Report ok "tampered file rejected"
else
	Report fail "tampered file rejected"
fi

printf 'okra release payload\n' > "$Payload"
sed 's/^signature:.*/signature: AAAA/' "$Sig" > "$Work/forged.sig"
Out="$("$Lunar" key verify --keyring "$Ring" --file "$Payload" --sig "$Work/forged.sig" 2>&1 || true)"
if echo "$Out" | grep -qi "verification failed"; then
	Report ok "forged signature rejected"
else
	Report fail "forged signature rejected"
fi

"$Lunar" key generate --kind private --out "$Work/other.key" > /dev/null
"$Lunar" key sign --key "$Work/other.key" --file "$Payload" --out "$Work/other.sig" > /dev/null 2>&1 || true
Out="$("$Lunar" key verify --keyring "$Ring" --file "$Payload" --sig "$Work/other.sig" 2>&1 || true)"
if echo "$Out" | grep -qi "verification failed"; then
	Report ok "unknown signer rejected"
else
	Report fail "unknown signer rejected"
fi

echo
if [ "$Failures" -gt 0 ]; then
	echo "$Failures checks failed"
	exit 1
fi
echo "all signing checks passed"
