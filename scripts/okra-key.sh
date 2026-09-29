#!/bin/bash
set -euo pipefail

Action="${1:?usage: okra-key.sh <init|new-developer|certify|sign|verify|list> [args]}"
shift

KeyringDir="${OKRA_KEYRING_DIR:-$PWD/okra-keyring}"
Keyring="$KeyringDir/keyring.pub"
Tool="${OKRA_KEY_TOOL:-}"

LunarBin() {
	if [ -n "$Tool" ]; then
		echo "$Tool"
	else
		echo "lunar"
	fi
}

EnsureDir() {
	mkdir -p "$KeyringDir/masters" "$KeyringDir/developers" "$KeyringDir/certs" "$KeyringDir/sigs"
}

CmdInit() {
	local Name="${1:?usage: okra-key.sh init <master-name>}"
	EnsureDir
	local Out="$KeyringDir/masters/$Name"
	if [ -e "$Out.key" ]; then
		echo "master key already exists: $Out.key" >&2
		exit 1
	fi
	"$(LunarBin)" key generate --kind private --out "$Out.key"
	"$(LunarBin)" key generate --kind public --out "$Out.pub"
	echo "master key written to $Out.key and $Out.pub"
	echo "store the private key offline, publish only $Out.pub"
}

CmdNewDeveloper() {
	local Name="${1:?usage: okra-key.sh new-developer <name>}"
	EnsureDir
	local Out="$KeyringDir/developers/$Name"
	if [ -e "$Out.key" ]; then
		echo "developer key already exists: $Out.key" >&2
		exit 1
	fi
	"$(LunarBin)" key generate --kind private --out "$Out.key"
	"$(LunarBin)" key generate --kind public --out "$Out.pub"
	echo "developer key written to $Out.key and $Out.pub"
}

CmdCertify() {
	local MasterName="${1:?usage: okra-key.sh certify <master> <developer>}"
	local DevName="${2:?usage: okra-key.sh certify <master> <developer>}"
	local MasterKey="$KeyringDir/masters/$MasterName.key"
	local DevPub="$KeyringDir/developers/$DevName.pub"
	[ -f "$MasterKey" ] || { echo "missing master key: $MasterKey" >&2; exit 1; }
	[ -f "$DevPub" ] || { echo "missing developer public key: $DevPub" >&2; exit 1; }
	local Cert="$KeyringDir/certs/$DevName.cert"
	"$(LunarBin)" key certify --master "$MasterKey" --developer "$DevPub" --name "$DevName" --out "$Cert"
	echo "certificate written to $Cert"
	"$(LunarBin)" keyring add --keyring "$Keyring" --cert "$Cert"
	echo "keyring updated: $Keyring"
}

CmdSign() {
	local DevName="${1:?usage: okra-key.sh sign <developer> <file>}"
	local Target="${2:?usage: okra-key.sh sign <developer> <file>}"
	local DevKey="$KeyringDir/developers/$DevName.key"
	[ -f "$DevKey" ] || { echo "missing developer key: $DevKey" >&2; exit 1; }
	"$(LunarBin)" key sign --key "$DevKey" --file "$Target" --out "$Target.sig"
	echo "signature written to $Target.sig"
}

CmdVerify() {
	local Target="${1:?usage: okra-key.sh verify <file>}"
	local Sig="$Target.sig"
	[ -f "$Sig" ] || { echo "missing signature: $Sig" >&2; exit 1; }
	"$(LunarBin)" key verify --keyring "$Keyring" --file "$Target" --sig "$Sig"
}

CmdList() {
	[ -f "$Keyring" ] || { echo "no keyring at $Keyring" >&2; exit 1; }
	"$(LunarBin)" keyring list --keyring "$Keyring"
}

case "$Action" in
	init) CmdInit "$@" ;;
	new-developer) CmdNewDeveloper "$@" ;;
	certify) CmdCertify "$@" ;;
	sign) CmdSign "$@" ;;
	verify) CmdVerify "$@" ;;
	list) CmdList "$@" ;;
	*) echo "unknown action: $Action" >&2; exit 1 ;;
esac