#!/bin/bash
set -euo pipefail

Lunar="${1:-./build/src/lunar/lunar}"
Work="${RUNNER_TEMP:-/tmp}/arch-gate"
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

MakeArtifact() {
	local Name="$1"
	local Arch="$2"
	local Abi="$3"
	local Dir="$Work/pkg-$Name"
	rm -rf "$Dir"
	mkdir -p "$Dir/rootfs/usr/bin"
	printf '#!/bin/sh\necho %s\n' "$Name" > "$Dir/rootfs/usr/bin/$Name"
	chmod +x "$Dir/rootfs/usr/bin/$Name"
	{
		echo "name: $Name"
		echo "namespace: test"
		echo "version: 1.0"
		echo "description: \"arch gate fixture\""
		echo "architecture: $Arch"
		[ -n "$Abi" ] && echo "abi: $Abi"
		echo "installed_size: 1"
		echo "dependencies: []"
		echo "files:"
		echo "  - /usr/bin/$Name"
	} > "$Dir/meta.yaml"
	tar --zstd -cf "$Work/$Name.oaa" -C "$Dir" meta.yaml rootfs
	sha256sum "$Work/$Name.oaa" | awk '{print $1"  "$2}' > "$Work/$Name.oaa.sha256"
	echo "$Work/$Name.oaa"
}

RunInstall() {
	local Archive="$1"
	shift
	LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" \
		"$Lunar" "$@" install "$Archive" 2>&1 || true
}

echo "== fixture: matching architecture"
MatchArchive="$(MakeArtifact matched x86_64 OAABI1)"
Out="$(RunInstall "$MatchArchive")"
if echo "$Out" | grep -qi "architecture mismatch\|ABI mismatch"; then
	Report fail "matching architecture is accepted"
else
	Report ok "matching architecture is accepted"
fi

echo "== fixture: wrong architecture"
WrongArchive="$(MakeArtifact wrongarch aarch64 OAABI1)"
Out="$(RunInstall "$WrongArchive")"
if echo "$Out" | grep -qi "architecture mismatch"; then
	Report ok "wrong architecture is rejected"
else
	Report fail "wrong architecture is rejected"
	echo "$Out"
fi

echo "== fixture: wrong ABI"
WrongAbi="$(MakeArtifact wrongabi x86_64 OAABI9)"
Out="$(RunInstall "$WrongAbi")"
if echo "$Out" | grep -qi "ABI mismatch"; then
	Report ok "wrong ABI is rejected"
else
	Report fail "wrong ABI is rejected"
	echo "$Out"
fi

echo "== fixture: no architecture declared"
AnyArchive="$(MakeArtifact anyarch "" "")"
Out="$(RunInstall "$AnyArchive")"
if echo "$Out" | grep -qi "mismatch"; then
	Report fail "absent architecture is accepted"
else
	Report ok "absent architecture is accepted"
fi

echo "== fixture: override target architecture"
Out="$(RunInstall "$WrongArchive" --arch aarch64)"
if echo "$Out" | grep -qi "architecture mismatch"; then
	Report fail "target architecture override works"
else
	Report ok "target architecture override works"
fi

echo "== fixture: force platform bypass"
Out="$(RunInstall "$WrongArchive" --force-platform)"
if echo "$Out" | grep -qi "architecture mismatch"; then
	Report fail "force platform bypass works"
else
	Report ok "force platform bypass works"
fi

echo
if [ "$Failures" -gt 0 ]; then
	echo "$Failures checks failed"
	exit 1
fi
echo "all architecture gate checks passed"