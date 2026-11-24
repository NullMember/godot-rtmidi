#!/bin/bash
# Signs every .dylib in a folder with a Developer ID Application certificate
# and, if credentials are given, notarizes them with Apple.
#
# Usage: macos-sign-notarize.sh <folder>
#
# Environment:
#   SIGNING_IDENTITY          Optional. Defaults to the first "Developer ID Application"
#                             identity found in the keychain search list.
#   Notarization (optional; skipped if neither is set):
#   NOTARY_KEYCHAIN_PROFILE   A profile saved with `xcrun notarytool store-credentials` (local use), or
#   NOTARY_API_KEY_PATH, NOTARY_API_KEY_ID, NOTARY_API_ISSUER_ID
#                             An App Store Connect API key (CI use).
#
# Bare .dylib files can't be stapled; Gatekeeper looks the notarization ticket
# up online the first time the library is loaded.

set -euo pipefail

dir="${1:?usage: $0 <folder>}"
shopt -s nullglob
libs=("$dir"/*.dylib)
if [ ${#libs[@]} -eq 0 ]; then
	echo "No .dylib files in $dir" >&2
	exit 1
fi

identity="${SIGNING_IDENTITY:-}"
if [ -z "$identity" ]; then
	identity=$(security find-identity -v -p codesigning | grep -o '"Developer ID Application: [^"]*"' | head -1 | tr -d '"' || true)
fi
if [ -z "$identity" ]; then
	echo "No Developer ID Application signing identity found." >&2
	exit 1
fi

echo "Signing with: $identity"
for lib in "${libs[@]}"; do
	# Hardened runtime and a secure timestamp are required for notarization.
	codesign --force --timestamp --options runtime --sign "$identity" "$lib"
	codesign --verify --strict --verbose=2 "$lib"
done

if [ -n "${NOTARY_KEYCHAIN_PROFILE:-}" ]; then
	auth=(--keychain-profile "$NOTARY_KEYCHAIN_PROFILE")
elif [ -n "${NOTARY_API_KEY_PATH:-}" ]; then
	auth=(--key "$NOTARY_API_KEY_PATH" --key-id "${NOTARY_API_KEY_ID:?}" --issuer "${NOTARY_API_ISSUER_ID:?}")
else
	echo "No notarization credentials given; libraries are signed but not notarized."
	exit 0
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
zip="$work/notarize.zip"
mkdir "$work/libs"
cp "${libs[@]}" "$work/libs/"
ditto -c -k --keepParent "$work/libs" "$zip"

echo "Submitting for notarization (this usually takes a few minutes)..."
xcrun notarytool submit "$zip" "${auth[@]}" --wait --timeout 30m --output-format json > "$work/result.json"
status=$(plutil -extract status raw -o - "$work/result.json")
id=$(plutil -extract id raw -o - "$work/result.json")
echo "Notarization $id: $status"
if [ "$status" != "Accepted" ]; then
	xcrun notarytool log "$id" "${auth[@]}" || true
	exit 1
fi

for lib in "${libs[@]}"; do
	spctl --assess --type open --context context:primary-signature -vv "$lib"
done
