#!/bin/sh
# Unpack the release assets DEPS lists into deps/<repo>:
# `sh tools/deps-fetch.sh [name]'.  <ref> is a tag or `latest'.
# Anything already present is left alone.
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
only=${1:-}
rc=0

case $(uname -s) in
MINGW*|MSYS*|CYGWIN*) host=windows-x86_64.zip ;;
*) host=linux-x86_64.tar.gz ;;
esac

# The tag /releases/latest redirects to.
latest() {
	u=$(curl -fsLI -o /dev/null -w '%{url_effective}' "$1/releases/latest") || return 1
	case $u in */releases/tag/*) echo "${u##*/tag/}" ;; esac
}

# $1 name  $2 url  $3 tag  $4 assets  $5 dest
release() {
	tmp=$5.tmp
	rm -rf "$tmp"
	mkdir -p "$tmp/.x"
	for a in $(echo "$4" | tr , ' '); do
		a=$(echo "$a" | sed "s/@REF@/$3/g; s/@HOST@/$host/g")
		echo "$1: $2/releases/download/$3/$a"
		curl -fsSL --retry 2 -o "$tmp/.x/$a" "$2/releases/download/$3/$a" ||
			{ rm -rf "$tmp"; echo "$1: no asset $a in $3" >&2; return 1; }
		case $a in
		*.tar.gz) tar xzf "$tmp/.x/$a" -C "$tmp/.x" ;;
		*.zip) unzip -q "$tmp/.x/$a" -d "$tmp/.x" ;;
		*) mv "$tmp/.x/$a" "$tmp/"; continue ;;
		esac
		rm "$tmp/.x/$a"
		# Archives hold one top directory: check it, then merge it.
		for d in "$tmp/.x"/*; do
			[ ! -f "$d/.contents" ] || (cd "$d" && md5sum -c --quiet .contents) ||
				{ rm -rf "$tmp"; echo "$1: $a fails its .contents" >&2; return 1; }
			cp -a "$d/." "$tmp/"
		done
		rm -rf "$tmp/.x"/*
	done
	rm -rf "$tmp/.x"
	echo "$3" > "$tmp/.tag"
	mv "$tmp" "$5"
}

while read -r name kind url ref asset; do
	case $name in ''|\#*) continue ;; esac
	[ -z "$only" ] || [ "$only" = "$name" ] || continue
	[ "$kind" = release ] || { echo "$name: unknown kind $kind" >&2; rc=1; continue; }
	dest=$root/deps/$(basename "$url" .git)
	if [ -e "$dest" ]; then echo "$name: $dest present"; continue; fi
	[ "$ref" != latest ] || ref=$(latest "$url") || :
	[ -n "$ref" ] || { echo "$name: no release at $url" >&2; rc=1; continue; }
	release "$name" "$url" "$ref" "$asset" "$dest" || rc=1
done < "$root/DEPS"
exit $rc
