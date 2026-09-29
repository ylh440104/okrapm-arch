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

echo "== fixture: file ownership"
OwnArchive="$(MakeArtifact owner x86_64 OAABI1)"
RunInstall "$OwnArchive" > /dev/null
Out="$(LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" "$Lunar" which /usr/bin/owner 2>&1 || true)"
if echo "$Out" | grep -q "test.owner"; then
	Report ok "which resolves file owner"
else
	Report fail "which resolves file owner"
	echo "$Out"
fi

Out="$(LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" "$Lunar" provides /usr/bin/ 2>&1 || true)"
if echo "$Out" | grep -q "test.owner"; then
	Report ok "provides resolves by prefix"
else
	Report fail "provides resolves by prefix"
	echo "$Out"
fi

Out="$(LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" "$Lunar" files test.owner 2>&1 || true)"
if echo "$Out" | grep -q "/usr/bin/owner"; then
	Report ok "files lists recorded paths"
else
	Report fail "files lists recorded paths"
	echo "$Out"
fi

Out="$(LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" "$Lunar" check 2>&1 || true)"
if echo "$Out" | grep -qi "all recorded files present"; then
	Report ok "check passes when files present"
else
	Report fail "check passes when files present"
	echo "$Out"
fi

rm -f "$Work/root/usr/bin/owner"
Out="$(LUNAR_DATA_DIR="$Work/state" LUNAR_INSTALL_ROOT="$Work/root" "$Lunar" check 2>&1 || true)"
if echo "$Out" | grep -q "/usr/bin/owner"; then
	Report ok "check detects missing file"
else
	Report fail "check detects missing file"
	echo "$Out"
fi

echo "== fixture: file conflict"
ClashDir="$Work/pkg-clash"
rm -rf "$ClashDir"
mkdir -p "$ClashDir/rootfs/usr/bin"
printf '#!/bin/sh\necho clash\n' > "$ClashDir/rootfs/usr/bin/owner"
chmod +x "$ClashDir/rootfs/usr/bin/owner"
{
	echo "name: clash"
	echo "namespace: test"
	echo "version: 1.0"
	echo "architecture: x86_64"
	echo "installed_size: 1"
	echo "dependencies: []"
	echo "files:"
	echo "  - /usr/bin/owner"
} > "$ClashDir/meta.yaml"
tar --zstd -cf "$Work/clash.oaa" -C "$ClashDir" meta.yaml rootfs
sha256sum "$Work/clash.oaa" | awk '{print $1"  "$2}' > "$Work/clash.oaa.sha256"
Out="$(RunInstall "$Work/clash.oaa")"
if echo "$Out" | grep -qi "file conflict"; then
	Report ok "file conflict is rejected"
else
	Report fail "file conflict is rejected"
	echo "$Out"
fi

echo
if [ "$Failures" -gt 0 ]; then
	echo "$Failures checks failed"
	exit 1
fi
echo "all architecture gate checks passed"